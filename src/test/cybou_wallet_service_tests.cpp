// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <boost/test/unit_test.hpp>

#include <cybou/authority_node.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
#include <cybou/wallet_service.h>
#include <test/util/setup_common.h>

#include <array>
#include <filesystem>
#include <vector>

namespace cybou_wallet_service_tests {

using namespace cybou;

namespace {

CybouState CreateTestGenesis(const uint256& val_pub)
{
    return CybouState{
        .onboarding_pool = 1'000'000,
        .security_reward_pool = 100,
        .pending_fee_pool = 0,
        .accounts = {},
        .validator_set = {
            .version = VALIDATOR_SET_VERSION,
            .validators = {{
                .validator_id = val_pub,
                .consensus_public_key = val_pub,
                .weight = 1,
            }},
        },
    };
}

CybouNetworkDefinitionV1 CreateTestNetworkDefinition(const CybouState& genesis)
{
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    return CybouNetworkDefinitionV1{
        .protocol_version = CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = CybouStateHash(genesis),
        .genesis_state_root = CybouStateHash(genesis),
        .protocol_parameters = params,
        .initial_validator_set_commitment = ComputeValidatorSetCommitment(genesis.validator_set),
        .operator_authority = std::nullopt,
    };
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_wallet_service_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(wallet_payment_success_and_ledger_sync)
{
    const auto test_dir = m_args.GetDataDirBase() / "test_wallet_service";
    std::filesystem::create_directories(test_dir);

    std::array<unsigned char, 32> val_priv{};
    val_priv[0] = 0xAA;
    const auto val_pub = *DeriveEd25519PublicKey(val_priv);

    auto gen_state = CreateTestGenesis(val_pub);

    // Setup Alice with 1000 balance and 500 system_balance
    std::array<unsigned char, 32> alice_seed{};
    alice_seed[0] = 0xA1;
    const auto alice_pub = *DeriveEd25519PublicKey(alice_seed);
    const AccountId alice_id{alice_pub};

    gen_state.accounts[alice_id] = AccountState{
        .balance = 1000,
        .system_balance = 500,
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = alice_pub,
        .next_nonce = 0,
    };

    // Setup Bob with 0 balance and 100 system_balance
    std::array<unsigned char, 32> bob_seed{};
    bob_seed[0] = 0xB2;
    const auto bob_pub = *DeriveEd25519PublicKey(bob_seed);
    const AccountId bob_id{bob_pub};

    gen_state.accounts[bob_id] = AccountState{
        .balance = 0,
        .system_balance = 100,
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = bob_pub,
        .next_nonce = 0,
    };

    auto net_def = CreateTestNetworkDefinition(gen_state);

    NodeRuntimeConfig runtime_config{
        .network_definition = net_def,
        .data_dir = test_dir / "node",
        .validator_private_key = val_priv,
        .wipe_data = true,
    };

    CybouNodeRuntime runtime{runtime_config};
    BOOST_REQUIRE(runtime.InitializeGenesis(gen_state));

    CybouKeyStore alice_keystore;
    BOOST_REQUIRE(alice_keystore.LoadFromSeed(alice_seed));

    CybouKeyStore bob_keystore;
    BOOST_REQUIRE(bob_keystore.LoadFromSeed(bob_seed));

    CybouWalletService alice_wallet{runtime, alice_keystore};
    CybouWalletService bob_wallet{runtime, bob_keystore};

    // Check initial balances
    const auto [a_bal, a_sys] = alice_wallet.GetBalances();
    BOOST_CHECK_EQUAL(a_bal, 1000);
    BOOST_CHECK_EQUAL(a_sys, 500);

    const auto [b_bal, b_sys] = bob_wallet.GetBalances();
    BOOST_CHECK_EQUAL(b_bal, 0);
    BOOST_CHECK_EQUAL(b_sys, 100);

    // Alice sends 250 CYBOU to Bob
    const auto pay_res = alice_wallet.SendPayment(bob_id, 250);
    BOOST_REQUIRE(pay_res);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(pay_res.error), static_cast<uint8_t>(WalletOperationError::NONE));

    // Finalize block containing payment
    const auto block_opt = runtime.ProduceBlock();
    BOOST_REQUIRE(block_opt.has_value());
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 1);

    // Verify balances after block execution
    const auto [a_bal_after, a_sys_after] = alice_wallet.GetBalances();
    BOOST_CHECK_EQUAL(a_bal_after, 750);
    BOOST_CHECK_EQUAL(a_sys_after, 499); // 500 - 1 fee

    const auto [b_bal_after, b_sys_after] = bob_wallet.GetBalances();
    BOOST_CHECK_EQUAL(b_bal_after, 250);
    BOOST_CHECK_EQUAL(b_sys_after, 100);

    // Alice syncs ledger
    const size_t a_new = alice_wallet.SyncLedger();
    BOOST_CHECK_GE(a_new, 0);
    const auto a_entries = alice_wallet.GetLedgerEntries();
    BOOST_REQUIRE_EQUAL(a_entries.size(), 1);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(a_entries[0].kind), static_cast<uint8_t>(WalletEntryKind::PAYMENT));
    BOOST_CHECK_EQUAL(a_entries[0].amount, -250);
    BOOST_CHECK_EQUAL(a_entries[0].system_side, false);
    BOOST_CHECK(a_entries[0].counterparty == bob_id);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(a_entries[0].finality), static_cast<uint8_t>(WalletEntryFinality::FINAL));

    // Bob syncs ledger
    const size_t b_new = bob_wallet.SyncLedger();
    BOOST_CHECK_EQUAL(b_new, 1);
    const auto b_entries = bob_wallet.GetLedgerEntries();
    BOOST_REQUIRE_EQUAL(b_entries.size(), 1);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(b_entries[0].kind), static_cast<uint8_t>(WalletEntryKind::PAYMENT));
    BOOST_CHECK_EQUAL(b_entries[0].amount, 250);
    BOOST_CHECK_EQUAL(b_entries[0].system_side, false);
    BOOST_CHECK(b_entries[0].counterparty == alice_id);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(b_entries[0].finality), static_cast<uint8_t>(WalletEntryFinality::FINAL));
}

BOOST_AUTO_TEST_CASE(wallet_lock_to_system_balance)
{
    const auto test_dir = m_args.GetDataDirBase() / "test_wallet_lock";
    std::filesystem::create_directories(test_dir);

    std::array<unsigned char, 32> val_priv{};
    val_priv[0] = 0xAA;
    const auto val_pub = *DeriveEd25519PublicKey(val_priv);

    auto gen_state = CreateTestGenesis(val_pub);

    std::array<unsigned char, 32> alice_seed{};
    alice_seed[0] = 0xA1;
    const auto alice_pub = *DeriveEd25519PublicKey(alice_seed);
    const AccountId alice_id{alice_pub};

    gen_state.accounts[alice_id] = AccountState{
        .balance = 500,
        .system_balance = 100,
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = alice_pub,
        .next_nonce = 0,
    };

    auto net_def = CreateTestNetworkDefinition(gen_state);

    NodeRuntimeConfig runtime_config{
        .network_definition = net_def,
        .data_dir = test_dir / "node",
        .validator_private_key = val_priv,
        .wipe_data = true,
    };

    CybouNodeRuntime runtime{runtime_config};
    BOOST_REQUIRE(runtime.InitializeGenesis(gen_state));

    CybouKeyStore alice_keystore;
    BOOST_REQUIRE(alice_keystore.LoadFromSeed(alice_seed));

    CybouWalletService alice_wallet{runtime, alice_keystore};

    // Alice locks 200 from Balance to System Balance
    const auto lock_res = alice_wallet.LockToSystemBalance(200);
    BOOST_REQUIRE(lock_res);

    const auto block_opt = runtime.ProduceBlock();
    BOOST_REQUIRE(block_opt.has_value());

    const auto [bal, sys] = alice_wallet.GetBalances();
    BOOST_CHECK_EQUAL(bal, 300); // 500 - 200
    BOOST_CHECK_EQUAL(sys, 300); // 100 + 200

    alice_wallet.SyncLedger();
    const auto entries = alice_wallet.GetLedgerEntries();
    BOOST_REQUIRE_EQUAL(entries.size(), 1);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(entries[0].kind), static_cast<uint8_t>(WalletEntryKind::LOCK_TO_SYSTEM));
    BOOST_CHECK_EQUAL(entries[0].amount, 200);
    BOOST_CHECK_EQUAL(entries[0].system_side, true);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(entries[0].finality), static_cast<uint8_t>(WalletEntryFinality::FINAL));
}

BOOST_AUTO_TEST_CASE(wallet_error_conditions)
{
    const auto test_dir = m_args.GetDataDirBase() / "test_wallet_errors";
    std::filesystem::create_directories(test_dir);

    std::array<unsigned char, 32> val_priv{};
    val_priv[0] = 0xAA;
    const auto val_pub = *DeriveEd25519PublicKey(val_priv);

    auto gen_state = CreateTestGenesis(val_pub);

    std::array<unsigned char, 32> alice_seed{};
    alice_seed[0] = 0xA1;
    const auto alice_pub = *DeriveEd25519PublicKey(alice_seed);
    const AccountId alice_id{alice_pub};

    gen_state.accounts[alice_id] = AccountState{
        .balance = 50,
        .system_balance = 0, // No system balance for fees
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = alice_pub,
        .next_nonce = 0,
    };

    auto net_def = CreateTestNetworkDefinition(gen_state);

    NodeRuntimeConfig runtime_config{
        .network_definition = net_def,
        .data_dir = test_dir / "node",
        .validator_private_key = val_priv,
        .wipe_data = true,
    };

    CybouNodeRuntime runtime{runtime_config};
    BOOST_REQUIRE(runtime.InitializeGenesis(gen_state));

    CybouKeyStore alice_keystore;
    BOOST_REQUIRE(alice_keystore.LoadFromSeed(alice_seed));

    CybouWalletService alice_wallet{runtime, alice_keystore};

    AccountId bob_id{uint256::FromUserHex("0101").value()};

    // Self payment
    const auto res_self = alice_wallet.SendPayment(alice_id, 10);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(res_self.error), static_cast<uint8_t>(WalletOperationError::SELF_PAYMENT));

    // Zero amount
    const auto res_zero = alice_wallet.SendPayment(bob_id, 0);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(res_zero.error), static_cast<uint8_t>(WalletOperationError::ZERO_AMOUNT));

    // Insufficient balance
    const auto res_over = alice_wallet.SendPayment(bob_id, 100);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(res_over.error), static_cast<uint8_t>(WalletOperationError::INSUFFICIENT_BALANCE));

    // Insufficient system balance for fee
    const auto res_fee = alice_wallet.SendPayment(bob_id, 10);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(res_fee.error), static_cast<uint8_t>(WalletOperationError::INSUFFICIENT_SYSTEM_BALANCE));

    // Zero lock
    const auto res_zero_lock = alice_wallet.LockToSystemBalance(0);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(res_zero_lock.error), static_cast<uint8_t>(WalletOperationError::ZERO_AMOUNT));

    // Over lock
    const auto res_over_lock = alice_wallet.LockToSystemBalance(100);
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(res_over_lock.error), static_cast<uint8_t>(WalletOperationError::INSUFFICIENT_BALANCE));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace cybou_wallet_service_tests
