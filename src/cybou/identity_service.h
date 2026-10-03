// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// Высокоуровневый сервис подготовки, создания, восстановления и ротации Identity.

#ifndef CYBOU_IDENTITY_SERVICE_H
#define CYBOU_IDENTITY_SERVICE_H

#include <cybou/account_creation.h>
#include <cybou/account_id.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace cybou {

/// Фаза локального жизненного цикла создания или восстановления Identity.
enum class IdentityCreationPhase : uint8_t {
    IDLE = 0,
    CREATING_KEYS,
    PERFORMING_WORK,
    BROADCASTING,
    WAITING_FOR_FINALITY,
    ACTIVE,
    FAILED,
};

/// Итог создания или восстановления Identity.
struct IdentityCreationResult {
    bool success{false};
    IdentityCreationPhase final_phase{IdentityCreationPhase::IDLE};
    AccountId account_id;
    uint64_t creation_height{0};
    uint64_t system_balance{0};
    std::string error_message;
};

/// Колбэк уведомления о смене фазы.
using PhaseCallback = std::function<void(IdentityCreationPhase phase, const std::string& detail)>;
/// Колбэк завершения операции сервиса Identity.
using CompletionCallback = std::function<void(const IdentityCreationResult& result)>;

/// Координирует локальные операции жизненного цикла одной Identity.
class CybouIdentityService {
public:
    /// Создаёт сервис Identity для заданного runtime и опционального пути vault.
    explicit CybouIdentityService(
        CybouNodeRuntime& runtime,
        std::optional<std::filesystem::path> storage_path = std::nullopt);
    ~CybouIdentityService();

    CybouIdentityService(const CybouIdentityService&) = delete;
    CybouIdentityService& operator=(const CybouIdentityService&) = delete;

    /// Настраивает путь постоянного хранения переносимого vault.
    void SetStoragePath(std::filesystem::path path);
    /// Возвращает текущий путь хранения vault, если он настроен.
    std::optional<std::filesystem::path> GetStoragePath() const;

    /// Возвращает текущую фазу жизненного цикла Identity.
    IdentityCreationPhase GetPhase() const { return m_phase.load(); }

    /// Возвращает AccountID разблокированной или уже созданной Identity.
    std::optional<AccountId> GetAccountId() const;
    /// Возвращает каноническое финализированное состояние текущего аккаунта.
    std::optional<AccountState> GetFinalizedAccountState() const;
    /// Возвращает финализированное primary name текущей Identity.
    std::optional<std::string> GetFinalizedPrimaryName() const;

    /// Готовит новую локальную Identity и возвращает её 24 слова для подтверждения.
    std::optional<RecoveryWords> PrepareNewIdentity();
    /// Отбрасывает ещё не сохранённую подготовленную Identity.
    void DiscardPreparedIdentity();
    /// Разблокирует существующий переносимый vault.
    bool LoadVault(std::string_view password);
    /// Останавливает текущую работу и стирает весь разблокированный секретный материал.
    void Lock();
    /// Возвращает true, если vault сейчас разблокирован.
    bool IsUnlocked() const;
    /// Проверяет, выводит ли текущая recovery phrase genesis-authorized PoA ключ сети.
    bool IsNetworkAuthority() const;

    /// Возвращает доступ к подлежащему keystore.
    CybouKeyStore& GetKeyStore() { return m_keystore; }
    /// Возвращает константный доступ к подлежащему keystore.
    const CybouKeyStore& GetKeyStore() const { return m_keystore; }
    /// Возвращает связанный NodeRuntime.
    CybouNodeRuntime& GetNodeRuntime() { return m_runtime; }

    /// Синхронно создаёт новую Identity и ждёт результат.
    IdentityCreationResult CreateIdentitySync(
        std::string password,
        const PhaseCallback& on_phase = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /// Асинхронно создаёт новую Identity.
    void CreateIdentityAsync(
        std::string password,
        PhaseCallback on_phase,
        CompletionCallback on_complete,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /// Синхронно восстанавливает Identity по 24 словам.
    IdentityCreationResult RestoreIdentitySync(
        const RecoveryWords& words,
        std::string password,
        const PhaseCallback& on_phase = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));
    /// Синхронно подготавливает и отправляет ротацию Identity.
    IdentityOperationResult RotateIdentitySync(const RecoveryWords& new_words, std::string password);
    /// Возвращает true, если ротация Identity уже подготовлена и ещё не завершена.
    bool HasPendingIdentityRotation();
    /// Возобновляет локальное завершение ротации из зашифрованного candidate vault.
    IdentityOperationResult ResumeIdentityRotationSync(std::string password);
    /// Асинхронно восстанавливает Identity по 24 словам.
    void RestoreIdentityAsync(
        RecoveryWords words,
        std::string password,
        PhaseCallback on_phase,
        CompletionCallback on_complete,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /// Отменяет текущую асинхронную работу сервиса Identity.
    void Cancel();

private:
    CybouNodeRuntime& m_runtime;
    std::atomic<IdentityCreationPhase> m_phase{IdentityCreationPhase::IDLE};
    std::atomic<bool> m_cancelled{false};
    std::optional<std::filesystem::path> m_storage_path;
    CybouKeyStore m_keystore;
    bool m_vault_saved{false};
    mutable std::mutex m_mutex;
    std::jthread m_worker;
};

} // namespace cybou

#endif // CYBOU_IDENTITY_SERVICE_H
