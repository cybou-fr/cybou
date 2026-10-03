// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_ECONOMICS_H
#define CYBOU_ECONOMICS_H

/// \file
/// \brief Канонические константы экономики, используемые текущей реализацией DEVNET.

#include <cstdint>
#include <string_view>

namespace cybou {
/// \brief Метка единственной genesis allocation Центральной Authority до её claim.
/// \details Это текстовый label внутри genesis allocation; он не даёт отдельной роли и не участвует в консенсусе.
inline constexpr std::string_view CENTRAL_AUTHORITY_NAME{"cybou"};
/// \brief Начальный размер DEV onboarding pool в spendable CYBOU.
/// \details Единица измерения — целые CYBOU без дробной части; 100 000 000 используется только как стартовый размер пула DEVNET.
inline constexpr uint64_t DEV_ONBOARDING_POOL{100'000'000};
} // namespace cybou

#endif // CYBOU_ECONOMICS_H
