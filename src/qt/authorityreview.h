// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_QT_AUTHORITYREVIEW_H
#define CYBOU_QT_AUTHORITYREVIEW_H

#include <cybou/storage_lease.h>
#include <vector>

class CybouDesktopModel;
class QWidget;

/** Reviews the exact prepared payouts; never signs or submits. Cancel is default. */
bool ReviewStorageSettlement(CybouDesktopModel* model, uint64_t period, uint64_t start,
    uint64_t end, const std::vector<cybou::StorageSettlementEntry>& entries, QWidget* parent = nullptr);

#endif
