// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_SERVICEPLACEHOLDERPAGE_H
#define BITCOIN_QT_PAGES_SERVICEPLACEHOLDERPAGE_H

#include <QWidget>

#include <qt/cyboutheme.h>

#include <QStringList>

class ServicePlaceholderPage : public QWidget
{
public:
    ServicePlaceholderPage(const QString& title, const QString& description, CybouTheme::NavIcon icon,
        const QStringList& what_to_expect, const QString& dependencies, QWidget* parent = nullptr);
};

#endif // BITCOIN_QT_PAGES_SERVICEPLACEHOLDERPAGE_H
