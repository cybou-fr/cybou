// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/operation_submit.h>
#include <cybou/p2p/session.h>

#include <cybou/chunk_id.h>
#include <cybou/identity_crypto.h>
#include <cybou/protocol_limits.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/node_runtime.h>

#include <boost/asio/ip/address.hpp>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <poll.h>
#endif

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace cybou::p2p {
namespace {
constexpr size_t HEADER_SIZE{10};
constexpr size_t HELLO_SIZE{80};
constexpr auto BLOCK_TRANSFER_TIMEOUT{std::chrono::seconds{5}};
constexpr auto TLS_HANDSHAKE_TIMEOUT{std::chrono::seconds{10}};
constexpr std::string_view TLS_EXPORTER_LABEL{"EXPORTER-CYBOU-CYP2-V5"};

struct TlsContexts {
    SSL_CTX* client{nullptr};
    SSL_CTX* server{nullptr};
    TlsContexts() = default;
    TlsContexts(const TlsContexts&) = delete;
    TlsContexts& operator=(const TlsContexts&) = delete;
    TlsContexts(TlsContexts&& other) noexcept
        : client{std::exchange(other.client, nullptr)}, server{std::exchange(other.server, nullptr)} {}
    ~TlsContexts()
    {
        SSL_CTX_free(client);
        SSL_CTX_free(server);
    }
};

std::optional<TlsContexts> CreateTlsContexts()
{
    TlsContexts contexts;
    contexts.client = SSL_CTX_new(TLS_method());
    contexts.server = SSL_CTX_new(TLS_method());
    if (!contexts.client || !contexts.server) return std::nullopt;
    for (SSL_CTX* ctx : {contexts.client, contexts.server}) {
        if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1 ||
            SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION) != 1 ||
            SSL_CTX_set1_groups_list(ctx, "X25519MLKEM768") != 1) return std::nullopt;
        SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> key_context{
        EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free};
    if (!key_context || EVP_PKEY_keygen_init(key_context.get()) <= 0 ||
        EVP_PKEY_CTX_set_group_name(key_context.get(), "prime256v1") <= 0) return std::nullopt;
    EVP_PKEY* raw_key{nullptr};
    if (EVP_PKEY_generate(key_context.get(), &raw_key) <= 0) return std::nullopt;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key{raw_key, EVP_PKEY_free};
    std::unique_ptr<X509, decltype(&X509_free)> certificate{X509_new(), X509_free};
    if (!certificate || X509_set_version(certificate.get(), 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1) != 1 ||
        !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0) ||
        !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 24 * 60 * 60) ||
        X509_set_pubkey(certificate.get(), key.get()) != 1) return std::nullopt;
    X509_NAME* subject = X509_get_subject_name(certificate.get());
    constexpr unsigned char common_name[]{'C','Y','B','O','U',' ','C','Y','P','2'};
    if (!subject || X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, common_name,
            sizeof(common_name), -1, 0) != 1 || X509_set_issuer_name(certificate.get(), subject) != 1 ||
        X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0 ||
        SSL_CTX_use_certificate(contexts.server, certificate.get()) != 1 ||
        SSL_CTX_use_PrivateKey(contexts.server, key.get()) != 1 ||
        SSL_CTX_check_private_key(contexts.server) != 1) return std::nullopt;

    return std::optional<TlsContexts>{std::move(contexts)};
}

SSL_CTX* CreateStableServerTlsContext(const std::filesystem::path& certificate_chain_file,
    const std::filesystem::path& private_key_file)
{
    SSL_CTX* context = SSL_CTX_new(TLS_method());
    if (!context) return nullptr;
    if (SSL_CTX_set_min_proto_version(context, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(context, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set1_groups_list(context, "X25519MLKEM768") != 1) {
        SSL_CTX_free(context);
        return nullptr;
    }
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
    const auto certificate_path = certificate_chain_file.string();
    const auto private_key_path = private_key_file.string();
    if (SSL_CTX_use_certificate_chain_file(context, certificate_path.c_str()) != 1 ||
        SSL_CTX_use_PrivateKey_file(context, private_key_path.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(context) != 1) {
        SSL_CTX_free(context);
        return nullptr;
    }
    return context;
}

std::optional<std::array<unsigned char, 32>> CertificateSpkiSha256(X509* certificate)
{
    if (!certificate) return std::nullopt;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> public_key{
        X509_get_pubkey(certificate), EVP_PKEY_free};
    if (!public_key) return std::nullopt;
    const int der_size = i2d_PUBKEY(public_key.get(), nullptr);
    if (der_size <= 0) return std::nullopt;
    std::vector<unsigned char> der(static_cast<size_t>(der_size));
    unsigned char* der_cursor = der.data();
    if (i2d_PUBKEY(public_key.get(), &der_cursor) != der_size) return std::nullopt;
    std::array<unsigned char, 32> digest{};
    unsigned int digest_size{0};
    if (EVP_Digest(der.data(), der.size(), digest.data(), &digest_size, EVP_sha256(), nullptr) != 1 ||
        digest_size != digest.size()) return std::nullopt;
    return digest;
}

bool MatchesPeerSpkiPin(SSL* ssl, const std::array<unsigned char, 32>& expected)
{
    std::unique_ptr<X509, decltype(&X509_free)> certificate{SSL_get1_peer_certificate(ssl), X509_free};
    const auto actual = CertificateSpkiSha256(certificate.get());
    return actual && CRYPTO_memcmp(actual->data(), expected.data(), expected.size()) == 0;
}

SSL_CTX* TlsContext(const bool server)
{
    static const auto contexts = CreateTlsContexts();
    if (!contexts) return nullptr;
    return server ? contexts->server : contexts->client;
}

int TlsBioCreate(BIO* bio)
{
    BIO_set_init(bio, 1);
    BIO_set_shutdown(bio, BIO_NOCLOSE);
    return 1;
}

int TlsBioDestroy(BIO* bio)
{
    if (!bio) return 0;
    BIO_set_init(bio, 0);
    BIO_set_data(bio, nullptr);
    return 1;
}

int TlsBioRead(BIO* bio, char* out, const size_t length, size_t* read_bytes)
{
    BIO_clear_retry_flags(bio);
    *read_bytes = 0;
    if (!BIO_get_init(bio) || !out || length == 0) return 0;
    auto* socket = static_cast<boost::asio::ip::tcp::socket*>(BIO_get_data(bio));
    if (!socket) return 0;
    boost::system::error_code error;
    const auto count = socket->read_some(boost::asio::buffer(out, length), error);
    if (!error) { *read_bytes = count; return 1; }
    if (error == boost::asio::error::would_block || error == boost::asio::error::try_again) {
        BIO_set_retry_read(bio);
    }
    return 0;
}

int TlsBioWrite(BIO* bio, const char* input, const size_t length, size_t* written_bytes)
{
    BIO_clear_retry_flags(bio);
    *written_bytes = 0;
    if (!BIO_get_init(bio) || !input || length == 0) return 0;
    auto* socket = static_cast<boost::asio::ip::tcp::socket*>(BIO_get_data(bio));
    if (!socket) return 0;
    boost::system::error_code error;
    const auto count = socket->write_some(boost::asio::buffer(input, length), error);
    if (!error) { *written_bytes = count; return 1; }
    if (error == boost::asio::error::would_block || error == boost::asio::error::try_again) {
        BIO_set_retry_write(bio);
    }
    return 0;
}

long TlsBioControl(BIO* bio, const int command, const long argument, void*)
{
    switch (command) {
    case BIO_CTRL_FLUSH: return 1;
    case BIO_CTRL_GET_CLOSE: return BIO_get_shutdown(bio);
    case BIO_CTRL_SET_CLOSE: BIO_set_shutdown(bio, static_cast<int>(argument)); return 1;
    case BIO_CTRL_PENDING:
    case BIO_CTRL_WPENDING:
    case BIO_CTRL_EOF:
        return 0;
    default:
        return 0;
    }
}

BIO_METHOD* TlsBioMethod()
{
    static BIO_METHOD* method = [] {
        BIO_METHOD* value = BIO_meth_new(BIO_TYPE_SOURCE_SINK | BIO_get_new_index(), "CYBOU Asio socket");
        if (!value || BIO_meth_set_create(value, TlsBioCreate) != 1 ||
            BIO_meth_set_destroy(value, TlsBioDestroy) != 1 ||
            BIO_meth_set_read_ex(value, TlsBioRead) != 1 ||
            BIO_meth_set_write_ex(value, TlsBioWrite) != 1 ||
            BIO_meth_set_ctrl(value, TlsBioControl) != 1) {
            BIO_meth_free(value);
            return static_cast<BIO_METHOD*>(nullptr);
        }
        return value;
    }();
    return method;
}

void Put64(std::vector<unsigned char>& out, uint64_t value)
{
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t Read64(const unsigned char* data)
{
    uint64_t value{0};
    for (int i = 0; i < 8; ++i) value |= uint64_t{data[i]} << (8 * i);
    return value;
}

void Put32(std::vector<unsigned char>& out, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint32_t Read32(const unsigned char* data)
{
    uint32_t value{0};
    for (int i = 0; i < 4; ++i) value |= uint32_t{data[i]} << (8 * i);
    return value;
}

bool IsZeroChunkId(const ChunkId& id)
{
    return std::all_of(id.begin(), id.end(), [](const auto byte) { return byte == 0; });
}

bool IsSupportedMessageType(const uint8_t type)
{
    switch (static_cast<MessageType>(type)) {
    case MessageType::HELLO:
    case MessageType::STORAGE_PROOF_REQUEST:
    case MessageType::STORAGE_PROOF:
    case MessageType::PING:
    case MessageType::PONG:
    case MessageType::BLOCK_META:
    case MessageType::BLOCK_DATA:
    case MessageType::BLOCKS_END:
    case MessageType::OP_META:
    case MessageType::OP_DATA:
    case MessageType::OP_RESULT:
    case MessageType::GET_BLOCKS:
    case MessageType::BLOCK_ANNOUNCE:
    case MessageType::BLOCK_RESULT:
    case MessageType::GET_PEERS:
    case MessageType::PEERS:
    case MessageType::PUT_AUTHORIZED_CHUNK:
    case MessageType::AUTHORIZED_CHUNK_DATA:
    case MessageType::CHUNK_ADMISSION_RESULT:
    case MessageType::GET_CHUNK_BY_ID:
    case MessageType::CHUNK_DATA:
    case MessageType::GET_CHUNK_AUTHORIZATION_PROOF:
    case MessageType::CHUNK_AUTHORIZATION_PROOF:
    case MessageType::OP_POLL:
    case MessageType::VALIDATION_ATTESTATION_POLL:
    case MessageType::VALIDATION_ATTESTATION:
        return true;
    default:
        return false;
    }
}
} // namespace

std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame)
{
    if (frame.payload.size() > MAX_FRAME_PAYLOAD ||
        !IsSupportedMessageType(static_cast<uint8_t>(frame.type))) return std::nullopt;
    std::vector<unsigned char> bytes{'C', 'Y', 'P', '2', WIRE_VERSION, static_cast<unsigned char>(frame.type)};
    const auto size = static_cast<uint32_t>(frame.payload.size());
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(size >> (8 * i)));
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}

std::optional<Frame> DecodeFrame(std::span<const unsigned char> bytes)
{
    if (bytes.size() < HEADER_SIZE || !std::equal(bytes.begin(), bytes.begin() + 4, "CYP2") ||
        bytes[4] != WIRE_VERSION || !IsSupportedMessageType(bytes[5])) return std::nullopt;
    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{bytes[6 + i]} << (8 * i);
    const auto type = static_cast<MessageType>(bytes[5]);
    if (size > MAX_FRAME_PAYLOAD || bytes.size() != HEADER_SIZE + size) return std::nullopt;
    return Frame{static_cast<MessageType>(bytes[5]),
        std::vector<unsigned char>{bytes.begin() + HEADER_SIZE, bytes.end()}};
}

std::vector<unsigned char> EncodeHello(const Hello& hello)
{
    std::vector<unsigned char> out;
    out.reserve(HELLO_SIZE);
    out.insert(out.end(), hello.network_binding.begin(), hello.network_binding.end());
    Put64(out, hello.finalized_height);
    out.insert(out.end(), hello.finalized_tip.begin(), hello.finalized_tip.end());
    Put64(out, hello.nonce);
    return out;
}

std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes)
{
    if (bytes.size() != HELLO_SIZE) return std::nullopt;
    Hello hello;
    std::copy_n(bytes.begin(), 32, hello.network_binding.begin());
    hello.finalized_height = Read64(bytes.data() + 32);
    std::copy_n(bytes.begin() + 40, 32, hello.finalized_tip.begin());
    hello.nonce = Read64(bytes.data() + 72);
    if (hello.network_binding.IsNull() || hello.finalized_tip.IsNull() || hello.nonce == 0) return std::nullopt;
    return hello;
}

std::vector<unsigned char> EncodePeersPayload(const std::vector<std::pair<std::string, uint16_t>>& peers)
{
    std::vector<unsigned char> out;
    const uint8_t count = static_cast<uint8_t>(std::min<size_t>(peers.size(), MAX_PEER_DISCOVERY_ENTRIES));
    out.push_back(count);
    size_t added = 0;
    for (const auto& [host, port] : peers) {
        if (added >= count) break;
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (addr.is_v4()) {
            out.push_back(4);
            const auto bytes = addr.to_v4().to_bytes();
            out.insert(out.end(), bytes.begin(), bytes.end());
            out.push_back(static_cast<unsigned char>(port & 0xFF));
            out.push_back(static_cast<unsigned char>((port >> 8) & 0xFF));
            ++added;
        } else if (addr.is_v6()) {
            out.push_back(6);
            const auto bytes = addr.to_v6().to_bytes();
            out.insert(out.end(), bytes.begin(), bytes.end());
            out.push_back(static_cast<unsigned char>(port & 0xFF));
            out.push_back(static_cast<unsigned char>((port >> 8) & 0xFF));
            ++added;
        }
    }
    out[0] = static_cast<uint8_t>(added);
    return out;
}

std::optional<std::vector<std::pair<std::string, uint16_t>>> DecodePeersPayload(std::span<const unsigned char> bytes)
{
    if (bytes.empty()) return std::nullopt;
    const uint8_t count = bytes[0];
    if (count > MAX_PEER_DISCOVERY_ENTRIES) return std::nullopt;
    size_t offset = 1;
    std::vector<std::pair<std::string, uint16_t>> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
        if (offset >= bytes.size()) return std::nullopt;
        const uint8_t family = bytes[offset++];
        if (family == 4) {
            if (offset + 6 > bytes.size()) return std::nullopt;
            boost::asio::ip::address_v4::bytes_type v4_bytes;
            std::copy_n(bytes.begin() + offset, 4, v4_bytes.begin());
            offset += 4;
            const uint16_t port = static_cast<uint16_t>(bytes[offset]) |
                (static_cast<uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;
            if (port != 0) {
                result.emplace_back(boost::asio::ip::make_address_v4(v4_bytes).to_string(), port);
            }
        } else if (family == 6) {
            if (offset + 18 > bytes.size()) return std::nullopt;
            boost::asio::ip::address_v6::bytes_type v6_bytes;
            std::copy_n(bytes.begin() + offset, 16, v6_bytes.begin());
            offset += 16;
            const uint16_t port = static_cast<uint16_t>(bytes[offset]) |
                (static_cast<uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;
            if (port != 0) {
                result.emplace_back(boost::asio::ip::make_address_v6(v6_bytes).to_string(), port);
            }
        } else {
            return std::nullopt;
        }
    }
    if (offset != bytes.size()) return std::nullopt;
    return result;
}

bool MatchesKnownFinalizedChain(const CybouNodeRuntime& runtime, const Hello& peer)
{
    if (peer.finalized_tip.IsNull()) return false;
    const auto status = runtime.GetStatus();
    if (!status.is_initialized) return false;
    if (peer.finalized_height == 0) {
        return peer.finalized_tip == runtime.GetNetworkGenesis().GetGenesisAnchor();
    }
    if (peer.finalized_height > status.finalized_height) return true;
    const auto known = runtime.GetBlockAtHeight(peer.finalized_height);
    return !known || ComputeBlockId(known->block) == peer.finalized_tip;
}

PeerSession::PeerSession(boost::asio::ip::tcp::socket socket, const TransportRole transport_role,
    TlsSessionConfig tls_config)
    : m_socket{std::move(socket)}, m_transport_role{transport_role}, m_tls_config{std::move(tls_config)}
{
    boost::system::error_code ec;
    m_socket.non_blocking(true, ec);
    if (ec) m_socket.close();
}

PeerSession::~PeerSession()
{
    SSL_free(m_ssl);
    SSL_CTX_free(m_owned_ssl_context);
}

bool PeerSession::AdvanceTlsOperation(const int result,
    const std::chrono::steady_clock::time_point deadline)
{
    const int error = SSL_get_error(m_ssl, result);
    if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) return false;
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return true;

    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    const int timeout_ms = static_cast<int>(std::clamp<int64_t>(remaining.count(), 1, 100));

#if defined(_WIN32)
    WSAPOLLFD pfd{};
    pfd.fd = m_socket.native_handle();
    pfd.events = (error == SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT;
    const int poll_res = WSAPoll(&pfd, 1, timeout_ms);
#else
    pollfd pfd{};
    pfd.fd = m_socket.native_handle();
    pfd.events = (error == SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT;
    const int poll_res = ::poll(&pfd, 1, timeout_ms);
#endif

    if (poll_res < 0) return false;
    return true;
}

bool PeerSession::EstablishSecureTransport(const std::chrono::steady_clock::time_point deadline)
{
    if (!m_socket.is_open() || m_ssl) return false;
    // The caller must provide the actual transport role. Inferring roles from
    // socket endpoints is unreliable across NAT and IPv4/IPv6 mappings.
    const bool server = m_transport_role == TransportRole::SERVER;
    SSL_CTX* context{nullptr};
    if (server && !m_tls_config.certificate_chain_file.empty() &&
        !m_tls_config.private_key_file.empty() && !m_tls_config.expected_server_spki_sha256) {
        m_owned_ssl_context = CreateStableServerTlsContext(m_tls_config.certificate_chain_file,
            m_tls_config.private_key_file);
        context = m_owned_ssl_context;
    } else {
        if (!m_tls_config.certificate_chain_file.empty() || !m_tls_config.private_key_file.empty() ||
            (server && m_tls_config.expected_server_spki_sha256)) return false;
        context = TlsContext(server);
    }
    if (!context) return false;
    m_ssl = SSL_new(context);
    BIO_METHOD* bio_method = TlsBioMethod();
    BIO* bio = bio_method ? BIO_new(bio_method) : nullptr;
    if (!m_ssl || !bio) {
        BIO_free(bio);
        SSL_free(std::exchange(m_ssl, nullptr));
        return false;
    }
    BIO_set_data(bio, &m_socket);
    SSL_set_bio(m_ssl, bio, bio);
    if (server) SSL_set_accept_state(m_ssl);
    else SSL_set_connect_state(m_ssl);
    while (std::chrono::steady_clock::now() < deadline) {
        const int result = SSL_do_handshake(m_ssl);
        if (result == 1) {
            const char* negotiated_group = SSL_get0_group_name(m_ssl);
            if (SSL_version(m_ssl) != TLS1_3_VERSION || !negotiated_group ||
                std::string_view{negotiated_group} != "X25519MLKEM768" ||
                (m_tls_config.expected_server_spki_sha256 &&
                    !MatchesPeerSpkiPin(m_ssl, *m_tls_config.expected_server_spki_sha256)) ||
                SSL_export_keying_material(m_ssl, m_tls_exporter.data(), m_tls_exporter.size(),
                    TLS_EXPORTER_LABEL.data(), TLS_EXPORTER_LABEL.size(), nullptr, 0, 0) != 1) break;
            return true;
        }
        if (!AdvanceTlsOperation(result, deadline)) break;
    }
    SSL_free(std::exchange(m_ssl, nullptr));
    boost::system::error_code ignored;
    m_socket.close(ignored);
    return false;
}

std::optional<std::array<unsigned char, 32>> TlsCertificateSpkiSha256(
    const std::filesystem::path& certificate_file)
{
    const auto path = certificate_file.string();
    std::unique_ptr<BIO, decltype(&BIO_free)> bio{BIO_new_file(path.c_str(), "rb"), BIO_free};
    if (!bio) return std::nullopt;
    std::unique_ptr<X509, decltype(&X509_free)> certificate{PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free};
    return CertificateSpkiSha256(certificate.get());
}

bool PeerSession::ReadExact(unsigned char* out, size_t length, std::chrono::steady_clock::time_point deadline)
{
    if (!m_ssl) return false;
    size_t done{0};
    while (done < length && std::chrono::steady_clock::now() < deadline) {
        size_t count{0};
        const int result = SSL_read_ex(m_ssl, out + done, length - done, &count);
        if (result == 1 && count != 0) {
            done += count;
            continue;
        }
        if (result != 1 && AdvanceTlsOperation(result, deadline)) continue;
        {
            boost::system::error_code close_ec;
            m_socket.close(close_ec);
            return false;
        }
    }
    return done == length;
}

bool PeerSession::WriteExact(const unsigned char* bytes, size_t length,
    std::chrono::steady_clock::time_point deadline)
{
    if (!m_ssl) return false;
    size_t done{0};
    while (done < length && std::chrono::steady_clock::now() < deadline) {
        size_t count{0};
        const int result = SSL_write_ex(m_ssl, bytes + done, length - done, &count);
        if (result == 1 && count != 0) {
            done += count;
            continue;
        }
        if (result != 1 && AdvanceTlsOperation(result, deadline)) continue;
        {
            boost::system::error_code close_ec;
            m_socket.close(close_ec);
            return false;
        }
    }
    return done == length;
}

bool PeerSession::Write(const Frame& frame)
{
    return Write(frame, std::chrono::steady_clock::now() + std::chrono::seconds(5));
}

bool PeerSession::Write(const Frame& frame, std::chrono::steady_clock::time_point deadline)
{
    const auto bytes = EncodeFrame(frame);
    if (!bytes) return false;
    return WriteExact(bytes->data(), bytes->size(), deadline);
}

std::optional<Frame> PeerSession::Read(std::chrono::steady_clock::time_point deadline)
{
    m_last_read_status = ReadStatus::UNAVAILABLE;
    std::array<unsigned char, HEADER_SIZE> header{};
    if (!ReadExact(header.data(), header.size(), deadline)) return std::nullopt;
    if (!std::equal(header.begin(), header.begin() + 4, "CYP2") ||
        header[4] != WIRE_VERSION || !IsSupportedMessageType(header[5])) {
        m_last_read_status = ReadStatus::INVALID_FRAME;
        return std::nullopt;
    }
    uint32_t size{0};
    for (int i = 0; i < 4; ++i) size |= uint32_t{header[6 + i]} << (8 * i);
    if (size > MAX_FRAME_PAYLOAD) {
        m_last_read_status = ReadStatus::INVALID_FRAME;
        return std::nullopt;
    }
    std::vector<unsigned char> bytes{header.begin(), header.end()};
    bytes.resize(HEADER_SIZE + size);
    if (size && !ReadExact(bytes.data() + HEADER_SIZE, size, deadline)) {
        return std::nullopt;
    }
    auto frame = DecodeFrame(bytes);
    m_last_read_status = frame ? ReadStatus::OK : ReadStatus::INVALID_FRAME;
    return frame;
}

std::optional<Frame> PeerSession::Read()
{
    return Read(std::chrono::steady_clock::now() + std::chrono::seconds(5));
}

bool PeerSession::SendFrame(const Frame& frame, const std::chrono::steady_clock::time_point deadline)
{
    return Write(frame, deadline);
}

std::optional<Frame> PeerSession::ReceiveFrame(const std::chrono::steady_clock::time_point deadline)
{
    return Read(deadline);
}

std::vector<unsigned char> StorageProofMessage(const Hello& signer,const Hello& verifier,
    const std::span<const unsigned char> tls_exporter)
{
    if (tls_exporter.size() != 32) return {};
    constexpr std::string_view DOMAIN{"CYBOU/CYP2/PROVIDER-PROOF/v5"};
    std::vector<unsigned char> message(DOMAIN.begin(),DOMAIN.end());
    message.insert(message.end(), tls_exporter.begin(), tls_exporter.end());
    const auto signer_bytes=EncodeHello(signer),verifier_bytes=EncodeHello(verifier);
    message.insert(message.end(),signer_bytes.begin(),signer_bytes.end());
    message.insert(message.end(),verifier_bytes.begin(),verifier_bytes.end());
    return message;
}

namespace {
constexpr std::size_t PROVIDER_ED25519_KEY{32};
constexpr std::size_t PROVIDER_MLDSA_KEY{1312};
constexpr std::size_t PROVIDER_ED25519_SIG{64};
constexpr std::size_t PROVIDER_MLDSA_SIG{2420};
constexpr std::size_t PROVIDER_PROOF_SIZE{PROVIDER_ED25519_KEY + PROVIDER_MLDSA_KEY + PROVIDER_ED25519_SIG + PROVIDER_MLDSA_SIG};
} // namespace

std::optional<StorageId> VerifyStorageProof(const std::span<const unsigned char> payload,
    const std::span<const unsigned char> message)
{
    if (payload.size() != PROVIDER_PROOF_SIZE) return std::nullopt;
    IdentityHybridPublicKey key{.purpose = IdentityKeyPurpose::STORAGE};
    std::copy_n(payload.begin(), PROVIDER_ED25519_KEY, key.ed25519.begin());
    key.ml_dsa.assign(payload.begin() + PROVIDER_ED25519_KEY, payload.begin() + PROVIDER_ED25519_KEY + PROVIDER_MLDSA_KEY);
    IdentityHybridSignature signature;
    const auto sig = payload.subspan(PROVIDER_ED25519_KEY + PROVIDER_MLDSA_KEY);
    std::copy_n(sig.begin(), PROVIDER_ED25519_SIG, signature.ed25519.begin());
    signature.ml_dsa.assign(sig.begin() + PROVIDER_ED25519_SIG, sig.end());
    if (!VerifyIdentityMessage(key, signature, message)) return std::nullopt;
    // ProviderID commits to both public keys under a provider domain.
    constexpr std::string_view DOMAIN{"CYBOU/PROVIDER-ID/v1"};
    std::vector<unsigned char> id_input(DOMAIN.begin(), DOMAIN.end());
    id_input.insert(id_input.end(), payload.begin(), payload.begin() + PROVIDER_ED25519_KEY + PROVIDER_MLDSA_KEY);
    return ComputeBlake3Digest(id_input);
}

bool PeerSession::Handshake(const Hello& local)
{
    m_peer.reset();
    m_peer_provider_id.reset();
    m_local.reset();
    m_handshake_status = HandshakeStatus::INVALID_LOCAL;
    if (local.network_binding.IsNull() || local.finalized_tip.IsNull() || local.nonce == 0) return false;
    m_handshake_status = HandshakeStatus::UNAVAILABLE;
    if (!EstablishSecureTransport(std::chrono::steady_clock::now() + TLS_HANDSHAKE_TIMEOUT)) return false;
    if (!Write(Frame{MessageType::HELLO, EncodeHello(local)})) return false;
    const auto frame = Read();
    if (!frame) {
        if (m_last_read_status == ReadStatus::INVALID_FRAME) m_handshake_status = HandshakeStatus::INVALID_PEER;
        return false;
    }
    m_handshake_status = HandshakeStatus::INVALID_PEER;
    if (frame->type != MessageType::HELLO) return false;
    const auto peer = DecodeHello(frame->payload);
    if (!peer || peer->nonce == local.nonce) return false;
    if (peer->network_binding != local.network_binding) {
        m_handshake_status = HandshakeStatus::WRONG_NETWORK;
        return false;
    }
    m_peer = *peer;
    m_local = local;
    m_handshake_status = HandshakeStatus::CONNECTED;
    return true;
}

std::optional<StorageId> PeerSession::ProveStorageIdentity()
{
    if (!m_peer || !m_local) return std::nullopt;
    if (m_peer_provider_id) return m_peer_provider_id;
    std::array<unsigned char, 32> challenge{};
    if (RAND_bytes(challenge.data(), challenge.size()) != 1) return std::nullopt;
    if (!Write(Frame{MessageType::STORAGE_PROOF_REQUEST, {challenge.begin(), challenge.end()}})) return std::nullopt;
    const auto response = Read();
    if (!response || response->type != MessageType::STORAGE_PROOF) return std::nullopt;
    auto message = StorageProofMessage(*m_peer, *m_local, m_tls_exporter);
    message.insert(message.end(), challenge.begin(), challenge.end());
    m_peer_provider_id = VerifyStorageProof(response->payload, message);
    return m_peer_provider_id;
}

bool PeerSession::Ping(uint64_t nonce)
{
    if (!m_peer || nonce == 0) return false;
    std::vector<unsigned char> payload;
    Put64(payload, nonce);
    if (!Write(Frame{MessageType::PING, payload})) return false;
    const auto response = Read();
    return response && response->type == MessageType::PONG && response->payload == payload;
}

bool PeerSession::AnswerPing()
{
    if (!m_peer) return false;
    const auto request = Read();
    return request && request->type == MessageType::PING && request->payload.size() == 8 &&
        Write(Frame{MessageType::PONG, request->payload});
}

bool PeerSession::SendBlock(uint64_t height, std::span<const unsigned char> bytes,
    std::chrono::steady_clock::time_point deadline)
{
    if (bytes.empty() || bytes.size() > MAX_FINALIZED_BLOCK_BYTES) return false;
    std::vector<unsigned char> meta;
    Put64(meta, height);
    Put32(meta, static_cast<uint32_t>(bytes.size()));
    if (!Write(Frame{MessageType::BLOCK_META, meta}, deadline)) return false;
    for (size_t offset = 0; offset < bytes.size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes.size() - offset);
        if (!Write(Frame{MessageType::BLOCK_DATA,
                std::vector<unsigned char>{bytes.begin() + offset, bytes.begin() + offset + count}}, deadline)) return false;
    }
    return true;
}

BlockRequestResult PeerSession::RequestBlock(uint64_t height)
{
    std::vector<unsigned char> bytes;
    const auto result = RequestBlocks(height, 1, [&](uint64_t, std::span<const unsigned char> block) {
        bytes.assign(block.begin(), block.end());
        return true;
    });
    return {.status = result.status, .bytes = std::move(bytes)};
}

BlockBatchResult PeerSession::RequestBlocks(uint64_t first_height, uint8_t max_blocks,
    const std::function<bool(uint64_t, std::span<const unsigned char>)>& consume)
{
    if (!m_peer || first_height == 0 || max_blocks == 0 || max_blocks > MAX_BLOCK_BATCH ||
        first_height > std::numeric_limits<uint64_t>::max() - max_blocks + 1 || !consume) return {};
    std::vector<unsigned char> request;
    Put64(request, first_height);
    request.push_back(max_blocks);
    auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::GET_BLOCKS, request}, deadline))
        return {.status = BlockRequestStatus::UNAVAILABLE};
    uint8_t count{0};
    for (;;) {
        deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        const auto meta = Read(deadline);
        if (!meta) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
            BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .count = count};
        if (meta->type == MessageType::BLOCKS_END) {
            if (meta->payload.size() != 1 || meta->payload[0] != count)
                return {.status = BlockRequestStatus::INVALID_RESPONSE, .count = count};
            return {.status = count ? BlockRequestStatus::OK : BlockRequestStatus::NOT_FOUND, .count = count};
        }
        if (count >= max_blocks || meta->type != MessageType::BLOCK_META || meta->payload.size() != 12 ||
            Read64(meta->payload.data()) != first_height + count)
            return {.status = BlockRequestStatus::INVALID_RESPONSE, .count = count};
        const uint32_t size = Read32(meta->payload.data() + 8);
        if (size == 0 || size > MAX_FINALIZED_BLOCK_BYTES)
            return {.status = BlockRequestStatus::INVALID_RESPONSE, .count = count};
        std::vector<unsigned char> bytes;
        bytes.reserve(size);
        while (bytes.size() < size) {
            const auto chunk = Read(deadline);
            if (!chunk) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
                BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .count = count};
            if (chunk->type != MessageType::BLOCK_DATA || chunk->payload.empty() || chunk->payload.size() > size - bytes.size())
                return {.status = BlockRequestStatus::INVALID_RESPONSE, .count = count};
            bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
        }
        if (!consume(first_height + count, bytes)) return {.status = BlockRequestStatus::INVALID_RESPONSE, .count = count};
        ++count;
    }
}

std::optional<BlockAnnounceResult> PeerSession::AdvertiseBlock(
    const BlockAnnouncement& announcement, const FinalizedBlock& block, uint64_t& peer_finalized_height)
{
    peer_finalized_height = 0;
    if (!m_peer || announcement.height == 0 || announcement.block_id.IsNull() ||
        block.block.height != announcement.height || ComputeBlockId(block.block) != announcement.block_id) return std::nullopt;
    std::vector<unsigned char> inventory{1};
    Put64(inventory, announcement.height);
    inventory.insert(inventory.end(), announcement.block_id.begin(), announcement.block_id.end());
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::BLOCK_ANNOUNCE, inventory}, deadline)) return std::nullopt;
    const auto answer = Read(deadline);
    if (!answer) return std::nullopt;
    if (answer->type == MessageType::GET_BLOCKS && answer->payload.size() == 9 &&
        Read64(answer->payload.data()) == announcement.height && answer->payload[8] == 1) {
        const auto encoded = SerializeFinalizedBlock(block);
        if (!encoded || !SendBlock(announcement.height, *encoded, deadline) ||
            !Write(Frame{MessageType::BLOCKS_END, {1}}, deadline)) return std::nullopt;
    } else if (answer->type != MessageType::BLOCK_RESULT) {
        return std::nullopt;
    }
    const auto result = answer->type == MessageType::BLOCK_RESULT ? answer : Read(deadline);
    // Payload is [result:1][height:8][id:32] plus, from newer peers, the
    // acker's finalized height [peer_height:8] (49 bytes total).
    if (!result || result->type != MessageType::BLOCK_RESULT ||
        result->payload.size() != 49 ||
        result->payload[0] > static_cast<uint8_t>(BlockAnnounceResult::GAP) ||
        Read64(result->payload.data() + 1) != announcement.height ||
        !std::equal(announcement.block_id.begin(), announcement.block_id.end(), result->payload.begin() + 9)) return std::nullopt;
    peer_finalized_height = Read64(result->payload.data() + 41);
    return static_cast<BlockAnnounceResult>(result->payload[0]);
}

bool PeerSession::SendOperation(std::span<const unsigned char> bytes, std::chrono::steady_clock::time_point deadline)
{
    if (bytes.size() > MAX_OPERATION_PAYLOAD_BYTES) return false;
    std::vector<unsigned char> meta;
    Put32(meta, static_cast<uint32_t>(bytes.size()));
    if (!Write(Frame{MessageType::OP_META, meta}, deadline)) return false;
    for (size_t offset = 0; offset < bytes.size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes.size() - offset);
        if (!Write(Frame{MessageType::OP_DATA,
                std::vector<unsigned char>{bytes.begin() + offset, bytes.begin() + offset + count}}, deadline)) return false;
    }
    return true;
}

bool PeerSession::SendOperationResult(const OperationSubmitResult& result, std::chrono::steady_clock::time_point deadline)
{
    std::vector<unsigned char> response{static_cast<unsigned char>(result.status)};
    response.insert(response.end(), result.op_id.begin(), result.op_id.end());
    return Write(Frame{MessageType::OP_RESULT, response}, deadline);
}

std::optional<OperationSubmitResult> PeerSession::ReadOperationResult(const cybou::Hash256& operation_id,
    std::chrono::steady_clock::time_point deadline)
{
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::OP_RESULT || response->payload.size() != 33 ||
        response->payload[0] > static_cast<uint8_t>(OperationSubmitStatus::RELAY_QUEUE_FULL) ||
        !std::equal(operation_id.begin(), operation_id.end(), response->payload.begin() + 1)) return std::nullopt;
    const auto status = static_cast<OperationSubmitStatus>(response->payload[0]);
    return OperationSubmitResult{.status = status, .op_id = operation_id,
        .delivery_uncertain = status == OperationSubmitStatus::FINALIZER_UNAVAILABLE ||
            status == OperationSubmitStatus::RELAY_QUEUE_FULL};
}

std::optional<OperationSubmitResult> PeerSession::ReceiveOperation(const Frame& meta, CybouNodeRuntime& runtime,
    bool allow_seen_retry, std::chrono::steady_clock::time_point deadline)
{
    if (meta.type != MessageType::OP_META || meta.payload.size() != 4) return std::nullopt;
    const uint32_t size = Read32(meta.payload.data());
    if (size == 0 || size > MAX_OPERATION_PAYLOAD_BYTES) return std::nullopt;
    boost::system::error_code error;
    const auto remote = m_socket.remote_endpoint(error);
    if (error || !runtime.AdmitIngress(remote.address().to_string(), IngressBudget::Work::OPERATION, size)) return std::nullopt;
    std::vector<unsigned char> bytes;
    bytes.reserve(size);
    while (bytes.size() < size) {
        const auto chunk = Read(deadline);
        if (!chunk || chunk->type != MessageType::OP_DATA || chunk->payload.empty() ||
            chunk->payload.size() > size - bytes.size()) return std::nullopt;
        bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
    }
    const auto operation = DeserializeProtocolOperation(bytes);
    if (!operation) return std::nullopt;
    const auto operation_id = ComputeOperationId(*operation);
    if (!operation_id) return std::nullopt;
    if (runtime.GetOperationStatus(*operation_id).kind == OperationStatusKind::FINALIZED)
        return OperationSubmitResult{.status = OperationSubmitStatus::ALREADY_FINALIZED, .op_id = *operation_id};
    // Both origin submission and hop-by-hop relay execute against local finalized state.
    const auto queued = runtime.EnqueueRelayedOperation(bytes, allow_seen_retry, remote.address().to_string());
    OperationSubmitStatus status = OperationSubmitStatus::INVALID_PAYLOAD;
    switch (queued) {
    case OperationRelayEnqueueStatus::QUEUED: status = OperationSubmitStatus::RELAY_QUEUED; break;
    case OperationRelayEnqueueStatus::DUPLICATE:
        status = runtime.HasRelayedOperation(*operation_id) ? OperationSubmitStatus::RELAY_QUEUED : OperationSubmitStatus::REJECTED;
        break;
    case OperationRelayEnqueueStatus::QUEUE_FULL: status = OperationSubmitStatus::RELAY_QUEUE_FULL; break;
    case OperationRelayEnqueueStatus::INVALID_OPERATION: break;
    }
    return OperationSubmitResult{.status = status, .op_id = *operation_id,
        .delivery_uncertain = status == OperationSubmitStatus::RELAY_QUEUE_FULL};
}

std::optional<OperationSubmitResult> PeerSession::SubmitOperation(const ProtocolOperation& operation)
{
    if (!m_peer) return std::nullopt;
    const auto bytes = SerializeProtocolOperation(operation);
    const auto operation_id = ComputeOperationId(operation);
    if (!bytes || !operation_id || bytes->empty()) return std::nullopt;
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!SendOperation(*bytes, deadline)) return std::nullopt;
    return ReadOperationResult(*operation_id, deadline);
}

bool PeerSession::PollOperationRelay(CybouNodeRuntime& runtime)
{
    if (!m_peer) return false;
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::OP_POLL, {}}, deadline)) return false;
    const auto meta = Read(deadline);
    if (!meta || meta->type != MessageType::OP_META || meta->payload.size() != 4) return false;
    if (Read32(meta->payload.data()) == 0) return false;
    const auto result = ReceiveOperation(*meta, runtime, false, deadline);
    return result && SendOperationResult(*result, deadline) && static_cast<bool>(*result);
}

bool PeerSession::PollValidationAttestation(CybouNodeRuntime& runtime)
{
    if (!m_peer) return false;
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::VALIDATION_ATTESTATION_POLL, {}}, deadline)) return false;
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::VALIDATION_ATTESTATION || response->payload.empty()) return false;
    const auto attestation = DeserializeValidationAttestation(response->payload);
    if (!attestation) return false;
    // Stored (and offered onward) only if this node executed the operation
    // itself and the signer is eligible in this node's finalized state.
    (void)runtime.AcceptValidationAttestation(*attestation);
    return true;
}

std::vector<std::pair<std::string, uint16_t>> PeerSession::RequestPeers(std::chrono::steady_clock::time_point deadline)
{
    if (!m_peer) return {};
    if (!Write(Frame{MessageType::GET_PEERS, {}}, deadline)) return {};
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::PEERS) return {};
    const auto peers = DecodePeersPayload(response->payload);
    return peers ? *peers : std::vector<std::pair<std::string, uint16_t>>{};
}

bool PeerSession::SendPeers(const std::vector<std::pair<std::string, uint16_t>>& peers,
    std::chrono::steady_clock::time_point deadline)
{
    const auto payload = EncodePeersPayload(peers);
    return Write(Frame{MessageType::PEERS, payload}, deadline);
}

std::optional<ChunkAdmissionResult> PeerSession::PutAuthorizedChunk(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    if (!m_peer || publication_operation_id.IsNull() ||
        IsZeroChunkId(chunk_id) || stored_bytes.size() < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
        stored_bytes.size() > ENCRYPTED_CHUNK_MAX_STORED_BYTES || proof.siblings.size() > 32) return std::nullopt;
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    std::vector<unsigned char> init;
    init.insert(init.end(), publication_operation_id.begin(), publication_operation_id.end());
    init.insert(init.end(), chunk_id.begin(), chunk_id.end());
    Put32(init, proof.leaf_index);
    init.push_back(static_cast<unsigned char>(proof.siblings.size()));
    for (const auto& sibling : proof.siblings) init.insert(init.end(), sibling.begin(), sibling.end());
    Put32(init, static_cast<uint32_t>(stored_bytes.size()));
    if (!Write(Frame{MessageType::PUT_AUTHORIZED_CHUNK, init}, deadline)) return std::nullopt;
    for (size_t offset = 0; offset < stored_bytes.size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, stored_bytes.size() - offset);
        if (!Write(Frame{MessageType::AUTHORIZED_CHUNK_DATA,
                std::vector<unsigned char>{stored_bytes.begin() + offset, stored_bytes.begin() + offset + count}}, deadline)) {
            return std::nullopt;
        }
    }
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::CHUNK_ADMISSION_RESULT || response->payload.size() != 1 ||
        response->payload[0] > static_cast<uint8_t>(ChunkAdmissionStatus::STORAGE_ERROR)) return std::nullopt;
    return ChunkAdmissionResult{static_cast<ChunkAdmissionStatus>(response->payload[0])};
}

std::optional<std::vector<unsigned char>> PeerSession::GetChunkById(const ChunkId& chunk_id)
{
    if (!m_peer || IsZeroChunkId(chunk_id)) return std::nullopt;
    const auto unavailable = [this]() -> std::optional<std::vector<unsigned char>> {
        m_peer.reset();
        m_peer_provider_id.reset();
        return std::nullopt;
    };
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::GET_CHUNK_BY_ID,
            std::vector<unsigned char>{chunk_id.begin(), chunk_id.end()}}, deadline)) return unavailable();
    const auto meta = Read(deadline);
    if (!meta || meta->type != MessageType::CHUNK_ADMISSION_RESULT || meta->payload.size() != 5 ||
        meta->payload[0] > 1) return unavailable();
    const uint32_t size = Read32(meta->payload.data() + 1);
    if (meta->payload[0] == 0) return size == 0 ? std::optional<std::vector<unsigned char>>{} : unavailable();
    if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES) return unavailable();
    std::vector<unsigned char> bytes;
    bytes.reserve(size);
    while (bytes.size() < size) {
        const auto data = Read(deadline);
        if (!data || data->type != MessageType::CHUNK_DATA || data->payload.empty() ||
            data->payload.size() > size - bytes.size()) return unavailable();
        bytes.insert(bytes.end(), data->payload.begin(), data->payload.end());
    }
    return ComputeChunkId(bytes) == chunk_id ? std::optional<std::vector<unsigned char>>{std::move(bytes)} : unavailable();
}

std::optional<ChunkAuthorizationProof> PeerSession::GetChunkAuthorizationProof(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id)
{
    if (!m_peer || publication_operation_id.IsNull() ||
        IsZeroChunkId(chunk_id)) return std::nullopt;
    const auto unavailable = [this]() -> std::optional<ChunkAuthorizationProof> {
        m_peer.reset();
        m_peer_provider_id.reset();
        return std::nullopt;
    };
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    std::vector<unsigned char> payload;
    payload.insert(payload.end(), publication_operation_id.begin(), publication_operation_id.end());
    payload.insert(payload.end(), chunk_id.begin(), chunk_id.end());
    if (!Write(Frame{MessageType::GET_CHUNK_AUTHORIZATION_PROOF, payload}, deadline)) return unavailable();
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::CHUNK_AUTHORIZATION_PROOF || response->payload.empty()) {
        return unavailable();
    }
    if (response->payload[0] == 0) return response->payload.size() == 1
        ? std::optional<ChunkAuthorizationProof>{} : unavailable();
    if (response->payload[0] != 1 || response->payload.size() < 6) return unavailable();
    const auto sibling_count = response->payload[5];
    if (sibling_count > 32 || response->payload.size() != 6 + size_t{sibling_count} * 32) return unavailable();
    ChunkAuthorizationProof proof;
    proof.leaf_index = Read32(response->payload.data() + 1);
    proof.siblings.resize(sibling_count);
    for (size_t i = 0; i < sibling_count; ++i) {
        std::copy_n(response->payload.begin() + 6 + i * 32, 32, proof.siblings[i].begin());
    }
    return proof;
}

bool PeerSession::ServeNext(CybouNodeRuntime& runtime)
{
    if (!m_peer) return false;
    const auto request = Read();
    if (!request) {
        return m_socket.is_open();
    }
    if (request->type == MessageType::STORAGE_PROOF_REQUEST) {
        boost::system::error_code ec;
        const auto remote = m_socket.remote_endpoint(ec);
        if (ec || !runtime.AdmitIngress(remote.address().to_string(), IngressBudget::Work::OPERATION, request->payload.size())) return false;
        if (!m_local || request->payload.size() != 32) return false;
        auto message = StorageProofMessage(*m_local, *m_peer, m_tls_exporter);
        message.insert(message.end(), request->payload.begin(), request->payload.end());
        const auto proof = runtime.SignProviderProof(message);
        return proof && Write(Frame{MessageType::STORAGE_PROOF, *proof});
    }
    if (request->type == MessageType::PUT_AUTHORIZED_CHUNK) {
        if (request->payload.size() < 73) return false;
        cybou::Hash256 publication_id;
        std::copy_n(request->payload.begin(), 32, publication_id.begin());
        ChunkId chunk_id{};
        std::copy_n(request->payload.begin() + 32, 32, chunk_id.begin());
        ChunkAuthorizationProof proof;
        proof.leaf_index = Read32(request->payload.data() + 64);
        const auto sibling_count = request->payload[68];
        const size_t size_offset = 69 + size_t{sibling_count} * ChunkId{}.size();
        if (sibling_count > 32 || request->payload.size() != size_offset + 4) return false;
        proof.siblings.resize(sibling_count);
        for (size_t i = 0; i < sibling_count; ++i) {
            std::copy_n(request->payload.begin() + 69 + i * 32, 32, proof.siblings[i].begin());
        }
        const uint32_t size = Read32(request->payload.data() + size_offset);
        if (size < ENCRYPTED_CHUNK_MIN_STORED_BYTES || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES) return false;
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        std::vector<unsigned char> bytes;
        bytes.reserve(size);
        while (bytes.size() < size) {
            const auto data = Read(deadline);
            if (!data || data->type != MessageType::AUTHORIZED_CHUNK_DATA || data->payload.empty() ||
                data->payload.size() > size - bytes.size()) return false;
            bytes.insert(bytes.end(), data->payload.begin(), data->payload.end());
        }
        const auto result = runtime.PutFinalizedChunk(publication_id, chunk_id, bytes, proof);
        return Write(Frame{MessageType::CHUNK_ADMISSION_RESULT,
            {static_cast<unsigned char>(result.status)}} , deadline);
    }
    if (request->type == MessageType::GET_CHUNK_BY_ID) {
        if (request->payload.size() != 32) return false;
        ChunkId chunk_id{};
        std::copy_n(request->payload.begin(), 32, chunk_id.begin());
        const auto bytes = runtime.GetFinalizedChunk(chunk_id);
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        std::vector<unsigned char> meta{static_cast<unsigned char>(bytes.has_value())};
        Put32(meta, bytes ? static_cast<uint32_t>(bytes->size()) : 0);
        if (!Write(Frame{MessageType::CHUNK_ADMISSION_RESULT, meta}, deadline)) return false;
        if (!bytes) return true;
        for (size_t offset = 0; offset < bytes->size(); offset += MAX_FRAME_PAYLOAD) {
            const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes->size() - offset);
            if (!Write(Frame{MessageType::CHUNK_DATA,
                    std::vector<unsigned char>{bytes->begin() + offset, bytes->begin() + offset + count}}, deadline)) {
                return false;
            }
        }
        return true;
    }
    if (request->type == MessageType::GET_CHUNK_AUTHORIZATION_PROOF) {
        if (request->payload.size() != 64) return false;
        cybou::Hash256 publication_id;
        std::copy_n(request->payload.begin(), 32, publication_id.begin());
        ChunkId chunk_id{};
        std::copy_n(request->payload.begin() + 32, 32, chunk_id.begin());
        const auto proof = runtime.GetFinalizedChunkAuthorizationProof(publication_id, chunk_id);
        std::vector<unsigned char> response{static_cast<unsigned char>(proof.has_value())};
        if (proof) {
            Put32(response, proof->leaf_index);
            response.push_back(static_cast<unsigned char>(proof->siblings.size()));
            for (const auto& sibling : proof->siblings) response.insert(response.end(), sibling.begin(), sibling.end());
        }
        return Write(Frame{MessageType::CHUNK_AUTHORIZATION_PROOF, response});
    }
    if (request->type == MessageType::GET_PEERS) {
            const auto endpoints = runtime.GetPeerEndpointsForGossip();
        return SendPeers(endpoints);
    }
    if (request->type == MessageType::OP_POLL) {
        if (!m_peer || !request->payload.empty()) return false;
        const auto item = runtime.ClaimRelayedOperation();
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        if (!item) return SendOperation({}, deadline);
        if (!SendOperation(item->exact_bytes, deadline)) {
            runtime.ReleaseRelayedOperation(item->operation_id);
            return false;
        }
        const auto result = ReadOperationResult(item->operation_id, deadline);
        if (result && static_cast<bool>(*result))
            return runtime.AcknowledgeRelayedOperation(item->operation_id);
        runtime.ReleaseRelayedOperation(item->operation_id);
        return result.has_value();
    }
    if (request->type == MessageType::VALIDATION_ATTESTATION_POLL) {
        if (!m_peer || !request->payload.empty()) return false;
        if (const auto tip = runtime.GetFinalizedTip(); tip && *tip != m_served_attestation_base) {
            m_served_attestation_base = *tip;
            m_served_attestations.clear();
        }
        std::vector<unsigned char> payload;
        const auto next = runtime.NextValidationAttestation([&](const ValidationPool::Key& key) {
            return m_served_attestations.contains(key);
        });
        if (next) {
            if (const auto bytes = SerializeValidationAttestation(*next)) {
                payload = *bytes;
                m_served_attestations.insert({next->operation_id, next->validator_account_id});
            }
        }
        return Write(Frame{MessageType::VALIDATION_ATTESTATION, std::move(payload)});
    }
    if (request->type == MessageType::PING) {
        return request->payload.size() == 8 && Write(Frame{MessageType::PONG, request->payload});
    }
    if (request->type == MessageType::OP_META) {
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        const auto result = ReceiveOperation(*request, runtime, true, deadline);
        return result && SendOperationResult(*result, deadline);
    }
    if (request->type == MessageType::BLOCK_ANNOUNCE) {
        if (request->payload.size() != 41 || request->payload[0] != 1) return false;
        const uint64_t height = Read64(request->payload.data() + 1);
        cybou::Hash256 id;
        std::copy_n(request->payload.begin() + 9, 32, id.begin());
        if (height == 0 || id.IsNull()) return false;
        auto status = runtime.GetStatus();
        if (!status.is_initialized) return false;
        // Fresh deadline per message: a bulk catch-up below performs many
        // round trips and must not inherit a stale timeout.
        auto acknowledge = [&](BlockAnnounceResult result) {
            std::vector<unsigned char> payload{static_cast<unsigned char>(result)};
            Put64(payload, height);
            payload.insert(payload.end(), id.begin(), id.end());
            // Report our current finalized height so the offerer can skip
            // heads we already have on its next fanout cycle.
            Put64(payload, runtime.GetStatus().finalized_height);
            const auto ack_deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
            return Write(Frame{MessageType::BLOCK_RESULT, payload}, ack_deadline);
        };
        // Pulls one block by height over this session and commits it after
        // canonical verification. Returns std::nullopt on transport/protocol
        // failure (the session must die); GAP when the peer cannot serve the
        // block; APPLIED on success. An expected_id of zero skips the id check
        // (used for catch-up pulls where we only know the height).
        auto fetch_and_commit = [&](uint64_t h, const cybou::Hash256& expected_id) ->
            std::optional<BlockAnnounceResult> {
            const auto response = RequestBlock(h);
            if (response.status == BlockRequestStatus::NOT_FOUND) return BlockAnnounceResult::GAP;
            if (response.status != BlockRequestStatus::OK) return std::nullopt;
            const auto& bytes = response.bytes;
            const auto block = DeserializeFinalizedBlock(bytes);
            if (!block || block->block.height != h ||
                block->certificate.network_binding != status.network_binding ||
                block->certificate.height != h) return std::nullopt;
            if (!expected_id.IsNull() &&
                (ComputeBlockId(block->block) != expected_id || block->certificate.block_id != expected_id)) {
                return std::nullopt;
            }
            // A failed commit (duplicate race or state conflict) must not kill
            // the session; the offered height simply stays unavailable.
            const bool committed = static_cast<bool>(runtime.CommitBlock(*block));
            return committed ? BlockAnnounceResult::APPLIED : BlockAnnounceResult::GAP;
        };
        auto have_height = [&](uint64_t h, const cybou::Hash256& expected) {
            const auto known = runtime.GetBlockAtHeight(h);
            return known && ComputeBlockId(known->block) == expected;
        };
        if (height <= status.finalized_height) {
            return have_height(height, id) && acknowledge(BlockAnnounceResult::ALREADY_HAVE);
        }
        if (status.finalized_height == std::numeric_limits<uint64_t>::max() ||
            height != status.finalized_height + 1) {
            // The offered height is not our next block. We cannot pull over
            // this accepted session: the peer's side of this connection is a
            // client session that never reads (only the accepted side runs
            // ServeNext), so an unsolicited GET_BLOCKS here would deadlock and
            // kill the session. Historical catch-up is instead driven by the
            // gossip worker through the client-side PeerManager::SyncFromPeer path.
            return acknowledge(BlockAnnounceResult::GAP);
        }
        const auto applied = fetch_and_commit(height, id);
        if (!applied) return false;
        return acknowledge(*applied);
    }
    if (request->type != MessageType::GET_BLOCKS || request->payload.size() != 9) return false;
    const uint64_t first_height = Read64(request->payload.data());
    const uint8_t requested = request->payload[8];
    if (first_height == 0 || requested == 0 || requested > MAX_BLOCK_BATCH ||
        first_height > std::numeric_limits<uint64_t>::max() - requested + 1) return false;
    uint8_t count{0};
    for (; count < requested; ++count) {
        const uint64_t height = first_height + count;
        const auto block = runtime.GetBlockAtHeight(height);
        if (!block) break;
        const auto bytes = SerializeFinalizedBlock(*block);
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        if (!bytes || !SendBlock(height, *bytes, deadline)) return false;
    }
    return Write(Frame{MessageType::BLOCKS_END, {count}});

}

} // namespace cybou::p2p
