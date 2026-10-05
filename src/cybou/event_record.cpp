// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Реализация локального журнала публично-безопасных node events.
#include <cybou/event_record.h>
#include <cybou/secret_file.h>
#include <chrono>
#include <set>
#include <sstream>
#include <stdexcept>
#include <openssl/rand.h>
namespace cybou {
namespace {
std::string Escape(const std::string& text) {
    std::string out;
    static constexpr char HEX[] = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 32 || c >= 127) { out += "\\u00"; out += HEX[c >> 4]; out += HEX[c & 15]; }
        else out += c;
    }
    return '"' + out + '"';
}
constexpr const char* NAMES[] = {
    "node_started","node_stopping","node_status","peer_connected","peer_disconnected",
    "peer_rejected","sync_started","sync_progress","sync_complete","sync_failed",
    "operation_received","operation_accepted","operation_rejected","operation_uncertain",
    "operation_finalized","block_produced","block_received","block_verified",
    "block_finalized","poa_safety_halt","storage_connected","storage_disconnected",
    "chunk_put","chunk_get","chunk_verify_failed","placement_created","placement_degraded",
    "placement_repaired","content_securing","content_protected","storage_audit_started",
    "storage_audit_failed","storage_audit_repaired","block_production_retry","chunk_missing",
};
}
EventWriter::EventWriter(const std::filesystem::path& path,EventLogMode mode) : m_file{OpenPrivateAppendFile(path)},m_mode{mode} {
    if (!m_file) throw std::runtime_error("cannot open event log");
    unsigned char random[16];
    if (RAND_bytes(random, sizeof random) != 1) { std::fclose(m_file);m_file=nullptr;throw std::runtime_error("event run id unavailable"); }
    static constexpr char HEX[] = "0123456789abcdef";
    for (auto b : random) { m_run += HEX[b >> 4]; m_run += HEX[b & 15]; }
}
void EventWriter::Observe(const NodeDiagnosticsSnapshot& d) {
    std::lock_guard lock{m_snapshot_mutex};
    std::map<std::string,PeerDiagnostics> peers;
    for (const auto& peer : d.peers) {
        peers.emplace(peer.endpoint,peer);
        if (!m_peers.contains(peer.endpoint)) {
            Write(NodeEvent::peer_connected,{{"peer",peer.endpoint},{"advertised_height",peer.advertised_height}});
        }
        const auto previous = m_peers.find(peer.endpoint);
        if (!peer.storage_id.empty() && (previous == m_peers.end() || previous->second.storage_id != peer.storage_id))
            Write(NodeEvent::storage_connected,{{"peer",peer.endpoint},{"storage_id",peer.storage_id}});
    }
    for (const auto& [endpoint,peer] : m_peers) if (!peers.contains(endpoint)) {
        Write(NodeEvent::peer_disconnected,{{"peer",endpoint}});
        if (!peer.storage_id.empty()) Write(NodeEvent::storage_disconnected,{{"storage_id",peer.storage_id}});
    }
    m_peers = std::move(peers);
    // node_status пишется только при изменении наблюдаемого состояния, а не на каждом тике.
    const EventFields status{{"network_binding",d.network_binding},{"node_type",d.node_type},
        {"poa_signer_active",d.poa_signer_active},{"height",d.height},
        {"tip",d.tip},{"state_root",d.state_root},{"peers",std::uint64_t{d.peers.size()}},
        {"storage_used",d.storage_used},{"storage_capacity",d.storage_capacity},{"safety_halted",d.safety_halted}};
    if (m_last_status && *m_last_status == status) return;
    Write(NodeEvent::node_status,status);
    if (d.safety_halted && !(m_last_status && m_last_halted)) Write(NodeEvent::poa_safety_halt);
    m_last_status = status;
    m_last_halted = d.safety_halted;
}
EventWriter::~EventWriter() { if(m_file)std::fclose(m_file); }
bool EventWriter::Good() const { std::lock_guard lock{m_mutex}; return m_file && std::ferror(m_file)==0; }
void EventWriter::Write(NodeEvent event, const EventFields& fields) {
    static const std::set<std::string> ALLOWED{
        "network_binding","node_type","poa_signer_active","height","tip","state_root","peers","storage_used",
        "storage_capacity","safety_halted","operation_id","block_id","storage_id",
        "chunk_id","peer","bytes","duration_ms","error_code","replicas","target","account_id","nonce",
        "nonce","account_id","base_height","advertised_height"};
    if (fields.size() > 24 || static_cast<size_t>(event) >= std::size(NAMES)) throw std::invalid_argument("invalid event");
    for (const auto& [key,value] : fields) {
        if (!ALLOWED.contains(key) || (std::holds_alternative<std::string>(value) && std::get<std::string>(value).size() > 256))
            throw std::invalid_argument("non-public or oversized event field");
    }
    std::lock_guard lock{m_mutex};
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream record;
    record << "{\"run_id\":" << Escape(m_run) << ",\"seq\":" << ++m_sequence
           << ",\"time_ms\":" << time << ",\"event\":" << Escape(NAMES[static_cast<size_t>(event)]);
    for (const auto& [key,value] : fields) {
        if(m_mode==EventLogMode::MINIMAL && (key=="account_id"||key=="nonce"||key=="peer"||key=="storage_id"||key=="chunk_id"||key=="operation_id"))continue;
        record << ',' << Escape(key) << ':';
        std::visit([&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T,std::string>) record << Escape(item);
            else if constexpr (std::is_same_v<T,bool>) record << (item ? "true" : "false");
            else record << item;
        },value);
    }
    record << "}\n";const auto bytes=record.str();
    if(std::fwrite(bytes.data(),1,bytes.size(),m_file)!=bytes.size()||std::fflush(m_file)!=0)throw std::runtime_error("event log write failed");
}
} // namespace cybou
