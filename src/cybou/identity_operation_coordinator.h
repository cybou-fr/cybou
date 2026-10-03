// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Координатор точной сериализации, журналирования и повторной отправки операций Identity.

#ifndef CYBOU_IDENTITY_OPERATION_COORDINATOR_H
#define CYBOU_IDENTITY_OPERATION_COORDINATOR_H

#include <cybou/identity_registry.h>
#include <cybou/protocol_operation.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace cybou {

class CybouKeyStore;
class CybouNodeRuntime;

/// Текущее состояние локально координируемой операции Identity.
enum class IdentityOperationPhase : uint8_t {
    PREPARED, SUBMITTING, UNCERTAIN, ACCEPTED, FINALIZED, REJECTED, CONFLICT,
};

/// Результат локального выполнения или согласования одной операции Identity.
struct IdentityOperationResult {
    IdentityOperationPhase phase{IdentityOperationPhase::REJECTED};
    cybou::Hash256 op_id;
    uint64_t finalized_height{0};
    std::string error;
    explicit operator bool() const
    {
        return phase == IdentityOperationPhase::ACCEPTED || phase == IdentityOperationPhase::UNCERTAIN ||
            phase == IdentityOperationPhase::FINALIZED;
    }
};

/// Фабрика канонической операции для уже подготовленной авторизации.
using IdentityOperationBuilder = std::function<std::optional<ProtocolOperation>(const IdentityOperationAuthorization&)>;

/// Сериализует операции одной Identity по nonce и журналирует точные байты.
class IdentityOperationCoordinator {
public:
    /// Создаёт координатор с журналом и интервалом повторной ретрансляции.
    IdentityOperationCoordinator(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
        std::filesystem::path journal_path,
        std::chrono::milliseconds relay_retry_interval = std::chrono::seconds{30});
    ~IdentityOperationCoordinator();
    IdentityOperationCoordinator(const IdentityOperationCoordinator&) = delete;
    IdentityOperationCoordinator& operator=(const IdentityOperationCoordinator&) = delete;

    /// Строит, подписывает, журналирует и отправляет обычную операцию Identity.
    IdentityOperationResult Execute(IdentityOperationKind kind, const IdentityKeyId& payload_commitment,
        const IdentityOperationBuilder& build);
    /// Выполняет атомарную ротацию всех публичных возможностей Identity.
    IdentityOperationResult RotateIdentity(std::span<const unsigned char, 32> new_identity_entropy);
    /// Повторяет отправку неопределённой операции, если наступил её срок.
    bool RetryRelayIfDue();
    /// Завершает локальную фазу ротации после её финализации в каноническом состоянии.
    bool CompleteIdentityRotation(const IdentityRecord& finalized_identity);
    /// Возвращает true, если локально есть незавершённая ротация Identity.
    bool HasPendingIdentityRotation();
    /// Запрашивает актуальный статус операции по её идентификатору.
    IdentityOperationResult GetStatus(const cybou::Hash256& op_id);

private:
    struct JournalEntry;
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    std::filesystem::path m_journal_path;
    const std::chrono::milliseconds m_relay_retry_interval;
    mutable std::mutex m_mutex;
    std::optional<std::chrono::steady_clock::time_point> m_next_relay_retry;
    std::unique_ptr<JournalEntry> m_entry;
    bool m_loaded{false};
    std::string m_load_error;

    bool LoadJournal();
    bool SaveJournal(const JournalEntry& entry);
    bool ClearJournal();
    IdentityOperationResult SubmitExact(JournalEntry& entry);
    IdentityOperationResult Reconcile(JournalEntry& entry);
};

} // namespace cybou

#endif // CYBOU_IDENTITY_OPERATION_COORDINATOR_H
