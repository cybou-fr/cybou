// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_QT_STORAGETRANSFERTEXT_H
#define CYBOU_QT_STORAGETRANSFERTEXT_H
#include <cybou/diagnostics.h>
#include <QCoreApplication>
#include <QLocale>
#include <QString>
struct StorageTransferText {
    QString put_title, get_title, put_rate, get_rate, put_detail, get_detail;
};
inline StorageTransferText cybouStorageTransferText(const cybou::NodeDiagnosticsSnapshot& d)
{
    const auto tr = [](const char* text) { return QCoreApplication::translate("StorageTransfers", text); };
    const auto unknown = tr("Unknown");
    const bool measured = d.observed_unix_ms != 0;
    const auto rate = [&](const cybou::TrafficDiagnostics& t) {
        return measured && t.window_ms == 60000 ? tr("↓ %1 B/s · ↑ %2 B/s")
            .arg(QLocale{}.toString(t.window_received_bytes / 60.0, 'f', 1),
                 QLocale{}.toString(t.window_sent_bytes / 60.0, 'f', 1)) : unknown;
    };
    const auto totals = [&](const cybou::TrafficDiagnostics& t) {
        return tr("Since runtime start: ↓ %1 bytes · ↑ %2 bytes. Encrypted payload, completed transfers; repeats included. Rates: 60 complete seconds. Local only.")
            .arg(measured ? QLocale{}.toString(t.received_bytes) : unknown,
                 measured ? QLocale{}.toString(t.sent_bytes) : unknown);
    };
    const auto& t = d.storage_transfers;
    return {tr("Local PUT payload"), tr("Local GET payload"), rate(t.put), rate(t.get),
        totals(t.put) + QStringLiteral("\n") + tr("Received: admitted. Sent: provider receipt verified."),
        totals(t.get) + QStringLiteral("\n") + tr("Received: ChunkID verified. Sent: complete write, remote receipt unknown. Recovery, repair and full-GET checks included.")};
}
#endif
