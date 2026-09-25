// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_IDENTITYPAGE_H
#define BITCOIN_QT_PAGES_IDENTITYPAGE_H

#include <QWidget>

#include <qt/cyboudesktopmodel.h>

#include <QVector>

class CybouDesktopModel;
class QLabel;
class QPushButton;
class QWidget;

class IdentityPage : public QWidget
{
public:
    IdentityPage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_state_label;
    QLabel* m_detail_label;
    QLabel* m_active_details;
    QLabel* m_dev_warning;
    QLabel* m_chip_protected;
    QLabel* m_chip_ready;
    QPushButton* m_share_button;
    QPushButton* m_add_device_button;
    QPushButton* m_security_button;
    QPushButton* m_create_button;
    QPushButton* m_restore_button;
    QPushButton* m_claim_button;
    QWidget* m_phase_row;
    QWidget* m_steps;
    QVector<QLabel*> m_phases;
    QWidget* m_active_panel;
    QWidget* m_cards;
    QWidget* m_advanced;

    void refresh();
    void rebuildForState(CybouIdentityState state);
    void startIdentityFlow();
    void startRestoreFlow();
    void startNameClaimFlow();
};

#endif // BITCOIN_QT_PAGES_IDENTITYPAGE_H
