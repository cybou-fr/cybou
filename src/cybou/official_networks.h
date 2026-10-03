// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_OFFICIAL_NETWORKS_H
#define CYBOU_OFFICIAL_NETWORKS_H

/// \file
/// \brief Verified official networks, жёстко собранные из public constants.

#include <cybou/network_genesis.h>
#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace cybou {

/// \brief Идентификатор официальной сети, поддерживаемой текущей сборкой.
enum class NetworkKind : uint8_t {
    /// \brief Рабочая DEV сеть с уже скомпилированными genesis и bootstrap locator.
    DEVNET = 0,
    /// \brief Зарезервированный профиль MAINNET; в текущей сборке не provisioned.
    MAINNET = 1,
};

/// \brief Известный rendezvous peer: обычный Full Node с transport-only TLS SPKI pin.
struct RendezvousLocator {
    /// \brief Числовой IP или hostname, используемый для первичного dial.
    std::string_view host;
    /// \brief TCP-порт CYBOU P2P listener.
    uint16_t port{0};
    /// \brief SHA-256 pin от SPKI TLS-сертификата для initial transport authentication.
    std::array<unsigned char, 32> tls_spki_sha256{};
};

/// \brief Verified official network: compiled NetworkID, signed immutable genesis и initial state.
struct OfficialNetwork {
    /// \brief Логический профиль официальной сети.
    NetworkKind kind{NetworkKind::DEVNET};
    /// \brief Человекочитаемое имя профиля, используемое CLI/UI.
    std::string_view name;
    /// \brief Криптографически проверенный immutable NetworkGenesis этой сети.
    VerifiedNetworkGenesis genesis;
    /// \brief Полное genesis state, чей state root совпадает с genesis.
    CybouState genesis_state;
    /// \brief Начальные rendezvous peers для формирования mesh; пустой span допустим только для непровижененной сети.
    std::span<const RendezvousLocator> rendezvous_locators;
};

/// \brief Возвращает verified official network по имени профиля; любые внешние файлы отвергаются.
/// \param name Имя compiled profile (`devnet`/`DEVNET`/`mainnet`/`MAINNET`).
/// \return Ссылка на неизменяемое singleton-описание сети.
/// \post Возвращаемый объект криптографически проверен и согласован с compiled constants.
/// \throws std::runtime_error Если профиль неизвестен или запрошенная сеть ещё не provisioned.
const OfficialNetwork& RequireOfficialNetwork(std::string_view name);
/// \brief Возвращает verified official network по enumerator-сети.
/// \param kind Перечислимый идентификатор официальной сети.
/// \return Ссылка на неизменяемое singleton-описание сети.
/// \throws std::runtime_error Если профиль в текущей сборке недоступен.
const OfficialNetwork& RequireOfficialNetwork(NetworkKind kind);


} // namespace cybou

#endif // CYBOU_OFFICIAL_NETWORKS_H
