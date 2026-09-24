// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/wallet_service.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(cybou_wallet_service_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(wallet_reads_finalized_balances_and_rejects_invalid_payments)
{
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("wallet-alice.cybou");
    auto bob = fixture.CreateIdentity("wallet-bob.cybou");
    cybou::CybouWalletService wallet{*fixture.runtime, alice->GetKeyStore()};
    const auto [balance, system_balance] = wallet.GetBalances();
    BOOST_CHECK_EQUAL(balance, 0);
    BOOST_CHECK_EQUAL(system_balance, fixture.definition.protocol_parameters.onboarding_bonus);
    const auto alice_id = alice->GetAccountId();
    const auto bob_id = bob->GetAccountId();
    BOOST_REQUIRE(alice_id && bob_id);
    BOOST_CHECK(wallet.SendPayment(*alice_id, 1).error == cybou::WalletOperationError::SELF_PAYMENT);
    BOOST_CHECK(wallet.SendPayment(*bob_id, 0).error == cybou::WalletOperationError::ZERO_AMOUNT);
    BOOST_CHECK(wallet.SendPayment(*bob_id, 1).error == cybou::WalletOperationError::INSUFFICIENT_BALANCE);
    BOOST_CHECK(wallet.LockToSystemBalance(1).error == cybou::WalletOperationError::INSUFFICIENT_BALANCE);
    BOOST_CHECK(wallet.GetLedgerEntries().empty());
    BOOST_CHECK_EQUAL(wallet.SyncLedger(), 1U);
    const auto entries = wallet.GetLedgerEntries();
    BOOST_REQUIRE_EQUAL(entries.size(), 1U);
    BOOST_CHECK(entries[0].kind == cybou::WalletEntryKind::ONBOARDING_BONUS);
    BOOST_CHECK(entries[0].finality == cybou::WalletEntryFinality::FINAL);
    BOOST_CHECK_EQUAL(entries[0].amount, static_cast<int64_t>(system_balance));
    BOOST_CHECK_EQUAL(wallet.SyncLedger(), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
