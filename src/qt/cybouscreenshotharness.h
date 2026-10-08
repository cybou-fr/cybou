// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Выделенный harness для создания скриншотов окон CYBOU без блокировки production UI.

#ifndef CYBOU_QT_CYBOUSCREENSHOTHARNESS_H
#define CYBOU_QT_CYBOUSCREENSHOTHARNESS_H

#include <QString>

class CybouMainWindow;

namespace cybou::gui {

/// \brief Запускает сценарий сохранения скриншотов фикстуры.
/// \param window Главное окно CYBOU.
/// \param directory Каталог для сохранения скриншотов.
void RunScreenshotHarness(CybouMainWindow* window, const QString& directory);

} // namespace cybou::gui

#endif // CYBOU_QT_CYBOUSCREENSHOTHARNESS_H
