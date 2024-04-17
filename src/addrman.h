// Copyright (c) 2012 Pieter Wuille
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_ADDRMAN_H
#define BITCOIN_ADDRMAN_H


#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "sync.h"
#include "streams.h"
#include "timedata.h"
#include "util.h"
#include "fs.h"
#include "clientversion.h"
#include "hash.h"
#include "netbase.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

/**
 * Extended statistics about a CAddress
 */
class CAddrInfo : public CAddress
{


public:
    //! last try whatsoever by us (memory only)
    int64_t nLastTry;

private:
    //! where knowledge about this address first came from
    CNetAddr source;

    //! last successful connection by us
    int64_t nLastSuccess;

    //! connection attempts since last successful attempt
    int nAttempts;

    //! reference count in new sets (memory only)
    int nRefCount;

    //! in tried set? (memory only)
    bool fInTried;

    //! position in vRandom
    int nRandomPos;

    //! Address is local
    bool fLocal;

    friend class CAddrMan;

public:

    SERIALIZE_METHODS(CAddrInfo, obj)
    {
        READWRITEAS(CAddress, obj);
        READ_WRITE(obj.source, obj.nLastSuccess, obj.nAttempts);
    }

    void Init()
    {
        nLastSuccess = 0;
        nLastTry = 0;
        nAttempts = 0;
        nRefCount = 0;
        fInTried = false;
        nRandomPos = -1;
        fLocal = false;
    }

    CAddrInfo(const CAddress &addrIn, const CNetAddr &addrSource) : CAddress(addrIn), source(addrSource)
    {
        Init();
    }

    CAddrInfo() : CAddress(), source()
    {
        Init();
    }

    //! Calculate in which "tried" bucket this entry belongs
    int GetTriedBucket(const uint256 &nKey, const std::vector<bool> &asmap) const;

    //! Calculate in which "new" bucket this entry belongs, given a certain source
    int GetNewBucket(const uint256 &nKey, const CNetAddr& src, const std::vector<bool> &asmap) const;

    //! Calculate in which "new" bucket this entry belongs, using its default source
    int GetNewBucket(const uint256 &nKey, const std::vector<bool> &asmap) const
    {
        return GetNewBucket(nKey, source, asmap);
    }

    //! Calculate in which position of a bucket to store this entry.
    int GetBucketPosition(const uint256 &nKey, bool fNew, int nBucket) const;

    //! Determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool IsTerrible(int64_t nNow = GetTime()) const;

    //Determine if this entry was just tried
    bool IsJustTried(int64_t nNow = GetTime()) const;

    //! Calculate the relative chance this entry should be given when selecting nodes to connect to
    double GetChance(int64_t nNow = GetTime()) const;

    //Returns the last successful connection
    int64_t GetLastSuccess() {return nTime;}

};

/** Stochastic address manager
 *
 * Design goals:
 *  * Keep the address tables in-memory, and asynchronously dump the entire table to peers.dat.
 *  * Make sure no (localized) attacker can fill the entire table with his nodes/addresses.
 *
 * To that end:
 *  * Addresses are organized into buckets.
 *    * Addresses that have not yet been tried go into 1024 "new" buckets.
 *      * Based on the address range (/16 for IPv4) of the source of information, 64 buckets are selected at random.
 *      * The actual bucket is chosen from one of these, based on the range in which the address itself is located.
 *      * One single address can occur in up to 8 different buckets to increase selection chances for addresses that
 *        are seen frequently. The chance for increasing this multiplicity decreases exponentially.
 *      * When adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
 *        ones) is removed from it first.
 *    * Addresses of nodes that are known to be accessible go into 256 "tried" buckets.
 *      * Each address range selects at random 8 of these buckets.
 *      * The actual bucket is chosen from one of these, based on the full address.
 *      * When adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
 *        tried ones) is evicted from it, back to the "new" buckets.
 *    * Bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
 *      be observable by adversaries.
 *    * Several indexes are kept for high performance. Defining DEBUG_ADDRMAN will introduce frequent (and expensive)
 *      consistency checks for the entire data structure.
 */

//! total number of buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_COUNT 256

//! total number of buckets for new addresses
#define ADDRMAN_NEW_BUCKET_COUNT 1024

//! maximum allowed number of entries in buckets for new and tried addresses
#define ADDRMAN_BUCKET_SIZE 64

//! over how many buckets entries with tried addresses from a single group (/16 for IPv4) are spread
#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 8

//! over how many buckets entries with new addresses originating from a single group are spread
#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 64

//! in how many buckets for entries with new addresses a single address may occur
#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 8

//! how old addresses can maximally be
#define ADDRMAN_HORIZON_DAYS 30

//! after how many failed attempts we give up on a new node
#define ADDRMAN_RETRIES 3

//! how many successive failures are allowed ...
#define ADDRMAN_MAX_FAILURES 10

//! ... in at least this many days
#define ADDRMAN_MIN_FAIL_DAYS 7

//! the maximum percentage of nodes to return in a getaddr call
#define ADDRMAN_GETADDR_MAX_PCT 23

//! the maximum number of nodes to return in a getaddr call
#define ADDRMAN_GETADDR_MAX 2500

/**
 * Stochastical (IP) address manager
 */
class CAddrMan
{
friend class CAddrManTest;
private:
    //! critical section to protect the inner data structures
    mutable CCriticalSection cs;

    //! Serialization versions.
    enum Format : uint8_t {
        V0_HISTORICAL = 0,    //!< historic format, before commit e6b343d88
        V1_DETERMINISTIC = 1, //!< for pre-asmap files
        V2_ASMAP = 2,         //!< for files including asmap version
        V3_BIP155 = 3,        //!< same as V2_ASMAP plus addresses are in BIP155 format
    };

    //! The maximum format this software knows it can unserialize. Also, we always serialize
    //! in this format.
    //! The format (first byte in the serialized stream) can be higher than this and
    //! still this software may be able to unserialize the file - if the second byte
    //! (see `lowest_compatible` in `Unserialize()`) is less or equal to this.
    static constexpr Format FILE_FORMAT = Format::V3_BIP155;

    //! The initial value of a field that is incremented every time an incompatible format
    //! change is made (such that old software versions would not be able to parse and
    //! understand the new file format). This is 32 because we overtook the "key size"
    //! field which was 32 historically.
    //! @note Don't increment this. Increment `lowest_compatible` in `Serialize()` instead.
    static constexpr uint8_t INCOMPATIBILITY_BASE = 32;

    //! last used nId
    int nIdCount GUARDED_BY(cs){0};

    //! table with information about all nIds
    std::map<int, CAddrInfo> mapInfo;

    //! find an nId based on its network address
    std::map<CNetAddr, int> mapAddr;

    //! randomly-ordered vector of all nIds
    std::vector<int> vRandom;

    // number of "tried" entries
    int nTried;

    //! list of "tried" buckets
    int vvTried[ADDRMAN_TRIED_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

    //! number of (unique) "new" entries
    int nNew;

    //! list of "new" buckets
    int vvNew[ADDRMAN_NEW_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

protected:
    //! secret key to randomize bucket select with
    uint256 nKey;

    //! Find an entry.
    CAddrInfo* Find(const CNetAddr& addr, int *pnId = NULL);

    //! find an entry, creating it if necessary.
    //! nTime and nServices of the found node are updated, if necessary.
    CAddrInfo* Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId = NULL);

    //! Swap two elements in vRandom.
    void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2);

    //! Move an entry from the "new" table(s) to the "tried" table
    void MakeTried(CAddrInfo& info, int nId);

    //! Delete an entry. It must not be in tried, and have refcount 0.
    void Delete(int nId);

    //! Clear a position in a "new" table. This is the only place where entries are actually deleted.
    void ClearNew(int nUBucket, int nUBucketPos);

    //! Mark an entry "good", possibly moving it from "new" to "tried".
    void Good_(const CService &addr, int64_t nTime);

    //! Add an entry to the "new" table.
    bool Add_(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty);

    //! Mark an entry as attempted to connect.
    void Attempt_(const CService &addr, int64_t nTime);

    //! Select an address to connect to, if newOnly is set to true, only the new table is selected from.
    CAddrInfo Select_(bool newOnly);

    //! Wraps GetRandInt to allow tests to override RandomInt and make it deterministic.
    virtual int RandomInt(int nMax);

    /***
     * @brief Clears the internal collections and fills them again
     * @note the mutex should be held before this method is called
     * @note the constructor calls this directly with no lock
     */
    void Clear_()
    {
        std::vector<int>().swap(vRandom);
        nKey = GetRandHash();
        for (size_t bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvNew[bucket][entry] = -1;
            }
        }
        for (size_t bucket = 0; bucket < ADDRMAN_TRIED_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvTried[bucket][entry] = -1;
            }
        }

        nIdCount = 0;
        nTried = 0;
        nNew = 0;
        mapInfo.clear();
        mapAddr.clear();
    }

#ifdef DEBUG_ADDRMAN
    //! Perform consistency check. Returns an error code or zero.
    int Check_();
#endif

    //! Select several addresses at once.
    void GetAddr_(std::vector<CAddress> &vAddr, bool wants_addrv2);

    //! Mark an entry as currently-connected-to.
    void Connected_(const CService &addr, int64_t nTime);

    //! Mark an entry as local
    void SetLocal_(const CService &addr);

public:
    void GetAllPeers(std::map<std::string, int64_t> &info);
    // Compressed IP->ASN mapping, loaded from a file when a node starts.
    // Should be always empty if no file was provided.
    // This mapping is then used for bucketing nodes in Addrman.
    //
    // If asmap is provided, nodes will be bucketed by
    // AS they belong to, in order to make impossible for a node
    // to connect to several nodes hosted in a single AS.
    // This is done in response to Erebus attack, but also to generally
    // diversify the connections every node creates,
    // especially useful when a large fraction of nodes
    // operate under a couple of cloud providers.
    //
    // If a new asmap was provided, the existing records
    // would be re-bucketed accordingly.
    std::vector<bool> m_asmap;

    // Read asmap from provided binary file
    static std::vector<bool> DecodeAsmap(fs::path path);


    /**
     * serialized format:
     * * version byte (1 for pre-asmap files, 2 for files including asmap version)
     * * 0x20 + nKey (serialized as if it were a vector, for backward compatibility)
     * * nNew
     * * nTried
     * * number of "new" buckets XOR 2**30
     * * all nNew addrinfos in vvNew
     * * all nTried addrinfos in vvTried
     * * for each bucket:
     *   * number of elements
     *   * for each element: index
     *
     * 2**30 is xorred with the number of buckets to make addrman deserializer v0 detect it
     * as incompatible. This is necessary because it did not check the version number on
     * deserialization.
     *
     * Notice that vvTried, mapAddr and vVector are never encoded explicitly;
     * they are instead reconstructed from the other information.
     *
     * vvNew is serialized, but only used if ADDRMAN_UNKNOWN_BUCKET_COUNT didn't change,
     * otherwise it is reconstructed as well.
     *
     * This format is more complex, but significantly smaller (at most 1.5 MiB), and supports
     * changes to the ADDRMAN_ parameters without breaking the on-disk structure.
     *
     * We don't use ADD_SERIALIZE_METHODS since the serialization and deserialization code has
     * very little in common.
     */
        template<typename Stream>
        void Serialize(Stream &s_) const
            REQUIRES(!cs)
    {
        LOCK(cs);

        // Always serialize in the latest version (FILE_FORMAT).

        OverrideStream<Stream> s(&s_, s_.GetType(), s_.GetVersion() | ADDRV2_FORMAT);

        s << static_cast<uint8_t>(FILE_FORMAT);

        // Increment `lowest_compatible` iff a newly introduced format is incompatible with
        // the previous one.
        static constexpr uint8_t lowest_compatible = Format::V3_BIP155;
        s << static_cast<uint8_t>(INCOMPATIBILITY_BASE + lowest_compatible);

        s << nKey;
        s << nNew;
        s << nTried;

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
        s << nUBuckets;
        std::map<int, int> mapUnkIds;
        int nIds = 0;
        for (const auto& entry : mapInfo) {
            mapUnkIds[entry.first] = nIds;
            const CAddrInfo &info = entry.second;
            if (info.nRefCount) {
                assert(nIds != nNew); // this means nNew was wrong, oh ow
                s << info;
                nIds++;
            }
        }
        nIds = 0;
        for (const auto& entry : mapInfo) {
            const CAddrInfo &info = entry.second;
            if (info.fInTried) {
                assert(nIds != nTried); // this means nTried was wrong, oh ow
                s << info;
                nIds++;
            }
        }
        for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            int nSize = 0;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1)
                    nSize++;
            }
            s << nSize;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1) {
                    int nIndex = mapUnkIds[vvNew[bucket][i]];
                    s << nIndex;
                }
            }
        }
        // Store asmap version after bucket entries so that it
        // can be ignored by older clients for backward compatibility.
        uint256 asmap_version;
        if (m_asmap.size() != 0) {
            asmap_version = SerializeHash(m_asmap);
        }
        s << asmap_version;
    }

    template<typename Stream>
    void Unserialize(Stream& s_)
    {
        LOCK(cs);

        assert(vRandom.empty());

        Format format;
        s_ >> Using<CustomUintFormatter<1>>(format);

        int stream_version = s_.GetVersion();
        if (format >= Format::V3_BIP155) {
            // Add ADDRV2_FORMAT to the version so that the CNetAddr and CAddress
            // unserialize methods know that an address in addrv2 format is coming.
            stream_version |= ADDRV2_FORMAT;
        }

        OverrideStream<Stream> s(&s_, s_.GetType(), stream_version);

        uint8_t compat;
        s >> compat;
        const uint8_t lowest_compatible = compat - INCOMPATIBILITY_BASE;
        if (lowest_compatible > FILE_FORMAT) {
            throw std::ios_base::failure(strprintf(
                "Unsupported format of addrman database: %u. It is compatible with formats >=%u, "
                "but the maximum supported by this version of %s is %u.",
                format, lowest_compatible, PACKAGE_NAME, static_cast<uint8_t>(FILE_FORMAT)));
        }

        s >> nKey;
        s >> nNew;
        s >> nTried;
        int nUBuckets = 0;
        s >> nUBuckets;
        if (format >= Format::V1_DETERMINISTIC) {
            nUBuckets ^= (1 << 30);
        }

        if (nNew > ADDRMAN_NEW_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
            throw std::ios_base::failure("Corrupt CAddrMan serialization, nNew exceeds limit.");
        }

        if (nTried > ADDRMAN_TRIED_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE) {
            throw std::ios_base::failure("Corrupt CAddrMan serialization, nTried exceeds limit.");
        }

        // Deserialize entries from the new table.
        for (int n = 0; n < nNew; n++) {
            CAddrInfo& info = mapInfo[n];
            s >> info;
            mapAddr[info] = n;
            info.nRandomPos = vRandom.size();
            vRandom.push_back(n);
        }
        nIdCount = nNew;

        // Deserialize entries from the tried table.
        int nLost = 0;
        for (int n = 0; n < nTried; n++) {
            CAddrInfo info;
            s >> info;
            int nKBucket = info.GetTriedBucket(nKey, m_asmap);
            int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);
            if (info.IsValid() && vvTried[nKBucket][nKBucketPos] == -1) {
                info.nRandomPos = vRandom.size();
                info.fInTried = true;
                vRandom.push_back(nIdCount);
                mapInfo[nIdCount] = info;
                mapAddr[info] = nIdCount;
                vvTried[nKBucket][nKBucketPos] = nIdCount;
                nIdCount++;
            } else {
                nLost++;
            }
        }
        nTried -= nLost;

        // Store positions in the new table buckets to apply later (if possible).
        // An entry may appear in up to ADDRMAN_NEW_BUCKETS_PER_ADDRESS buckets,
        // so we store all bucket-entry_index pairs to iterate through later.
        std::vector<std::pair<int, int>> bucket_entries;

        for (int bucket = 0; bucket < nUBuckets; ++bucket) {
            int num_entries{0};
            s >> num_entries;
            for (int n = 0; n < num_entries; ++n) {
                int entry_index{0};
                s >> entry_index;
                if (entry_index >= 0 && entry_index < nNew) {
                    bucket_entries.emplace_back(bucket, entry_index);
                }
            }
        }

        // If the bucket count and asmap checksum haven't changed, then attempt
        // to restore the entries to the buckets/positions they were in before
        // serialization.
        uint256 supplied_asmap_checksum;
        if (m_asmap.size() != 0) {
            supplied_asmap_checksum = SerializeHash(m_asmap);
        }
        uint256 serialized_asmap_checksum;
        if (format >= Format::V2_ASMAP) {
            s >> serialized_asmap_checksum;
        }
        const bool restore_bucketing{nUBuckets == ADDRMAN_NEW_BUCKET_COUNT &&
                                     serialized_asmap_checksum == supplied_asmap_checksum};

        if (!restore_bucketing) {
            LogPrint("addrman", "Bucketing method was updated, re-bucketing addrman entries from disk\n");
        }

        for (auto bucket_entry : bucket_entries) {
            int bucket{bucket_entry.first};
            const int entry_index{bucket_entry.second};
            // CAddrInfo& info = mapInfo[entry_index];

            const auto it{mapInfo.find(entry_index)};
            if (it != mapInfo.end()) {
                CAddrInfo& info = (*it).second;

                // Don't store the entry in the new bucket if it's not a valid address for our addrman
                if (!info.IsValid()) continue;

                // The entry shouldn't appear in more than
                // ADDRMAN_NEW_BUCKETS_PER_ADDRESS. If it has already, just skip
                // this bucket_entry.
                if (info.nRefCount >= ADDRMAN_NEW_BUCKETS_PER_ADDRESS) continue;

                int bucket_position = info.GetBucketPosition(nKey, true, bucket);
                if (restore_bucketing && vvNew[bucket][bucket_position] == -1) {
                    // Bucketing has not changed, using existing bucket positions for the new table
                    vvNew[bucket][bucket_position] = entry_index;
                    ++info.nRefCount;
                } else {
                    // In case the new table data cannot be used (bucket count wrong or new asmap),
                    // try to give them a reference based on their primary source address.
                    bucket = info.GetNewBucket(nKey, m_asmap);
                    bucket_position = info.GetBucketPosition(nKey, true, bucket);
                    if (vvNew[bucket][bucket_position] == -1) {
                        vvNew[bucket][bucket_position] = entry_index;
                        ++info.nRefCount;
                    }
                }
            }
        }

        // Prune new entries with refcount 0 (as a result of collisions).
        int nLostUnk = 0;
        for (auto it = mapInfo.cbegin(); it != mapInfo.cend(); ) {
            if (it->second.fInTried == false && it->second.nRefCount == 0) {
                const auto itCopy = it++;
                Delete(itCopy->first);
                ++nLostUnk;
            } else {
                ++it;
            }
        }
        if (nLost + nLostUnk > 0) {
            LogPrint("addrman", "addrman lost %i new and %i tried addresses due to collisions\n", nLostUnk, nLost);
        }

        Check();
    }

    void Clear()
    {
        LOCK(cs);
        Clear_();
    }

    CAddrMan()
    {
        Clear_();
    }

    ~CAddrMan()
    {
        nKey.SetNull();
    }

    //! Return the number of (unique) addresses in all tables.
    size_t size() const
    {
        return vRandom.size();
    }

    //! Consistency check
    void Check()
    {
#ifdef DEBUG_ADDRMAN
        {
            LOCK(cs);
            int err;
            if ((err=Check_()))
                LogPrintf("ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i\n", err);
        }
#endif
    }

    //! Add a single address.
    bool Add(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        bool fRet = false;
        {
            LOCK(cs);
            Check();
            fRet |= Add_(addr, source, nTimePenalty);
            Check();
        }
        if (fRet)
            LogPrint("addrman", "Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort(), source.ToString(), nTried, nNew);
        return fRet;
    }

    //! Add multiple addresses.
    bool Add(const std::vector<CAddress> &vAddr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        int nAdd = 0;
        {
            LOCK(cs);
            Check();
            for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
                nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
            Check();
        }
        if (nAdd)
            LogPrint("addrman", "Added %i addresses from %s: %i tried, %i new\n", nAdd, source.ToString(), nTried, nNew);
        return nAdd > 0;
    }

    //! Mark an entry as accessible.
    void Good(const CService &addr, int64_t nTime = GetTime())
    {
        {
            LOCK(cs);
            Check();
            Good_(addr, nTime);
            Check();
        }
    }

    //! Mark an entry as connection attempted to.
    void Attempt(const CService &addr, int64_t nTime = GetTime())
    {
        {
            LOCK(cs);
            Check();
            Attempt_(addr, nTime);
            Check();
        }
    }

    /**
     * Choose an address to connect to.
     */
    CAddrInfo Select(bool newOnly = false)
    {
        CAddrInfo addrRet;
        {
            LOCK(cs);
            Check();
            addrRet = Select_(newOnly);
            Check();
        }
        return addrRet;
    }

    //! Return a bunch of addresses, selected at random.
    std::vector<CAddress> GetAddr(bool wants_addrv2 = false)
    {
        Check();
        std::vector<CAddress> vAddr;
        {
            LOCK(cs);
            GetAddr_(vAddr, wants_addrv2);
        }
        Check();
        return vAddr;
    }

    //! Mark an entry as currently-connected-to.
    void Connected(const CService &addr, int64_t nTime = GetTime())
    {
        {
            LOCK(cs);
            Check();
            Connected_(addr, nTime);
            Check();
        }
    }

    //! Mark an entry as currently-connected-to.
    void SetLocal(const CService &addr)
    {
        {
            LOCK(cs);
            Check();
            SetLocal_(addr);
            Check();
        }
    }

};

#endif // BITCOIN_ADDRMAN_H
