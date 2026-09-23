// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_CHAINTYPE_H
#define BITCOIN_UTIL_CHAINTYPE_H

#include <optional>
#include <string>

// CYBOU has a single sovereign development network (CYBOU-DEV, ChainType::MAIN).
// Inherited Bitcoin networks (testnet3, testnet4, signet, regtest) were removed;
// see spec/bitcoin_code_removal.yaml.
enum class ChainType {
    MAIN,
};

std::string ChainTypeToString(ChainType chain);

std::optional<ChainType> ChainTypeFromString(std::string_view chain);

#endif // BITCOIN_UTIL_CHAINTYPE_H
