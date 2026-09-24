// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/wallet_service.h>

#include <algorithm>
#include <chrono>

namespace cybou {

CybouWalletService::CybouWalletService(CybouNodeRuntime& runtime, CybouKeyStore& keystore)
    : m_runtime(runtime),
      m_keystore(keystore)
{
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
    std::lock_guard lock(m_mutex);

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

    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    if (sender_state->system_balance < params.payment_fee) {
        return {.error = WalletOperationError::INSUFFICIENT_SYSTEM_BALANCE, .error_message = "Insufficient system balance for fee"};
    }

    const uint64_t nonce = sender_state->next_nonce;
    PaymentOpV1 payment_op{
        .version = PAYMENT_OP_VERSION,
        .recipient = recipient,
        .amount = amount,
    };

    const uint256 signing_digest = ComputeUserOperationDigest(
        m_runtime.GetNetworkId(),
        *my_account,
        nonce,
        payment_op);

    const auto sig = m_keystore.Sign(signing_digest);
    if (!sig) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to sign payment operation"};
    }

    AuthorizedOperationV1 auth_op{
        .version = AUTHORIZED_OPERATION_VERSION,
        .account_id = *my_account,
        .nonce = nonce,
        .payload = payment_op,
        .signature = *sig,
    };

    ProtocolOperationV1 proto_op{auth_op};

    const uint256 op_id = ComputeOperationId(proto_op);
    const auto submit_res = m_runtime.SubmitOperation(proto_op);
    if (!submit_res) {
        return {.error = WalletOperationError::SUBMIT_FAILED, .error_message = "Network rejected payment operation"};
    }

    WalletLedgerEntryV1 pending_entry{
        .entry_id = op_id,
        .kind = WalletEntryKind::PAYMENT,
        .amount = -static_cast<int64_t>(amount),
        .system_side = false,
        .counterparty = recipient,
        .timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()),
        .height = m_runtime.GetFinalizedHeight().value_or(0),
        .finality = WalletEntryFinality::PENDING,
    };
    m_entries.insert(m_entries.begin(), pending_entry);

    return {.error = WalletOperationError::NONE, .op_id = op_id};
}

WalletOperationResult CybouWalletService::LockToSystemBalance(const uint64_t amount)
{
    std::lock_guard lock(m_mutex);

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

    const uint64_t nonce = sender_state->next_nonce;
    SystemLockOpV1 lock_op{
        .version = SYSTEM_LOCK_OP_VERSION,
        .amount = amount,
    };

    const uint256 signing_digest = ComputeUserOperationDigest(
        m_runtime.GetNetworkId(),
        *my_account,
        nonce,
        lock_op);

    const auto sig = m_keystore.Sign(signing_digest);
    if (!sig) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to sign system lock operation"};
    }

    AuthorizedOperationV1 auth_op{
        .version = AUTHORIZED_OPERATION_VERSION,
        .account_id = *my_account,
        .nonce = nonce,
        .payload = lock_op,
        .signature = *sig,
    };

    ProtocolOperationV1 proto_op{auth_op};

    const uint256 op_id = ComputeOperationId(proto_op);
    const auto submit_res = m_runtime.SubmitOperation(proto_op);
    if (!submit_res) {
        return {.error = WalletOperationError::SUBMIT_FAILED, .error_message = "Network rejected system lock operation"};
    }

    WalletLedgerEntryV1 pending_entry{
        .entry_id = op_id,
        .kind = WalletEntryKind::LOCK_TO_SYSTEM,
        .amount = static_cast<int64_t>(amount),
        .system_side = true,
        .counterparty = AccountId{},
        .timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()),
        .height = m_runtime.GetFinalizedHeight().value_or(0),
        .finality = WalletEntryFinality::PENDING,
    };
    m_entries.insert(m_entries.begin(), pending_entry);

    return {.error = WalletOperationError::NONE, .op_id = op_id};
}

size_t CybouWalletService::SyncLedger()
{
    std::lock_guard lock(m_mutex);

    const auto my_account = m_keystore.GetAccountId();
    if (!my_account) {
        return 0;
    }

    const auto tip_height = m_runtime.GetFinalizedHeight();
    if (!tip_height) {
        return 0;
    }

    size_t new_entries_count = 0;
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;

    for (uint64_t h = m_last_scanned_height + 1; h <= *tip_height; ++h) {
        const auto block_opt = m_runtime.GetBlockAtHeight(h);
        if (!block_opt) break;
        const auto& fin_block = *block_opt;

        for (const auto& proto_op : fin_block.block.operations) {
            const uint256 op_id = ComputeOperationId(proto_op);

            std::visit([&](const auto& op) {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, AccountCreateOpV1>) {
                    if (op.account_id == *my_account) {
                        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == m_entries.end()) {
                            WalletLedgerEntryV1 entry{
                                .entry_id = op_id,
                                .kind = WalletEntryKind::ONBOARDING_BONUS,
                                .amount = static_cast<int64_t>(params.onboarding_bonus),
                                .system_side = true,
                                .counterparty = AccountId{},
                                .timestamp = 0,
                                .height = h,
                                .finality = WalletEntryFinality::FINAL,
                            };
                            m_entries.push_back(entry);
                            new_entries_count++;
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedOperationV1>) {
                    std::visit([&](const auto& payload) {
                        using P = std::decay_t<decltype(payload)>;
                        if constexpr (std::is_same_v<P, PaymentOpV1>) {
                            if (op.account_id == *my_account) {
                                auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                                    return e.entry_id == op_id;
                                });
                                if (it != m_entries.end()) {
                                    it->finality = WalletEntryFinality::FINAL;
                                    it->height = h;
                                } else {
                                    WalletLedgerEntryV1 entry{
                                        .entry_id = op_id,
                                        .kind = WalletEntryKind::PAYMENT,
                                        .amount = -static_cast<int64_t>(payload.amount),
                                        .system_side = false,
                                        .counterparty = payload.recipient,
                                        .timestamp = 0,
                                        .height = h,
                                        .finality = WalletEntryFinality::FINAL,
                                    };
                                    m_entries.push_back(entry);
                                    new_entries_count++;
                                }
                            } else if (payload.recipient == *my_account) {
                                const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                                    return e.entry_id == op_id;
                                });
                                if (it == m_entries.end()) {
                                    WalletLedgerEntryV1 entry{
                                        .entry_id = op_id,
                                        .kind = WalletEntryKind::PAYMENT,
                                        .amount = static_cast<int64_t>(payload.amount),
                                        .system_side = false,
                                        .counterparty = op.account_id,
                                        .timestamp = 0,
                                        .height = h,
                                        .finality = WalletEntryFinality::FINAL,
                                    };
                                    m_entries.push_back(entry);
                                    new_entries_count++;
                                }
                            }
                        } else if constexpr (std::is_same_v<P, SystemLockOpV1>) {
                            if (op.account_id == *my_account) {
                                auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                                    return e.entry_id == op_id;
                                });
                                if (it != m_entries.end()) {
                                    it->finality = WalletEntryFinality::FINAL;
                                    it->height = h;
                                } else {
                                    WalletLedgerEntryV1 entry{
                                        .entry_id = op_id,
                                        .kind = WalletEntryKind::LOCK_TO_SYSTEM,
                                        .amount = static_cast<int64_t>(payload.amount),
                                        .system_side = true,
                                        .counterparty = AccountId{},
                                        .timestamp = 0,
                                        .height = h,
                                        .finality = WalletEntryFinality::FINAL,
                                    };
                                    m_entries.push_back(entry);
                                    new_entries_count++;
                                }
                            }
                        } else if constexpr (std::is_same_v<P, MailOpV1>) {
                            if (op.account_id == *my_account) {
                                const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                                    return e.entry_id == op_id;
                                });
                                if (it == m_entries.end()) {
                                    const uint64_t fee = MailFeeForSize(payload.ciphertext.size(), params);
                                    WalletLedgerEntryV1 entry{
                                        .entry_id = op_id,
                                        .kind = WalletEntryKind::MAIL_FEE,
                                        .amount = -static_cast<int64_t>(fee),
                                        .system_side = true,
                                        .counterparty = payload.recipient,
                                        .timestamp = 0,
                                        .height = h,
                                        .finality = WalletEntryFinality::FINAL,
                                    };
                                    m_entries.push_back(entry);
                                    new_entries_count++;
                                }
                            }
                        }
                    }, op.payload);
                }
            }, proto_op.payload);
        }

        m_last_scanned_height = h;
    }

    return new_entries_count;
}

std::vector<WalletLedgerEntryV1> CybouWalletService::GetLedgerEntries() const
{
    std::lock_guard lock(m_mutex);
    auto sorted = m_entries;
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.height != b.height) return a.height > b.height;
        return a.timestamp > b.timestamp;
    });
    return sorted;
}

} // namespace cybou
