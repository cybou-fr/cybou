// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_TEST_UTIL_LOADGEN_METRICS_H
#define CYBOU_TEST_UTIL_LOADGEN_METRICS_H
#include <cybou/hash256.h>
#include <map>
#include <set>
#include <string>

// Test-tool accounting: only this run's unique submitted operations can finalize.
struct LoadgenOperationMetrics {
    std::map<std::string,uint64_t> attempted, submitted, finalized, failed, busy;
    std::set<std::string> submitted_ids, finalized_ids;
    void Submitted(const std::string& profile, const cybou::Hash256& id) {
        if (id.IsNull()) return;
        if (submitted_ids.insert(id.GetHex()).second) ++submitted[profile];
    }
    void Finalized(const std::string& profile, const cybou::Hash256& id) {
        if (submitted_ids.contains(id.GetHex()) && finalized_ids.insert(id.GetHex()).second) ++finalized[profile];
    }
};
#endif
