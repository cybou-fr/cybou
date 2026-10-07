// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#include <qt/benchmarkreference.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QDateTime>
#include <cmath>

std::optional<CybouBenchmarkReference> CybouBenchmarkReference::Parse(const QByteArray& json)
{
    if (json.size()>1024*1024) return std::nullopt;
    const auto doc=QJsonDocument::fromJson(json);
    if (!doc.isObject()) return std::nullopt;
    const auto o=doc.object();
    if (o.value("result").toString()!=QStringLiteral("PASS")) return std::nullopt;
    const auto hash=[](const QString& s, int length) {
        return s.size()==length && QRegularExpression{QStringLiteral("^[0-9a-f]+$")}.match(s).hasMatch();
    };
    CybouBenchmarkReference r;
    r.run_id=o.value("run_id").toString();
    if (!QRegularExpression{QStringLiteral("^[0-9]{8}-[0-9]{6}$")}.match(r.run_id).hasMatch()) return std::nullopt;
    if (!QDateTime::fromString(r.run_id,QStringLiteral("yyyyMMdd-HHmmss")).isValid()) return std::nullopt;
    r.network_binding=o.value("network_binding").toString();
    if (!hash(r.network_binding,64)) return std::nullopt;
    r.profile=o.value("profile").toString();
    if (!o.value("co_located_wsl").isBool()) return std::nullopt;
    r.co_located_wsl=o.value("co_located_wsl").toBool();
    if (!QStringList{"mixed","files","mail","payments","system-locks"}.contains(r.profile)) return std::nullopt;
    r.clients=o.value("clients").toObject().size();
    r.replicas=o.value("replicas").toInt();
    r.file_size=o.value("file_size").toString();
    if (r.clients<=0 || r.replicas<1 || r.replicas>2 ||
        !QRegularExpression{QStringLiteral("^[1-9][0-9]*(B|KiB|MiB|GiB)$")}.match(r.file_size).hasMatch()) return std::nullopt;
    const auto provenance=o.value("provenance").toObject();
    r.revision=provenance.value("revision").toString();
    r.binary_sha256=provenance.value("windows_loadgen_sha256").toString();
    if (!hash(r.revision,40) || !hash(r.binary_sha256,64) || !provenance.value("dirty").isBool()) return std::nullopt;
    r.dirty=provenance.value("dirty").toBool();
    const auto count=[&](const char* key, quint64& n) {
        const auto value=o.value(key);
        const double d=value.toDouble(-1);
        if (!value.isDouble() || !std::isfinite(d) || d<0 || d>9007199254740991.0 || std::floor(d)!=d) return false;
        n=static_cast<quint64>(d); return true;
    };
    if (!count("attempted_operations",r.attempted) || !count("submitted_operations",r.submitted) ||
        !count("finalized_operations",r.finalized) || r.attempted<r.submitted || r.submitted!=r.finalized || !r.finalized) return std::nullopt;
    r.window_s=o.value("measurement_window_s").toDouble(-1);
    r.finalized_per_s=o.value("finalized_ops_per_s").toDouble(-1);
    if (!std::isfinite(r.window_s) || r.window_s<=0 || !std::isfinite(r.finalized_per_s) ||
        std::abs(r.finalized_per_s-r.finalized/r.window_s)>std::max(1e-6,r.finalized_per_s*1e-4)) return std::nullopt;
    const auto checks=o.value("checks").toArray();
    if (checks.isEmpty()) return std::nullopt;
    for (const auto& check : checks) {
        const auto row=check.toArray();
        if (row.size()!=3 || !row.at(1).isBool() || !row.at(1).toBool()) return std::nullopt;
    }
    return r;
}
