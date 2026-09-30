// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license.
#include <cybou/event_record.h>
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
    "block_finalized","poa_safety_halt","provider_connected","provider_disconnected",
    "chunk_put","chunk_get","chunk_verify_failed","placement_created","placement_degraded",
    "placement_repaired","content_securing","content_protected","storage_audit_started",
    "storage_audit_failed","storage_audit_repaired",
};
}
EventWriter::EventWriter(const std::filesystem::path& path) : m_file{path, std::ios::app} {
    if (!m_file) throw std::runtime_error("cannot open event log");
    unsigned char random[16];
    if (RAND_bytes(random, sizeof random) != 1) throw std::runtime_error("event run id unavailable");
    static constexpr char HEX[] = "0123456789abcdef";
    for (auto b : random) { m_run += HEX[b >> 4]; m_run += HEX[b & 15]; }
}
void EventWriter::Observe(const NodeDiagnosticsSnapshot& d) {
    std::lock_guard lock{m_snapshot_mutex};
    std::map<std::string,PeerDiagnostics> peers;
    for (const auto& peer : d.peers) {
        peers.emplace(peer.endpoint,peer);
        if (!m_peers.contains(peer.endpoint)) {
            Write(NodeEvent::peer_connected,{{"peer",peer.endpoint},{"advertised_height",peer.advertised_height},
                {"capabilities",peer.capabilities}});
            if (!peer.provider_id.empty()) Write(NodeEvent::provider_connected,{{"peer",peer.endpoint},{"provider_id",peer.provider_id}});
        }
    }
    for (const auto& [endpoint,peer] : m_peers) if (!peers.contains(endpoint)) {
        Write(NodeEvent::peer_disconnected,{{"peer",endpoint}});
        if (!peer.provider_id.empty()) Write(NodeEvent::provider_disconnected,{{"provider_id",peer.provider_id}});
    }
    m_peers = std::move(peers);
    Write(NodeEvent::node_status,{{"network_id",d.network_id},{"role",d.role},{"height",d.height},
        {"tip",d.tip},{"state_root",d.state_root},{"peers",std::uint64_t{d.peers.size()}},
        {"storage_used",d.storage_used},{"storage_capacity",d.storage_capacity},{"safety_halted",d.safety_halted}});
    if (d.safety_halted) Write(NodeEvent::poa_safety_halt);
}
bool EventWriter::Good() const { std::lock_guard lock{m_mutex}; return m_file.good(); }
void EventWriter::Write(NodeEvent event, const EventFields& fields) {
    static const std::set<std::string> ALLOWED{
        "network_id","role","height","tip","state_root","peers","storage_used",
        "storage_capacity","safety_halted","operation_id","block_id","provider_id",
        "chunk_id","peer","bytes","duration_ms","error_code","replicas","target","account_id","nonce",
        "nonce","account_id","base_height","advertised_height","capabilities"};
    if (fields.size() > 24 || static_cast<size_t>(event) >= std::size(NAMES)) throw std::invalid_argument("invalid event");
    for (const auto& [key,value] : fields) {
        if (!ALLOWED.contains(key) || (std::holds_alternative<std::string>(value) && std::get<std::string>(value).size() > 256))
            throw std::invalid_argument("non-public or oversized event field");
    }
    std::lock_guard lock{m_mutex};
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_file << "{\"v\":1,\"run_id\":" << Escape(m_run) << ",\"seq\":" << ++m_sequence
           << ",\"time_ms\":" << time << ",\"event\":" << Escape(NAMES[static_cast<size_t>(event)]);
    for (const auto& [key,value] : fields) {
        m_file << ',' << Escape(key) << ':';
        std::visit([&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T,std::string>) m_file << Escape(item);
            else if constexpr (std::is_same_v<T,bool>) m_file << (item ? "true" : "false");
            else m_file << item;
        },value);
    }
    m_file << "}\n"; m_file.flush();
}
} // namespace cybou
