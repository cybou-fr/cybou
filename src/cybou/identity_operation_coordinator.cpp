// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/operation_submit.h>
#include <cybou/identity_operation_coordinator.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/sha256.h>
#include <cybou/identity_crypto.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cybou {
namespace {
constexpr std::array<unsigned char, 5> JOURNAL_MAGIC{'C', 'Y', 'I', 'O', 1};
constexpr std::string_view JOURNAL_DOMAIN{"CYBOU/IDENTITY-OPERATION-JOURNAL/V1"};
constexpr size_t JOURNAL_FIXED_SIZE{5 + 32 + 32 + 8 + 8 + 1 + 32 + 32 + 4 + 32};
constexpr size_t MAX_JOURNALED_OPERATION_BYTES{8U * 1024U * 1024U};
constexpr size_t MAX_JOURNAL_BYTES{JOURNAL_FIXED_SIZE + MAX_JOURNALED_OPERATION_BYTES};

void Append64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (i * 8)));
}
void Append32(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (i * 8)));
}
uint64_t Read64(std::span<const unsigned char> bytes)
{
    uint64_t value{0};
    for (unsigned i = 0; i < 8; ++i) value |= uint64_t{bytes[i]} << (i * 8);
    return value;
}
uint32_t Read32(std::span<const unsigned char> bytes)
{
    uint32_t value{0};
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[i]} << (i * 8);
    return value;
}

bool DurableReplace(const std::filesystem::path& path, std::span<const unsigned char> bytes)
{
    std::error_code ec;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    auto temporary = path;
    temporary += ".tmp";
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    size_t written{0};
    bool ok{true};
    while (written < bytes.size()) {
        const DWORD count = static_cast<DWORD>(std::min<size_t>(bytes.size() - written, MAXDWORD));
        DWORD actual{0};
        if (!WriteFile(file, bytes.data() + written, count, &actual, nullptr) || actual == 0) { ok = false; break; }
        written += actual;
    }
    if (ok) ok = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
#else
    const int fd = ::open(temporary.c_str(), O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    size_t written{0};
    bool ok{true};
    while (written < bytes.size()) {
        const ssize_t count = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (count <= 0) { ok = false; break; }
        written += static_cast<size_t>(count);
    }
    if (ok) ok = ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (!ok || ::rename(temporary.c_str(), path.c_str()) != 0) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    if (path.parent_path().empty()) return true;
    const int dir_fd = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0) return false;
    ok = ::fsync(dir_fd) == 0;
    ::close(dir_fd);
    return ok;
#endif
}

std::optional<std::array<unsigned char, 32>> Checksum(std::span<const unsigned char> bytes)
{
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(JOURNAL_DOMAIN), bytes}, digest.data())) return std::nullopt;
    return digest;
}

std::optional<IdentityOperationAuthorization> AuthorizationOf(const ProtocolOperation& operation)
{
    return std::visit([](const auto& value) -> std::optional<IdentityOperationAuthorization> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AuthorizedPayment> || std::is_same_v<T, AuthorizedSystemLock> ||
            std::is_same_v<T, AuthorizedNameCommit> || std::is_same_v<T, AuthorizedNameReveal> ||
            std::is_same_v<T, AuthorizedRootPublication>) return value.authorization;
        return std::nullopt;
    }, operation);
}

std::optional<std::array<unsigned char, 32>> PackageCommitment(
    const CybouNodeRuntime& runtime, const AccountId& account, uint64_t epoch,
    const CybouKeyStore& keystore)
{
    const auto public_key = keystore.GetIdentityXWingPublicKey();
    const auto package = public_key ? EncodeIdentityKemPackage(*public_key) : std::nullopt;
    const auto raw_account = account.Value();
    return package ? ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{runtime.GetNetworkBinding().begin(), 32},
        std::span<const unsigned char, 32>{raw_account.begin(), 32}, epoch, *package) : std::nullopt;
}
} // namespace

struct IdentityOperationCoordinator::JournalEntry {
    cybou::Hash256 network_binding;
    AccountId account_id;
    uint64_t nonce{0};
    uint64_t key_epoch{0};
    uint8_t kind{0}; // zero denotes atomic IdentityRotate; other values are IdentityOperationKind.
    IdentityKeyId payload_commitment{};
    IdentityOperationPhase phase{IdentityOperationPhase::PREPARED};
    cybou::Hash256 op_id;
    std::vector<unsigned char> operation_bytes;
};

IdentityOperationCoordinator::IdentityOperationCoordinator(CybouNodeRuntime& runtime,
    CybouKeyStore& keystore, std::filesystem::path journal_path,
    const std::chrono::milliseconds relay_retry_interval)
    : m_runtime{runtime}, m_keystore{keystore}, m_journal_path{std::move(journal_path)},
      m_relay_retry_interval{relay_retry_interval}
{
    if (m_relay_retry_interval <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument{"relay retry interval must be positive"};
    }
}
IdentityOperationCoordinator::~IdentityOperationCoordinator() = default;

bool IdentityOperationCoordinator::LoadJournal()
{
    if (m_loaded) return m_load_error.empty();
    m_loaded = true;
    if (m_journal_path.empty() || !std::filesystem::exists(m_journal_path)) return true;
    std::ifstream file(m_journal_path, std::ios::binary);
    if (!file) { m_load_error = "Cannot open Identity operation journal"; return false; }
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    if (bytes.size() < JOURNAL_FIXED_SIZE || bytes.size() > MAX_JOURNAL_BYTES ||
        !std::equal(JOURNAL_MAGIC.begin(), JOURNAL_MAGIC.end(), bytes.begin())) {
        m_load_error = "Identity operation journal has an invalid format"; return false;
    }
    const size_t checksum_offset = bytes.size() - 32;
    const auto checksum = Checksum(std::span<const unsigned char>{bytes}.first(checksum_offset));
    if (!checksum || !std::equal(checksum->begin(), checksum->end(), bytes.begin() + checksum_offset)) {
        m_load_error = "Identity operation journal checksum failed"; return false;
    }
    size_t offset{5};
    auto entry = std::make_unique<JournalEntry>();
    std::copy_n(bytes.begin() + offset, 32, entry->network_binding.begin()); offset += 32;
    const auto account = AccountId::FromBytes(std::span<const unsigned char>{bytes}.subspan(offset, 32)); offset += 32;
    if (!account) { m_load_error = "Identity operation journal has an invalid AccountID"; return false; }
    entry->account_id = *account;
    entry->nonce = Read64(std::span<const unsigned char>{bytes}.subspan(offset, 8)); offset += 8;
    entry->key_epoch = Read64(std::span<const unsigned char>{bytes}.subspan(offset, 8)); offset += 8;
    entry->kind = bytes[offset++];
    std::copy_n(bytes.begin() + offset, 32, entry->payload_commitment.begin()); offset += 32;
    std::copy_n(bytes.begin() + offset, 32, entry->op_id.begin()); offset += 32;
    const uint32_t operation_size = Read32(std::span<const unsigned char>{bytes}.subspan(offset, 4)); offset += 4;
    if (entry->network_binding != m_runtime.GetNetworkBinding() || operation_size > MAX_JOURNALED_OPERATION_BYTES ||
        bytes.size() != offset + operation_size + 32) {
        m_load_error = "Identity operation journal belongs to a different network or has invalid lengths"; return false;
    }
    entry->operation_bytes.assign(bytes.begin() + offset, bytes.begin() + offset + operation_size);
    const auto operation = DeserializeProtocolOperation(entry->operation_bytes);
    const auto operation_id = operation ? ComputeOperationId(*operation) : std::nullopt;
    if (!operation || !operation_id || *operation_id != entry->op_id) {
        m_load_error = "Identity operation journal does not match its canonical operation"; return false;
    }
    if (entry->kind == 0) {
        const auto* rotate = std::get_if<IdentityRotate>(&*operation);
        if (!rotate || rotate->account_id != entry->account_id || rotate->nonce != entry->nonce ||
            rotate->key_epoch != entry->key_epoch) {
            m_load_error = "Identity rotation journal binding mismatch"; return false;
        }
    } else {
        const auto auth = AuthorizationOf(*operation);
        if (!auth || auth->account_id != entry->account_id || auth->nonce != entry->nonce ||
            auth->key_epoch != entry->key_epoch || static_cast<uint8_t>(auth->kind) != entry->kind ||
            auth->payload_commitment != entry->payload_commitment) {
            m_load_error = "Identity operation journal authorization mismatch"; return false;
        }
    }
    entry->phase = IdentityOperationPhase::UNCERTAIN;
    m_entry = std::move(entry);
    return true;
}

bool IdentityOperationCoordinator::SaveJournal(const JournalEntry& entry)
{
    if (m_journal_path.empty() || entry.operation_bytes.empty() || entry.operation_bytes.size() > MAX_JOURNALED_OPERATION_BYTES ||
        entry.operation_bytes.size() > std::numeric_limits<uint32_t>::max()) return false;
    std::vector<unsigned char> bytes(JOURNAL_MAGIC.begin(), JOURNAL_MAGIC.end());
    bytes.insert(bytes.end(), entry.network_binding.begin(), entry.network_binding.end());
    bytes.insert(bytes.end(), entry.account_id.Value().begin(), entry.account_id.Value().end());
    Append64(bytes, entry.nonce);
    Append64(bytes, entry.key_epoch);
    bytes.push_back(entry.kind);
    bytes.insert(bytes.end(), entry.payload_commitment.begin(), entry.payload_commitment.end());
    bytes.insert(bytes.end(), entry.op_id.begin(), entry.op_id.end());
    Append32(bytes, static_cast<uint32_t>(entry.operation_bytes.size()));
    bytes.insert(bytes.end(), entry.operation_bytes.begin(), entry.operation_bytes.end());
    const auto checksum = Checksum(bytes);
    if (!checksum) return false;
    bytes.insert(bytes.end(), checksum->begin(), checksum->end());
    return DurableReplace(m_journal_path, bytes);
}

bool IdentityOperationCoordinator::ClearJournal()
{
    if (m_journal_path.empty()) { m_entry.reset(); m_next_relay_retry.reset(); return true; }
    std::error_code ec;
    const bool removed = !std::filesystem::exists(m_journal_path, ec) || std::filesystem::remove(m_journal_path, ec);
    if (ec || !removed) return false;
    m_entry.reset();
    m_next_relay_retry.reset();
    return true;
}

IdentityOperationResult IdentityOperationCoordinator::SubmitExact(JournalEntry& entry)
{
    const auto operation = DeserializeProtocolOperation(entry.operation_bytes);
    if (!operation) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
        .error = "Journaled Identity operation cannot be decoded"};
    const bool relay_retry_in_flight = m_next_relay_retry.has_value();
    entry.phase = IdentityOperationPhase::SUBMITTING;
    if (!SaveJournal(entry)) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
        .error = "Could not persist Identity operation phase"};
    const auto result = m_runtime.SubmitOperation(*operation);
    if (!result) {
        if (result.delivery_uncertain) {
            if (relay_retry_in_flight) {
                m_next_relay_retry = std::chrono::steady_clock::now() + m_relay_retry_interval;
            }
            entry.phase = IdentityOperationPhase::UNCERTAIN;
            SaveJournal(entry);
            return {.phase = IdentityOperationPhase::UNCERTAIN, .op_id = entry.op_id,
                .error = "Operation delivery is uncertain; exact bytes are retained"};
        }
        const auto op_id = entry.op_id;
        ClearJournal();
        return {.phase = IdentityOperationPhase::REJECTED, .op_id = op_id,
            .error = "Identity operation was rejected"};
    }
    const auto result_phase = result.status == OperationSubmitStatus::ALREADY_FINALIZED ?
        IdentityOperationPhase::FINALIZED : IdentityOperationPhase::ACCEPTED;
    // A volatile queue acknowledgment must survive restart as retryable state.
    entry.phase = result.status == OperationSubmitStatus::RELAY_QUEUED ?
        IdentityOperationPhase::UNCERTAIN : result_phase;
    if (result.status == OperationSubmitStatus::RELAY_QUEUED ||
        (m_next_relay_retry && result.delivery_uncertain)) {
        m_next_relay_retry = std::chrono::steady_clock::now() + m_relay_retry_interval;
    } else {
        m_next_relay_retry.reset();
    }
    if (!SaveJournal(entry)) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
        .error = "Operation was admitted but its journal could not be updated"};
    const auto phase = result_phase;
    const auto op_id = entry.op_id;
    if (phase == IdentityOperationPhase::FINALIZED && entry.kind != 0) ClearJournal();
    return {.phase = phase, .op_id = op_id};
}

IdentityOperationResult IdentityOperationCoordinator::Reconcile(JournalEntry& entry)
{
    const auto status = m_runtime.GetOperationStatus(entry.op_id);
    if (status.kind == OperationStatusKind::FINALIZED) {
        m_next_relay_retry.reset();
        const auto op_id = entry.op_id;
        if (entry.kind != 0) {
            if (!ClearJournal()) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
                .finalized_height = status.finalized_height, .error = "Finalized journal could not be cleared"};
        }
        return {.phase = IdentityOperationPhase::FINALIZED, .op_id = op_id, .finalized_height = status.finalized_height};
    }
    if (status.kind == OperationStatusKind::REJECTED_KNOWN) {
        const auto id = entry.op_id;
        ClearJournal();
        return {.phase = IdentityOperationPhase::REJECTED, .op_id = id, .error = "Operation was explicitly rejected"};
    }
    const auto state = m_runtime.GetStore().LoadState();
    const auto* record = state && state.state ? state.state->identities.Find(entry.account_id) : nullptr;
    if (!record) return {.phase = IdentityOperationPhase::UNCERTAIN, .op_id = entry.op_id,
        .error = "Identity state is unavailable; journal retained"};
    if (entry.kind == 0) {
        const auto op = DeserializeProtocolOperation(entry.operation_bytes);
        const auto* rotate = op ? std::get_if<IdentityRotate>(&*op) : nullptr;
        if (!rotate) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
            .error = "Identity rotation journal is invalid"};
        const auto account_bytes = entry.account_id.Value();
        const auto expected_package = ComputeIdentityKemPackageCommitment(
            std::span<const unsigned char, 32>{entry.network_binding.begin(), 32},
            std::span<const unsigned char, 32>{account_bytes.begin(), 32},
            rotate->key_epoch, rotate->new_kem_package);
        if (record->nonce == entry.nonce + 1 && record->key_epoch == entry.key_epoch && expected_package &&
            record->recovery_key == rotate->new_recovery_key &&
            record->authorization_key == rotate->new_authorization_key &&
            record->kem_package_id == *expected_package) {
            entry.phase = IdentityOperationPhase::FINALIZED;
            return {.phase = IdentityOperationPhase::FINALIZED, .op_id = entry.op_id,
                .finalized_height = status.finalized_height};
        }
        if (record->nonce != entry.nonce || record->key_epoch + 1 != entry.key_epoch) {
            return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
                .error = "Finalized Identity state conflicts with pending rotation"};
        }
    } else if (record->nonce != entry.nonce || record->key_epoch != entry.key_epoch) {
        return {.phase = IdentityOperationPhase::CONFLICT, .op_id = entry.op_id,
            .error = "Finalized Identity nonce/epoch advanced without the journaled operation"};
    }
    if (status.kind == OperationStatusKind::LOCAL_PENDING || status.kind == OperationStatusKind::ACCEPTED_REMOTE) {
        entry.phase = IdentityOperationPhase::ACCEPTED;
        if (status.kind == OperationStatusKind::ACCEPTED_REMOTE && m_next_relay_retry &&
            std::chrono::steady_clock::now() >= *m_next_relay_retry) return SubmitExact(entry);
        return {.phase = IdentityOperationPhase::ACCEPTED, .op_id = entry.op_id};
    }
    if (m_next_relay_retry && std::chrono::steady_clock::now() < *m_next_relay_retry) {
        return {.phase = IdentityOperationPhase::UNCERTAIN, .op_id = entry.op_id,
            .error = "Relay acknowledgment is volatile; exact operation retry is scheduled"};
    }
    return SubmitExact(entry);
}

IdentityOperationResult IdentityOperationCoordinator::Execute(IdentityOperationKind kind,
    const IdentityKeyId& payload_commitment, const IdentityOperationBuilder& build)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = IdentityOperationPhase::CONFLICT, .error = m_load_error};
    const auto account = m_keystore.GetAccountId();
    if (!account) return {.phase = IdentityOperationPhase::REJECTED, .error = "No Identity is unlocked"};
    if (m_entry) {
        const bool same_request = m_entry->account_id == *account &&
            m_entry->kind == static_cast<uint8_t>(kind) && m_entry->payload_commitment == payload_commitment;
        if (!same_request) {
            const auto status = m_runtime.GetOperationStatus(m_entry->op_id);
            if (status.kind != OperationStatusKind::FINALIZED &&
                status.kind != OperationStatusKind::REJECTED_KNOWN) {
                return {.phase = IdentityOperationPhase::CONFLICT, .op_id = m_entry->op_id,
                    .error = "Another Identity operation is unresolved"};
            }
        }
        const auto previous = Reconcile(*m_entry);
        // A finalized NameCommit may have expired and been pruned from state.
        // NameService retries the same saved claim in that case, with the next
        // Identity nonce, so a resolved journal must not make that retry idempotent.
        if (same_request && (previous.phase != IdentityOperationPhase::FINALIZED ||
            kind != IdentityOperationKind::NAME_COMMIT)) return previous;
        if (m_entry) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = previous.op_id,
            .error = previous.error.empty() ? "Another Identity operation is unresolved" : previous.error};
    }
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    const auto auth_key = m_keystore.GetAuthorizationPublicKey();
    const auto root_key = m_keystore.GetRecoveryPublicKey();
    const auto package_id = PackageCommitment(m_runtime, *account, record ? record->key_epoch : 0, m_keystore);
    if (!record || !auth_key || !root_key || record->authorization_key != *auth_key ||
        record->recovery_key != *root_key || !package_id || *package_id != record->kem_package_id) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Unlocked Identity does not match finalized state"};
    }
    IdentityOperationAuthorization auth{
        .account_id = *account, .nonce = record->nonce, .key_epoch = record->key_epoch,
        .kind = kind, .payload_commitment = payload_commitment, .signature = {},
    };
    const auto digest = ComputeIdentityOperationDigest(m_runtime.GetNetworkBinding(), auth);
    const auto signature = digest ? m_keystore.SignAuthorization(*digest) : std::nullopt;
    const auto operation = signature ? [&] {
        auth.signature = *signature;
        return build(auth);
    }() : std::nullopt;
    const auto bytes = operation ? SerializeProtocolOperation(*operation) : std::nullopt;
    const auto op_id = operation ? ComputeOperationId(*operation) : std::nullopt;
    if (!bytes || !op_id) return {.phase = IdentityOperationPhase::REJECTED,
        .error = "Could not sign or build canonical Identity operation"};
    auto entry = std::make_unique<JournalEntry>(JournalEntry{
        .network_binding = m_runtime.GetNetworkBinding(), .account_id = *account, .nonce = record->nonce,
        .key_epoch = record->key_epoch, .kind = static_cast<uint8_t>(kind),
        .payload_commitment = payload_commitment, .phase = IdentityOperationPhase::PREPARED,
        .op_id = *op_id, .operation_bytes = *bytes,
    });
    if (!SaveJournal(*entry)) return {.phase = IdentityOperationPhase::REJECTED,
        .error = "Could not durably journal Identity operation before submission"};
    m_entry = std::move(entry);
    return SubmitExact(*m_entry);
}

IdentityOperationResult IdentityOperationCoordinator::RotateIdentity(
    std::span<const unsigned char, 32> new_identity_entropy)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = IdentityOperationPhase::CONFLICT, .error = m_load_error};
    const auto account = m_keystore.GetAccountId();
    if (!account) return {.phase = IdentityOperationPhase::REJECTED, .error = "No Identity is unlocked"};
    if (m_entry) {
        if (m_entry->account_id == *account && m_entry->kind == 0) return Reconcile(*m_entry);
        // A finalized or rejected earlier operation (for example the
        // RecoveryBridge publication) is cleared and must not block rotation.
        if (m_entry->account_id == *account) (void)Reconcile(*m_entry);
        if (m_entry) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = m_entry->op_id,
            .error = "Another Identity operation is unresolved"};
    }
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    const auto old_root = m_keystore.GetRecoveryPublicKey();
    if (!record || !old_root || record->recovery_key != *old_root) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Unlocked recovery key does not match finalized Identity"};
    }
    if (record->nonce == std::numeric_limits<uint64_t>::max() || record->key_epoch == std::numeric_limits<uint64_t>::max()) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Identity nonce or key epoch is exhausted"};
    }
    const auto new_root = DeriveIdentityPublicKey(new_identity_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto new_authorization = DeriveIdentityPublicKey(new_identity_entropy, IdentityKeyPurpose::AUTHORIZATION);
    auto seed = DeriveIdentityXWingSeed(new_identity_entropy);
    const auto public_key = seed ? DeriveXWingPublicKey(*seed) : std::nullopt;
    if (seed) crypto::CleanseMemory(seed->data(), seed->size());
    const auto package = public_key ? EncodeIdentityKemPackage(*public_key) : std::nullopt;
    if (!new_root || !new_authorization || !package || !m_keystore.GetRecoveryPublicKey() ||
        *new_root == *old_root) return {.phase = IdentityOperationPhase::REJECTED,
            .error = "New Identity key material is invalid or unchanged"};
    IdentityRotate rotate{
        .account_id = *account,
        .new_recovery_key = *new_root,
        .new_authorization_key = *new_authorization,
        .new_kem_package = *package,
        .nonce = record->nonce,
        .key_epoch = record->key_epoch + 1,
    };
    const auto digest = ComputeIdentityRotateDigest(m_runtime.GetNetworkBinding(), rotate);
    const auto old_signature = digest ? m_keystore.SignRecovery(*digest) : std::nullopt;
    const auto new_root_pop = digest ? SignIdentityMessage(new_identity_entropy, IdentityKeyPurpose::RECOVERY_ROOT, *digest) : std::nullopt;
    const auto new_auth_pop = digest ? SignIdentityMessage(new_identity_entropy, IdentityKeyPurpose::AUTHORIZATION, *digest) : std::nullopt;
    if (!digest || !old_signature || !new_root_pop || !new_auth_pop) return {.phase = IdentityOperationPhase::REJECTED,
        .error = "Could not sign atomic Identity rotation"};
    rotate.old_recovery_signature = *old_signature;
    rotate.new_recovery_pop = *new_root_pop;
    rotate.new_authorization_pop = *new_auth_pop;
    const ProtocolOperation operation{rotate};
    const auto bytes = SerializeProtocolOperation(operation);
    const auto op_id = ComputeOperationId(operation);
    if (!bytes || !op_id) return {.phase = IdentityOperationPhase::REJECTED,
        .error = "Could not encode atomic Identity rotation"};
    IdentityKeyId payload{};
    std::copy_n(op_id->begin(), payload.size(), payload.begin());
    auto entry = std::make_unique<JournalEntry>(JournalEntry{
        .network_binding = m_runtime.GetNetworkBinding(), .account_id = *account, .nonce = record->nonce,
        .key_epoch = rotate.key_epoch, .kind = 0, .payload_commitment = payload,
        .phase = IdentityOperationPhase::PREPARED, .op_id = *op_id, .operation_bytes = *bytes,
    });
    if (!SaveJournal(*entry)) return {.phase = IdentityOperationPhase::REJECTED,
        .error = "Could not durably journal Identity rotation before submission"};
    m_entry = std::move(entry);
    return SubmitExact(*m_entry);
}

bool IdentityOperationCoordinator::RetryRelayIfDue()
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal() || !m_entry || (!m_next_relay_retry &&
        m_entry->phase != IdentityOperationPhase::UNCERTAIN &&
        m_entry->phase != IdentityOperationPhase::SUBMITTING &&
        m_entry->phase != IdentityOperationPhase::PREPARED)) return false;
    const auto status = m_runtime.GetOperationStatus(m_entry->op_id);
    if (status.kind == OperationStatusKind::FINALIZED ||
        status.kind == OperationStatusKind::REJECTED_KNOWN ||
        status.kind == OperationStatusKind::LOCAL_PENDING) return false;

    const auto now = std::chrono::steady_clock::now();
    if (!m_next_relay_retry) m_next_relay_retry = now;
    if (now < *m_next_relay_retry) return false;
    (void)SubmitExact(*m_entry);
    return true;
}

bool IdentityOperationCoordinator::CompleteIdentityRotation(const IdentityRecord& finalized_identity)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal() || !m_entry || m_entry->kind != 0) return false;
    const auto operation = DeserializeProtocolOperation(m_entry->operation_bytes);
    const auto* rotate = operation ? std::get_if<IdentityRotate>(&*operation) : nullptr;
    const auto account_bytes = rotate ? rotate->account_id.Value() : cybou::Hash256{};
    const auto expected_package = rotate ? ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32},
        rotate->key_epoch, rotate->new_kem_package) : std::nullopt;
    if (!rotate || !expected_package || finalized_identity.kem_package_id != *expected_package ||
        finalized_identity.recovery_key != rotate->new_recovery_key ||
        finalized_identity.authorization_key != rotate->new_authorization_key ||
        finalized_identity.key_epoch != rotate->key_epoch || finalized_identity.nonce != rotate->nonce + 1) return false;
    return ClearJournal();
}

bool IdentityOperationCoordinator::HasPendingIdentityRotation()
{
    std::lock_guard lock(m_mutex);
    return LoadJournal() && m_entry && m_entry->kind == 0;
}

IdentityOperationResult IdentityOperationCoordinator::GetStatus(const cybou::Hash256& op_id)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = op_id, .error = m_load_error};
    const auto status = m_runtime.GetOperationStatus(op_id);
    if (status.kind == OperationStatusKind::FINALIZED) {
        if (m_entry && m_entry->op_id == op_id) m_next_relay_retry.reset();
        return {.phase = IdentityOperationPhase::FINALIZED, .op_id = op_id,
            .finalized_height = status.finalized_height};
    }
    if (status.kind == OperationStatusKind::REJECTED_KNOWN) {
        if (m_entry && m_entry->op_id == op_id) m_next_relay_retry.reset();
        return {.phase = IdentityOperationPhase::REJECTED, .op_id = op_id,
            .error = "Operation was rejected"};
    }
    if (m_entry && m_entry->op_id == op_id && m_next_relay_retry &&
        std::chrono::steady_clock::now() >= *m_next_relay_retry) return SubmitExact(*m_entry);
    if (status.kind == OperationStatusKind::LOCAL_PENDING || status.kind == OperationStatusKind::ACCEPTED_REMOTE)
        return {.phase = IdentityOperationPhase::ACCEPTED, .op_id = op_id};
    return {.phase = IdentityOperationPhase::UNCERTAIN, .op_id = op_id,
        .error = "Operation status is unknown; exact bytes remain journaled"};
}

} // namespace cybou
