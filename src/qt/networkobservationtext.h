// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_QT_NETWORKOBSERVATIONTEXT_H
#define CYBOU_QT_NETWORKOBSERVATIONTEXT_H
#include <cybou/diagnostics.h>
#include <cybou/network_observation.h>
#include <cybou/hash256.h>
#include <QCoreApplication>
#include <QLocale>
#include <QString>
#include <algorithm>
struct NetworkObservationText {
    QString capacity_title, capacity, storage_detail, traffic, traffic_detail, cpu, cpu_detail, coverage;
};
inline NetworkObservationText cybouNetworkObservationText(const cybou::NodeDiagnosticsSnapshot& d,
    std::chrono::steady_clock::time_point at = std::chrono::steady_clock::now())
{
    const auto tr = [](const char* text) { return QCoreApplication::translate("NetworkObservation", text); };
    const auto unknown = tr("Unknown");
    const auto* snapshot = d.network_observation.get();
    if (snapshot && cybou::Hash256{snapshot->network_binding}.GetHex() != d.network_binding) snapshot = nullptr;
    const auto* r = snapshot && snapshot->remote.clock_valid ? &snapshot->remote : nullptr;
    uint64_t elapsed{0};
    if (r) {
        if (at < snapshot->captured_at) r = nullptr;
        else elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(at - snapshot->captured_at).count();
        if (elapsed >= 90000) r = nullptr;
        if (r) for (const auto& row : r->reports) if (!row.local_loopback && elapsed + row.receipt_age_ms >= 90000) { r = nullptr; break; }
    }
    const bool cpu_known = r && r->cpu.contributors && elapsed + r->cpu.max_age_ms <= 60000;
    const auto number = [&](std::optional<uint64_t> n) { return n ? QLocale{}.toString(static_cast<qulonglong>(*n)) : unknown; };
    const auto decimal = [&](std::optional<double> n) { return n ? QLocale{}.toString(*n, 'f', 1) : unknown; };
    NetworkObservationText out;
    out.capacity_title = tr("Observed storage capacity · %1 reporting groups").arg(r ? QString::number(r->storage.contributors) : unknown);
    out.capacity = tr("%1 bytes").arg(number(r ? r->storage.capacity_bytes : std::nullopt));
    out.storage_detail = tr("Stored copies: %1 bytes · utilization: %2 %\nProvider budget: %3 bytes · admitted provider bytes: %4")
        .arg(number(r ? r->storage.stored_copy_bytes : std::nullopt), decimal(r ? r->storage.utilization_percent : std::nullopt),
             number(r ? r->storage.provider_budget_bytes : std::nullopt), number(r ? r->storage.provider_used_bytes : std::nullopt));
    out.traffic = tr("↓ %1 B/s · ↑ %2 B/s").arg(decimal(r ? r->traffic.received_bytes_per_second : std::nullopt), decimal(r ? r->traffic.sent_bytes_per_second : std::nullopt));
    out.traffic_detail = tr("%1 reporting groups · declared 60-second frame windows · service/retries/observation included · TLS/TCP overhead excluded")
        .arg(r ? QString::number(r->traffic.contributors) : unknown);
    out.cpu = tr("%1 %").arg(decimal(cpu_known ? r->cpu.mean_percent : std::nullopt));
    out.cpu_detail = tr("%1 reporting groups · arithmetic mean of normalized process CPU · windows %2–%3 ms · ages %4–%5 ms")
        .arg(r ? QString::number(r->cpu.contributors) : unknown)
        .arg(cpu_known ? QString::number(r->cpu.min_window_ms) : unknown,
             cpu_known ? QString::number(r->cpu.max_window_ms) : unknown,
             cpu_known ? QString::number(r->cpu.min_age_ms + elapsed) : unknown,
             cpu_known ? QString::number(r->cpu.max_age_ms + elapsed) : unknown);
    uint32_t min_age = 90000, max_age = 0, cache_age = 0;
    if (r) for (const auto& row : r->reports) if (!row.local_loopback) {
        min_age = std::min(min_age, row.receipt_age_ms); max_age = std::max(max_age, row.receipt_age_ms);
        cache_age = std::max(cache_age, row.declared_cache_age_ms);
    }
    out.coverage = tr("Fresh reporting address groups: %1 / %2 · missing: %3 · slot limit reached: %4\nReceipt ages: %5–%6 ms · declared cache age up to %7 ms\nPartial unverified declarations, rounded MiB/KiB. Whole-network coverage and independent hosts: Unknown.")
        .arg(r ? QString::number(r->fresh_remote_groups) : unknown, r ? QString::number(r->selected_remote_groups) : unknown,
             r ? QString::number(r->missing_remote_groups) : unknown, r ? (r->slots_full ? tr("Yes") : tr("No")) : unknown)
        .arg(r && r->fresh_remote_groups ? QString::number(min_age + elapsed) : unknown, r && r->fresh_remote_groups ? QString::number(max_age + elapsed) : unknown,
             r && r->fresh_remote_groups ? QString::number(cache_age) : unknown);
    return out;
}
#endif
