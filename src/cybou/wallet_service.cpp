// Copyright (c) 2026 Stanislav SAVELIEV
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

    const auto dev_id = m_keystore.GetDeviceId();
    if (!dev_id) {
        return {.error = WalletOperationError::NO_IDENTITY, .error_message = "No active device key in keystore"};
    }

    uint64_t nonce = 0;
    uint64_t activation_nonce = 0;
    const auto loaded = m_runtime.GetStore().LoadState();
    if (loaded && loaded.state) {
        const auto* rec = loaded.state->identities.Find(*my_account);
        if (rec) {
            auto it = rec->devices.find(*dev_id);
            if (it != rec->devices.end()) {
                nonce = it->second.next_nonce;
                activation_nonce = it->second.activation_nonce;
            }
        }
    }

    PaymentPayload payment_payload{
        .recipient = recipient,
        .amount = amount,
    };

    const auto commitment = ComputePaymentPayloadCommitment(payment_payload);
    if (!commitment) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to commit payment payload"};
    }

    DeviceAuthorization auth{
        .account_id = *my_account,
        .device_id = *dev_id,
        .nonce = nonce,
        .activation_nonce = activation_nonce,
        .kind = DeviceOperationKind::PAYMENT,
        .payload_commitment = *commitment,
    };

    const auto digest = ComputeDeviceOperationDigest(m_runtime.GetNetworkId(), auth);
    if (!digest) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to compute payment digest"};
    }

    const auto sig = m_keystore.SignDevice(*digest);
    if (!sig) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to sign payment operation"};
    }
    auth.signature = *sig;

    AuthorizedPayment auth_payment{
        .authorization = auth,
        .payment = payment_payload,
    };

    ProtocolOperation proto_op{auth_payment};

    const auto op_id_opt = ComputeOperationId(proto_op);
    const uint256 op_id = op_id_opt.value_or(uint256{});
    const auto submit_res = m_runtime.SubmitOperation(proto_op);
    if (!submit_res) {
        return {.error = WalletOperationError::SUBMIT_FAILED, .error_message = "Network rejected payment operation"};
    }

    WalletLedgerEntry pending_entry{
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

    const auto dev_id = m_keystore.GetDeviceId();
    if (!dev_id) {
        return {.error = WalletOperationError::NO_IDENTITY, .error_message = "No active device key in keystore"};
    }

    uint64_t nonce = 0;
    uint64_t activation_nonce = 0;
    const auto loaded = m_runtime.GetStore().LoadState();
    if (loaded && loaded.state) {
        const auto* rec = loaded.state->identities.Find(*my_account);
        if (rec) {
            auto it = rec->devices.find(*dev_id);
            if (it != rec->devices.end()) {
                nonce = it->second.next_nonce;
                activation_nonce = it->second.activation_nonce;
            }
        }
    }

    SystemLockPayload lock_payload{
        .amount = amount,
    };

    const auto commitment = ComputeSystemLockPayloadCommitment(lock_payload);
    if (!commitment) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to commit system lock payload"};
    }

    DeviceAuthorization auth{
        .account_id = *my_account,
        .device_id = *dev_id,
        .nonce = nonce,
        .activation_nonce = activation_nonce,
        .kind = DeviceOperationKind::SYSTEM_LOCK,
        .payload_commitment = *commitment,
    };

    const auto digest = ComputeDeviceOperationDigest(m_runtime.GetNetworkId(), auth);
    if (!digest) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to compute system lock digest"};
    }

    const auto sig = m_keystore.SignDevice(*digest);
    if (!sig) {
        return {.error = WalletOperationError::CRYPTO_FAILURE, .error_message = "Failed to sign system lock operation"};
    }
    auth.signature = *sig;

    AuthorizedSystemLock auth_lock{
        .authorization = auth,
        .lock = lock_payload,
    };

    ProtocolOperation proto_op{auth_lock};

    const auto op_id_opt = ComputeOperationId(proto_op);
    const uint256 op_id = op_id_opt.value_or(uint256{});
    const auto submit_res = m_runtime.SubmitOperation(proto_op);
    if (!submit_res) {
        return {.error = WalletOperationError::SUBMIT_FAILED, .error_message = "Network rejected system lock operation"};
    }

    WalletLedgerEntry pending_entry{
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
            const auto op_id_opt = ComputeOperationId(proto_op);
            const uint256 op_id = op_id_opt.value_or(uint256{});

            std::visit([&](const auto& op) {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, AccountCreateOp>) {
                    if (op.account_id == *my_account) {
                        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == m_entries.end()) {
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
                            m_entries.push_back(entry);
                            new_entries_count++;
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedPayment>) {
                    const auto& auth = op.authorization;
                    const auto& payload = op.payment;
                    if (auth.account_id == *my_account) {
                        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it != m_entries.end()) {
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
                            m_entries.push_back(entry);
                            new_entries_count++;
                        }
                    } else if (payload.recipient == *my_account) {
                        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == m_entries.end()) {
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
                            m_entries.push_back(entry);
                            new_entries_count++;
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedSystemLock>) {
                    const auto& auth = op.authorization;
                    const auto& payload = op.lock;
                    if (auth.account_id == *my_account) {
                        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it != m_entries.end()) {
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
                            m_entries.push_back(entry);
                            new_entries_count++;
                        }
                    }
                } else if constexpr (std::is_same_v<T, AuthorizedMail>) {
                    const auto& auth = op.authorization;
                    const auto& payload = op.mail;
                    if (auth.account_id == *my_account) {
                        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) {
                            return e.entry_id == op_id;
                        });
                        if (it == m_entries.end()) {
                            const uint64_t fee = MailFeeForSize(payload.ciphertext.size(), params);
                            WalletLedgerEntry entry{
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
            }, proto_op);
        }

        m_last_scanned_height = h;
    }

    return new_entries_count;
}

std::vector<WalletLedgerEntry> CybouWalletService::GetLedgerEntries() const
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
