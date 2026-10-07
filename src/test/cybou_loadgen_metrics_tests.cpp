// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#include <test/util/loadgen_metrics.h>
#include <boost/test/unit_test.hpp>
BOOST_AUTO_TEST_SUITE(cybou_loadgen_metrics_tests)
BOOST_AUTO_TEST_CASE(measured_cohort_deduplicates_and_excludes_history) {
    LoadgenOperationMetrics m;
    const cybou::Hash256 payment{uint8_t{1}}, publication{uint8_t{2}}, historical{uint8_t{3}};
    m.attempted["files"]=2;
    m.attempted["payments"]=2;
    m.busy["payments"]=1;
    m.Finalized("files",historical);
    m.Submitted("files",{}); // A staged intent has no OperationID yet.
    BOOST_CHECK(m.submitted_ids.empty());
    BOOST_CHECK(m.finalized_ids.empty());
    m.Submitted("payments",payment);
    m.Submitted("payments",payment);
    m.Submitted("files",publication);
    m.Finalized("payments",payment);
    m.Finalized("payments",payment);
    BOOST_CHECK_EQUAL(m.submitted_ids.size(),2);
    BOOST_CHECK_EQUAL(m.finalized_ids.size(),1);
    // Delayed completion in drain counts once, regardless of repeated observations.
    m.Finalized("files",publication);
    m.Finalized("files",publication);
    BOOST_CHECK_EQUAL(m.finalized_ids.size(),2);
    BOOST_CHECK_EQUAL(m.submitted.at("payments"),1);
    BOOST_CHECK_EQUAL(m.finalized.at("files"),1);
}
BOOST_AUTO_TEST_SUITE_END()
