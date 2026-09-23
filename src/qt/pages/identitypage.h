// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_IDENTITYPAGE_H
#define BITCOIN_QT_PAGES_IDENTITYPAGE_H

#include <QWidget>

class CybouDesktopModel;
class QLabel;
class QPushButton;

class IdentityPage : public QWidget
{
public:
    explicit IdentityPage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_state_label;
    QLabel* m_detail_label;
    QPushButton* m_create_button;

    void refresh();
};

#endif // BITCOIN_QT_PAGES_IDENTITYPAGE_H
