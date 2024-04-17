#include <gtest/gtest.h>

#include "cc/eval.h"
#include "core_io.h"
#include "key.h"
#include "testutils.h"
#include "assetchain.h"
#include "komodo_utils.h"
#include "komodo_notary.h"
#include "komodo_structs.h"
#include "komodo_extern_globals.h"
#include "test_parse_notarisation.h"
#include "chainparamsbase.h"

#include <boost/filesystem.hpp>
#include <fstream>

// todo remove
/*komodo_state *komodo_stateptr(char *symbol,char *dest);
void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,
        uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth);
const notarized_checkpoint *komodo_npptr(int32_t height);
int32_t komodo_prevMoMheight();
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);
// method in komodo_utils.cpp:
void set_kmd_user_password_port(const std::string& ltc_config_filename);
extern char KMDUSERPASS[8705];
extern char BTCUSERPASS[8192];
extern uint16_t DEST_PORT;*/

#ifdef __WINDOWS__
// for when HOMEDRIVE and HOMEPATH is used...
std::pair<std::string, std::string> parse_drive(const std::string& in)
{
    size_t pos = in.find(":");
    std::string drive;
    std::string path;
    if (pos < 3 && pos != 0)
    {
        drive = in.substr(0, pos+1);
        path = in.substr(pos + 1);
    }
    return {drive, path};
}

char concat[1024];
bool set_home(const std::string& in)
{
    const char* profile = getenv("USERPROFILE");
    if (profile == nullptr)
    {
        // divide homedrive and homepath
        auto pair = parse_drive(in);
        std::string val = "HOMEDRIVE=" + pair.first;
        _putenv(val.c_str());
        val = "HOMEPATH=" + pair.second;
        _putenv(val.c_str());
        return true;
    }
    std::string val = "USERPROFILE=" + in;
    _putenv(val.c_str());
    return true;
}

const char* get_home()
{
    concat[0] = 0;
    const char* profile = getenv("USERPROFILE");
    if (profile == nullptr)
    {
        strcpy(concat, getenv("HOMEDRIVE"));
        strcat(concat, getenv("HOMEPATH") );
        return concat;
    }
    return profile;
}
#else
const char* get_home()
{
    return getenv("HOME");
}
bool set_home(const std::string& in)
{
    setenv("HOME", in.c_str(), true);
    return true;
}
#endif

class komodo_state_accessor : public komodo_state
{
public:
    void clear_npoints()
    {
        NPOINTS.clear();
    }
    const notarized_checkpoint *last_checkpoint()
    {
        const auto &cp = NPOINTS.back();
        return &cp;
    }
};

#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock
/***
 * Test the old way (KomodoPlatform/komodo tag 0.7.0) of doing things, to assure backwards compatibility
 */
namespace old_space {

    struct komodo_state
    {
        uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,MoM;
        int32_t SAVEDHEIGHT,CURRENT_HEIGHT,NOTARIZED_HEIGHT,MoMdepth;
        uint32_t SAVEDTIMESTAMP;
        uint64_t deposited,issued,withdrawn,approved,redeemed,shorted;
        struct notarized_checkpoint *NPOINTS; int32_t NUM_NPOINTS,last_NPOINTSi;
        //std::list<std::shared_ptr<komodo::event>> events;
        //bool add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in);
    };

    komodo_state ks_old;
    komodo_state *sp = &ks_old;
    pthread_mutex_t komodo_mutex,staked_mutex;

    /* komodo_notary.h */
    void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,
            uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth)
    {
        struct notarized_checkpoint *np;
        if ( notarized_height >= nHeight )
        {
            fprintf(stderr,"komodo_notarized_update REJECT notarized_height %d > %d nHeight\n",notarized_height,nHeight);
            return;
        }
        portable_mutex_lock(&komodo_mutex);
        sp->NPOINTS = (struct notarized_checkpoint *)realloc(sp->NPOINTS,(sp->NUM_NPOINTS+1) * sizeof(*sp->NPOINTS));
        np = &sp->NPOINTS[sp->NUM_NPOINTS++];
        memset(np,0,sizeof(*np));
        np->nHeight = nHeight;
        sp->NOTARIZED_HEIGHT = np->notarized_height = notarized_height;
        sp->NOTARIZED_HASH = np->notarized_hash = notarized_hash;
        sp->NOTARIZED_DESTTXID = np->notarized_desttxid = notarized_desttxid;
        sp->MoM = np->MoM = MoM;
        sp->MoMdepth = np->MoMdepth = MoMdepth;
        portable_mutex_unlock(&komodo_mutex);
    }

    struct notarized_checkpoint *komodo_npptr_for_height(int32_t height, int *idx)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; 
        int32_t i; 
        struct komodo_state *sp; 
        struct notarized_checkpoint *np = 0;
        sp = &old_space::ks_old;

        {
            for (i=sp->NUM_NPOINTS-1; i>=0; i--) // iterate backwards from the end of the array
            {
                *idx = i;
                np = &sp->NPOINTS[i];
                if ( np->MoMdepth != 0 
                        && height > np->notarized_height-(np->MoMdepth&0xffff) 
                        && height <= np->notarized_height )
                    return(np);
            }
        }
        *idx = -1;
        return(0);
    }

    struct notarized_checkpoint *komodo_npptr(int32_t height)
    {
        int idx;
        return komodo_npptr_for_height(height, &idx);
    }

    struct notarized_checkpoint *komodo_npptr_at(int idx)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
        sp = &old_space::ks_old;

            if (idx < sp->NUM_NPOINTS)
                return &sp->NPOINTS[idx];
        return(0);
    }

    int32_t komodo_prevMoMheight()
    {
        static uint256 zero;
        zero.SetNull();

        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; int32_t i; struct komodo_state *sp; struct notarized_checkpoint *np = 0;
        sp = &old_space::ks_old;
        {
            for (i=sp->NUM_NPOINTS-1; i>=0; i--)
            {
                np = &sp->NPOINTS[i];
                if ( np->MoM != zero )
                    return(np->notarized_height);
            }
        }
        return(0);
    }

    int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
    {
        struct notarized_checkpoint *np = 0;
        int32_t i=0,flag = 0;
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN];
        struct komodo_state *sp;

        sp = &old_space::ks_old;
        {
            if ( sp->NUM_NPOINTS > 0 )
            {
                flag = 0;
                if ( sp->last_NPOINTSi < sp->NUM_NPOINTS && sp->last_NPOINTSi > 0 ) // if we cached an NPOINT index
                {
                    np = &sp->NPOINTS[sp->last_NPOINTSi-1]; // grab the previous
                    if ( np->nHeight < nHeight ) // if that previous is below the height we are looking for
                    {
                        for (i=sp->last_NPOINTSi; i<sp->NUM_NPOINTS; i++) // move forward
                        {
                            if ( sp->NPOINTS[i].nHeight >= nHeight ) // if we found the height we are looking for (or beyond)
                            {
                                flag = 1; // notify ourselves we were here
                                break; // get out
                            }
                            np = &sp->NPOINTS[i];
                            sp->last_NPOINTSi = i;
                        }
                    }
                }
                if ( flag == 0 ) // we still haven't found what we were looking for
                {
                    np = 0;
                    for (i=0; i<sp->NUM_NPOINTS; i++) // linear search from the start
                    {
                        if ( sp->NPOINTS[i].nHeight >= nHeight )
                        {
                            break;
                        }
                        np = &sp->NPOINTS[i];
                        sp->last_NPOINTSi = i;
                    }
                }
            }
            if ( np != 0 )
            {
                if ( np->nHeight >= nHeight || (i < sp->NUM_NPOINTS && np[1].nHeight < nHeight) )
                    printf("warning: flag.%d i.%d np->ht %d [1].ht %d >= nHeight.%d\n",flag,i,np->nHeight,np[1].nHeight,nHeight);
                *notarized_hashp = np->notarized_hash;
                *notarized_desttxidp = np->notarized_desttxid;
                return(np->notarized_height);
            }
        }
        memset(notarized_hashp,0,sizeof(*notarized_hashp));
        memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
        return(0);
    }
}

namespace TestParseNotarisation {

class TestParseNotarisation : public ::testing::Test, public Eval {};


TEST(TestParseNotarisation, test_ee2fa)
{
    // ee2fa47820a31a979f9f21cb3fedbc484bf9a8957cb6c9acd0af28ced29bdfe1
    std::vector<uint8_t> opret = ParseHex("c349ff90f3bce62c1b7b49d1da0423b1a3d9b733130cce825b95b9e047c729066e020d00743a06fdb95ad5775d032b30bbb3680dac2091a0f800cf54c79fd3461ce9b31d4b4d4400");
    NotarisationData nd;
    ASSERT_TRUE(E_UNMARSHAL(opret, ss >> nd));
}

TEST(TestParseNotarisation, test__)
{
    // 576e910a1f704207bcbcf724124ff9adc5237f45cb6919589cd0aa152caec424
    std::vector<uint8_t> opret = ParseHex("b3ed7fbbfbc027caeeeec81e65489ec5d9cd47cda675a5cbb75b4a845e67cf0ef6330300b5a6bd8385feb833f3be961c9d8a46fcecd36dcdfa42ad81a20a892433722f0b4b4d44004125a06024eae24c11f36ea110acd707b041d5355b6e1b42de5e2614357999c6aa02000d26ad0300000000404b4c000000000005130300500d000061f22ba7d19fe29ac3baebd839af8b7127d1f90755534400");
    NotarisationData nd;
    // We can't parse this one
    ASSERT_FALSE(E_UNMARSHAL(opret, ss >> nd));
}

TEST(TestParseNotarisation, test__a)
{
    // be55101e6c5a93fb3611a44bd66217ad8714d204275ea4e691cfff9d65dff85c TXSCL
    std::vector<uint8_t> opret = ParseHex("fb9ea2818eec8b07f8811bab49d64379db074db478997f8114666f239bd79803cc460000d0fac4e715b7e2b917a5d79f85ece0c423d27bd3648fd39ac1dc7db8e1bd4b16545853434c00a69eab9f23d7fb63c4624973e7a9079d6ada2f327040936356d7af5e849f6d670a0003001caf7b7b9e1c9bc59d0c7a619c9683ab1dd0794b6f3ea184a19f8fda031150e700000000");
    NotarisationData nd(1);
    bool res = E_UNMARSHAL(opret, ss >> nd);
    ASSERT_TRUE(res);
}

TEST(TestParseNotarisation, test__b)
{
    // 03085dafed656aaebfda25bf43ffe9d1fb72565bb1fc8b2a12a631659f28f877 TXSCL
    std::vector<uint8_t> opret = ParseHex("48c71a10aa060eab1a43f52acefac3b81fb2a2ce310186b06141884c0501d403c246000052e6d49afd82d9ab3d97c996dd9b6a78a554ffa1625e8dadf0494bd1f8442e3e545853434c007cc5c07e3b67520fd14e23cd5b49f2aa022f411500fd3326ff91e6dc0544a1c90c0003008b69117bb1376ac8df960f785d8c208c599d3a36248c98728256bb6d4737e59600000000");
    NotarisationData nd(1);
    bool res = E_UNMARSHAL(opret, ss >> nd);
    ASSERT_TRUE(res);
}

void clear_npoints(komodo_state *sp)
{
    reinterpret_cast<komodo_state_accessor*>(sp)->clear_npoints();
}

size_t count_npoints(komodo_state *sp)
{
    return sp->NumCheckpoints();
}

const notarized_checkpoint *last_checkpoint(komodo_state *sp)
{
    return reinterpret_cast<komodo_state_accessor*>(sp)->last_checkpoint();
}

TEST(TestParseNotarisation, test_notarized_update)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    clear_npoints(sp);
    // height lower than notarized_height
    komodo_notarized_update(sp, 9, 10, uint256(), uint256(), uint256(), 1);
    EXPECT_EQ(0, count_npoints(sp));
    auto npptr = komodo_npptr(11);
    EXPECT_EQ(npptr, nullptr);

    // 1 inserted with height 10
    komodo_notarized_update(sp, 10, 8, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(1, count_npoints(sp));
    const notarized_checkpoint *tmp = last_checkpoint(sp);
    EXPECT_EQ(tmp->nHeight, 10);
    EXPECT_EQ(tmp->notarized_height, 8);
    EXPECT_EQ(tmp->MoMdepth, 2);
    clear_npoints(sp);
}

TEST(TestParseNotarisation, test_npptr)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    komodo_notarized_update(sp, 9, 10, uint256(), uint256(), uint256(), 1);
    EXPECT_EQ(0, count_npoints(sp));
    auto npptr = komodo_npptr(11);
    EXPECT_EQ(npptr, nullptr);

    // 1 inserted with height 10
    komodo_notarized_update(sp, 10, 8, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(1, count_npoints(sp));
    const notarized_checkpoint *tmp = last_checkpoint(sp);
    EXPECT_EQ(tmp->nHeight, 10);
    EXPECT_EQ(tmp->notarized_height, 8);
    EXPECT_EQ(tmp->MoMdepth, 2);
    // test komodo_npptr
    npptr = komodo_npptr(-1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(0); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(6); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(7); // one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 8);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(9); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);

    // add another with the same index
    komodo_notarized_update(sp, 10, 9, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(2, count_npoints(sp)); 

    npptr = komodo_npptr(-1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(0); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(6); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(7); // original found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 8);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(8); // new one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 9);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(9); // new one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 9);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(10); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(11); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);
    clear_npoints(sp);
}

TEST(TestParseNotarisation, test_prevMoMheight)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    EXPECT_EQ(komodo_prevMoMheight(), 0);
    uint256 mom;
    mom.SetHex("A0");
    komodo_notarized_update(sp, 10, 9, uint256(), uint256(), mom, 1);
    EXPECT_EQ(komodo_prevMoMheight(), 9);
    komodo_notarized_update(sp, 11, 10, uint256(), uint256(), mom, 1);
    EXPECT_EQ(komodo_prevMoMheight(), 10);
    komodo_notarized_update(sp, 9, 8, uint256(), uint256(), mom, 1); // we're not sorted by anything other than chronological
    EXPECT_EQ(komodo_prevMoMheight(), 8);
}

TEST(TestParseNotarisation, test_notarizeddata)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    uint256 hash;
    uint256 expected_hash;
    uint256 txid;
    uint256 expected_txid;
    auto rslt = komodo_notarizeddata(0, &hash, &txid);
    EXPECT_EQ(rslt, 0);
    EXPECT_EQ(hash, expected_hash);
    EXPECT_EQ(txid, expected_txid);

    // now add a notarization
    expected_hash.SetHex("0A");
    expected_txid.SetHex("0B");
    komodo_notarized_update(sp, 10, 9, expected_hash, expected_txid, uint256(), 1);
    rslt = komodo_notarizeddata(0, &hash, &txid); // too low
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(9, &hash, &txid); // too low
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(10, &hash, &txid); // just right, but will return nothing (still too low)
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(11, &hash, &txid); // over the height in the array, so should find the one below
    EXPECT_EQ(rslt, 9);
    EXPECT_EQ(hash, expected_hash);
    EXPECT_EQ(txid, expected_txid);
 }

TEST(TestParseNotarisation, DISABLED_OldVsNew)
{
    /***
     * This test requires a binary file of notarization data
     * as well as a long time to run. Re-enable this test to check
     * the notarization checkpoints.
     */
    chainName = assetchain("");
    char symbol[4] = { 0 };
    char dest[4] = { 0 };

    // clear any checkpoints from previous tests
    class my_komodo_state : public komodo_state
    {
    public:
        void clear_checkpoints() { return komodo_state::clear_checkpoints(); }
    };

    my_komodo_state* new_ks = reinterpret_cast<my_komodo_state*>(komodo_stateptr(symbol, dest));
    new_ks->clear_checkpoints();

    // see test_parse_notarisation.h for notarized_checkpoints
    // how many are in the array?
    std::vector<notarized_checkpoint> notarized_checkpoints = get_test_checkpoints_from_file("notarizationdata.tst");
    size_t npoints_max =  notarized_checkpoints.size();
    EXPECT_EQ(npoints_max, 8043);

    // set the MoMdepth for tests
    notarized_checkpoints[npoints_max-1].MoMdepth = 1; // set the last one to a depth of 1
    notarized_checkpoints[npoints_max-3].MoMdepth = 20; // set the one 3 from the end to a depth of 20

    notarized_checkpoints[777].MoM.SetHex("0xdead"); // set the 778th to a depth of 1
    notarized_checkpoints[777].MoMdepth = 1;

    int32_t max_chain_height = 0;
    int32_t start_chain_height = 0;
    // fill the structures
    for (size_t idx = 0; idx < npoints_max; idx++)
    {
        old_space::komodo_notarized_update(old_space::sp,
        notarized_checkpoints[idx].nHeight,
        notarized_checkpoints[idx].notarized_height,
        notarized_checkpoints[idx].notarized_hash,
        notarized_checkpoints[idx].notarized_desttxid,
        notarized_checkpoints[idx].MoM,
        notarized_checkpoints[idx].MoMdepth);

        ::komodo_notarized_update(new_ks,
        notarized_checkpoints[idx].nHeight,
        notarized_checkpoints[idx].notarized_height,
        notarized_checkpoints[idx].notarized_hash,
        notarized_checkpoints[idx].notarized_desttxid,
        notarized_checkpoints[idx].MoM,
        notarized_checkpoints[idx].MoMdepth);
        if (notarized_checkpoints[idx].nHeight > max_chain_height)
            max_chain_height = notarized_checkpoints[idx].nHeight;
        if (start_chain_height == 0 || start_chain_height > notarized_checkpoints[idx].nHeight)
            start_chain_height = notarized_checkpoints[idx].nHeight;
    }

    EXPECT_EQ(old_space::sp->NUM_NPOINTS, new_ks->NumCheckpoints() );

    // Check retrieval of notarization for height

    for (size_t i = start_chain_height - 100; i <= max_chain_height+100; i++)
    {
        int idx_old = 0;
        notarized_checkpoint *np_old = old_space::komodo_npptr_for_height(i, &idx_old);
        const notarized_checkpoint *np_new = ::komodo_npptr(i);
        if (np_old != nullptr && np_new == nullptr)
            FAIL();
        if (np_old == nullptr && np_new != nullptr)
            FAIL();
        if (np_old != nullptr && np_new != nullptr)
            EXPECT_EQ( *(np_old), *(np_new) );
        /*
        if (!equal(np_old, np_new) )
        {
            std::cout << "Chceckpoints did not match at index " << std::to_string(i);
            if (np_old == nullptr)
                std::cout << " Old is nullptr";
            else
                std::cout << ". Old has MoM depth of " << std::to_string(np_old->MoMdepth)
                        << " and a chain height of " << std::to_string(np_old->nHeight)
                        << " and a notarization height of " << std::to_string(np_old->notarized_height);
            if (np_new == nullptr)
                std::cout << ". New is nullptr";
            else
                std::cout << ". New has MoM depth of " << std::to_string(np_new->MoMdepth)
                        << " and a chain height of " << std::to_string(np_new->nHeight)
                        << " and a notarization height of " << std::to_string(np_new->notarized_height);
            std::cout << ".\n";
            all_good = false;
        }
        */
    }

    // Check retrieval of data using komodo_notarizeddata()

    for (size_t i = start_chain_height - 100; i < max_chain_height + 100; i++) {
        uint256 old_notarized_hash, old_notarized_desttxid;
        int32_t old_ret_height = old_space::komodo_notarizeddata(i, 
                &old_notarized_hash, &old_notarized_desttxid);

        uint256 new_notarized_hash;
        uint256 new_notarized_desttxid;
        int32_t new_ret_height = komodo_notarizeddata(i, &new_notarized_hash, &new_notarized_desttxid);
        EXPECT_EQ(old_ret_height, new_ret_height);
        EXPECT_EQ(old_notarized_hash, new_notarized_hash);
        EXPECT_EQ(old_notarized_desttxid, new_notarized_desttxid);
    }

    // check komodo_prevMoMheight()
    EXPECT_EQ( old_space::komodo_prevMoMheight(), ::komodo_prevMoMheight());

}

// for l in `g 'parse notarisation' ~/.komodo/debug.log | pyline 'l.split()[8]'`; do hoek decodeTx '{"hex":"'`src/komodo-cli getrawtransaction "$l"`'"}' | jq '.outputs[1].script.op_return' | pyline 'import base64; print base64.b64decode(l).encode("hex")'; done

TEST(TestParseNotarisation, FilePaths)
{
#if defined(__APPLE__)
    std::string komodo_conf = "Komodo.conf";
#else
    std::string komodo_conf = "komodo.conf";
#endif
    std::string ltc_conf = "litecoin.conf";

    // helper for home directory
    class MockDataDirectory
    {
        public:
        MockDataDirectory()
        {
            ClearDatadirCache();
            data_path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
            auto komodo_path = data_path / os_dir / kmd_dir / "regtest";
            auto ltc_path = data_path / os_dir / ltc_dir / "regtest";
            boost::filesystem::create_directories(komodo_path);
            boost::filesystem::create_directories(ltc_path);
            orig_home = get_home();
            set_home(data_path.string().c_str());
        }
        ~MockDataDirectory()
        {
            boost::filesystem::remove_all(data_path);
            set_home(orig_home.string().c_str());
            ClearDatadirCache();
        }
        bool create_config(const boost::filesystem::path& filesubpath, const std::string& user,
                const std::string& pass, uint16_t port)
        {
            std::string file = (data_path / os_dir / filesubpath).string();
            std::ofstream komodo(file);
            komodo << "rpcuser=" << user << "\n"
                    << "rpcpassword=" << pass << "\n"
                    << "rpcport=" << std::to_string(port) << "\n";
            return true;
        }
        boost::filesystem::path data_path;
        boost::filesystem::path orig_home;
#ifdef __WINDOWS__
        const boost::filesystem::path os_dir = "AppData/Roaming";
        const boost::filesystem::path kmd_dir = "Komodo";
        const boost::filesystem::path ltc_dir = "Litecoin";
#elif __APPLE__
        const boost::filesystem::path os_dir = "Library/Application Support";
        const boost::filesystem::path kmd_dir = "Komodo";
        const boost::filesystem::path ltc_dir = "Litecoin";
#else
        const boost::filesystem::path os_dir = "";
        const boost::filesystem::path kmd_dir = ".komodo";
        const boost::filesystem::path ltc_dir = ".litecoin";
#endif
    };

#ifndef __WINDOWS__ // we cannot use temp path on windows due to implementation of GetAppDir()
    SelectParams(CBaseChainParams::REGTEST);
    {
        // default
        MockDataDirectory home;
        mapArgs.erase("-datadir");
        ASSETCHAINS_P2PPORT = 0;
        ASSETCHAINS_RPCPORT = 0;
        memset(KMDUSERPASS, 0, sizeof(KMDUSERPASS) );
        memset(BTCUSERPASS, 0, sizeof(BTCUSERPASS) );
        DEST_PORT=0;
        IS_KOMODO_NOTARY = 0;
        home.create_config(home.kmd_dir / "regtest" / komodo_conf, "test1", "my_password", 1234);
        set_kmd_user_password_port("");
        EXPECT_EQ( std::string(KMDUSERPASS), std::string("test1:my_password") );
        EXPECT_EQ( std::string(BTCUSERPASS), std::string(""));
        EXPECT_EQ(DEST_PORT, 0);
        EXPECT_EQ(ASSETCHAINS_P2PPORT, 7770);
        EXPECT_EQ(ASSETCHAINS_RPCPORT, 7771);
    }
    {
        // with -datadir
        MockDataDirectory home;
        mapArgs["-datadir"] = (home.data_path / home.os_dir / home.kmd_dir).string();
        ASSETCHAINS_P2PPORT = 0;
        ASSETCHAINS_RPCPORT = 0;
        memset(KMDUSERPASS, 0, sizeof(KMDUSERPASS) );
        memset(BTCUSERPASS, 0, sizeof(BTCUSERPASS) );
        DEST_PORT=0;
        IS_KOMODO_NOTARY = 0;
        std::string expected_kmd("test1:my_password");
        home.create_config(home.kmd_dir / "regtest" / komodo_conf, "test1", "my_password", 1234);
        set_kmd_user_password_port("");
        EXPECT_EQ( std::string(KMDUSERPASS), std::string("test1:my_password") );
        EXPECT_EQ( std::string(BTCUSERPASS), std::string(""));
        EXPECT_EQ(DEST_PORT, 0);
        EXPECT_EQ(ASSETCHAINS_P2PPORT, 7770);
        EXPECT_EQ(ASSETCHAINS_RPCPORT, 7771);
    }
    {
        // with -notary
        MockDataDirectory home;
        mapArgs["-datadir"] = (home.data_path / home.os_dir / home.kmd_dir).string();
        ASSETCHAINS_P2PPORT = 0;
        ASSETCHAINS_RPCPORT = 0;
        memset(KMDUSERPASS, 0, sizeof(KMDUSERPASS) );
        memset(BTCUSERPASS, 0, sizeof(BTCUSERPASS) );
        DEST_PORT=0;
        IS_KOMODO_NOTARY = 1;
        std::string expected_kmd("test1:my_password");
        home.create_config(home.kmd_dir / "regtest" / komodo_conf, "test1", "my_password", 1234);
        home.create_config(home.ltc_dir / "regtest" / ltc_conf, "test2", "ltc_password", 5678);
        set_kmd_user_password_port((home.ltc_dir / "regtest" / ltc_conf).string());
        EXPECT_EQ(std::string(KMDUSERPASS), std::string("test1:my_password"));
        EXPECT_EQ(std::string(BTCUSERPASS), std::string("test2:ltc_password"));
        EXPECT_EQ(DEST_PORT, 5678);
        EXPECT_EQ(ASSETCHAINS_P2PPORT, 7770);
        EXPECT_EQ(ASSETCHAINS_RPCPORT, 7771);
    }
#endif // #ifndef __WINDOWS__
}

} // namespace TestParseNotarisation
