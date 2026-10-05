// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see COPYING.
#ifndef CYBOU_P2P_STORAGE_SESSION_POOL_H
#define CYBOU_P2P_STORAGE_SESSION_POOL_H
#include <cybou/p2p/peer_manager.h>
#include <condition_variable>
#include <mutex>

namespace cybou::p2p {
// Four cached ordinary sessions, exclusively leased per request. Independent
// from the mesh manager lock; the same dial policy and protocol apply.
class StorageSessionPool {
    struct Slot {
        boost::asio::io_context io;
        std::unique_ptr<PeerSession> session;
        std::string address;
        uint16_t port{0};
        StorageId provider{};
        bool busy{false};
        bool read{false};
    };
public:
    explicit StorageSessionPool(CybouNodeRuntime& runtime) : m_runtime{runtime} {}
    template <typename F>
    auto Run(const std::string& address, uint16_t port, const StorageId& provider, F&& request, bool read = false)
        -> std::invoke_result_t<F, PeerSession&>
    {
        using Result = std::invoke_result_t<F, PeerSession&>;
        Slot* slot = Acquire(address, port, provider, read);
        struct Release {
            StorageSessionPool& pool;
            Slot& slot;
            ~Release() {
                if (slot.session && !slot.session->Peer()) slot.session.reset();
                {
                    std::lock_guard lock{pool.m_mutex};
                    if (slot.read) --pool.m_reads;
                    slot.busy = false;
                }
                pool.m_cv.notify_all();
            }
        } release{*this, *slot};
        if (!Allows(address)) { slot->session.reset(); return Result{}; }
        if (!slot->session) {
            PeerConnectStatus status;
            slot->session = DialPeer(m_runtime, slot->io, address, port, status);
            if (!slot->session) return Result{};
            const auto proven = slot->session->ProveStorageIdentity();
            if (!proven || *proven != provider) { slot->session.reset(); return Result{}; }
        }
        try { return request(*slot->session); }
        catch (...) { slot->session.reset(); throw; }
    }
private:
    bool Allows(const std::string& address) const;
    Slot* Acquire(const std::string& address, uint16_t port, const StorageId& provider, bool read);
    CybouNodeRuntime& m_runtime;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::array<Slot, 4> m_slots;
    size_t m_reads{0};
};
} // namespace cybou::p2p
#endif
