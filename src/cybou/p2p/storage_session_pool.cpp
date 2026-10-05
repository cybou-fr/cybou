// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/p2p/storage_session_pool.h>
#include <cybou/node_runtime.h>
#include <algorithm>

namespace cybou::p2p {
bool StorageSessionPool::Allows(const std::string& address) const
{
    return m_runtime.AdmitPeerAddress(address);
}
StorageSessionPool::Slot* StorageSessionPool::Acquire(const std::string& address,
    const uint16_t port, const StorageId& provider, const bool read)
{
    std::unique_lock lock{m_mutex};
    m_cv.wait(lock, [&] {
        return (!read || m_reads < 2) && std::none_of(m_slots.begin(), m_slots.end(), [&](const Slot& slot) {
            return slot.busy && slot.provider == provider;
        }) && std::any_of(m_slots.begin(), m_slots.end(), [](const Slot& slot) { return !slot.busy; });
    });
    auto it = std::find_if(m_slots.begin(), m_slots.end(), [&](const Slot& slot) {
        return !slot.busy && slot.address == address && slot.port == port && slot.provider == provider;
    });
    if (it == m_slots.end()) {
        it = std::find_if(m_slots.begin(), m_slots.end(), [](const Slot& slot) { return !slot.busy && !slot.session; });
        if (it == m_slots.end()) it = std::find_if(m_slots.begin(), m_slots.end(), [](const Slot& slot) { return !slot.busy; });
        it->session.reset();
        it->address = address;
        it->port = port;
        it->provider = provider;
    }
    it->busy = true;
    it->read = read;
    if (read) ++m_reads;
    return &*it;
}
} // namespace cybou::p2p
