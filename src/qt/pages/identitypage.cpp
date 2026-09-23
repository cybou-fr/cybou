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

    // What will happen, step by step. Visible only before a creation starts;
    // the numbered list mirrors the protocol phases, nothing more.
    m_steps = new QWidget{card};
    auto* steps_layout = new QVBoxLayout{m_steps};
    steps_layout->setContentsMargins(0, 0, 0, 0);
    steps_layout->setSpacing(8);
    const QStringList steps{
        tr("Keys are generated on this device and stay local."),
        tr("The node performs AccountCreationWork — protocol anti-Sybil computation."),
        tr("The signed AccountCreateOp is broadcast to the validator set."),
        tr("A BFT finality certificate commits the account."),
        tr("SystemBalance is funded atomically from the OnboardingPool."),
    };
    for (int i = 0; i < steps.size(); ++i) {
        auto* step = new QLabel{QStringLiteral("%1. %2").arg(i + 1).arg(steps.at(i)), m_steps};
        step->setObjectName("bodyText");
        step->setWordWrap(true);
        steps_layout->addWidget(step);
    }
    m_steps->setVisible(false);
    card_layout->addWidget(m_steps);

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

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });

    // Two-column body: main state card on the left, protocol facts on the right.
    auto* content = new QHBoxLayout;
    content->setSpacing(16);
    content->addWidget(card, 3);

    auto* side = new QVBoxLayout;
    side->setSpacing(16);
    auto* how_card = new QFrame{this};
    how_card->setObjectName("card");
    auto* how_layout = new QVBoxLayout{how_card};
    how_layout->setContentsMargins(22, 20, 22, 20);
    how_layout->setSpacing(10);
    auto* how_title = new QLabel{tr("How identity works"), how_card};
    how_title->setObjectName("sectionTitle");
    how_layout->addWidget(how_title);
    const QStringList facts{
        tr("Your identity is controlled by keys that are generated and stored locally on this device."),
        tr("Registration is a permissionless protocol operation (AccountCreateOp) — no operator approval, no central activation."),
        tr("Protocol anti-Sybil work (AccountCreationWork) keeps mass registrations out."),
        tr("A successful creation automatically funds your SystemBalance from the OnboardingPool."),
    };
    for (const QString& fact : facts) {
        auto* bullet = new QLabel{QStringLiteral("\u2022 %1").arg(fact), how_card};
        bullet->setObjectName("bodyText");
        bullet->setWordWrap(true);
        how_layout->addWidget(bullet);
    }

    auto* econ_card = new QFrame{this};
    econ_card->setObjectName("card");
    auto* econ_layout = new QVBoxLayout{econ_card};
    econ_layout->setContentsMargins(22, 20, 22, 20);
    econ_layout->setSpacing(10);
    auto* econ_title = new QLabel{tr("Onboarding economics"), econ_card};
    econ_title->setObjectName("sectionTitle");
    econ_layout->addWidget(econ_title);
    const QStringList econ_facts{
        tr("DEV, Beta and Mainnet run separate economic parameters and a separate genesis."),
        tr("Beta balances do not carry over to Mainnet."),
        tr("The Mainnet onboarding bonus is frozen until aggregate Beta operational data exists."),
        tr("Services consume SystemBalance — there are no service-specific free credits."),
    };
    for (const QString& fact : econ_facts) {
        auto* bullet = new QLabel{QStringLiteral("\u2022 %1").arg(fact), econ_card};
        bullet->setObjectName("bodyText");
        bullet->setWordWrap(true);
        econ_layout->addWidget(bullet);
    }

    auto* dev_card = new QFrame{this};
    dev_card->setObjectName("card");
    auto* dev_layout = new QVBoxLayout{dev_card};
    dev_layout->setContentsMargins(22, 20, 22, 20);
    dev_layout->setSpacing(10);
    auto* dev_badge = new QLabel{tr("Development network"), dev_card};
    dev_badge->setObjectName("warningBadge");
    auto* dev_body = new QLabel{tr("CYBOU-DEV is a development network: balances have no real-world value and the chain may be reset as the protocol evolves."), dev_card};
    dev_body->setObjectName("mutedText");
    dev_body->setWordWrap(true);
    dev_layout->addWidget(dev_badge, 0, Qt::AlignLeft);
    dev_layout->addWidget(dev_body);

    side->addWidget(how_card);
    side->addWidget(econ_card);
    side->addWidget(dev_card);
    side->addStretch();
    content->addLayout(side, 2);
    layout->addLayout(content, 1);
    layout->addStretch();

    refresh();
}

void IdentityPage::rebuildForState(CybouIdentityState state)
{
    const bool creating = state != CybouIdentityState::None && state != CybouIdentityState::Active;
    const bool active = state == CybouIdentityState::Active;
    m_phase_row->setVisible(creating);
    m_steps->setVisible(state == CybouIdentityState::None);
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
            tr("AccountID: %1\nCreation height: %2\nNetwork: %3\nSystemBalance was funded atomically from the OnboardingPool at creation.")
                .arg(status.account_id)
                .arg(status.creation_height)
                .arg(status.network_name));
        return;
    }

    if (status.identity_state != CybouIdentityState::None) {
        m_state_label->setText(tr("Creating identity"));
        switch (status.identity_state) {
        case CybouIdentityState::CreatingKeys:
            m_detail_label->setText(tr("Keys are being generated on this device. They never leave it — losing them means losing the identity."));
            break;
        case CybouIdentityState::PerformingWork:
            m_detail_label->setText(tr("The node is performing AccountCreationWork — protocol anti-Sybil computation. One identity costs real work, so mass registrations stay out."));
            break;
        case CybouIdentityState::Broadcasting:
            m_detail_label->setText(tr("The signed AccountCreateOp is being broadcast to the validator set."));
            break;
        case CybouIdentityState::WaitingForFinality:
            m_detail_label->setText(tr("Waiting for a BFT finality certificate. The account becomes real only once validators commit the block — this page flips to Active at that point."));
            break;
        case CybouIdentityState::Active:
        case CybouIdentityState::None:
            break;
        }
        return;
    }

    m_state_label->setText(tr("No CYBOU identity"));
    const bool pending = m_model->identityCreationRequestPending();
    m_detail_label->setText(pending
        ? tr("Creation requested. The node will drive the protocol phases — keys, anti-Sybil work, broadcast and BFT finality — and this page will follow them.")
        : tr("Your identity will be controlled by local keys and registered through a permissionless protocol operation with protocol anti-Sybil work."));
    m_create_button->setEnabled(!pending && m_model->capabilities().account_creation);
    m_create_button->setText(pending ? tr("Creation requested…") : tr("Create identity"));
    m_create_button->setToolTip(pending
        ? tr("Waiting for the node to pick up the request.")
        : tr("Identity creation is not connected to the desktop yet."));
}
