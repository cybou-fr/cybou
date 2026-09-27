// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/device_operation_coordinator.h>

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

constexpr std::array<unsigned char, 5> JOURNAL_MAGIC{'C', 'Y', 'D', 'O', 1};
constexpr std::string_view JOURNAL_DOMAIN{"CYBOU/DEVICE-OPERATION-JOURNAL/V1"};
constexpr size_t JOURNAL_FIXED_SIZE{5 + 32 + 32 + 32 + 8 + 8 + 1 + 32 + 1 + 32 + 4 + 32};
constexpr size_t MAX_JOURNALED_OPERATION_BYTES{8U * 1024U * 1024U};
constexpr size_t MAX_JOURNAL_BYTES{JOURNAL_FIXED_SIZE + MAX_JOURNALED_OPERATION_BYTES};

enum class JournalOperationKind : uint8_t {
    PAYMENT = 1,
    MAIL = 2,
    SYSTEM_LOCK = 3,
    NAME_COMMIT = 4,
    NAME_REVEAL = 5,
    RECOVERY_DEVICE_ADD = 6,
    ROOT_DEVICE_REVOKE = 7,
};

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
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    auto temporary = path;
    temporary += ".tmp";
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    size_t written{0};
    bool ok{true};
    while (written < bytes.size()) {
        const DWORD count = static_cast<DWORD>(std::min<size_t>(bytes.size() - written, MAXDWORD));
        DWORD actual{0};
        if (!WriteFile(file, bytes.data() + written, count, &actual, nullptr) || actual == 0) {
            ok = false;
            break;
        }
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
        if (count <= 0) {
            ok = false;
            break;
        }
        written += static_cast<size_t>(count);
    }
    if (ok) ok = ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (!ok || ::rename(temporary.c_str(), path.c_str()) != 0) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    const int dir_fd = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0) return false;
    ok = ::fsync(dir_fd) == 0;
    ::close(dir_fd);
    return ok;
#endif
}

std::optional<DeviceAuthorization> AuthorizationOf(const ProtocolOperation& operation)
{
    return std::visit([](const auto& value) -> std::optional<DeviceAuthorization> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AuthorizedPayment> ||
            std::is_same_v<T, AuthorizedSystemLock> ||
            std::is_same_v<T, AuthorizedNameCommit> ||
            std::is_same_v<T, AuthorizedNameReveal> ||
            std::is_same_v<T, AuthorizedMail>) return value.authorization;
        return std::nullopt;
    }, operation);
}

const DeviceAdd* RecoveredDeviceAddOf(const ProtocolOperation& operation)
{
    const auto* add = std::get_if<DeviceAdd>(&operation);
    return add ? add : nullptr;
}

bool IsRootAuthorizedOperation(JournalOperationKind kind)
{
    return kind == JournalOperationKind::RECOVERY_DEVICE_ADD ||
        kind == JournalOperationKind::ROOT_DEVICE_REVOKE;
}

std::optional<IdentityKeyId> RecoveredDeviceId(const DeviceAdd& operation)
{
    return ComputeDeviceKeyId(operation.new_device);
}

} // namespace

struct DeviceOperationCoordinator::JournalEntry {
    uint256 network_id;
    AccountId account_id;
    IdentityKeyId device_id{};
    uint64_t nonce{0};
    uint64_t activation_nonce{0};
    JournalOperationKind kind{JournalOperationKind::PAYMENT};
    IdentityKeyId payload_commitment{};
    DeviceOperationPhase phase{DeviceOperationPhase::PREPARED};
    uint256 op_id;
    std::vector<unsigned char> operation_bytes;
};

DeviceOperationCoordinator::DeviceOperationCoordinator(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
    std::filesystem::path journal_path)
    : m_runtime{runtime}, m_keystore{keystore}, m_journal_path{std::move(journal_path)}
{
}

DeviceOperationCoordinator::~DeviceOperationCoordinator() = default;

bool DeviceOperationCoordinator::LoadJournal()
{
    if (m_loaded) return m_load_error.empty();
    m_loaded = true;
    if (m_journal_path.empty()) return true;
    if (!std::filesystem::exists(m_journal_path)) return true;
    std::ifstream input{m_journal_path, std::ios::binary};
    if (!input) {
        m_load_error = "Cannot open device operation journal";
        return false;
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>{input}), std::istreambuf_iterator<char>{});
    if (input.bad() || bytes.size() < JOURNAL_FIXED_SIZE || bytes.size() > MAX_JOURNAL_BYTES) {
        m_load_error = "Device operation journal has an invalid size";
        return false;
    }
    const size_t operation_size_offset = JOURNAL_FIXED_SIZE - 4 - 32;
    const uint32_t operation_size = Read32(std::span<const unsigned char>{bytes}.subspan(operation_size_offset, 4));
    if (operation_size == 0 || operation_size > MAX_JOURNALED_OPERATION_BYTES ||
        bytes.size() != JOURNAL_FIXED_SIZE + operation_size) {
        m_load_error = "Device operation journal length is invalid";
        return false;
    }
    std::array<unsigned char, 32> checksum{};
    crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(JOURNAL_DOMAIN.data()), JOURNAL_DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size() - checksum.size());
    hasher.Finalize(checksum.data());
    if (!std::equal(checksum.begin(), checksum.end(), bytes.end() - checksum.size()) ||
        !std::equal(JOURNAL_MAGIC.begin(), JOURNAL_MAGIC.end(), bytes.begin())) {
        m_load_error = "Device operation journal checksum or format is invalid";
        return false;
    }

    auto entry = std::make_unique<JournalEntry>();
    size_t offset{5};
    std::copy_n(bytes.begin() + offset, 32, entry->network_id.begin()); offset += 32;
    const auto account = AccountId::FromBytes(std::span<const unsigned char>{bytes}.subspan(offset, 32));
    if (!account) {
        m_load_error = "Device operation journal account is invalid";
        return false;
    }
    entry->account_id = *account; offset += 32;
    std::copy_n(bytes.begin() + offset, 32, entry->device_id.begin()); offset += 32;
    entry->nonce = Read64(std::span<const unsigned char>{bytes}.subspan(offset, 8)); offset += 8;
    entry->activation_nonce = Read64(std::span<const unsigned char>{bytes}.subspan(offset, 8)); offset += 8;
    const uint8_t kind = bytes[offset++];
    if (kind < static_cast<uint8_t>(JournalOperationKind::PAYMENT) ||
        kind > static_cast<uint8_t>(JournalOperationKind::ROOT_DEVICE_REVOKE)) {
        m_load_error = "Device operation journal kind is invalid";
        return false;
    }
    entry->kind = static_cast<JournalOperationKind>(kind);
    std::copy_n(bytes.begin() + offset, 32, entry->payload_commitment.begin()); offset += 32;
    const uint8_t phase = bytes[offset++];
    if (phase > static_cast<uint8_t>(DeviceOperationPhase::CONFLICT)) {
        m_load_error = "Device operation journal phase is invalid";
        return false;
    }
    entry->phase = static_cast<DeviceOperationPhase>(phase);
    std::copy_n(bytes.begin() + offset, 32, entry->op_id.begin()); offset += 32;
    offset += 4;
    entry->operation_bytes.assign(bytes.begin() + offset, bytes.end() - 32);
    const auto operation = DeserializeProtocolOperation(entry->operation_bytes);
    if (!operation || ComputeOperationId(*operation) != entry->op_id) {
        m_load_error = "Device operation journal contains an invalid operation";
        return false;
    }
    const auto auth = AuthorizationOf(*operation);
    const auto* root_add = RecoveredDeviceAddOf(*operation);
    const auto root_device_id = root_add ? RecoveredDeviceId(*root_add) : std::nullopt;
    const auto* root_revoke = std::get_if<DeviceRevoke>(&*operation);
    const bool valid_device_operation = auth && !IsRootAuthorizedOperation(entry->kind) &&
        auth->account_id == entry->account_id && auth->device_id == entry->device_id &&
        auth->nonce == entry->nonce && auth->activation_nonce == entry->activation_nonce &&
        static_cast<uint8_t>(auth->kind) == static_cast<uint8_t>(entry->kind) &&
        auth->payload_commitment == entry->payload_commitment;
    const bool valid_root_device_add = entry->kind == JournalOperationKind::RECOVERY_DEVICE_ADD &&
        root_add && root_device_id &&
        root_add->account_id == entry->account_id && root_add->root_nonce == entry->nonce &&
        *root_device_id == entry->payload_commitment && entry->device_id == IdentityKeyId{} &&
        entry->activation_nonce == 0;
    const bool valid_root_device_revoke = entry->kind == JournalOperationKind::ROOT_DEVICE_REVOKE &&
        root_revoke && root_revoke->account_id == entry->account_id &&
        root_revoke->root_nonce == entry->nonce && root_revoke->device_id == entry->payload_commitment &&
        entry->device_id == IdentityKeyId{} && entry->activation_nonce == 0;
    if ((!valid_device_operation && !valid_root_device_add && !valid_root_device_revoke) ||
        entry->network_id != m_runtime.GetNetworkId()) {
        m_load_error = "Device operation journal binding is invalid";
        return false;
    }
    m_entry = std::move(entry);
    return true;
}

bool DeviceOperationCoordinator::SaveJournal(const JournalEntry& entry)
{
    if (entry.operation_bytes.empty() || entry.operation_bytes.size() > MAX_JOURNALED_OPERATION_BYTES) return false;
    if (m_journal_path.empty()) return true;
    std::vector<unsigned char> bytes(JOURNAL_MAGIC.begin(), JOURNAL_MAGIC.end());
    bytes.insert(bytes.end(), entry.network_id.begin(), entry.network_id.end());
    bytes.insert(bytes.end(), entry.account_id.Value().begin(), entry.account_id.Value().end());
    bytes.insert(bytes.end(), entry.device_id.begin(), entry.device_id.end());
    Append64(bytes, entry.nonce);
    Append64(bytes, entry.activation_nonce);
    bytes.push_back(static_cast<unsigned char>(entry.kind));
    bytes.insert(bytes.end(), entry.payload_commitment.begin(), entry.payload_commitment.end());
    bytes.push_back(static_cast<unsigned char>(entry.phase));
    bytes.insert(bytes.end(), entry.op_id.begin(), entry.op_id.end());
    Append32(bytes, static_cast<uint32_t>(entry.operation_bytes.size()));
    bytes.insert(bytes.end(), entry.operation_bytes.begin(), entry.operation_bytes.end());
    std::array<unsigned char, 32> checksum{};
    crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(JOURNAL_DOMAIN.data()), JOURNAL_DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(checksum.data());
    bytes.insert(bytes.end(), checksum.begin(), checksum.end());
    return DurableReplace(m_journal_path, bytes);
}

bool DeviceOperationCoordinator::ClearJournal()
{
    if (m_journal_path.empty()) {
        m_entry.reset();
        return true;
    }
    std::error_code ec;
    const bool removed = std::filesystem::remove(m_journal_path, ec);
    if (ec) return false;
    m_entry.reset();
    return removed || !std::filesystem::exists(m_journal_path);
}

DeviceOperationResult DeviceOperationCoordinator::SubmitExact(JournalEntry& entry)
{
    entry.phase = DeviceOperationPhase::SUBMITTING;
    if (!SaveJournal(entry)) return {.phase = DeviceOperationPhase::UNCERTAIN, .op_id = entry.op_id,
        .error = "Could not durably record submission state"};
    const auto operation = DeserializeProtocolOperation(entry.operation_bytes);
    if (!operation) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = entry.op_id,
        .error = "Saved operation bytes are invalid"};
    const auto submitted = m_runtime.SubmitOperation(*operation);
    if (submitted.op_id != entry.op_id) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = entry.op_id,
        .error = "Runtime returned a different OperationID"};
    if (submitted.delivery_uncertain) {
        entry.phase = DeviceOperationPhase::UNCERTAIN;
        if (!SaveJournal(entry)) return {.phase = entry.phase, .op_id = entry.op_id,
            .error = "Delivery is uncertain and journal update failed; saved operation remains reserved"};
        return {.phase = entry.phase, .op_id = entry.op_id,
            .error = "Delivery is uncertain; the exact saved operation remains reserved"};
    }
    if (submitted) {
        entry.phase = DeviceOperationPhase::ACCEPTED;
        if (!SaveJournal(entry)) return {.phase = DeviceOperationPhase::UNCERTAIN, .op_id = entry.op_id,
            .error = "Operation was submitted but its accepted state could not be saved"};
        const auto status = m_runtime.GetOperationStatus(entry.op_id);
        if (status.kind == OperationStatusKind::FINALIZED) {
            entry.phase = DeviceOperationPhase::FINALIZED;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id, .finalized_height = status.finalized_height};
        }
        return {.phase = entry.phase, .op_id = entry.op_id};
    }
    entry.phase = DeviceOperationPhase::REJECTED;
    if (!SaveJournal(entry)) return {.phase = entry.phase, .op_id = entry.op_id,
        .error = "Operation was rejected and journal update failed"};
    return {.phase = entry.phase, .op_id = entry.op_id, .error = "Operation was rejected"};
}

DeviceOperationResult DeviceOperationCoordinator::Reconcile(JournalEntry& entry)
{
    const auto status = m_runtime.GetOperationStatus(entry.op_id);
    if (status.kind == OperationStatusKind::FINALIZED) {
        entry.phase = DeviceOperationPhase::FINALIZED;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id, .finalized_height = status.finalized_height};
    }
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(entry.account_id) : nullptr;
    if (IsRootAuthorizedOperation(entry.kind)) {
        if (!record) {
            entry.phase = DeviceOperationPhase::CONFLICT;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id,
                .error = "Recovered account is unavailable in finalized identity state"};
        }
        const auto recovery_id = ComputeRecoveryKeyId(record->recovery_root);
        const auto local_recovery_key = m_keystore.GetRecoveryPublicKey();
        if (!recovery_id || !local_recovery_key || *local_recovery_key != record->recovery_root) {
            entry.phase = DeviceOperationPhase::CONFLICT;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id,
                .error = "Saved recovery authorization no longer matches finalized identity state"};
        }
        if (record->next_root_nonce > entry.nonce) {
            const auto lookup = m_runtime.FindFinalizedOperation(entry.op_id);
            if (lookup.status == FinalizedOperationLookupStatus::FOUND) {
                entry.phase = DeviceOperationPhase::FINALIZED;
                SaveJournal(entry);
                return {.phase = entry.phase, .op_id = entry.op_id, .finalized_height = lookup.height};
            }
            entry.phase = DeviceOperationPhase::CONFLICT;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id,
                .error = "Finalized recovery nonce advanced without this OperationID; reconciliation required"};
        }
        if (record->next_root_nonce < entry.nonce) {
            entry.phase = DeviceOperationPhase::CONFLICT;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id,
                .error = "Saved recovery nonce is ahead of finalized identity state"};
        }
        if (status.kind == OperationStatusKind::REJECTED_KNOWN) {
            entry.phase = DeviceOperationPhase::REJECTED;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id,
                .error = "Root-authorized identity operation was rejected"};
        }
        if (status.kind == OperationStatusKind::LOCAL_PENDING || status.kind == OperationStatusKind::ACCEPTED_REMOTE) {
            entry.phase = DeviceOperationPhase::ACCEPTED;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id};
        }
        entry.phase = DeviceOperationPhase::UNCERTAIN;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id,
            .error = "Root-authorized operation status is unknown; its root nonce remains reserved"};
    }
    const auto device = record ? record->devices.find(entry.device_id) : std::map<IdentityKeyId, IdentityDevice>::const_iterator{};
    if (!record || device == record->devices.end() || device->second.activation_nonce != entry.activation_nonce) {
        entry.phase = DeviceOperationPhase::CONFLICT;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id,
            .error = "Saved device authorization no longer matches finalized identity state"};
    }
    if (device->second.next_nonce > entry.nonce) {
        const auto lookup = m_runtime.FindFinalizedOperation(entry.op_id);
        if (lookup.status == FinalizedOperationLookupStatus::FOUND) {
            entry.phase = DeviceOperationPhase::FINALIZED;
            SaveJournal(entry);
            return {.phase = entry.phase, .op_id = entry.op_id, .finalized_height = lookup.height};
        }
        entry.phase = DeviceOperationPhase::CONFLICT;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id,
            .error = "Finalized device nonce advanced without this OperationID; reconciliation required (nonce=" +
                std::to_string(device->second.next_nonce) + ", reserved=" + std::to_string(entry.nonce) +
                ", lookup=" + std::to_string(static_cast<unsigned>(lookup.status)) + ")"};
    }
    if (device->second.next_nonce < entry.nonce) {
        entry.phase = DeviceOperationPhase::CONFLICT;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id,
            .error = "Saved nonce is ahead of finalized device state; reconciliation required"};
    }
    if (status.kind == OperationStatusKind::REJECTED_KNOWN) {
        entry.phase = DeviceOperationPhase::REJECTED;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id, .error = "Operation was rejected by the node"};
    }
    if (status.kind == OperationStatusKind::LOCAL_PENDING || status.kind == OperationStatusKind::ACCEPTED_REMOTE) {
        entry.phase = DeviceOperationPhase::ACCEPTED;
        SaveJournal(entry);
        return {.phase = entry.phase, .op_id = entry.op_id};
    }
    entry.phase = DeviceOperationPhase::UNCERTAIN;
    SaveJournal(entry);
    return {.phase = entry.phase, .op_id = entry.op_id,
        .error = "Operation status is unknown; its nonce remains reserved"};
}

DeviceOperationResult DeviceOperationCoordinator::AuthorizeRecoveredDevice()
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .error = m_load_error};
    const auto account = m_keystore.GetAccountId();
    const auto device_key = m_keystore.GetDevicePublicKey();
    const auto root_key = m_keystore.GetRecoveryPublicKey();
    const auto device_id = device_key ? ComputeDeviceKeyId(*device_key) : std::nullopt;
    if (!account || !device_key || !root_key || !device_id) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Recovered identity keys are unavailable"};
    }

    if (m_entry) {
        const auto reconciliation = Reconcile(*m_entry);
        if (reconciliation.phase == DeviceOperationPhase::FINALIZED ||
            reconciliation.phase == DeviceOperationPhase::REJECTED) {
            if (!ClearJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = reconciliation.op_id,
                .error = "Reconciled operation journal could not be cleared"};
        } else if (reconciliation.phase == DeviceOperationPhase::CONFLICT) {
            return reconciliation;
        } else {
            const bool same_request = m_entry->kind == JournalOperationKind::RECOVERY_DEVICE_ADD &&
                m_entry->account_id == *account &&
                m_entry->payload_commitment == *device_id;
            if (reconciliation.phase == DeviceOperationPhase::UNCERTAIN) {
                const auto retry = SubmitExact(*m_entry);
                if (!same_request && retry.phase != DeviceOperationPhase::REJECTED) {
                    return {.phase = DeviceOperationPhase::CONFLICT, .op_id = retry.op_id,
                        .error = "An earlier operation is unresolved; its exact bytes were retried"};
                }
                return retry;
            }
            if (same_request) return reconciliation;
            return {.phase = DeviceOperationPhase::CONFLICT, .op_id = m_entry->op_id,
                .error = "An earlier operation is still pending"};
        }
    }

    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    if (!record || record->recovery_root != *root_key) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Recovery root is not authorized by finalized identity state"};
    }
    if (record->devices.contains(*device_id)) {
        return {.phase = DeviceOperationPhase::FINALIZED, .error = "Recovered device is already authorized"};
    }
    if (record->devices.size() >= MAX_ACTIVE_DEVICES) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "The account has reached its device limit"};
    }
    if (record->next_root_nonce == std::numeric_limits<uint64_t>::max()) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Recovery nonce is exhausted"};
    }

    DeviceAdd operation{.account_id = *account, .new_device = *device_key,
        .root_nonce = record->next_root_nonce, .root_signature = {}, .device_pop = {}};
    const auto digest = ComputeDeviceAddDigest(m_runtime.GetNetworkId(), operation);
    const auto root_signature = digest ? m_keystore.SignRecovery(*digest) : std::nullopt;
    const auto device_pop = digest ? m_keystore.SignDevice(*digest) : std::nullopt;
    if (!digest || !root_signature || !device_pop) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Cannot sign recovered device authorization"};
    }
    operation.root_signature = *root_signature;
    operation.device_pop = *device_pop;
    const ProtocolOperation protocol_operation{operation};
    const auto bytes = SerializeProtocolOperation(protocol_operation);
    const auto op_id = ComputeOperationId(protocol_operation);
    if (!bytes || bytes->empty() || bytes->size() > MAX_JOURNALED_OPERATION_BYTES || !op_id || op_id->IsNull()) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Cannot encode recovered device authorization"};
    }

    auto entry = std::make_unique<JournalEntry>();
    entry->network_id = m_runtime.GetNetworkId();
    entry->account_id = *account;
    entry->device_id = {};
    entry->nonce = operation.root_nonce;
    entry->activation_nonce = 0;
    entry->kind = JournalOperationKind::RECOVERY_DEVICE_ADD;
    entry->payload_commitment = *device_id;
    entry->phase = DeviceOperationPhase::PREPARED;
    entry->op_id = *op_id;
    entry->operation_bytes = *bytes;
    if (!SaveJournal(*entry)) return {.phase = DeviceOperationPhase::REJECTED, .op_id = *op_id,
        .error = "Could not durably save recovered device operation before submission"};
    m_entry = std::move(entry);
    return SubmitExact(*m_entry);
}

DeviceOperationResult DeviceOperationCoordinator::RevokeDevice(const IdentityKeyId& target_device_id)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .error = m_load_error};
    const auto account = m_keystore.GetAccountId();
    const auto root_key = m_keystore.GetRecoveryPublicKey();
    if (!account || !root_key) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Recovery authority is unavailable"};
    }

    if (m_entry) {
        const auto reconciliation = Reconcile(*m_entry);
        if (reconciliation.phase == DeviceOperationPhase::FINALIZED ||
            reconciliation.phase == DeviceOperationPhase::REJECTED) {
            if (!ClearJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = reconciliation.op_id,
                .error = "Reconciled operation journal could not be cleared"};
        } else if (reconciliation.phase == DeviceOperationPhase::CONFLICT) {
            return reconciliation;
        } else {
            const bool same_request = m_entry->kind == JournalOperationKind::ROOT_DEVICE_REVOKE &&
                m_entry->account_id == *account && m_entry->payload_commitment == target_device_id;
            if (reconciliation.phase == DeviceOperationPhase::UNCERTAIN) {
                const auto retry = SubmitExact(*m_entry);
                if (!same_request && retry.phase != DeviceOperationPhase::REJECTED) {
                    return {.phase = DeviceOperationPhase::CONFLICT, .op_id = retry.op_id,
                        .error = "An earlier operation is unresolved; its exact bytes were retried"};
                }
                return retry;
            }
            if (same_request) return reconciliation;
            return {.phase = DeviceOperationPhase::CONFLICT, .op_id = m_entry->op_id,
                .error = "An earlier operation is still pending"};
        }
    }

    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    if (!record || record->recovery_root != *root_key) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Recovery root is not authorized by finalized identity state"};
    }
    if (!record->devices.contains(target_device_id)) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Target device is not active"};
    }
    if (record->next_root_nonce == std::numeric_limits<uint64_t>::max()) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Recovery nonce is exhausted"};
    }

    DeviceRevoke operation{.account_id = *account, .device_id = target_device_id,
        .root_nonce = record->next_root_nonce, .root_signature = {}};
    const auto digest = ComputeDeviceRevokeDigest(m_runtime.GetNetworkId(), operation);
    const auto signature = digest ? m_keystore.SignRecovery(*digest) : std::nullopt;
    if (!digest || !signature) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Cannot sign device revocation"};
    }
    operation.root_signature = *signature;
    const ProtocolOperation protocol_operation{operation};
    const auto bytes = SerializeProtocolOperation(protocol_operation);
    const auto op_id = ComputeOperationId(protocol_operation);
    if (!bytes || bytes->empty() || bytes->size() > MAX_JOURNALED_OPERATION_BYTES || !op_id || op_id->IsNull()) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Cannot encode device revocation"};
    }

    auto entry = std::make_unique<JournalEntry>();
    entry->network_id = m_runtime.GetNetworkId();
    entry->account_id = *account;
    entry->device_id = {};
    entry->nonce = operation.root_nonce;
    entry->activation_nonce = 0;
    entry->kind = JournalOperationKind::ROOT_DEVICE_REVOKE;
    entry->payload_commitment = target_device_id;
    entry->phase = DeviceOperationPhase::PREPARED;
    entry->op_id = *op_id;
    entry->operation_bytes = *bytes;
    if (!SaveJournal(*entry)) return {.phase = DeviceOperationPhase::REJECTED, .op_id = *op_id,
        .error = "Could not durably save device revocation before submission"};
    m_entry = std::move(entry);
    return SubmitExact(*m_entry);
}

DeviceOperationResult DeviceOperationCoordinator::Execute(DeviceOperationKind kind,
    const IdentityKeyId& payload_commitment, const DeviceOperationBuilder& build)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .error = m_load_error};
    const auto account = m_keystore.GetAccountId();
    const auto device_id = m_keystore.GetDeviceId();
    if (!account || !device_id) return {.phase = DeviceOperationPhase::REJECTED, .error = "No active device identity"};

    if (m_entry) {
        const auto reconciliation = Reconcile(*m_entry);
        if (reconciliation.phase == DeviceOperationPhase::FINALIZED) {
            if (!ClearJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = reconciliation.op_id,
                .error = "Finalized operation journal could not be cleared"};
        } else if (reconciliation.phase == DeviceOperationPhase::REJECTED) {
            if (!ClearJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = reconciliation.op_id,
                .error = "Rejected operation journal could not be cleared"};
        } else if (reconciliation.phase == DeviceOperationPhase::CONFLICT) {
            return reconciliation;
        } else {
            const bool same_request = m_entry->account_id == *account && m_entry->device_id == *device_id &&
                static_cast<uint8_t>(m_entry->kind) == static_cast<uint8_t>(kind) &&
                m_entry->payload_commitment == payload_commitment;
            if (reconciliation.phase == DeviceOperationPhase::UNCERTAIN) {
                const auto retry = SubmitExact(*m_entry);
                if (!same_request && retry.phase != DeviceOperationPhase::REJECTED) {
                    return {.phase = DeviceOperationPhase::CONFLICT, .op_id = retry.op_id,
                        .error = "An earlier operation is unresolved; its exact bytes were retried"};
                }
                return retry;
            }
            if (same_request) return reconciliation;
            return {.phase = DeviceOperationPhase::CONFLICT, .op_id = m_entry->op_id,
                .error = "An earlier device operation is still pending"};
        }
    }

    const auto loaded = m_runtime.GetStore().LoadState();
    if (!loaded || !loaded.state) return {.phase = DeviceOperationPhase::REJECTED, .error = "Finalized identity state is unavailable"};
    const auto* record = loaded.state->identities.Find(*account);
    if (!record) return {.phase = DeviceOperationPhase::REJECTED, .error = "Account is not finalized"};
    const auto device = record->devices.find(*device_id);
    const auto public_key = m_keystore.GetDevicePublicKey();
    if (device == record->devices.end() || !public_key || device->second.key != *public_key) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Device is not authorized in finalized state"};
    }

    DeviceAuthorization authorization{.account_id = *account, .device_id = *device_id,
        .nonce = device->second.next_nonce, .activation_nonce = device->second.activation_nonce,
        .kind = kind, .payload_commitment = payload_commitment};
    const auto digest = ComputeDeviceOperationDigest(m_runtime.GetNetworkId(), authorization);
    const auto signature = digest ? m_keystore.SignDevice(*digest) : std::nullopt;
    if (!signature) return {.phase = DeviceOperationPhase::REJECTED, .error = "Device authorization signing failed"};
    authorization.signature = *signature;
    const auto operation = build ? build(authorization) : std::nullopt;
    if (!operation) return {.phase = DeviceOperationPhase::REJECTED, .error = "Operation builder rejected authorization"};
    const auto embedded_authorization = AuthorizationOf(*operation);
    const auto bytes = SerializeProtocolOperation(*operation);
    const auto op_id = ComputeOperationId(*operation);
    if (!embedded_authorization || *embedded_authorization != authorization || !bytes || bytes->empty() ||
        bytes->size() > MAX_JOURNALED_OPERATION_BYTES || !op_id || op_id->IsNull()) {
        return {.phase = DeviceOperationPhase::REJECTED, .error = "Built operation does not match its authorization"};
    }

    auto entry = std::make_unique<JournalEntry>();
    entry->network_id = m_runtime.GetNetworkId();
    entry->account_id = *account;
    entry->device_id = *device_id;
    entry->nonce = authorization.nonce;
    entry->activation_nonce = authorization.activation_nonce;
    entry->kind = static_cast<JournalOperationKind>(kind);
    entry->payload_commitment = payload_commitment;
    entry->phase = DeviceOperationPhase::PREPARED;
    entry->op_id = *op_id;
    entry->operation_bytes = *bytes;
    if (!SaveJournal(*entry)) return {.phase = DeviceOperationPhase::REJECTED,
        .op_id = *op_id, .error = "Could not durably save exact operation before submission"};
    m_entry = std::move(entry);
    return SubmitExact(*m_entry);
}

DeviceOperationResult DeviceOperationCoordinator::GetStatus(const uint256& op_id)
{
    std::lock_guard lock(m_mutex);
    if (!LoadJournal()) return {.phase = DeviceOperationPhase::CONFLICT, .op_id = op_id, .error = m_load_error};
    if (m_entry && m_entry->op_id == op_id) return Reconcile(*m_entry);
    const auto status = m_runtime.GetOperationStatus(op_id);
    if (status.kind == OperationStatusKind::FINALIZED) {
        return {.phase = DeviceOperationPhase::FINALIZED, .op_id = op_id,
            .finalized_height = status.finalized_height};
    }
    if (status.kind == OperationStatusKind::REJECTED_KNOWN) {
        return {.phase = DeviceOperationPhase::REJECTED, .op_id = op_id, .error = "Operation was rejected"};
    }
    if (status.kind == OperationStatusKind::LOCAL_PENDING || status.kind == OperationStatusKind::ACCEPTED_REMOTE) {
        return {.phase = DeviceOperationPhase::ACCEPTED, .op_id = op_id};
    }
    return {.phase = DeviceOperationPhase::UNCERTAIN, .op_id = op_id,
        .error = "Operation status is unknown"};
}

} // namespace cybou
