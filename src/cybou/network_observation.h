// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_NETWORK_OBSERVATION_H
#define CYBOU_NETWORK_OBSERVATION_H
#include <cybou/network_observation_history.h>
namespace cybou {
// Local presentation data only, never serialized. Local cache and remote groups
// retain their own ages/availability; this is not a globally atomic measurement.
// Consumers receive a shared_ptr<const ...>; reading it performs no network I/O.
struct NetworkObservationSnapshot {
    ObservationBytes32 network_binding{};
    int64_t observed_unix_ms{0};
    std::chrono::steady_clock::time_point captured_at{std::chrono::steady_clock::now()};
    p2p::ObservationRow local;
    p2p::ObservationGroupSnapshot remote;
    std::vector<NetworkObservationPoint> remote_history;
};
}
#endif
