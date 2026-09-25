// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/identitypage.h>

#include <qt/cyboutheme.h>
#include <qt/cybouui.h>
#include <cybou/identity_service.h>

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QStyle>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>

#include <filesystem>
#include <functional>
#include <utility>

using namespace CybouUi;

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

/** Section card with a tinted chip header and a chevron affordance. */
QWidget* SectionCard(Glyph glyph, Tint tint, const QString& title, const QString& description,
    const QString& button_text, std::function<void()> on_button, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->addWidget(Chip(glyph, tint, card, 38, 19));
    auto* heading = new QLabel{title, card};
    heading->setObjectName(QStringLiteral("serviceTitle"));
    header->addWidget(heading, 0, Qt::AlignVCenter);
    header->addStretch();
    auto* chevron = new QLabel{card};
    chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
    header->addWidget(chevron, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    auto* body = MutedText(description, card);
    layout->addWidget(body);
    layout->addStretch();

    if (!button_text.isEmpty()) {
        auto* button = new QPushButton{button_text, card};
        button->setObjectName(QStringLiteral("secondaryButton"));
        if (on_button) QObject::connect(button, &QPushButton::clicked, parent, std::move(on_button));
        layout->addWidget(button, 0, Qt::AlignLeft);
    }
    return card;
}

} // namespace

IdentityPage::IdentityPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(18);

    auto* left = new QVBoxLayout;
    left->setSpacing(16);

    // ---- Hero: identity state, chips and primary actions ------------------
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_outer = new QHBoxLayout{hero};
    hero_outer->setContentsMargins(30, 26, 30, 26);
    hero_outer->setSpacing(20);
    auto* hero_layout = new QVBoxLayout;
    hero_layout->setSpacing(10);
    hero_layout->addWidget(Eyebrow(tr("YOUR IDENTITY"), hero));

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
        auto* phase = new QLabel{phaseName(state), hero};
        phase->setObjectName(QStringLiteral("phaseLabel"));
        m_phases.append(phase);
        phases_row->addWidget(phase);
        if (state != CybouIdentityState::Active) {
            auto* arrow = new QLabel{QStringLiteral("\u2192"), hero};
            arrow->setObjectName(QStringLiteral("phaseLabel"));
            phases_row->addWidget(arrow);
        }
    }
    phases_row->addStretch();
    m_phase_row = new QWidget{hero}; // container to toggle the whole flow
    m_phase_row->setLayout(phases_row);
    m_phase_row->setVisible(false);
    hero_layout->addWidget(m_phase_row);

    m_state_label = HeroTitle({}, hero, true);
    hero_layout->addWidget(m_state_label);
    m_detail_label = HeroSubtitle({}, hero);
    hero_layout->addWidget(m_detail_label);

    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    m_chip_protected = Pill(tr("Protected"), Tint::Mint, hero);
    m_chip_ready = Pill(tr("Ready to use"), Tint::Blue, hero);
    chips->addWidget(m_chip_protected);
    chips->addWidget(m_chip_ready);
    chips->addStretch();
    hero_layout->addLayout(chips);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_share_button = new QPushButton{tr("Share identity"), hero};
    m_share_button->setObjectName(QStringLiteral("primaryButton"));
    m_share_button->setIcon(QIcon{glyphPixmap(Glyph::Share, {16, 16}, QColor{0xffffff})});
    connect(m_share_button, &QPushButton::clicked, this, [this] {
        const QString account = m_model->status().account_id;
        if (account.isEmpty()) return;
        QGuiApplication::clipboard()->setText(account);
        m_share_button->setText(tr("Copied!"));
        QTimer::singleShot(1500, this, [this] { m_share_button->setText(tr("Share identity")); });
    });
    m_claim_button = new QPushButton{tr("Manage identity"), hero};
    m_claim_button->setObjectName(QStringLiteral("secondaryButton"));
    m_claim_button->setIcon(QIcon{glyphPixmap(Glyph::Compose, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    connect(m_claim_button, &QPushButton::clicked, this, [this] { startNameClaimFlow(); });
    m_add_device_button = new QPushButton{tr("Add device"), hero};
    m_add_device_button->setObjectName(QStringLiteral("secondaryButton"));
    m_add_device_button->setIcon(QIcon{glyphPixmap(Glyph::Plus, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    connect(m_add_device_button, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Planned"),
            tr("Device authorization (Ed25519 + ML-DSA-44) arrives with the portable vault sync. For now this device is the only authorized one."));
    });
    m_security_button = new QPushButton{tr("Security settings"), hero};
    m_security_button->setObjectName(QStringLiteral("secondaryButton"));
    m_security_button->setIcon(QIcon{glyphPixmap(Glyph::ShieldCheck, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    connect(m_security_button, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Planned"),
            tr("A dedicated security surface (vault password, key rotation, active sessions) is planned. Recovery and restore stay on this page."));
    });
    actions->addWidget(m_share_button);
    actions->addWidget(m_claim_button);
    actions->addWidget(m_add_device_button);
    actions->addWidget(m_security_button);
    actions->addStretch();
    hero_layout->addLayout(actions);

    m_dev_warning = new QLabel{tr("Development network balance. No Mainnet value."), hero};
    m_dev_warning->setObjectName(QStringLiteral("warningBadge"));
    m_dev_warning->setVisible(false);
    hero_layout->addWidget(m_dev_warning, 0, Qt::AlignLeft);

    // What will happen, step by step. Visible only before a creation starts;
    // the numbered list mirrors the protocol phases, nothing more.
    m_steps = new QWidget{hero};
    auto* steps_layout = new QVBoxLayout{m_steps};
    steps_layout->setContentsMargins(0, 6, 0, 0);
    steps_layout->setSpacing(8);
    const QStringList steps{
        tr("Keys are generated on this device and stay local."),
        tr("The node performs AccountCreationWork \u2014 protocol anti-Sybil computation."),
        tr("The signed AccountCreateOp is broadcast to the validator set."),
        tr("A BFT finality certificate commits the account."),
        tr("SystemBalance is funded atomically from the OnboardingPool."),
    };
    for (int i = 0; i < steps.size(); ++i) {
        auto* step = new QLabel{QStringLiteral("%1. %2").arg(i + 1).arg(steps.at(i)), m_steps};
        step->setObjectName(QStringLiteral("bodyText"));
        step->setWordWrap(true);
        steps_layout->addWidget(step);
    }
    m_steps->setVisible(false);
    hero_layout->addWidget(m_steps);

    m_create_button = new QPushButton{tr("Create identity"), hero};
    m_create_button->setObjectName(QStringLiteral("primaryButton"));
    m_create_button->setProperty("cybouId", "createIdentity");
    m_create_button->setEnabled(false);
    m_create_button->setToolTip(tr("Create a portable recovery vault before network submission."));
    connect(m_create_button, &QPushButton::clicked, this, [this] { startIdentityFlow(); });
    m_restore_button = new QPushButton{tr("Restore identity"), hero};
    m_restore_button->setObjectName(QStringLiteral("primaryButton"));
    m_restore_button->setProperty("cybouId", "restoreIdentity");
    connect(m_restore_button, &QPushButton::clicked, this, [this] { startRestoreFlow(); });
    auto* onboarding_actions = new QHBoxLayout;
    onboarding_actions->addWidget(m_create_button);
    onboarding_actions->addWidget(m_restore_button);
    onboarding_actions->addStretch();
    hero_layout->addLayout(onboarding_actions);
    hero_layout->addStretch();
    hero_outer->addLayout(hero_layout, 1);

    // Large avatar emblem with a camera badge (sketch), active state only.
    m_avatar_emblem = new QWidget{hero};
    m_avatar_emblem->setFixedSize(116, 116);
    auto* emblem_disc = new QLabel{m_avatar_emblem};
    emblem_disc->setFixedSize(116, 116);
    emblem_disc->setAlignment(Qt::AlignCenter);
    emblem_disc->setStyleSheet(QStringLiteral(
        "background: qradialgradient(cx:0.5, cy:0.4, radius:0.9, stop:0 #d9f6e7, stop:1 #b9ecd6);"
        "border-radius: 58px;"));
    emblem_disc->setPixmap(glyphPixmap(Glyph::User, {54, 54}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    auto* camera_badge = new QLabel{m_avatar_emblem};
    camera_badge->setFixedSize(30, 30);
    camera_badge->move(82, 80);
    camera_badge->setAlignment(Qt::AlignCenter);
    camera_badge->setStyleSheet(QStringLiteral(
        "background: #ffffff; border: 1px solid %1; border-radius: 15px;")
        .arg(CybouTheme::color(CybouTheme::BORDER).name()));
    camera_badge->setPixmap(glyphPixmap(Glyph::Camera, {15, 15}, CybouTheme::color(CybouTheme::TEXT_SECONDARY)));
    camera_badge->setToolTip(tr("Profile images are planned; your identity is identified by its name and AccountID."));
    hero_outer->addWidget(m_avatar_emblem, 0, Qt::AlignVCenter);
    m_avatar_emblem->setVisible(false);

    left->addWidget(hero);

    // ---- Active identity: recovery / devices / trusted contacts -----------
    m_cards = new QWidget{this};
    auto* cards_layout = new QHBoxLayout{m_cards};
    cards_layout->setContentsMargins(0, 0, 0, 0);
    cards_layout->setSpacing(14);

    auto* recovery = Card(m_cards);
    auto* recovery_layout = new QVBoxLayout{recovery};
    recovery_layout->setContentsMargins(22, 18, 22, 18);
    recovery_layout->setSpacing(10);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Key, Tint::Mint, recovery, 38, 19));
        auto* heading = new QLabel{tr("Recovery"), recovery};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{recovery};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        recovery_layout->addLayout(header);
        recovery_layout->addWidget(MutedText(tr("Your 24-word recovery phrase keeps your identity safe and lets you restore it on any clean machine."), recovery));
        auto* phrase_row = new QHBoxLayout;
        auto* phrase_icon = new QLabel{recovery};
        phrase_icon->setPixmap(glyphPixmap(Glyph::FileText, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
        phrase_row->addWidget(phrase_icon, 0, Qt::AlignVCenter);
        auto* phrase_label = new QLabel{tr("Recovery phrase"), recovery};
        phrase_label->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
            .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
        phrase_row->addWidget(phrase_label);
        phrase_row->addStretch();
        phrase_row->addWidget(Pill(tr("Backed up and protected"), Tint::Mint, recovery), 0, Qt::AlignVCenter);
        recovery_layout->addLayout(phrase_row);
        recovery_layout->addStretch();
        auto* options = new QPushButton{tr("View recovery options"), recovery};
        options->setObjectName(QStringLiteral("secondaryButton"));
        connect(options, &QPushButton::clicked, this, [this] { startRestoreFlow(); });
        recovery_layout->addWidget(options, 0, Qt::AlignLeft);
    }
    cards_layout->addWidget(recovery, 1);

    auto* devices = Card(m_cards);
    auto* devices_layout = new QVBoxLayout{devices};
    devices_layout->setContentsMargins(22, 18, 22, 18);
    devices_layout->setSpacing(10);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Monitor, Tint::Blue, devices, 38, 19));
        auto* heading = new QLabel{tr("Devices"), devices};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{devices};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        devices_layout->addLayout(header);
        devices_layout->addWidget(MutedText(tr("All devices using your identity are synced and protected."), devices));
        auto* this_device = ActivityRow(Glyph::Monitor, Tint::Indigo, QSysInfo::machineHostName(),
            tr("This device \u00b7 Active now"), {}, devices, true);
        devices_layout->addWidget(this_device);
        devices_layout->addStretch();
        auto* manage = new QPushButton{tr("Manage devices"), devices};
        manage->setObjectName(QStringLiteral("secondaryButton"));
        connect(manage, &QPushButton::clicked, this, [this] {
            QMessageBox::information(this, tr("Planned"),
                tr("Device authorization arrives with the portable vault sync; this node is the only authorized device today."));
        });
        devices_layout->addWidget(manage, 0, Qt::AlignLeft);
    }
    cards_layout->addWidget(devices, 1);

    auto* contacts = SectionCard(Glyph::Users, Tint::Violet, tr("Trusted contacts"),
        tr("Share your identity with people you trust so they can find and message you on CYBOU."),
        tr("Manage trusted contacts"), {}, m_cards);
    cards_layout->addWidget(contacts, 1);
    left->addWidget(m_cards);

    // ---- Advanced details --------------------------------------------------
    m_advanced = Card(this);
    auto* advanced_layout = new QVBoxLayout{m_advanced};
    advanced_layout->setContentsMargins(22, 18, 22, 18);
    advanced_layout->setSpacing(10);
    {
        auto* header = new QHBoxLayout;
        auto* gear = new QLabel{m_advanced};
        gear->setPixmap(glyphPixmap(Glyph::Gear, {18, 18}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
        header->addWidget(gear, 0, Qt::AlignVCenter);
        auto* heading = new QLabel{tr("Advanced details"), m_advanced};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addSpacing(8);
        header->addWidget(MutedText(tr("Technical information about your identity. You won't need this for everyday use."), m_advanced), 1);
        auto* chevron = new QLabel{m_advanced};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        advanced_layout->addLayout(header);
        m_active_details = new QLabel{m_advanced};
        m_active_details->setObjectName(QStringLiteral("bodyText"));
        m_active_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_active_details->setWordWrap(true);
        advanced_layout->addWidget(m_active_details);
    }
    left->addWidget(m_advanced);
    left->addStretch();
    root->addLayout(left, 3);

    // ---- Right panel: identity status --------------------------------------
    m_active_panel = new QWidget{this};
    auto* panel_layout = new QVBoxLayout{m_active_panel};
    panel_layout->setContentsMargins(0, 0, 0, 0);
    panel_layout->setSpacing(16);
    auto* status_card = Card(m_active_panel);
    auto* status_layout = new QVBoxLayout{status_card};
    status_layout->setContentsMargins(22, 18, 22, 18);
    status_layout->setSpacing(4);
    {
        auto* header = new QHBoxLayout;
        auto* heading = SectionTitle(tr("Identity status"), status_card);
        header->addWidget(heading);
        header->addStretch();
        header->addWidget(Pill(tr("Protected"), Tint::Mint, status_card), 0, Qt::AlignVCenter);
        status_layout->addLayout(header);
        status_layout->addSpacing(6);
        auto add_status = [this, status_card, status_layout](Glyph glyph, const QString& title, const QString& sub) {
            auto* row = ActivityRow(glyph, Tint::Mint, title, sub, {}, status_card, true);
            status_layout->addWidget(row);
        };
        add_status(Glyph::User, tr("Your identity is active"), tr("All services are available"));
        add_status(Glyph::Lock, tr("Encrypted and private"), tr("Only you control your identity"));
        add_status(Glyph::Monitor, tr("Ready across your devices"), tr("Use your identity on all your devices"));
    }
    panel_layout->addWidget(status_card);

    // Dev facts move below the status card while the identity surface exists.
    auto* how_card = Card(m_active_panel);
    auto* how_layout = new QVBoxLayout{how_card};
    how_layout->setContentsMargins(22, 18, 22, 18);
    how_layout->setSpacing(8);
    {
        how_layout->addWidget(SectionTitle(tr("How identity works"), how_card));
        const QStringList facts{
            tr("Your identity is controlled by keys that are generated and stored locally on this device."),
            tr("Registration is a permissionless protocol operation (AccountCreateOp) \u2014 no operator approval, no central activation."),
            tr("Protocol anti-Sybil work (AccountCreationWork) keeps mass registrations out."),
            tr("A successful creation automatically funds your SystemBalance from the OnboardingPool."),
        };
        for (const QString& fact : facts) {
            auto* bullet = new QLabel{QStringLiteral("\u2022 %1").arg(fact), how_card};
            bullet->setObjectName(QStringLiteral("bodyText"));
            bullet->setWordWrap(true);
            how_layout->addWidget(bullet);
        }
    }
    panel_layout->addWidget(how_card);
    panel_layout->addStretch();
    root->addWidget(m_active_panel, 2);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::identityCreationFailed, this, [this](const QString& reason) {
        QMessageBox::warning(this, tr("Identity creation failed"), reason);
    });
    connect(m_model, &CybouDesktopModel::nameClaimFailed, this, [this](const QString& reason) {
        QMessageBox::warning(this, tr("Name claim failed"), reason);
    });

    refresh();
}

void IdentityPage::startIdentityFlow()
{
    auto* service = m_model->identityService();
    if (!service) return;
    const auto vault_path = service->GetStoragePath();
    if (!vault_path) return;

    if (std::filesystem::exists(*vault_path)) {
        if (!service->GetKeyStore().HasKey()) {
            bool accepted{false};
            QString password = QInputDialog::getText(this, tr("Unlock identity"),
                tr("Vault password"), QLineEdit::Password, {}, &accepted);
            if (!accepted) return;
            const bool loaded = m_model->requestUnlockIdentity(password);
            password.fill(QChar{0});
            if (!loaded) {
                QMessageBox::warning(this, tr("Cannot unlock identity"),
                    tr("The password is incorrect or the vault is damaged."));
                return;
            }
            if (service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE) return;
        }
        m_model->requestCreateIdentity({});
        return;
    }

    bool accepted{false};
    QString password = QInputDialog::getText(this, tr("Create identity"),
        tr("Set a vault password (at least 12 characters)"), QLineEdit::Password, {}, &accepted);
    if (!accepted) return;
    if (password.size() < 12) {
        password.fill(QChar{0});
        QMessageBox::warning(this, tr("Weak vault password"), tr("Use at least 12 characters."));
        return;
    }
    QString confirmation = QInputDialog::getText(this, tr("Confirm vault password"),
        tr("Enter the password again"), QLineEdit::Password, {}, &accepted);
    const bool matches = accepted && password == confirmation;
    confirmation.fill(QChar{0});
    if (!matches) {
        password.fill(QChar{0});
        if (accepted) QMessageBox::warning(this, tr("Passwords differ"), tr("The passwords did not match."));
        return;
    }

    const auto words = service->PrepareNewIdentity();
    if (!words) {
        password.fill(QChar{0});
        QMessageBox::warning(this, tr("Cannot prepare identity"), tr("The local identity material could not be generated."));
        return;
    }
    QStringList numbered_words;
    for (size_t i{0}; i < words->size(); ++i) {
        numbered_words << QStringLiteral("%1. %2").arg(i + 1).arg(QString::fromStdString((*words)[i]));
    }
    const auto decision = QMessageBox::question(this, tr("Write down your recovery words"),
        tr("Write these 24 words down in order. They recover your identity on a clean machine.\n\n%1\n\nContinue after you have saved them privately.")
            .arg(numbered_words.join(QLatin1Char('\n'))),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    if (decision != QMessageBox::Ok) {
        service->DiscardPreparedIdentity();
        password.fill(QChar{0});
        return;
    }
    for (const size_t index : {size_t{3}, size_t{16}}) {
        QString answer = QInputDialog::getText(this, tr("Confirm recovery words"),
            tr("Enter word %1").arg(index + 1), QLineEdit::Normal, {}, &accepted);
        const bool correct = accepted && answer == QString::fromStdString((*words)[index]);
        answer.fill(QChar{0});
        if (!correct) {
            service->DiscardPreparedIdentity();
            password.fill(QChar{0});
            if (accepted) QMessageBox::warning(this, tr("Recovery words differ"), tr("Start again and record the words in order."));
            return;
        }
    }
    m_model->requestCreateIdentity(password);
    password.fill(QChar{0});
}

void IdentityPage::startRestoreFlow()
{
    if (!m_model->identityService()) return;
    bool accepted{false};
    QString phrase = QInputDialog::getMultiLineText(this, tr("Restore identity"),
        tr("Enter your 24 recovery words in order"), {}, &accepted);
    if (!accepted) return;
    QString password = QInputDialog::getText(this, tr("Recovery vault"),
        tr("Set a vault password (at least 12 characters)"), QLineEdit::Password, {}, &accepted);
    if (!accepted) {
        phrase.fill(QChar{0});
        return;
    }
    if (password.size() < 12) {
        phrase.fill(QChar{0});
        password.fill(QChar{0});
        QMessageBox::warning(this, tr("Weak vault password"), tr("Use at least 12 characters."));
        return;
    }
    QString confirmation = QInputDialog::getText(this, tr("Confirm vault password"),
        tr("Enter the password again"), QLineEdit::Password, {}, &accepted);
    const bool matches = accepted && password == confirmation;
    confirmation.fill(QChar{0});
    if (!matches) {
        phrase.fill(QChar{0});
        password.fill(QChar{0});
        if (accepted) QMessageBox::warning(this, tr("Passwords differ"), tr("The passwords did not match."));
        return;
    }
    const bool started = m_model->requestRestoreIdentity(phrase, password);
    phrase.fill(QChar{0});
    password.fill(QChar{0});
    if (!started) QMessageBox::warning(this, tr("Invalid recovery phrase"),
        tr("Enter exactly 24 valid words in their original order."));
}

void IdentityPage::startNameClaimFlow()
{
    bool accepted{false};
    QString name = QInputDialog::getText(this, tr("Claim a .cybou name"),
        tr("Enter the lowercase label (5\u201332 ASCII characters, without .cybou)"),
        QLineEdit::Normal, {}, &accepted);
    if (!accepted) return;
    QString password = QInputDialog::getText(this, tr("Confirm vault password"),
        tr("Identity vault password"), QLineEdit::Password, {}, &accepted);
    if (!accepted) { password.fill(QChar{0}); return; }
    const bool started = m_model->requestClaimName(name, password);
    password.fill(QChar{0});
    if (!started) QMessageBox::warning(this, tr("Cannot claim name"), tr("Unlock an active identity before claiming a name."));
}

void IdentityPage::rebuildForState(CybouIdentityState state)
{
    const bool creating = state != CybouIdentityState::None && state != CybouIdentityState::Active;
    const bool active = state == CybouIdentityState::Active;
    m_phase_row->setVisible(creating);
    m_steps->setVisible(state == CybouIdentityState::None);
    m_create_button->setVisible(!active);
    m_restore_button->setVisible(!active);
    m_chip_protected->setVisible(active);
    m_chip_ready->setVisible(active);
    m_share_button->setVisible(active && !m_model->status().account_id.isEmpty());
    m_claim_button->setVisible(active);
    m_claim_button->setText(m_model->status().primary_name.isEmpty()
        ? tr("Manage identity")
        : tr("Manage identity"));
    m_add_device_button->setVisible(active);
    m_security_button->setVisible(active);
    m_active_panel->setVisible(active);
    m_cards->setVisible(active);
    m_advanced->setVisible(active);
    m_active_details->setVisible(active);
    m_avatar_emblem->setVisible(active);
    m_dev_warning->setVisible(active);

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
        m_state_label->setText(status.primary_name.isEmpty() ? tr("Identity active") : status.primary_name);
        m_detail_label->setText(status.name_claim_pending ? status.name_claim_status :
            tr("Your CYBOU identity for messages, payments and files. A single, human-friendly identity that works across all CYBOU services \u2014 you stay in control of your data, devices and who can reach you."));
        m_active_details->setText(
            tr("AccountID: %1\nCreation height: %2\nNetwork: %3\nSystemBalance was funded atomically from the OnboardingPool at creation.")
                .arg(status.account_id)
                .arg(status.creation_height)
                .arg(status.network_name));
        m_claim_button->setEnabled(!status.name_claim_pending);
        return;
    }

    if (status.identity_state != CybouIdentityState::None) {
        m_state_label->setText(tr("Processing identity"));
        switch (status.identity_state) {
        case CybouIdentityState::CreatingKeys:
            m_detail_label->setText(tr("Preparing local identity keys and checking verified state."));
            break;
        case CybouIdentityState::PerformingWork:
            m_detail_label->setText(tr("The node is performing AccountCreationWork \u2014 protocol anti-Sybil computation. One identity costs real work, so mass registrations stay out."));
            break;
        case CybouIdentityState::Broadcasting:
            m_detail_label->setText(tr("The signed identity operation is being submitted to the network."));
            break;
        case CybouIdentityState::WaitingForFinality:
            m_detail_label->setText(tr("Waiting for verified BFT finality before activating this device."));
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
        ? tr("Identity operation requested. The node will drive each protocol phase and report finality.")
        : tr("Your identity will be controlled by local keys and registered through a permissionless protocol operation with protocol anti-Sybil work."));
    m_create_button->setEnabled(!pending && m_model->capabilities().account_creation);
    m_restore_button->setEnabled(!pending && m_model->capabilities().account_creation);
    const auto* service = m_model->identityService();
    const auto vault_path = service ? service->GetStoragePath() : std::nullopt;
    const bool has_vault = vault_path && std::filesystem::exists(*vault_path);
    m_create_button->setText(pending ? tr("Creation requested\u2026") :
        has_vault ? (service->GetKeyStore().HasKey() ? tr("Resume account creation") : tr("Unlock identity")) :
        tr("Create identity"));
    m_create_button->setToolTip(pending
        ? tr("Waiting for the node to pick up the request.")
        : tr("Create or unlock a password-protected recovery vault."));
}
