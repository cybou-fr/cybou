// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_QT_BENCHMARKREFERENCE_H
#define CYBOU_QT_BENCHMARKREFERENCE_H
#include <QByteArray>
#include <QString>
#include <optional>

struct CybouBenchmarkReference {
    QString run_id, network_binding, profile, revision, binary_sha256;
    bool dirty{false};
    bool co_located_wsl{false};
    quint64 attempted{0}, submitted{0}, finalized{0};
    int clients{0}, replicas{0};
    QString file_size;
    double window_s{0}, finalized_per_s{0};
    static std::optional<CybouBenchmarkReference> Parse(const QByteArray& json);
};
#endif
