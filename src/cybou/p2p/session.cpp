// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/session.h>

#include <cybou/chunk_id.h>
#include <cybou/identity_crypto.h>
#include <cybou/protocol_limits.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/node_runtime.h>

#include <boost/asio/ip/address.hpp>

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
constexpr size_t HELLO_SIZE{88};
constexpr auto BLOCK_TRANSFER_TIMEOUT{std::chrono::seconds{30}};
constexpr auto TLS_HANDSHAKE_TIMEOUT{std::chrono::seconds{10}};
constexpr std::string_view TLS_EXPORTER_LABEL{"EXPORTER-CYBOU-CYP2-V3"};

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
    case MessageType::PROVIDER_PROOF:
    case MessageType::FINALIZER_PROOF:
    case MessageType::BOOTSTRAP_REQUEST:
    case MessageType::BOOTSTRAP_RESPONSE:
    case MessageType::BOOTSTRAP_PROOF:
    case MessageType::PING:
    case MessageType::PONG:
    case MessageType::GET_BLOCK:
    case MessageType::BLOCK_META:
    case MessageType::BLOCK_CHUNK:
    case MessageType::OP_META:
    case MessageType::OP_CHUNK:
    case MessageType::OP_RESULT:
    case MessageType::GET_BLOCKS:
    case MessageType::BLOCK_INV:
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
        return true;
    default:
        return false;
    }
}
} // namespace

std::optional<std::vector<unsigned char>> EncodeFrame(const Frame& frame)
{
    const auto maximum_payload = frame.type == MessageType::BOOTSTRAP_REQUEST ||
        frame.type == MessageType::BOOTSTRAP_RESPONSE ? MAX_BOOTSTRAP_FRAME_PAYLOAD : MAX_FRAME_PAYLOAD;
    if (frame.payload.size() > maximum_payload ||
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
    const auto maximum_payload = type == MessageType::BOOTSTRAP_REQUEST ||
        type == MessageType::BOOTSTRAP_RESPONSE ? MAX_BOOTSTRAP_FRAME_PAYLOAD : MAX_FRAME_PAYLOAD;
    if (size > maximum_payload || bytes.size() != HEADER_SIZE + size) return std::nullopt;
    return Frame{static_cast<MessageType>(bytes[5]),
        std::vector<unsigned char>{bytes.begin() + HEADER_SIZE, bytes.end()}};
}

std::vector<unsigned char> EncodeHello(const Hello& hello)
{
    std::vector<unsigned char> out;
    out.reserve(HELLO_SIZE);
    out.insert(out.end(), hello.network_id.begin(), hello.network_id.end());
    Put64(out, hello.finalized_height);
    out.insert(out.end(), hello.finalized_tip.begin(), hello.finalized_tip.end());
    Put64(out, hello.capabilities);
    Put64(out, hello.nonce);
    return out;
}

std::optional<Hello> DecodeHello(std::span<const unsigned char> bytes)
{
    if (bytes.size() != HELLO_SIZE) return std::nullopt;
    Hello hello;
    std::copy_n(bytes.begin(), 32, hello.network_id.begin());
    hello.finalized_height = Read64(bytes.data() + 32);
    std::copy_n(bytes.begin() + 40, 32, hello.finalized_tip.begin());
    hello.capabilities = Read64(bytes.data() + 72);
    hello.nonce = Read64(bytes.data() + 80);
    if (hello.network_id.IsNull() || hello.finalized_tip.IsNull() || hello.nonce == 0) return std::nullopt;
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
        return peer.finalized_tip == runtime.GetNetworkDefinition().genesis_block_id;
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
    if (std::chrono::steady_clock::now() >= deadline) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
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
    const auto message_type = static_cast<MessageType>(header[5]);
    const auto maximum_payload = message_type == MessageType::BOOTSTRAP_REQUEST ||
        message_type == MessageType::BOOTSTRAP_RESPONSE ? MAX_BOOTSTRAP_FRAME_PAYLOAD : MAX_FRAME_PAYLOAD;
    if (size > maximum_payload) {
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

std::optional<Frame> PeerSession::RequestBootstrap(const Frame& request)
{
    if (m_transport_role != TransportRole::CLIENT || !m_tls_config.expected_server_spki_sha256 ||
        request.type != MessageType::BOOTSTRAP_REQUEST ||
        !EstablishSecureTransport(std::chrono::steady_clock::now() + TLS_HANDSHAKE_TIMEOUT)) return std::nullopt;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    if (!Write(request, deadline)) return std::nullopt;
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::BOOTSTRAP_RESPONSE) return std::nullopt;
    return response;
}

std::optional<BootstrapIdentityClaim> PeerSession::RequestBootstrapIdentityClaim()
{
    if (m_transport_role != TransportRole::CLIENT || !m_tls_config.expected_server_spki_sha256 ||
        !EstablishSecureTransport(std::chrono::steady_clock::now() + TLS_HANDSHAKE_TIMEOUT)) return std::nullopt;
    std::array<unsigned char, 32> challenge{};
    if (RAND_bytes(challenge.data(), static_cast<int>(challenge.size())) != 1) return std::nullopt;
    std::vector<unsigned char> request{'C','Y','B','I','Q','1'};
    request.insert(request.end(), challenge.begin(), challenge.end());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    if (!Write(Frame{MessageType::BOOTSTRAP_REQUEST, std::move(request)}, deadline)) return std::nullopt;
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::BOOTSTRAP_RESPONSE) return std::nullopt;
    auto claim = DecodeBootstrapIdentityClaim(response->payload);
    if (!claim || claim->challenge != challenge || !VerifyBootstrapIdentityClaim(*claim, m_tls_exporter)) return std::nullopt;
    return claim;
}

bool PeerSession::ServeBootstrapIdentityClaim(const IdentityHybridPublicKey& recovery_key,
    const ProviderProofSigner& recovery_signer)
{
    if (m_transport_role != TransportRole::SERVER || !recovery_signer ||
        recovery_key.purpose != IdentityKeyPurpose::RECOVERY_ROOT || !ComputeRecoveryKeyId(recovery_key) ||
        m_tls_config.certificate_chain_file.empty() || m_tls_config.private_key_file.empty() ||
        !EstablishSecureTransport(std::chrono::steady_clock::now() + TLS_HANDSHAKE_TIMEOUT)) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    const auto request = Read(deadline);
    if (!request || request->type != MessageType::BOOTSTRAP_REQUEST || request->payload.size() != 38 ||
        !std::equal(request->payload.begin(), request->payload.begin() + 6,
            std::array<unsigned char, 6>{'C','Y','B','I','Q','1'}.begin())) return false;
    std::array<unsigned char, 32> challenge{};
    std::copy_n(request->payload.begin() + 6, challenge.size(), challenge.begin());
    if (std::ranges::all_of(challenge, [](unsigned char byte) { return byte == 0; })) return false;
    const auto message = BootstrapIdentityClaimMessage(m_tls_exporter, challenge, recovery_key);
    const auto encoded_signature = recovery_signer(message);
    if (message.empty() || !encoded_signature || encoded_signature->size() != 64 + 3309) return false;
    BootstrapIdentityClaim claim;
    claim.challenge = challenge;
    claim.recovery_key = recovery_key;
    std::copy_n(encoded_signature->begin(), claim.proof.ed25519.size(), claim.proof.ed25519.begin());
    claim.proof.ml_dsa.assign(encoded_signature->begin() + claim.proof.ed25519.size(), encoded_signature->end());
    const auto payload = EncodeBootstrapIdentityClaim(claim);
    return payload && Write(Frame{MessageType::BOOTSTRAP_RESPONSE, *payload}, deadline);
}

bool PeerSession::ServeBootstrapRequest(
    const std::function<std::optional<Frame>(const Frame&)>& handler)
{
    if (m_transport_role != TransportRole::SERVER || !handler ||
        m_tls_config.certificate_chain_file.empty() || m_tls_config.private_key_file.empty() ||
        !EstablishSecureTransport(std::chrono::steady_clock::now() + TLS_HANDSHAKE_TIMEOUT)) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    const auto request = Read(deadline);
    if (!request || request->type != MessageType::BOOTSTRAP_REQUEST) return false;
    const auto response = handler(*request);
    return response && response->type == MessageType::BOOTSTRAP_RESPONSE && Write(*response, deadline);
}

std::vector<unsigned char> ProviderProofMessage(const Hello& signer,const Hello& verifier,
    const std::span<const unsigned char> tls_exporter)
{
    if (tls_exporter.size() != 32) return {};
    constexpr std::string_view DOMAIN{"CYBOU/CYP2/PROVIDER-PROOF/v3"};
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

std::optional<ProviderId> VerifyProviderProof(const std::span<const unsigned char> payload,
    const std::span<const unsigned char> message)
{
    if (payload.size() != PROVIDER_PROOF_SIZE) return std::nullopt;
    IdentityHybridPublicKey key{.purpose = IdentityKeyPurpose::STORAGE_PROVIDER};
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

std::vector<unsigned char> FinalizerProofMessage(const Hello& signer, const Hello& verifier,
    const std::span<const unsigned char> tls_exporter)
{
    if (tls_exporter.size() != 32) return {};
    constexpr std::string_view DOMAIN{"CYBOU/CYP2/FINALIZER-PROOF/v1"};
    std::vector<unsigned char> message(DOMAIN.begin(), DOMAIN.end());
    message.insert(message.end(), tls_exporter.begin(), tls_exporter.end());
    const auto signer_bytes = EncodeHello(signer), verifier_bytes = EncodeHello(verifier);
    message.insert(message.end(), signer_bytes.begin(), signer_bytes.end());
    message.insert(message.end(), verifier_bytes.begin(), verifier_bytes.end());
    return message;
}

bool VerifyFinalizerProof(const std::span<const unsigned char> payload,
    const std::span<const unsigned char> message, const IdentityHybridPublicKey& genesis_finalizer_key)
{
    if (payload.size() != 64 + 3309 || genesis_finalizer_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        genesis_finalizer_key.ml_dsa.size() != 1952) return false;
    IdentityHybridSignature signature;
    std::copy_n(payload.begin(), signature.ed25519.size(), signature.ed25519.begin());
    signature.ml_dsa.assign(payload.begin() + signature.ed25519.size(), payload.end());
    return VerifyIdentityMessage(genesis_finalizer_key, signature, message);
}

std::vector<unsigned char> BootstrapProofMessage(const Hello& signer, const Hello& verifier,
    const std::span<const unsigned char> tls_exporter, const AccountId& account_id)
{
    if (tls_exporter.size() != 32 || signer.network_id.IsNull() ||
        signer.network_id != verifier.network_id || account_id.IsNull()) return {};
    constexpr std::string_view DOMAIN{"CYBOU/CYP2/BOOTSTRAP-PROOF/v1"};
    std::vector<unsigned char> message(DOMAIN.begin(), DOMAIN.end());
    message.insert(message.end(), signer.network_id.begin(), signer.network_id.end());
    message.insert(message.end(), tls_exporter.begin(), tls_exporter.end());
    const auto signer_bytes = EncodeHello(signer), verifier_bytes = EncodeHello(verifier);
    message.insert(message.end(), signer_bytes.begin(), signer_bytes.end());
    message.insert(message.end(), verifier_bytes.begin(), verifier_bytes.end());
    const auto account = account_id.Value();
    message.insert(message.end(), account.begin(), account.end());
    return message;
}

std::optional<AccountId> VerifyBootstrapProof(const std::span<const unsigned char> payload,
    const std::span<const unsigned char> message, const IdentityHybridPublicKey& authorization_key)
{
    constexpr size_t SIGNATURE_SIZE{64 + 2420};
    if (payload.size() != AccountId::SIZE + SIGNATURE_SIZE ||
        authorization_key.purpose != IdentityKeyPurpose::AUTHORIZATION || authorization_key.ml_dsa.size() != 1312) {
        return std::nullopt;
    }
    const auto account_id = AccountId::FromBytes(payload.first(AccountId::SIZE));
    if (!account_id) return std::nullopt;
    IdentityHybridSignature signature;
    std::copy_n(payload.begin() + AccountId::SIZE, signature.ed25519.size(), signature.ed25519.begin());
    signature.ml_dsa.assign(payload.begin() + AccountId::SIZE + signature.ed25519.size(), payload.end());
    return VerifyIdentityMessage(authorization_key, signature, message) ? account_id : std::nullopt;
}

bool PeerSession::Handshake(const Hello& local, const ProviderProofSigner& provider_signer,
    const FinalizerProofSigner& finalizer_signer, const IdentityHybridPublicKey* genesis_finalizer_key,
    const BootstrapProofIdentity* local_bootstrap_identity,
    const BootstrapIdentityResolver& bootstrap_identity_resolver)
{
    m_peer.reset();
    m_peer_provider_id.reset();
    m_peer_finalizer_authenticated = false;
    m_peer_bootstrap_account.reset();
    m_handshake_status = HandshakeStatus::INVALID_LOCAL;
    if (local.network_id.IsNull() || local.finalized_tip.IsNull() || local.nonce == 0) return false;
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
    if (peer->network_id != local.network_id) {
        m_handshake_status = HandshakeStatus::WRONG_NETWORK;
        return false;
    }
    // Storage peers prove their provider key over both session nonces.
    if (local.capabilities & CAP_STORAGE) {
        if (!provider_signer) {
            m_handshake_status = HandshakeStatus::INVALID_LOCAL;
            return false;
        }
        const auto proof = provider_signer(ProviderProofMessage(local, *peer, m_tls_exporter));
        if (!proof || !Write(Frame{MessageType::PROVIDER_PROOF, *proof})) {
            m_handshake_status = HandshakeStatus::UNAVAILABLE;
            return false;
        }
    }
    if (local.capabilities & CAP_ACCEPT_OPERATIONS) {
        if (!finalizer_signer) {
            m_handshake_status = HandshakeStatus::INVALID_LOCAL;
            return false;
        }
        const auto proof = finalizer_signer(FinalizerProofMessage(local, *peer, m_tls_exporter));
        if (!proof || !Write(Frame{MessageType::FINALIZER_PROOF, *proof})) {
            m_handshake_status = HandshakeStatus::UNAVAILABLE;
            return false;
        }
    }
    if (local.capabilities & CAP_BOOTSTRAP) {
        if (!local_bootstrap_identity || local_bootstrap_identity->account_id.IsNull() ||
            !local_bootstrap_identity->signer) {
            m_handshake_status = HandshakeStatus::INVALID_LOCAL;
            return false;
        }
        const auto message = BootstrapProofMessage(local, *peer, m_tls_exporter,
            local_bootstrap_identity->account_id);
        const auto signature = local_bootstrap_identity->signer(message);
        if (message.empty() || !signature || signature->size() != 64 + 2420) {
            m_handshake_status = HandshakeStatus::INVALID_LOCAL;
            return false;
        }
        std::vector<unsigned char> payload;
        const auto account = local_bootstrap_identity->account_id.Value();
        payload.insert(payload.end(), account.begin(), account.end());
        payload.insert(payload.end(), signature->begin(), signature->end());
        if (!Write(Frame{MessageType::BOOTSTRAP_PROOF, std::move(payload)})) return false;
    }
    if (peer->capabilities & CAP_STORAGE) {
        const auto proof_frame = Read();
        if (!proof_frame || proof_frame->type != MessageType::PROVIDER_PROOF) return false;
        m_peer_provider_id = VerifyProviderProof(proof_frame->payload,
            ProviderProofMessage(*peer, local, m_tls_exporter));
        if (!m_peer_provider_id) return false;
    }
    if (peer->capabilities & CAP_ACCEPT_OPERATIONS) {
        if (!genesis_finalizer_key || genesis_finalizer_key->purpose != IdentityKeyPurpose::POA_FINALIZER) {
            m_handshake_status = HandshakeStatus::INVALID_LOCAL;
            return false;
        }
        const auto proof_frame = Read();
        if (!proof_frame || proof_frame->type != MessageType::FINALIZER_PROOF ||
            !VerifyFinalizerProof(proof_frame->payload,
                FinalizerProofMessage(*peer, local, m_tls_exporter), *genesis_finalizer_key)) return false;
        m_peer_finalizer_authenticated = true;
    }
    if (peer->capabilities & CAP_BOOTSTRAP) {
        if (!bootstrap_identity_resolver) {
            m_handshake_status = HandshakeStatus::INVALID_LOCAL;
            return false;
        }
        const auto proof_frame = Read();
        if (!proof_frame || proof_frame->type != MessageType::BOOTSTRAP_PROOF ||
            proof_frame->payload.size() < AccountId::SIZE) return false;
        const auto account_id = AccountId::FromBytes(
            std::span<const unsigned char>{proof_frame->payload}.first(AccountId::SIZE));
        const auto key = account_id ? bootstrap_identity_resolver(*account_id) : std::nullopt;
        if (!account_id || !key) return false;
        const auto message = BootstrapProofMessage(*peer, local, m_tls_exporter, *account_id);
        const auto verified_account = VerifyBootstrapProof(proof_frame->payload, message, *key);
        if (!verified_account || *verified_account != *account_id) return false;
        m_peer_bootstrap_account = *account_id;
    }
    m_peer = *peer;
    m_local_capabilities = local.capabilities;
    m_handshake_status = HandshakeStatus::CONNECTED;
    return true;
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

BlockRequestResult PeerSession::RequestBlock(uint64_t height)
{
    if (!m_peer || !(m_peer->capabilities & CAP_SERVE_BLOCKS) || height == 0)
        return {.status = BlockRequestStatus::INVALID_REQUEST, .bytes = {}};
    if (!SendBlockRequest(height)) return {.status = BlockRequestStatus::UNAVAILABLE, .bytes = {}};
    return ReadBlockResponse();
}

bool PeerSession::SendBlockRequest(uint64_t height)
{
    if (!m_peer || !(m_peer->capabilities & CAP_SERVE_BLOCKS) || height == 0) return false;
    std::vector<unsigned char> request;
    Put64(request, height);
    return Write(Frame{MessageType::GET_BLOCK, request}, std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT);
}

BlockRequestResult PeerSession::ReadBlockResponse()
{
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    const auto meta = Read(deadline);
    if (!meta) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
        BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .bytes = {}};
    if (meta->type != MessageType::BLOCK_META || meta->payload.size() != 4)
        return {.status = BlockRequestStatus::INVALID_RESPONSE, .bytes = {}};
    const uint32_t size = Read32(meta->payload.data());
    if (size > MAX_FINALIZED_BLOCK_BYTES) return {.status = BlockRequestStatus::INVALID_RESPONSE, .bytes = {}};
    if (size == 0) return {.status = BlockRequestStatus::NOT_FOUND, .bytes = {}};
    std::vector<unsigned char> bytes;
    bytes.reserve(size);
    while (bytes.size() < size) {
        const auto chunk = Read(deadline);
        if (!chunk) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
            BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .bytes = {}};
        if (chunk->type != MessageType::BLOCK_CHUNK || chunk->payload.empty() ||
            chunk->payload.size() > size - bytes.size()) return {.status = BlockRequestStatus::INVALID_RESPONSE, .bytes = {}};
        bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
    }
    return {.status = BlockRequestStatus::OK, .bytes = std::move(bytes)};
}

BlockInventoryResult PeerSession::RequestBlockInventory(uint64_t first_height, uint8_t max_blocks)
{
    if (!m_peer || !(m_peer->capabilities & CAP_SERVE_BLOCKS) ||
        !(m_peer->capabilities & CAP_BLOCK_INVENTORY) || first_height == 0 ||
        max_blocks == 0 || max_blocks > MAX_BLOCK_INVENTORY ||
        first_height > std::numeric_limits<uint64_t>::max() - max_blocks + 1) {
        return {.status = BlockRequestStatus::INVALID_REQUEST, .blocks = {}};
    }
    std::vector<unsigned char> request;
    Put64(request, first_height);
    request.push_back(max_blocks);
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::GET_BLOCKS, request}, deadline)) {
        return {.status = BlockRequestStatus::UNAVAILABLE, .blocks = {}};
    }
    const auto response = Read(deadline);
    if (!response) return {.status = m_last_read_status == ReadStatus::INVALID_FRAME ?
        BlockRequestStatus::INVALID_RESPONSE : BlockRequestStatus::UNAVAILABLE, .blocks = {}};
    if (response->type != MessageType::BLOCK_INV || response->payload.empty()) {
        return {.status = BlockRequestStatus::INVALID_RESPONSE, .blocks = {}};
    }
    const uint8_t count = response->payload[0];
    if (count > max_blocks || response->payload.size() != 1U + size_t{count} * 40U) {
        return {.status = BlockRequestStatus::INVALID_RESPONSE, .blocks = {}};
    }
    if (count == 0) return {.status = BlockRequestStatus::NOT_FOUND, .blocks = {}};
    BlockInventoryResult result{.status = BlockRequestStatus::OK, .blocks = {}};
    result.blocks.reserve(count);
    for (uint8_t index{0}; index < count; ++index) {
        const size_t offset = 1U + size_t{index} * 40U;
        const uint64_t height = Read64(response->payload.data() + offset);
        uint256 block_id;
        std::copy_n(response->payload.begin() + offset + 8, 32, block_id.begin());
        if (height != first_height + index || block_id.IsNull()) {
            return {.status = BlockRequestStatus::INVALID_RESPONSE, .blocks = {}};
        }
        result.blocks.push_back({height, block_id});
    }
    return result;
}

std::optional<BlockAnnounceResult> PeerSession::AdvertiseBlock(
    const BlockAnnouncement& announcement, const FinalizedBlock& block, uint64_t& peer_finalized_height)
{
    peer_finalized_height = 0;
    if (!m_peer || !(m_peer->capabilities & CAP_BLOCK_ANNOUNCEMENTS) ||
        announcement.height == 0 || announcement.block_id.IsNull() ||
        block.block.height != announcement.height || ComputeBlockId(block.block) != announcement.block_id) return std::nullopt;
    std::vector<unsigned char> inventory{1};
    Put64(inventory, announcement.height);
    inventory.insert(inventory.end(), announcement.block_id.begin(), announcement.block_id.end());
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::BLOCK_INV, inventory}, deadline)) return std::nullopt;
    const auto answer = Read(deadline);
    if (!answer) return std::nullopt;
    if (answer->type == MessageType::GET_BLOCK && answer->payload.size() == 8 &&
        Read64(answer->payload.data()) == announcement.height) {
        const auto encoded = SerializeFinalizedBlock(block);
        if (!encoded || encoded->empty() || encoded->size() > MAX_FINALIZED_BLOCK_BYTES) return std::nullopt;
        std::vector<unsigned char> meta;
        Put32(meta, static_cast<uint32_t>(encoded->size()));
        if (!Write(Frame{MessageType::BLOCK_META, meta}, deadline)) return std::nullopt;
        for (size_t offset = 0; offset < encoded->size(); offset += MAX_FRAME_PAYLOAD) {
            const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, encoded->size() - offset);
            if (!Write(Frame{MessageType::BLOCK_CHUNK,
                {encoded->begin() + offset, encoded->begin() + offset + count}}, deadline)) return std::nullopt;
        }
    } else if (answer->type != MessageType::BLOCK_RESULT) {
        return std::nullopt;
    }
    const auto result = answer->type == MessageType::BLOCK_RESULT ? answer : Read(deadline);
    // Payload is [result:1][height:8][id:32] plus, from newer peers, the
    // acker's finalized height [peer_height:8] (49 bytes total).
    if (!result || result->type != MessageType::BLOCK_RESULT ||
        (result->payload.size() != 41 && result->payload.size() != 49) ||
        result->payload[0] > static_cast<uint8_t>(BlockAnnounceResult::GAP) ||
        Read64(result->payload.data() + 1) != announcement.height ||
        !std::equal(announcement.block_id.begin(), announcement.block_id.end(), result->payload.begin() + 9)) return std::nullopt;
    if (result->payload.size() == 49) peer_finalized_height = Read64(result->payload.data() + 41);
    return static_cast<BlockAnnounceResult>(result->payload[0]);
}

std::optional<OperationSubmitResult> PeerSession::SubmitOperation(const ProtocolOperation& operation)
{
    if (!m_peer || !(m_peer->capabilities & CAP_ACCEPT_OPERATIONS)) return std::nullopt;
    const auto bytes = SerializeProtocolOperation(operation);
    const auto op_id = ComputeOperationId(operation);
    if (!bytes || !op_id || bytes->empty() || bytes->size() > MAX_OPERATION_PAYLOAD_BYTES) return std::nullopt;
    std::vector<unsigned char> meta;
    Put32(meta, static_cast<uint32_t>(bytes->size()));
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::OP_META, meta}, deadline)) return std::nullopt;
    for (size_t offset = 0; offset < bytes->size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes->size() - offset);
        if (!Write(Frame{MessageType::OP_CHUNK,
            std::vector<unsigned char>{bytes->begin() + offset, bytes->begin() + offset + count}}, deadline)) return std::nullopt;
    }
    const auto response = Read(deadline);
    if (!response || response->type != MessageType::OP_RESULT || response->payload.size() != 33 ||
        response->payload[0] > static_cast<uint8_t>(OperationSubmitStatus::NETWORK_MISMATCH) ||
        !std::equal(op_id->begin(), op_id->end(), response->payload.begin() + 1)) return std::nullopt;
    return OperationSubmitResult{.status = static_cast<OperationSubmitStatus>(response->payload[0]),
        .op_id = *op_id};
}

std::vector<std::pair<std::string, uint16_t>> PeerSession::RequestPeers(std::chrono::steady_clock::time_point deadline)
{
    if (!m_peer || !(m_peer->capabilities & CAP_PEER_DISCOVERY)) return {};
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
    const uint256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    if (!m_peer || !(m_peer->capabilities & CAP_STORAGE) || publication_operation_id.IsNull() ||
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
    if (!m_peer || !(m_peer->capabilities & CAP_STORAGE) || IsZeroChunkId(chunk_id)) return std::nullopt;
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
    const uint256& publication_operation_id, const ChunkId& chunk_id)
{
    if (!m_peer || !(m_peer->capabilities & CAP_STORAGE_PROOFS) || publication_operation_id.IsNull() ||
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
    if (request->type == MessageType::PUT_AUTHORIZED_CHUNK) {
        if (!(m_local_capabilities & CAP_STORAGE) || request->payload.size() < 73) return false;
        uint256 publication_id;
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
        if (!(m_local_capabilities & CAP_STORAGE) || request->payload.size() != 32) return false;
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
        if (!(m_local_capabilities & CAP_STORAGE_PROOFS) || request->payload.size() != 64) return false;
        uint256 publication_id;
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
        if (!(m_local_capabilities & CAP_PEER_DISCOVERY)) return false;
        const auto endpoints = runtime.GetPeerEndpointsForGossip();
        return SendPeers(endpoints);
    }
    if (request->type == MessageType::PING) {
        return request->payload.size() == 8 && Write(Frame{MessageType::PONG, request->payload});
    }
    if (request->type == MessageType::OP_META) {
        if (!(m_local_capabilities & CAP_ACCEPT_OPERATIONS)) return false;
        if (request->payload.size() != 4) return false;
        const uint32_t size = Read32(request->payload.data());
        if (size == 0 || size > MAX_OPERATION_PAYLOAD_BYTES) return false;
        boost::system::error_code budget_error;
        const auto remote=m_socket.remote_endpoint(budget_error);
        if (budget_error || !runtime.AdmitIngress(remote.address().to_string(),IngressBudget::Work::OPERATION,size)) return false;
        const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
        std::vector<unsigned char> bytes;
        bytes.reserve(size);
        while (bytes.size() < size) {
            const auto chunk = Read(deadline);
            if (!chunk || chunk->type != MessageType::OP_CHUNK || chunk->payload.empty() ||
                chunk->payload.size() > size - bytes.size()) return false;
            bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
        }
        const auto operation = DeserializeProtocolOperation(bytes);
        if (!operation) return false;
        boost::system::error_code endpoint_error;
        const auto endpoint = m_socket.remote_endpoint(endpoint_error);
        if (endpoint_error) return false;
        const auto result = runtime.SubmitPeerOperation(*operation, endpoint.address().to_string());
        std::vector<unsigned char> response{static_cast<unsigned char>(result.status)};
        response.insert(response.end(), result.op_id.begin(), result.op_id.end());
        return Write(Frame{MessageType::OP_RESULT, response});
    }
    if (request->type == MessageType::BLOCK_INV) {
        if (!(m_local_capabilities & CAP_BLOCK_ANNOUNCEMENTS) ||
            request->payload.size() != 41 || request->payload[0] != 1) return false;
        const uint64_t height = Read64(request->payload.data() + 1);
        uint256 id;
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
        auto fetch_and_commit = [&](uint64_t h, const uint256& expected_id) ->
            std::optional<BlockAnnounceResult> {
            const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
            std::vector<unsigned char> query;
            Put64(query, h);
            if (!Write(Frame{MessageType::GET_BLOCK, query}, deadline)) return std::nullopt;
            const auto meta = Read(deadline);
            if (!meta || meta->type != MessageType::BLOCK_META || meta->payload.size() != 4) return std::nullopt;
            const uint32_t size = Read32(meta->payload.data());
            if (size == 0) {
                return BlockAnnounceResult::GAP; // peer does not have this height
            }
            if (size > MAX_FINALIZED_BLOCK_BYTES) return std::nullopt;
            std::vector<unsigned char> bytes;
            bytes.reserve(size);
            while (bytes.size() < size) {
                const auto chunk = Read(deadline);
                if (!chunk || chunk->type != MessageType::BLOCK_CHUNK || chunk->payload.empty() ||
                    chunk->payload.size() > size - bytes.size()) return std::nullopt;
                bytes.insert(bytes.end(), chunk->payload.begin(), chunk->payload.end());
            }
            const auto block = DeserializeFinalizedBlock(bytes);
            if (!block || block->block.height != h ||
                block->certificate.network_id != status.network_id ||
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
        auto have_height = [&](uint64_t h, const uint256& expected) {
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
            // ServeNext), so an unsolicited GET_BLOCK here would deadlock and
            // kill the session. Historical catch-up is instead driven by the
            // gossip worker through the client-side PeerManager::SyncFromPeer path.
            return acknowledge(BlockAnnounceResult::GAP);
        }
        const auto applied = fetch_and_commit(height, id);
        if (!applied) return false;
        return acknowledge(*applied);
    }
    if (request->type == MessageType::GET_BLOCKS) {
        if (!(m_local_capabilities & CAP_SERVE_BLOCKS) ||
            !(m_local_capabilities & CAP_BLOCK_INVENTORY) || request->payload.size() != 9) return false;
        const uint64_t first_height = Read64(request->payload.data());
        const uint8_t count = request->payload[8];
        if (first_height == 0 || count == 0 || count > MAX_BLOCK_INVENTORY ||
            first_height > std::numeric_limits<uint64_t>::max() - count + 1) return false;
        std::vector<unsigned char> inventory{0};
        for (uint8_t index{0}; index < count; ++index) {
            const uint64_t height = first_height + index;
            const auto finalized = runtime.GetBlockAtHeight(height);
            if (!finalized) break;
            const auto id = ComputeBlockId(finalized->block);
            if (id.IsNull()) return false;
            Put64(inventory, height);
            inventory.insert(inventory.end(), id.begin(), id.end());
            ++inventory[0];
        }
        return Write(Frame{MessageType::BLOCK_INV, inventory});
    }
    if (request->type != MessageType::GET_BLOCK || request->payload.size() != 8) return false;
    if (!(m_local_capabilities & CAP_SERVE_BLOCKS)) return false;
    const uint64_t height = Read64(request->payload.data());
    if (height == 0) return false;
    const auto block = runtime.GetBlockAtHeight(height);
    auto encoded = block ? SerializeFinalizedBlock(*block) : std::nullopt;
    if (block && !encoded) return false;
    const std::vector<unsigned char> empty;
    const auto& bytes = encoded ? *encoded : empty;
    if (bytes.size() > MAX_FINALIZED_BLOCK_BYTES) return false;
    std::vector<unsigned char> meta;
    Put32(meta, static_cast<uint32_t>(bytes.size()));
    const auto deadline = std::chrono::steady_clock::now() + BLOCK_TRANSFER_TIMEOUT;
    if (!Write(Frame{MessageType::BLOCK_META, meta}, deadline)) return false;
    for (size_t offset = 0; offset < bytes.size(); offset += MAX_FRAME_PAYLOAD) {
        const size_t count = std::min<size_t>(MAX_FRAME_PAYLOAD, bytes.size() - offset);
        if (!Write(Frame{MessageType::BLOCK_CHUNK,
            std::vector<unsigned char>{bytes.begin() + offset, bytes.begin() + offset + count}}, deadline)) return false;
    }
    return true;
}

} // namespace cybou::p2p
