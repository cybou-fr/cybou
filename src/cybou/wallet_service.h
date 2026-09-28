// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_WALLET_SERVICE_H
#define CYBOU_WALLET_SERVICE_H

#include <cybou/account_id.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <uint256.h>

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

enum class WalletEntryKind : uint8_t {
    ONBOARDING_BONUS = 0,
    MAIL_FEE = 1,
    PAYMENT = 2,
    LOCK_TO_SYSTEM = 3,
};

enum class WalletEntryFinality : uint8_t {
    PENDING = 0,
    FINAL = 1,
};

struct WalletLedgerEntry {
    uint256 entry_id;
    WalletEntryKind kind{WalletEntryKind::ONBOARDING_BONUS};
    int64_t amount{0};        // Positive for credit, negative for debit
    bool system_side{true};    // true: System Balance; false: Balance
    AccountId counterparty;   // Peer account ID if payment or mail, empty otherwise
    uint64_t timestamp{0};
    uint64_t height{0};
    WalletEntryFinality finality{WalletEntryFinality::FINAL};

    friend bool operator==(const WalletLedgerEntry&, const WalletLedgerEntry&) = default;
};

enum class WalletOperationError : uint8_t {
    NONE = 0,
    NO_IDENTITY,
    ACCOUNT_NOT_FOUND,
    INVALID_RECIPIENT,
    SELF_PAYMENT,
    ZERO_AMOUNT,
    INSUFFICIENT_BALANCE,
    INSUFFICIENT_SYSTEM_BALANCE,
    CRYPTO_FAILURE,
    SUBMIT_FAILED,
};

struct WalletOperationResult {
    WalletOperationError error{WalletOperationError::NONE};
    uint256 op_id{};
    std::string error_message{};
    IdentityOperationPhase operation_phase{IdentityOperationPhase::PREPARED};

    explicit operator bool() const { return error == WalletOperationError::NONE; }
};

/**
 * CybouWalletService manages balance queries, payment transactions (PaymentPayload),
 * irreversible system locks (SystemLockPayload), and on-chain activity ledger sync.
 */
class CybouWalletService {
public:
    explicit CybouWalletService(CybouNodeRuntime& runtime, CybouKeyStore& keystore);
    ~CybouWalletService();

    CybouWalletService(const CybouWalletService&) = delete;
    CybouWalletService& operator=(const CybouWalletService&) = delete;

    /** Send a payment from Balance to recipient AccountId */
    WalletOperationResult SendPayment(const AccountId& recipient, uint64_t amount);

    /** Lock an amount from Balance to System Balance (one-way, irreversible) */
    WalletOperationResult LockToSystemBalance(uint64_t amount);

    /** Submit wallet operations off the caller's thread. Completion runs on the worker thread. */
    void SendPaymentAsync(const AccountId& recipient, uint64_t amount,
        std::function<void(WalletOperationResult)> completion);
    void LockToSystemBalanceAsync(uint64_t amount,
        std::function<void(WalletOperationResult)> completion);

    /** Sync ledger entries against newly finalized BFT blocks */
    size_t SyncLedger();

    /** Get all ledger entries (most recent first) */
    std::vector<WalletLedgerEntry> GetLedgerEntries() const;

    /** Query current balances (balance, system_balance) directly from node runtime state */
    std::pair<uint64_t, uint64_t> GetBalances() const;

private:
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    IdentityOperationCoordinator& m_operation_coordinator;
    uint64_t m_last_scanned_height{0};
    std::vector<WalletLedgerEntry> m_entries;
    mutable std::mutex m_mutex;
    // Serialize nonce selection and operation submission without blocking
    // ledger readers while the runtime waits on the network.
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
