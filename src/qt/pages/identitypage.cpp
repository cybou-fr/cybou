// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/identitypage.h>

#include <qt/cyboudesktopmodel.h>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

IdentityPage::IdentityPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(34, 32, 34, 32);
    layout->setSpacing(18);
    auto* heading = new QLabel{tr("Identity"), this};
    heading->setObjectName("pageTitle");
    layout->addWidget(heading);

    auto* card = new QFrame{this};
    card->setObjectName("card");
    auto* card_layout = new QVBoxLayout{card};
    card_layout->setContentsMargins(28, 26, 28, 26);
    card_layout->setSpacing(12);
    m_state_label = new QLabel{card};
    m_state_label->setObjectName("cardTitle");
    m_detail_label = new QLabel{card};
    m_detail_label->setObjectName("bodyText");
    m_detail_label->setWordWrap(true);
    m_create_button = new QPushButton{tr("Create identity"), card};
    m_create_button->setObjectName("primaryButton");
    m_create_button->setEnabled(false);
    m_create_button->setToolTip(tr("Account creation is not connected to the desktop yet."));
    card_layout->addWidget(m_state_label);
    card_layout->addWidget(m_detail_label);
    card_layout->addSpacing(8);
    card_layout->addWidget(m_create_button, 0, Qt::AlignLeft);
    layout->addWidget(card);
    layout->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    refresh();
}

void IdentityPage::refresh()
{
    const auto& status = m_model->status();
    if (status.has_identity) {
        m_state_label->setText(tr("Identity active"));
        m_detail_label->setText(status.account_id);
        m_create_button->hide();
        return;
    }
    m_state_label->setText(tr("No CYBOU identity"));
    m_detail_label->setText(tr("Your identity will be controlled by local keys and registered through a permissionless protocol operation."));
    m_create_button->setVisible(true);
    m_create_button->setEnabled(m_model->capabilities().account_creation);
}
