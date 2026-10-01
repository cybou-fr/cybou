// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_P2P_INGRESS_BUDGET_H
#define CYBOU_P2P_INGRESS_BUDGET_H
#include <chrono>
#include <map>
#include <mutex>
#include <string>
namespace cybou::p2p {
/** Local, bounded CPU admission policy; never changes global Identity Authority. */
class IngressBudget {
public:
    enum class Work { CONNECTION, OPERATION };
    bool Admit(const std::string& address,Work work,size_t bytes=0,
        std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) {
        std::lock_guard lock(m_mutex);
        for(auto it=m_peers.begin();it!=m_peers.end();) {
            if(now-it->second.start>=std::chrono::seconds{60}) it=m_peers.erase(it); else ++it;
        }
        auto it=m_peers.find(address);
        if(it==m_peers.end()) {
            if(m_peers.size()>=4096) return false;
            it=m_peers.emplace(address,Window{now}).first;
        }
        auto& w=it->second;
        // Sliding refill with no idle credit beyond one small burst.
        const auto second=std::chrono::duration_cast<std::chrono::seconds>(now-w.start).count();
        if(second!=w.second) { w.second=second;w.operations_second=0;w.bytes_second=0;w.connections_second=0; }
        if(work==Work::CONNECTION) {
            if(w.connections_second>=4||w.connections>=60) return false;
            ++w.connections_second;++w.connections;return true;
        }
        if(w.operations_second>=8||w.operations>=120||bytes>(1U<<20)-w.bytes_second) return false;
        ++w.operations_second;++w.operations;w.bytes_second+=bytes;return true;
    }
private:
    struct Window {
        std::chrono::steady_clock::time_point start;
        int64_t second{0};size_t operations{0},operations_second{0},connections{0},connections_second{0},bytes_second{0};
    };
    std::mutex m_mutex;std::map<std::string,Window> m_peers;
};
}
#endif
