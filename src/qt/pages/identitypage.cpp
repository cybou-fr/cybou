// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/identitypage.h>

#include <qt/cyboutheme.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString phaseName(CybouIdentityState state)
{
    switch (state) {
    case CybouIdentityState::CreatingKeys: return IdentityPage::tr("Creating keys");
    case CybouIdentityState::PerformingWork: return IdentityPage::tr("Performing work");
    case CybouIdentityState::Broadcasting: return IdentityPage::tr("Broadcasting");
    case CybouIdentityState::WaitingForFinality: return IdentityPage::tr("Waiting for finality");
    case CybouIdentityState::Active: return IdentityPage::tr("Active");
    case CybouIdentityState::None: break;
    }
    return {};
}

} // namespace

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

    // Creation phase flow. Invisible until a creation is actually requested
    // by the backend; the UI never advances these phases on its own.
    auto* phases_row = new QHBoxLayout;
    phases_row->setSpacing(10);
    const QVector<CybouIdentityState> flow{
        CybouIdentityState::CreatingKeys,
        CybouIdentityState::PerformingWork,
        CybouIdentityState::Broadcasting,
        CybouIdentityState::WaitingForFinality,
        CybouIdentityState::Active,
    };
    for (const auto state : flow) {
        auto* phase = new QLabel{phaseName(state), card};
        phase->setObjectName("phaseLabel");
        m_phases.append(phase);
        phases_row->addWidget(phase);
        if (state != CybouIdentityState::Active) {
            auto* arrow = new QLabel{QStringLiteral("→"), card};
            arrow->setObjectName("phaseLabel");
            phases_row->addWidget(arrow);
        }
    }
    phases_row->addStretch();
    m_phase_row = new QWidget{card}; // container to toggle the whole flow
    m_phase_row->setLayout(phases_row);
    m_phase_row->setVisible(false);
    card_layout->addWidget(m_phase_row);

    m_state_label = new QLabel{card};
    m_state_label->setObjectName("cardTitle");
    m_detail_label = new QLabel{card};
    m_detail_label->setObjectName("bodyText");
    m_detail_label->setWordWrap(true);
    card_layout->addWidget(m_state_label);
    card_layout->addWidget(m_detail_label);

    // Active identity facts (populated only from real backend state).
    m_active_details = new QLabel{card};
    m_active_details->setObjectName("bodyText");
    m_active_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_active_details->setWordWrap(true);
    m_active_details->setVisible(false);
    card_layout->addWidget(m_active_details);

    m_dev_warning = new QLabel{tr("Development network balance. No Mainnet value."), card};
    m_dev_warning->setObjectName("warningBadge");
    m_dev_warning->setVisible(false);
    card_layout->addWidget(m_dev_warning, 0, Qt::AlignLeft);

    m_create_button = new QPushButton{tr("Create identity"), card};
    m_create_button->setObjectName("primaryButton");
    m_create_button->setProperty("cybouId", "createIdentity");
    m_create_button->setEnabled(false);
    m_create_button->setToolTip(tr("Identity creation is not connected to the desktop yet."));
    connect(m_create_button, &QPushButton::clicked, this, [this] { m_model->requestCreateIdentity(); });
    card_layout->addSpacing(8);
    card_layout->addWidget(m_create_button, 0, Qt::AlignLeft);
    layout->addWidget(card);
    layout->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    refresh();
}

void IdentityPage::rebuildForState(CybouIdentityState state)
{
    const bool creating = state != CybouIdentityState::None && state != CybouIdentityState::Active;
    const bool active = state == CybouIdentityState::Active;
    m_phase_row->setVisible(creating);
    m_active_details->setVisible(active);
    m_dev_warning->setVisible(active);
    m_create_button->setVisible(!active);

    const QVector<CybouIdentityState> flow{
        CybouIdentityState::CreatingKeys,
        CybouIdentityState::PerformingWork,
        CybouIdentityState::Broadcasting,
        CybouIdentityState::WaitingForFinality,
        CybouIdentityState::Active,
    };
    for (int index = 0; index < m_phases.size() && index < flow.size(); ++index) {
        const auto phase_state = flow.at(index);
        QString object_name = QStringLiteral("phaseLabel");
        if (active || phase_state == state) {
            object_name = QStringLiteral("phaseLabelActive");
        } else if (flow.indexOf(state) > index) {
            object_name = QStringLiteral("phaseLabelDone");
        }
        m_phases.at(index)->setObjectName(object_name);
        // Force stylesheet re-evaluation after objectName change.
        m_phases.at(index)->style()->unpolish(m_phases.at(index));
        m_phases.at(index)->style()->polish(m_phases.at(index));
    }
}

void IdentityPage::refresh()
{
    const auto& status = m_model->status();
    rebuildForState(status.identity_state);

    if (status.identity_state == CybouIdentityState::Active) {
        m_state_label->setText(tr("Identity active"));
        m_detail_label->setText(tr("Your CYBOU identity is registered on the network."));
        m_active_details->setText(
            tr("AccountID: %1\nCreation height: %2\nNetwork: %3")
                .arg(status.account_id)
                .arg(status.creation_height)
                .arg(status.network_name));
        return;
    }

    if (status.identity_state != CybouIdentityState::None) {
        m_state_label->setText(tr("Creating identity"));
        m_detail_label->setText(tr("Your identity is being registered through a permissionless protocol operation."));
        return;
    }

    m_state_label->setText(tr("No CYBOU identity"));
    m_detail_label->setText(tr("Your identity will be controlled by local keys and registered through a permissionless protocol operation with protocol anti-Sybil work."));
    m_create_button->setEnabled(m_model->capabilities().account_creation);
}
