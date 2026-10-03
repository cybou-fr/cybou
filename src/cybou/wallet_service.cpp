// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// Реализация сервиса кошелька и локальной синхронизации журнала операций.

#include <cybou/wallet_service.h>
#include <cybou/identity_operation_coordinator.h>

#include <algorithm>
#include <chrono>

namespace cybou {
namespace {
std::uint64_t NowUnixSeconds()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
} // namespace

CybouWalletService::CybouWalletService(CybouNodeRuntime& runtime, CybouKeyStore& keystore)
    : m_runtime(runtime),
      m_keystore(keystore),
      m_operation_coordinator(runtime.GetIdentityOperationCoordinator(keystore)),
      m_worker([this](std::stop_token) {
          for (;;) {
              std::function<void()> task;
              {
                  std::unique_lock lock(m_worker_mutex);
                  m_worker_cv.wait(lock, [this] { return m_worker_stopping || !m_worker_tasks.empty(); });
                  if (m_worker_stopping && m_worker_tasks.empty()) return;
                  task = std::move(m_worker_tasks.front());
                  m_worker_tasks.pop();
              }
              try {
                  task();
              } catch (...) {
                  // Each operation reports its own result; keep the worker alive
                  // if a completion callback unexpectedly throws.
              }
          }
      })
{
}

CybouWalletService::~CybouWalletService()
{
    {
        std::lock_guard lock(m_worker_mutex);
        m_worker_stopping = true;
    }
    m_worker_cv.notify_all();
}

void CybouWalletService::Enqueue(std::function<void()> task)
{
    {
        std::lock_guard lock(m_worker_mutex);
        if (m_worker_stopping) return;
        m_worker_tasks.push(std::move(task));
    }
    m_worker_cv.notify_one();
}

std::pair<uint64_t, uint64_t> CybouWalletService::GetBalances() const
{
    const auto my_account = m_keystore.GetAccountId();
    if (!my_account) {
        return {0, 0};
    }
    const auto state = m_runtime.GetAccountState(*my_account);
    if (!state) {
        return {0, 0};
    }
    return {state->balance, state->system_balance};
}

WalletOperationResult CybouWalletService::SendPayment(const AccountId& recipient, const uint64_t amount)
{
    std::lock_guard operation_lock(m_operation_mutex);

    const auto my_account = m_keystore.GetAccountId();
    if (!my_account) {
        return {.error = WalletOperationError::NO_IDENTITY, .error_message = "No active identity in keystore"};
    }

    if (recipient.IsNull()) {
        return {.error = WalletOperationError::INVALID_RECIPIENT, .error_message = "Recipient Account ID is null"};
    }

    if (*my_account == recipient) {
        return {.error = WalletOperationError::SELF_PAYMENT, .error_message = "Cannot send payment to self"};
    }

    if (amount == 0) {
        return {.error = WalletOperationError::ZERO_AMOUNT, .error_message = "Payment amount must be greater than zero"};
    }

    const auto sender_state = m_runtime.GetAccountState(*my_account);
    if (!sender_state) {
        return {.error = WalletOperationError::ACCOUNT_NOT_FOUND, .error_message = "Sender account not found on-chain"};
    }

    if (sender_state->balance < amount) {
        return {.error = WalletOperationError::INSUFFICIENT_BALANCE, .error_message = "Insufficient balance"};
    }

    const auto& params = m_runtime.GetNetworkGenesis().GetProtocolParameters();
    if (sender_state->system_balance < params.payment_fee) {
        return {.error = WalletOperationError::INSUFFICIENT_SYSTEM_BALANCE, .error_message = "Insufficient system balance for fee"};
    }

    PaymentPayload payment_payload{.recipient = recipient, .amount = amount};
    const auto commitment = ComputePaymentPayloadCommitment(payment_payload);
    if (!commitment) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to commit payment payload"};
    }

    const auto submitted = m_operation_coordinator.Execute(IdentityOperationKind::PAYMENT, *commitment,
        [&](const IdentityOperationAuthorization& authorization) -> std::optional<ProtocolOperation> {
            return ProtocolOperation{AuthorizedPayment{.authorization = authorization, .payment = payment_payload}};
        });
    if (!submitted) return {.error = WalletOperationError::SUBMIT_FAILED, .op_id = submitted.op_id,
        .error_message = submitted.error, .operation_phase = submitted.phase};
    const cybou::Hash256 op_id = submitted.op_id;
    WalletLedgerEntry pending_entry{
        .entry_id = op_id,
        .kind = WalletEntryKind::PAYMENT,
        .amount = -static_cast<int64_t>(amount),
        .system_side = false,
        .counterparty = recipient,
        .timestamp = NowUnixSeconds(),
        .height = m_runtime.GetFinalizedHeight().value_or(0),
        .finality = WalletEntryFinality::PENDING,
    };
    {
        std::lock_guard lock(m_mutex);
        const auto existing = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) {
            return entry.entry_id == pending_entry.entry_id;
        });
        if (existing == m_entries.end()) m_entries.insert(m_entries.begin(), pending_entry);
    }

    return {.error = WalletOperationError::NONE, .op_id = op_id, .operation_phase = submitted.phase};
}

WalletOperationResult CybouWalletService::LockToSystemBalance(const uint64_t amount)
{
    std::lock_guard operation_lock(m_operation_mutex);

    const auto my_account = m_keystore.GetAccountId();
    if (!my_account) {
        return {.error = WalletOperationError::NO_IDENTITY, .error_message = "No active identity in keystore"};
    }

    if (amount == 0) {
        return {.error = WalletOperationError::ZERO_AMOUNT, .error_message = "Lock amount must be greater than zero"};
    }

    const auto sender_state = m_runtime.GetAccountState(*my_account);
    if (!sender_state) {
        return {.error = WalletOperationError::ACCOUNT_NOT_FOUND, .error_message = "Account not found on-chain"};
    }

    if (sender_state->balance < amount) {
        return {.error = WalletOperationError::INSUFFICIENT_BALANCE, .error_message = "Insufficient balance"};
    }

    SystemLockPayload lock_payload{.amount = amount};
    const auto commitment = ComputeSystemLockPayloadCommitment(lock_payload);
    if (!commitment) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to commit system lock payload"};
    }

    const auto submitted = m_operation_coordinator.Execute(IdentityOperationKind::SYSTEM_LOCK, *commitment,
        [&](const IdentityOperationAuthorization& authorization) -> std::optional<ProtocolOperation> {
            return ProtocolOperation{AuthorizedSystemLock{.authorization = authorization, .lock = lock_payload}};
        });
    if (!submitted) return {.error = WalletOperationError::SUBMIT_FAILED, .op_id = submitted.op_id,
        .error_message = submitted.error, .operation_phase = submitted.phase};
    const cybou::Hash256 op_id = submitted.op_id;
    WalletLedgerEntry pending_entry{
        .entry_id = op_id,
        .kind = WalletEntryKind::LOCK_TO_SYSTEM,
        .amount = static_cast<int64_t>(amount),
        .system_side = true,
        .counterparty = AccountId{},
        .timestamp = NowUnixSeconds(),
        .height = m_runtime.GetFinalizedHeight().value_or(0),
        .finality = WalletEntryFinality::PENDING,
    };
    {
        std::lock_guard lock(m_mutex);
        const auto existing = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) {
            return entry.entry_id == pending_entry.entry_id;
        });
        if (existing == m_entries.end()) m_entries.insert(m_entries.begin(), pending_entry);
    }

    return {.error = WalletOperationError::NONE, .op_id = op_id, .operation_phase = submitted.phase};
}

void CybouWalletService::SendPaymentAsync(const AccountId& recipient, const uint64_t amount,
    std::function<void(WalletOperationResult)> completion)
{
    Enqueue([this, recipient, amount, completion = std::move(completion)]() mutable {
        WalletOperationResult result;
        try {
            result = SendPayment(recipient, amount);
        } catch (const std::exception& e) {
            result = {.error = WalletOperationError::SUBMIT_FAILED, .error_message = e.what()};
        } catch (...) {
            result = {.error = WalletOperationError::SUBMIT_FAILED, .error_message = "Unexpected wallet operation failure"};
        }
        if (completion) {
            try { completion(std::move(result)); } catch (...) { }
        }
    });
}

void CybouWalletService::LockToSystemBalanceAsync(const uint64_t amount,
    std::function<void(WalletOperationResult)> completion)
{
    Enqueue([this, amount, completion = std::move(completion)]() mutable {
        WalletOperationResult result;
        try {
            result = LockToSystemBalance(amount);
        } catch (const std::exception& e) {
            result = {.error = WalletOperationError::SUBMIT_FAILED, .error_message = e.what()};
        } catch (...) {
            result = {.error = WalletOperationError::SUBMIT_FAILED, .error_message = "Unexpected wallet operation failure"};
        }
        if (completion) {
            try { completion(std::move(result)); } catch (...) { }
        }
    });
}

size_t CybouWalletService::SyncLedger()
{
    std::lock_guard sync_lock(m_sync_mutex);

    std::vector<WalletLedgerEntry> original_entries;
    uint64_t original_height{0};
    {
        std::lock_guard lock(m_mutex);
        original_entries = m_entries;
        original_height = m_last_scanned_height;
    }

    const auto my_account = m_keystore.GetAccountId();
    if (!my_account) {
        return 0;
    }

    const auto tip_height = m_runtime.GetFinalizedHeight();
    if (!tip_height) {
        return 0;
    }

    auto working_entries = original_entries;
    if (*tip_height > original_height) {
        working_entries.reserve(original_entries.size() + static_cast<std::size_t>(*tip_height - original_height));
    }
    const auto& params = m_runtime.GetNetworkGenesis().GetProtocolParameters();

    uint64_t scanned_height = original_height;
    for (uint64_t h = original_height + 1; h <= *tip_height; ++h) {
        const auto block_opt = m_runtime.GetBlockAtHeight(h);
        if (!block_opt) break;
        const auto& fin_block = *block_opt;

        for (const auto& proto_op : fin_block.block.operations) {
            const auto op_id_opt = ComputeOperationId(proto_op);
            const cybou::Hash256 op_id = op_id_opt.value_or(cybou::Hash256{});

            std::visit([&](const auto& op) {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, AccountCreateOp>) {
                    if (op.account_id == *my_account) {
                        const auto it = std::find_if(working_entries.begin(), working_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == working_entries.end()) {
                            WalletLedgerEntry entry{
                                .entry_id = op_id,
                                .kind = WalletEntryKind::ONBOARDING_BONUS,
                                .amount = static_cast<int64_t>(params.onboarding_bonus),
                                .system_side = true,
                                .counterparty = AccountId{},
                                .timestamp = 0,
                                .height = h,
                                .finality = WalletEntryFinality::FINAL,
                            };
                            working_entries.push_back(entry);
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedPayment>) {
                    const auto& auth = op.authorization;
                    const auto& payload = op.payment;
                    if (auth.account_id == *my_account) {
                        auto it = std::find_if(working_entries.begin(), working_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it != working_entries.end()) {
                            it->finality = WalletEntryFinality::FINAL;
                            it->height = h;
                        } else {
                            WalletLedgerEntry entry{
                                .entry_id = op_id,
                                .kind = WalletEntryKind::PAYMENT,
                                .amount = -static_cast<int64_t>(payload.amount),
                                .system_side = false,
                                .counterparty = payload.recipient,
                                .timestamp = 0,
                                .height = h,
                                .finality = WalletEntryFinality::FINAL,
                            };
                            working_entries.push_back(entry);
                        }
                    } else if (payload.recipient == *my_account) {
                        const auto it = std::find_if(working_entries.begin(), working_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == working_entries.end()) {
                            WalletLedgerEntry entry{
                                .entry_id = op_id,
                                .kind = WalletEntryKind::PAYMENT,
                                .amount = static_cast<int64_t>(payload.amount),
                                .system_side = false,
                                .counterparty = auth.account_id,
                                .timestamp = 0,
                                .height = h,
                                .finality = WalletEntryFinality::FINAL,
                            };
                            working_entries.push_back(entry);
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedSystemLock>) {
                    const auto& auth = op.authorization;
                    const auto& payload = op.lock;
                    if (auth.account_id == *my_account) {
                        auto it = std::find_if(working_entries.begin(), working_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it != working_entries.end()) {
                            it->finality = WalletEntryFinality::FINAL;
                            it->height = h;
                        } else {
                            WalletLedgerEntry entry{
                                .entry_id = op_id,
                                .kind = WalletEntryKind::LOCK_TO_SYSTEM,
                                .amount = static_cast<int64_t>(payload.amount),
                                .system_side = true,
                                .counterparty = AccountId{},
                                .timestamp = 0,
                                .height = h,
                                .finality = WalletEntryFinality::FINAL,
                            };
                            working_entries.push_back(entry);
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedRootPublication>) {
                    if (op.authorization.account_id == *my_account) {
                        const auto it = std::find_if(working_entries.begin(), working_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == working_entries.end()) {
                            const auto encoded = SerializeProtocolOperation(ProtocolOperation{op});
                            const auto fee = encoded ? ComputeRootPublicationFee(params,
                                encoded->size(), op.publication.chunk_count) : std::nullopt;
                            if (fee) {
                                WalletLedgerEntry entry{
                                    .entry_id = op_id,
                                    .kind = WalletEntryKind::ROOT_PUBLICATION_FEE,
                                    .amount = -static_cast<int64_t>(*fee),
                                    .system_side = true,
                                    .counterparty = AccountId{},
                                    .timestamp = 0,
                                    .height = h,
                                    .finality = WalletEntryFinality::FINAL,
                                };
                                working_entries.push_back(entry);
                            }
                        }
                    }
                }
            }, proto_op);
        }

        scanned_height = h;
    }

    size_t committed_entries_count{0};
    {
        std::lock_guard lock(m_mutex);
        for (const auto& scanned_entry : working_entries) {
            const auto existing = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) {
                return entry.entry_id == scanned_entry.entry_id;
            });
            if (existing == m_entries.end()) {
                m_entries.push_back(scanned_entry);
                ++committed_entries_count;
            } else if (scanned_entry.finality == WalletEntryFinality::FINAL) {
                existing->finality = WalletEntryFinality::FINAL;
                existing->height = scanned_entry.height;
            }
        }
        m_last_scanned_height = std::max(m_last_scanned_height, scanned_height);
    }
    return committed_entries_count;
}

std::vector<WalletLedgerEntry> CybouWalletService::GetLedgerEntries() const
{
    std::vector<WalletLedgerEntry> sorted;
    {
        std::lock_guard lock(m_mutex);
        sorted = m_entries;
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.height != b.height) return a.height > b.height;
        return a.timestamp > b.timestamp;
    });
    return sorted;
}

} // namespace cybou
