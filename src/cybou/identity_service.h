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
    /// Сервис без активной длительной операции.
    IDLE = 0,
    /// Локальная подготовка или вывод ключевых ролей.
    CREATING_KEYS,
    /// Выполнение обязательной локальной работы перед отправкой.
    PERFORMING_WORK,
    /// Подписание и отправка кандидат-операции в сеть.
    BROADCASTING,
    /// Ожидание PoA-финализации в каноническом состоянии.
    WAITING_FOR_FINALITY,
    /// Identity подтверждена и согласована с финализированным состоянием.
    ACTIVE,
    /// Операция завершилась ошибкой или была отменена.
    FAILED,
};

/// Итог создания или восстановления Identity.
struct IdentityCreationResult {
    /// Успешно ли завершилась операция.
    bool success{false};
    /// Финальная локальная фаза сервиса.
    IdentityCreationPhase final_phase{IdentityCreationPhase::IDLE};
    /// AccountID созданной/восстановленной Identity, если он известен.
    AccountId account_id;
    /// Высота финализированного блока создания аккаунта.
    uint64_t creation_height{0};
    /// Финализированный System Balance после создания/восстановления.
    uint64_t system_balance{0};
    /// Диагностика ошибки; не должна содержать пароль или recovery words.
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
    /// \param runtime Текущий Full Node runtime.
    /// \param storage_path Путь переносимого vault или `std::nullopt`, если он будет задан позже.
    /// \thread_safety Методы сервиса синхронизируют доступ к локальному keystore; колбэки вызываются без внешней сериализации.
    explicit CybouIdentityService(
        CybouNodeRuntime& runtime,
        std::optional<std::filesystem::path> storage_path = std::nullopt);
    ~CybouIdentityService();

    CybouIdentityService(const CybouIdentityService&) = delete;
    CybouIdentityService& operator=(const CybouIdentityService&) = delete;

    /// Настраивает путь постоянного хранения переносимого vault.
    /// \post Если путь изменён, флаг `m_vault_saved` сбрасывается.
    void SetStoragePath(std::filesystem::path path);
    /// Возвращает текущий путь хранения vault, если он настроен.
    std::optional<std::filesystem::path> GetStoragePath() const;

    /// Возвращает текущую фазу жизненного цикла Identity.
    IdentityCreationPhase GetPhase() const { return m_phase.load(); }

    /// Возвращает AccountID разблокированной или уже созданной Identity.
    /// \return AccountID либо `std::nullopt`, если keystore заблокирован.
    std::optional<AccountId> GetAccountId() const;
    /// Возвращает каноническое финализированное состояние текущего аккаунта.
    /// \return Финализированное `AccountState` либо `std::nullopt`, если Identity не разблокирована или аккаунт ещё не найден.
    std::optional<AccountState> GetFinalizedAccountState() const;
    /// Возвращает финализированное primary name текущей Identity.
    /// \return Primary name либо `std::nullopt`, если Identity не разблокирована или имя ещё не финализировано.
    std::optional<std::string> GetFinalizedPrimaryName() const;

    /// Готовит новую локальную Identity и возвращает её 24 слова для подтверждения.
    /// \return Recovery words или `std::nullopt`, если vault уже существует/сохранён либо генерация не удалась.
    /// \pre `storage_path` настроен и ещё не занят существующим vault.
    /// \post Слова являются секретом: не логировать, показать пользователю один раз и затем очистить во внешнем коде.
    std::optional<RecoveryWords> PrepareNewIdentity();
    /// Отбрасывает ещё не сохранённую подготовленную Identity.
    /// \post При подходящей фазе стирает локальные секреты keystore.
    void DiscardPreparedIdentity();
    /// Разблокирует существующий переносимый vault.
    /// \param password Пароль vault; не логировать.
    /// \return `true`, если vault открыт и локальная Identity загружена.
    /// \post При успехе сервис переводится в `ACTIVE` только если keystore совпадает с финализированным состоянием.
    bool LoadVault(std::string_view password);
    /// Останавливает текущую работу и стирает весь разблокированный секретный материал.
    /// \post Рабочий поток остановлен, keystore очищен, фаза сброшена в `IDLE`.
    void Lock();
    /// Возвращает true, если vault сейчас разблокирован.
    bool IsUnlocked() const;
    /// Проверяет, выводит ли текущая recovery phrase genesis-authorized PoA ключ сети.
    /// \return `true`, если локальная Identity совпадает с PoA key из genesis.
    bool IsNetworkAuthority() const;

    /// Возвращает доступ к подлежащему keystore.
    CybouKeyStore& GetKeyStore() { return m_keystore; }
    /// Возвращает константный доступ к подлежащему keystore.
    const CybouKeyStore& GetKeyStore() const { return m_keystore; }
    /// Возвращает связанный NodeRuntime.
    CybouNodeRuntime& GetNodeRuntime() { return m_runtime; }

    /// Синхронно создаёт новую Identity и ждёт результат.
    /// \param password Пароль для нового vault; не логировать.
    /// \param on_phase Опциональный callback прогресса.
    /// \param timeout Максимальное ожидание PoA-финализации после отправки.
    /// \return Итог создания или причина ошибки.
    /// \pre `PrepareNewIdentity()` уже вызван и `storage_path` настроен.
    /// \post Пароль очищается внутри реализации; сервис не публикует секреты в ошибках.
    IdentityCreationResult CreateIdentitySync(
        std::string password,
        const PhaseCallback& on_phase = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /// Асинхронно создаёт новую Identity.
    /// \post Предыдущая асинхронная работа отменяется перед запуском новой.
    void CreateIdentityAsync(
        std::string password,
        PhaseCallback on_phase,
        CompletionCallback on_complete,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /// Синхронно восстанавливает Identity по 24 словам.
    /// \param words Recovery words; секрет не логировать.
    /// \param password Пароль нового/переоткрываемого vault.
    /// \return Итог восстановления или причина ошибки.
    /// \post Пароль очищается внутри реализации; разблокированный keystore остаётся локально только при успехе.
    IdentityCreationResult RestoreIdentitySync(
        const RecoveryWords& words,
        std::string password,
        const PhaseCallback& on_phase = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));
    /// Синхронно подготавливает и отправляет ротацию Identity.
    /// \param new_words Новая recovery phrase; секрет не логировать.
    /// \param password Пароль candidate vault.
    /// \return Результат локальной координации IdentityRotate.
    /// \post Новый vault не становится активным до подтверждения финализации.
    IdentityOperationResult RotateIdentitySync(const RecoveryWords& new_words, std::string password);
    /// Возвращает true, если ротация Identity уже подготовлена и ещё не завершена.
    bool HasPendingIdentityRotation();
    /// Возобновляет локальное завершение ротации из зашифрованного candidate vault.
    /// \param password Пароль candidate vault; не логировать.
    /// \return Результат локального согласования с финализированным состоянием.
    IdentityOperationResult ResumeIdentityRotationSync(std::string password);
    /// Асинхронно восстанавливает Identity по 24 словам.
    /// \post Предыдущая асинхронная работа отменяется перед запуском новой.
    void RestoreIdentityAsync(
        RecoveryWords words,
        std::string password,
        PhaseCallback on_phase,
        CompletionCallback on_complete,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /// Отменяет текущую асинхронную работу сервиса Identity.
    /// \post Уже идущий шаг увидит флаг отмены при ближайшей проверке.
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
