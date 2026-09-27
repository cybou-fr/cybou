// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_DEVICE_OPERATION_COORDINATOR_H
#define CYBOU_DEVICE_OPERATION_COORDINATOR_H

#include <cybou/identity_registry.h>
#include <cybou/protocol_operation.h>

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace cybou {

class CybouKeyStore;
class CybouNodeRuntime;

enum class DeviceOperationPhase : uint8_t {
    PREPARED,
    SUBMITTING,
    UNCERTAIN,
    ACCEPTED,
    FINALIZED,
    REJECTED,
    CONFLICT,
};

struct DeviceOperationResult {
    DeviceOperationPhase phase{DeviceOperationPhase::REJECTED};
    uint256 op_id;
    uint64_t finalized_height{0};
    std::string error;

    explicit operator bool() const
    {
        return phase == DeviceOperationPhase::ACCEPTED ||
            phase == DeviceOperationPhase::UNCERTAIN ||
            phase == DeviceOperationPhase::FINALIZED;
    }
};

using DeviceOperationBuilder = std::function<std::optional<ProtocolOperation>(const DeviceAuthorization&)>;

/** Serializes device nonce use and durably journals exact bytes before submission. */
class DeviceOperationCoordinator {
public:
    DeviceOperationCoordinator(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
        std::filesystem::path journal_path);
    ~DeviceOperationCoordinator();

    DeviceOperationCoordinator(const DeviceOperationCoordinator&) = delete;
    DeviceOperationCoordinator& operator=(const DeviceOperationCoordinator&) = delete;

    DeviceOperationResult Execute(DeviceOperationKind kind, const IdentityKeyId& payload_commitment,
        const DeviceOperationBuilder& build);
    /** Signs, journals, submits, and reconciles root-authorized DeviceAdd for this recovered device. */
    DeviceOperationResult AuthorizeRecoveredDevice();
    /** Signs, journals, submits, and reconciles a root-authorized DeviceRevoke. */
    DeviceOperationResult RevokeDevice(const IdentityKeyId& target_device_id);
    /** Signs, journals, submits, and reconciles a hybrid RecoveryRotate. */
    DeviceOperationResult RotateRecoveryRoot(std::span<const unsigned char, 32> new_root_entropy);
    /** Clears a finalized rotation journal only after its candidate vault is promoted. */
    bool CompleteRecoveryRootRotation(const IdentityHybridPublicKey& active_root);
    bool HasPendingRecoveryRootRotation();
    DeviceOperationResult GetStatus(const uint256& op_id);

private:
    struct JournalEntry;
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    std::filesystem::path m_journal_path;
    mutable std::mutex m_mutex;
    std::unique_ptr<JournalEntry> m_entry;
    bool m_loaded{false};
    std::string m_load_error;

    bool LoadJournal();
    bool SaveJournal(const JournalEntry& entry);
    bool ClearJournal();
    DeviceOperationResult SubmitExact(JournalEntry& entry);
    DeviceOperationResult Reconcile(JournalEntry& entry);
};

} // namespace cybou

#endif // CYBOU_DEVICE_OPERATION_COORDINATOR_H
