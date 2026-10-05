// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Высокоуровневый сервис балансов, переводов и локального журнала кошелька.

#ifndef CYBOU_WALLET_SERVICE_H
#define CYBOU_WALLET_SERVICE_H

#include <cybou/account_id.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/hash256.h>

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace cybou {

/// Вид записи локального журнала кошелька.
enum class WalletEntryKind : uint8_t {
    /// Начисление onboarding System Balance при создании аккаунта.
    ONBOARDING_BONUS = 0,
    /// Обычный платёж между аккаунтами.
    PAYMENT = 1,
    /// Необратимый перевод Balance -> System Balance.
    LOCK_TO_SYSTEM = 2,
    /// Локально распознанное списание комиссии RootPublication.
    ROOT_PUBLICATION_FEE = 3,
};

/// Финальность записи локального журнала кошелька.
enum class WalletEntryFinality : uint8_t {
    /// Локально известная операция ещё не финализирована PoA.
    PENDING = 0,
    /// Запись подтверждена финализированным состоянием/блоком.
    FINAL = 1,
};

/// Одна строка локально синхронизированного журнала кошелька.
struct WalletLedgerEntry {
    /// Стабильный идентификатор записи локального журнала.
    cybou::Hash256 entry_id;
    /// Тип локально отображаемого движения средств.
    WalletEntryKind kind{WalletEntryKind::ONBOARDING_BONUS};
    /// Положительное значение = кредит, отрицательное = дебет.
    int64_t amount{0};
    /// `true` = System Balance, `false` = spendable Balance.
    bool system_side{true};
    /// Peer AccountID для платежа; нулевой для внутренних/системных записей.
    AccountId counterparty;
    /// Локальная отметка времени журнала в Unix seconds.
    uint64_t timestamp{0};
    /// Высота финализированного блока, связанного с записью, либо локальная наблюдаемая высота для pending.
    uint64_t height{0};
    /// Финальность записи в локальном представлении.
    WalletEntryFinality finality{WalletEntryFinality::FINAL};

    friend bool operator==(const WalletLedgerEntry&, const WalletLedgerEntry&) = default;
};

/// Причина отказа пользовательской операции кошелька.
enum class WalletOperationError : uint8_t {
    /// Ошибки нет.
    NONE = 0,
    /// В keystore нет активной Identity.
    NO_IDENTITY,
    /// Отправитель отсутствует в финализированном состоянии.
    ACCOUNT_NOT_FOUND,
    /// AccountID получателя нулевой или иначе недопустим.
    INVALID_RECIPIENT,
    /// Запрещён перевод самому себе.
    SELF_PAYMENT,
    /// Сумма операции равна нулю.
    ZERO_AMOUNT,
    /// Недостаточно spendable Balance.
    INSUFFICIENT_BALANCE,
    /// Недостаточно System Balance на комиссию.
    INSUFFICIENT_SYSTEM_BALANCE,
    /// Не удалось вычислить криптографический коммитмент/подпись.
    CRYPTO_FAILURE,
    /// Координатор операции не смог надёжно отправить exact bytes.
    SUBMIT_FAILED,
};

/// Результат отправки пользовательской операции кошелька.
struct WalletOperationResult {
    /// Категория ошибки или `NONE`.
    WalletOperationError error{WalletOperationError::NONE};
    /// OperationID пользовательской кандидат-операции.
    cybou::Hash256 op_id{};
    /// Диагностическое сообщение без секретов.
    std::string error_message{};
    /// Последняя известная фаза exact bytes операции.
    IdentityOperationPhase operation_phase{IdentityOperationPhase::PREPARED};

    explicit operator bool() const { return error == WalletOperationError::NONE; }
};

/// Управляет балансами, пользовательскими операциями и локальным журналом кошелька.
class CybouWalletService {
public:
    /// Создаёт сервис кошелька для текущей Identity и runtime.
    /// \thread_safety Отдельные чтения журнала/балансов потокобезопасны; операции отправки сериализуются внутренними mutex.
    explicit CybouWalletService(CybouNodeRuntime& runtime, CybouKeyStore& keystore);
    ~CybouWalletService();

    CybouWalletService(const CybouWalletService&) = delete;
    CybouWalletService& operator=(const CybouWalletService&) = delete;

    /// Отправляет платёж из Balance получателю.
    /// \return Результат локальной проверки и координации exact bytes.
    /// \pre В keystore разблокирована Identity.
    /// \post При успехе в локальном журнале может появиться pending-запись до финализации.
    WalletOperationResult SendPayment(const AccountId& recipient, uint64_t amount);

    /// Необратимо переводит сумму из Balance в System Balance.
    /// \return Результат локальной проверки и координации exact bytes.
    WalletOperationResult LockToSystemBalance(uint64_t amount);

    /// Выполняет отправку платежа на рабочем потоке сервиса.
    /// \post `completion` вызывается с итогом без передачи секретов наружу.
    void SendPaymentAsync(const AccountId& recipient, uint64_t amount,
        std::function<void(WalletOperationResult)> completion);
    /// Выполняет системный лок на рабочем потоке сервиса.
    /// \post `completion` вызывается с итогом без передачи секретов наружу.
    void LockToSystemBalanceAsync(uint64_t amount,
        std::function<void(WalletOperationResult)> completion);

    /// Синхронизирует локальный журнал по новым финализированным блокам.
    /// \return Число новых или обновлённых локальных записей.
    size_t SyncLedger();

    /// Возвращает все записи локального журнала, отсортированные от новых к старым.
    /// \return Копия локального журнала.
    std::vector<WalletLedgerEntry> GetLedgerEntries() const;

    /// Возвращает текущие канонические балансы (Balance, System Balance).
    /// \return Пара `(balance, system_balance)`; `(0, 0)` при отсутствии активной Identity или аккаунта.
    std::pair<uint64_t, uint64_t> GetBalances() const;

private:
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    IdentityOperationCoordinator& m_operation_coordinator;
    uint64_t m_last_scanned_height{0};
    std::vector<WalletLedgerEntry> m_entries;
    mutable std::mutex m_mutex;
    // Сериализует выбор nonce и отправку, не блокируя чтения журнала на время сетевого ожидания runtime.
    std::mutex m_operation_mutex;
    std::mutex m_sync_mutex;
    std::mutex m_worker_mutex;
    std::condition_variable m_worker_cv;
    std::queue<std::function<void()>> m_worker_tasks;
    bool m_worker_stopping{false};
    std::jthread m_worker;

    void Enqueue(std::function<void()> task);
};

} // namespace cybou

#endif // CYBOU_WALLET_SERVICE_H
