// Copyright (c) 2020-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <init.h>
#include <chainparams.h>
#include <compat.h>
#include <compat/endian.h>
#include <crypto/sha256.h>
#include <fs.h>
#include <i2p.h>
#include <netbase.h>
#include <random.h>
#include <util/strencodings.h>
#include <tinyformat.h>
#include <util/readwritefile.h>
#include <util/sock.h>
#include <util/spanparsing.h>
#include <util.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace i2p {

/**
 * Swap Standard Base64 <-> I2P Base64.
 * Standard Base64 uses `+` and `/` as last two characters of its alphabet.
 * I2P Base64 uses `-` and `~` respectively.
 * So it is easy to detect in which one is the input and convert to the other.
 * @param[in] from Input to convert.
 * @return converted `from`
 */
static std::string SwapBase64(const std::string& from)
{
    std::string to;
    to.resize(from.size());
    for (size_t i = 0; i < from.size(); ++i) {
        switch (from[i]) {
        case '-':
            to[i] = '+';
            break;
        case '~':
            to[i] = '/';
            break;
        case '+':
            to[i] = '-';
            break;
        case '/':
            to[i] = '~';
            break;
        default:
            to[i] = from[i];
            break;
        }
    }
    return to;
}

/**
 * Decode an I2P-style Base64 string.
 * @param[in] i2p_b64 I2P-style Base64 string.
 * @return decoded `i2p_b64`
 * @throw std::runtime_error if decoding fails
 */
static Binary DecodeI2PBase64(const std::string& i2p_b64)
{
    const std::string& std_b64 = SwapBase64(i2p_b64);
    bool invalid;
    Binary decoded = DecodeBase64(std_b64.c_str(), &invalid);
    if (invalid) {
        throw std::runtime_error(strprintf("Cannot decode Base64: \"%s\"", i2p_b64));
    }
    return decoded;
}

/**
 * Derive the .b32.i2p address of an I2P destination (binary).
 * @param[in] dest I2P destination.
 * @return the address that corresponds to `dest`
 * @throw std::runtime_error if conversion fails
 */
static CNetAddr DestBinToAddr(const Binary& dest)
{
    CSHA256 hasher;
    hasher.Write(dest.data(), dest.size());
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(hash);

    CNetAddr addr;
    const std::string addr_str = EncodeBase32(hash, false) + ".b32.i2p";
    if (!addr.SetSpecial(addr_str)) {
        throw std::runtime_error(strprintf("Cannot parse I2P address: \"%s\"", addr_str));
    }

    return addr;
}

/**
 * Derive the .b32.i2p address of an I2P destination (I2P-style Base64).
 * @param[in] dest I2P destination.
 * @return the address that corresponds to `dest`
 * @throw std::runtime_error if conversion fails
 */
static CNetAddr DestB64ToAddr(const std::string& dest)
{
    const Binary& decoded = DecodeI2PBase64(dest);
    return DestBinToAddr(decoded);
}

namespace sam {

Session::Session(const fs::path& private_key_file,
                 const CService& control_host)
    : m_private_key_file(private_key_file), m_control_host(control_host),
      m_control_sock(std::unique_ptr<Sock>(new Sock(INVALID_SOCKET)))
{
}

Session::~Session()
{
}

bool Session::Check()
{
    try {
        LOCK(cs_i2p);
        CreateIfNotCreatedAlready();
        return true;
    } catch (const std::runtime_error& e) {
        LogPrint("i2p","I2P: Error Checking Session: %s\n", e.what());
        CheckControlSock();
    }
    return false;
}

bool Session::Listen(Connection& conn)
{
    try {
        LOCK(cs_i2p);
        CreateIfNotCreatedAlready();
        conn.me = m_my_addr;
        conn.sock = StreamAccept();
        return true;
    } catch (const std::runtime_error& e) {
        LogPrint("i2p","I2P: Error listening: %s\n", e.what());
        CheckControlSock();
    }
    return false;
}

bool Session::Accept(Connection& conn)
{
    try {
        while (true) {

            // boost::this_thread::interruption_point();
            if (ShutdownRequested()) {
                Disconnect();
                return false;
            }


            Sock::Event occurred;
            if (!conn.sock->Wait(std::chrono::milliseconds{MAX_WAIT_FOR_IO}, Sock::RECV, &occurred)) {
                throw std::runtime_error("wait on socket failed");
            }

            if ((occurred & Sock::RECV) == 0) {
                // Timeout, no incoming connections within MAX_WAIT_FOR_IO.
                continue;
            }

            const std::string& peer_dest =
                conn.sock->RecvUntilTerminator('\n', std::chrono::milliseconds{MAX_WAIT_FOR_IO}, MAX_MSG_SIZE);

            if (ShutdownRequested()) {
                Disconnect();
                return false;
            }

            conn.peer = CService(DestB64ToAddr(peer_dest), Params().GetDefaultPort());
            return true;
        }
    } catch (const std::runtime_error& e) {
        LogPrint("i2p","I2P: Error accepting: %s\n", e.what());
        CheckControlSock();
    }
    return false;
}

bool Session::Connect(const CService& to, Connection& conn, bool& proxy_error)
{
    proxy_error = true;

    std::string session_id;
    std::unique_ptr<Sock> sock;
    conn.peer = to;

    try {
        {
            LOCK(cs_i2p);
            CreateIfNotCreatedAlready();
            session_id = m_session_id;
            conn.me = m_my_addr;
            sock = Hello();
        }

        const Reply& lookup_reply =
            SendRequestAndGetReply(*sock, strprintf("NAMING LOOKUP NAME=%s", to.ToStringIP()));

        const std::string& dest = lookup_reply.Get("VALUE");

        const Reply& connect_reply = SendRequestAndGetReply(
            *sock, strprintf("STREAM CONNECT ID=%s DESTINATION=%s SILENT=false", session_id, dest),
            false);

        const std::string& result = connect_reply.Get("RESULT");

        if (result == "OK") {
            conn.sock = std::move(sock);
            return true;
        }

        if (result == "INVALID_ID") {
            LOCK(cs_i2p);
            Disconnect();
            throw std::runtime_error("Invalid session id");
        }

        if (result == "CANT_REACH_PEER" || result == "TIMEOUT" || "KEY_NOT_FOUND") {
            proxy_error = false;
        }

        throw std::runtime_error(strprintf("\"%s\"", connect_reply.full));
    } catch (const std::runtime_error& e) {
        LogPrint("i2p","I2P: Error connecting to %s: %s\n", to.ToString(), e.what());
        CheckControlSock();
        return false;
    }
}

// Private methods

std::string Session::Reply::Get(const std::string& key) const
{
    const auto& pos = keys.find(key);
    if (pos == keys.end() || !pos->second.has_value()) {
        throw std::runtime_error(
            strprintf("Missing %s= in the reply to \"%s\": \"%s\"", key, request, full));
    }
    return pos->second.value();
}

Session::Reply Session::SendRequestAndGetReply(const Sock& sock,
                                               const std::string& request,
                                               bool check_result_ok) const
{
    sock.SendComplete(request + "\n", std::chrono::milliseconds{MAX_WAIT_FOR_IO});

    Reply reply;

    // Don't log the full "SESSION CREATE ..." because it contains our private key.
    reply.request = request.substr(0, 14) == "SESSION CREATE" ? "SESSION CREATE ..." : request;

    // It could take a few minutes for the I2P router to reply as it is querying the I2P network
    // (when doing name lookup, for example).

    reply.full = sock.RecvUntilTerminator('\n', std::chrono::minutes{3}, MAX_MSG_SIZE);

    for (const auto& kv : spanparsing::Split(reply.full, ' ')) {
        const auto& pos = std::find(kv.begin(), kv.end(), '=');
        if (pos != kv.end()) {
            reply.keys.emplace(std::string{kv.begin(), pos}, std::string{pos + 1, kv.end()});
        } else {
            reply.keys.emplace(std::string{kv.begin(), kv.end()}, boost::none);
        }
    }

    LogPrint("i2p","I2P: Handshake reply %s\n", reply.full);

    if (check_result_ok && reply.Get("RESULT") != "OK") {
        if (!ShutdownRequested()) {
            throw std::runtime_error(strprintf("Unexpected reply to \"%s\": \"%s\"", request, reply.full));
        }
    }

    return reply;
}

std::unique_ptr<Sock> Session::Hello() const
{
    auto sock = CreateSock(m_control_host);

    if (!sock) {
        throw std::runtime_error("Cannot create socket");
    }

    if (!ConnectSocketDirectly(m_control_host, *sock, nConnectTimeout)) {
        throw std::runtime_error(strprintf("Cannot connect to %s", m_control_host.ToString()));
    }

    SendRequestAndGetReply(*sock, "HELLO VERSION MIN=3.1 MAX=3.1");

    return sock;
}

void Session::CheckControlSock()
{
    LOCK(cs_i2p);

    std::string errmsg;
    if (!m_control_sock->IsConnected(errmsg)) {
        LogPrint("i2p","I2P: Control socket error: %s\n", errmsg);
        Disconnect();
    }
}

void Session::DestGenerate(const Sock& sock)
{
    // https://geti2p.net/spec/common-structures#key-certificates
    // "7" or "EdDSA_SHA512_Ed25519" - "Recent Router Identities and Destinations".
    // Use "7" because i2pd <2.24.0 does not recognize the textual form.
    const Reply& reply = SendRequestAndGetReply(sock, "DEST GENERATE SIGNATURE_TYPE=7", false);

    m_private_key = DecodeI2PBase64(reply.Get("PRIV"));
}

void Session::GenerateAndSavePrivateKey(const Sock& sock)
{
    DestGenerate(sock);

    // umask is set to 077 in init.cpp, which is ok (unless -sysperms is given)
    if (!WriteBinaryFile(m_private_key_file,
                         std::string(m_private_key.begin(), m_private_key.end()))) {
        throw std::runtime_error(
            strprintf("Cannot save I2P private key to %s", m_private_key_file));
    }
}

Binary Session::MyDestination() const
{
    // From https://geti2p.net/spec/common-structures#destination:
    // "They are 387 bytes plus the certificate length specified at bytes 385-386, which may be
    // non-zero"
    static constexpr size_t DEST_LEN_BASE = 387;
    static constexpr size_t CERT_LEN_POS = 385;

    uint16_t cert_len;
    memcpy(&cert_len, &m_private_key.at(CERT_LEN_POS), sizeof(cert_len));
    cert_len = be16toh(cert_len);

    const size_t dest_len = DEST_LEN_BASE + cert_len;

    return Binary{m_private_key.begin(), m_private_key.begin() + dest_len};
}

void Session::CreateIfNotCreatedAlready()
{
    LOCK(cs_i2p);

    std::string errmsg;
    if (m_control_sock->IsConnected(errmsg)) {
        return;
    }

    LogPrint("i2p","I2P: Creating SAM session with %s\n", m_control_host.ToString());

    auto sock = Hello();

    const std::pair<bool,std::string> i2pRead = ReadBinaryFile(m_private_key_file);
    if (i2pRead.first) {
        m_private_key.assign(i2pRead.second.begin(), i2pRead.second.end());
    } else {
        GenerateAndSavePrivateKey(*sock);
    }

    const std::string& session_id = GetRandHash().GetHex().substr(0, 10); // full is an overkill, too verbose in the logs
    const std::string& private_key_b64 = SwapBase64(EncodeBase64(m_private_key));

    SendRequestAndGetReply(*sock, strprintf("SESSION CREATE STYLE=STREAM ID=%s DESTINATION=%s",
                                            session_id, private_key_b64));

    m_my_addr = CService(DestBinToAddr(MyDestination()), Params().GetDefaultPort());
    m_session_id = session_id;
    m_control_sock = std::move(sock);

    LogPrint("i2p","I2P: SAM session created: session id=%s, my address=%s\n", m_session_id,
              m_my_addr.ToString());
}

std::unique_ptr<Sock> Session::StreamAccept()
{
    auto sock = Hello();

    const Reply& reply = SendRequestAndGetReply(
        *sock, strprintf("STREAM ACCEPT ID=%s SILENT=false", m_session_id), false);

    const std::string& result = reply.Get("RESULT");

    if (result == "OK") {
        return sock;
    }

    if (result == "INVALID_ID") {
        // If our session id is invalid, then force session re-creation on next usage.
        Disconnect();
    }

    throw std::runtime_error(strprintf("\"%s\"", reply.full));
}

void Session::Disconnect()
{
    LOCK(cs_i2p);
    try
    {
        if (m_control_sock->Get() != INVALID_SOCKET) {
            if (m_session_id.empty()) {
                LogPrint("i2p","I2P: Destroying incomplete session\n");
            } else {
                LogPrint("i2p","I2P: Destroying session %s\n", m_session_id);
            }
        }
        m_control_sock->Reset();
        m_session_id.clear();
    }
    catch(std::bad_alloc&)
    {
        // when the node is shutting down, the call above might use invalid memory resulting in a
        // std::bad_alloc exception when instantiating internal objs for handling log category
        LogPrintf("(node is probably shutting down) Destroying session=%d\n", m_session_id);
    }


}
} // namespace sam
} // namespace i2p
