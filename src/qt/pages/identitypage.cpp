// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/identitypage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>
#include <qt/recoveryphrasedialog.h>

#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

using namespace CybouUi;

namespace {

QPushButton* Button(const QString& text, bool primary, QWidget* parent)
{
    auto* button = new QPushButton{text, parent};
    button->setObjectName(primary ? QStringLiteral("primaryButton") : QStringLiteral("secondaryButton"));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

/** "Label ........ value [trailing]" row inside a section card. */
QLabel* DetailRow(QVBoxLayout* section, const QString& label, QWidget* parent, QWidget* trailing = nullptr)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* key = new QLabel{label, parent};
    key->setObjectName(QStringLiteral("rowSub"));
    key->setMinimumWidth(170);
    auto* value = new QLabel{parent};
    value->setObjectName(QStringLiteral("rowTitle"));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    value->setMinimumWidth(0);
    row->addWidget(key);
    row->addWidget(value, 1);
    if (trailing) row->addWidget(trailing);
    section->addLayout(row);
    return value;
}

QVBoxLayout* Section(QVBoxLayout* root, const QString& title, QWidget* parent, QFrame** card_out = nullptr)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);
    layout->addWidget(SectionTitle(title, card));
    root->addWidget(card);
    if (card_out) *card_out = card;
    return layout;
}

void ClearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->layout()) ClearLayout(item->layout());
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

/**
 * Re-authentication before revealing recovery words: warning, vault
 * password and an explicit acknowledgement.
 */
class RevealDialog final : public QDialog
{
public:
    explicit RevealDialog(QWidget* parent) : QDialog{parent}
    {
        setWindowTitle(IdentityPage::tr("Show recovery phrase"));
        setObjectName(QStringLiteral("revealRecoveryDialog"));
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(24, 22, 24, 18);
        layout->setSpacing(12);
        layout->addWidget(SectionTitle(IdentityPage::tr("Show recovery phrase"), this));
        auto* warning = BodyText(IdentityPage::tr(
            "Anyone who sees your recovery phrase can take over your Identity, Mail, Files, Names and Wallet. "
            "Make sure nobody is watching your screen and nothing is recording it."), this);
        layout->addWidget(warning);
        m_password = new QLineEdit{this};
        m_password->setObjectName(QStringLiteral("revealPassword"));
        m_password->setEchoMode(QLineEdit::Password);
        m_password->setPlaceholderText(IdentityPage::tr("Vault password"));
        layout->addWidget(m_password);
        m_ack = new QCheckBox{IdentityPage::tr("I understand and want to show my recovery phrase"), this};
        m_ack->setObjectName(QStringLiteral("revealAcknowledge"));
        layout->addWidget(m_ack);
        m_error = MutedText({}, this);
        layout->addWidget(m_error);
        auto* buttons = new QDialogButtonBox{this};
        m_reveal = buttons->addButton(IdentityPage::tr("Reveal"), QDialogButtonBox::AcceptRole);
        m_reveal->setObjectName(QStringLiteral("primaryButton"));
        buttons->addButton(QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        const auto update = [this] { m_reveal->setEnabled(m_ack->isChecked() && !m_password->text().isEmpty()); };
        connect(m_password, &QLineEdit::textChanged, this, update);
        connect(m_ack, &QCheckBox::toggled, this, update);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        update();
    }

    QString takePassword()
    {
        QString password = m_password->text();
        m_password->clear();
        return password;
    }
    void setError(const QString& text) { m_error->setText(text); }

private:
    QLineEdit* m_password;
    QCheckBox* m_ack;
    QLabel* m_error;
    QPushButton* m_reveal;
};

} // namespace

IdentityPage::IdentityPage(CybouDesktopModel* model, std::function<void()> home_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_home_requested{std::move(home_requested)}
{
    auto* stack = new QStackedLayout{this};
    m_setup = buildSetupPrompt();
    m_content = buildContent();
    stack->addWidget(m_setup);
    stack->addWidget(m_content);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::namesChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::nameClaimFailed, this, [this](const QString& reason) {
        QMessageBox::warning(this, tr("Name not claimed"),
            reason.isEmpty() ? tr("The name could not be claimed.") : reason);
    });
    connect(m_model, &CybouDesktopModel::recoveryRotationFinished, this,
        [this](CybouOperationOutcome outcome, const QString& error) {
            if (outcome == CybouOperationOutcome::Finalized) {
                QMessageBox::information(this, tr("Recovery phrase replaced"),
                    tr("Your new recovery phrase is now active. The old phrase no longer restores this Identity."));
            } else if (outcome == CybouOperationOutcome::Pending) {
                QMessageBox::information(this, tr("Waiting for network confirmation"),
                    tr("The new recovery phrase becomes active after network confirmation. Keep both phrases until then."));
            } else {
                QMessageBox::warning(this, tr("Recovery phrase not replaced"),
                    error.isEmpty() ? tr("Your current recovery phrase is still active.") : error);
            }
        });
    refresh();
}

QWidget* IdentityPage::buildSetupPrompt()
{
    auto* page = new QWidget{this};
    auto* layout = new QVBoxLayout{page};
    layout->setContentsMargins(28, 40, 28, 28);
    auto* card = Card(page);
    card->setMaximumWidth(560);
    auto* card_layout = new QVBoxLayout{card};
    card_layout->setContentsMargins(28, 24, 28, 24);
    card_layout->setSpacing(10);
    card_layout->addWidget(SectionTitle(tr("No Identity on this computer yet"), card));
    card_layout->addWidget(MutedText(tr("Create a new Identity or restore one from your recovery phrase on Home."), card));
    auto* go = Button(tr("Go to Home"), true, card);
    connect(go, &QPushButton::clicked, this, [this] { if (m_home_requested) m_home_requested(); });
    card_layout->addWidget(go, 0, Qt::AlignLeft);
    layout->addWidget(card, 0, Qt::AlignHCenter);
    layout->addStretch();
    return page;
}

QWidget* IdentityPage::buildContent()
{
    auto* page = new QWidget{this};
    auto* root = new QVBoxLayout{page};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(16);

    // Identity header.
    auto* hero = new QFrame{page};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(26, 20, 26, 20);
    hero_layout->setSpacing(16);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(2);
    m_name = HeroTitle({}, hero, true);
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_name_caption = HeroSubtitle({}, hero);
    hero_text->addWidget(m_name);
    hero_text->addWidget(m_name_caption);
    hero_layout->addLayout(hero_text, 1);
    m_lock = Button(tr("Lock vault"), false, hero);
    m_lock->setObjectName(QStringLiteral("secondaryButton"));
    m_lock->setProperty("cybouId", QStringLiteral("lockVault"));
    connect(m_lock, &QPushButton::clicked, this, [this] {
        m_model->requestLockVault();
        if (m_home_requested) m_home_requested();
    });
    hero_layout->addWidget(m_lock, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    // Account.
    auto* account = Section(root, tr("Account"), page);
    auto* copy = Button(tr("Copy"), false, page);
    copy->setProperty("cybouId", QStringLiteral("copyAccountId"));
    connect(copy, &QPushButton::clicked, this, [this] { copyAccountId(); });
    m_account_id = DetailRow(account, tr("Account ID"), page, copy);

    // Names.
    auto* names = Section(root, tr("CYBOU names"), page);
    auto* names_host = new QWidget{page};
    m_names_rows = new QVBoxLayout{names_host};
    m_names_rows->setContentsMargins(0, 0, 0, 0);
    m_names_rows->setSpacing(6);
    names->addWidget(names_host);
    m_claim_status = MutedText({}, page);
    names->addWidget(m_claim_status);
    m_claim = Button(tr("Claim CYBOU name"), false, page);
    m_claim->setProperty("cybouId", QStringLiteral("claimName"));
    connect(m_claim, &QPushButton::clicked, this, [this] { claimName(); });
    names->addWidget(m_claim, 0, Qt::AlignLeft);

    // Recovery.
    auto* recovery = Section(root, tr("Recovery"), page);
    m_recovery_state = DetailRow(recovery, tr("Recovery phrase"), page);
    m_vault_state = DetailRow(recovery, tr("Local vault"), page);
    auto* options = new QToolButton{page};
    options->setObjectName(QStringLiteral("secondaryButton"));
    options->setText(tr("Show recovery options"));
    options->setProperty("cybouId", QStringLiteral("recoveryOptions"));
    options->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu{options};
    menu->addAction(tr("Show recovery phrase…"), this, [this] { revealRecoveryPhrase(); });
    menu->addAction(tr("Replace recovery phrase…"), this, [this] { replaceRecoveryPhrase(); });
    options->setMenu(menu);
    recovery->addWidget(options, 0, Qt::AlignLeft);

    // Security.
    auto* security = Section(root, tr("Security"), page);
    m_pq_state = DetailRow(security, tr("Post-quantum protection"), page);

    // Advanced security details (collapsed).
    m_advanced_toggle = new QToolButton{page};
    m_advanced_toggle->setObjectName(QStringLiteral("sectionLink"));
    m_advanced_toggle->setText(tr("Advanced security details"));
    m_advanced_toggle->setCheckable(true);
    m_advanced_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_advanced_toggle->setIcon(QIcon{glyphPixmap(Glyph::ChevronRight, {14, 14}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    m_advanced_toggle->setAutoRaise(true);
    root->addWidget(m_advanced_toggle, 0, Qt::AlignLeft);
    m_advanced = Card(page);
    m_advanced->setObjectName(QStringLiteral("card"));
    m_advanced_rows = new QVBoxLayout{m_advanced};
    m_advanced_rows->setContentsMargins(22, 16, 22, 16);
    m_advanced_rows->setSpacing(8);
    m_advanced->setVisible(false);
    root->addWidget(m_advanced);
    connect(m_advanced_toggle, &QToolButton::toggled, this, [this](bool open) {
        m_advanced->setVisible(open);
        m_advanced_toggle->setIcon(QIcon{glyphPixmap(open ? Glyph::DotsH : Glyph::ChevronRight, {14, 14},
            CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    });
    root->addStretch();
    return page;
}

void IdentityPage::refresh()
{
    const auto& status = m_model->status();
    const bool active = status.identity_state == CybouIdentityState::Active ||
        status.identity_state == CybouIdentityState::Syncing;
    static_cast<QStackedLayout*>(layout())->setCurrentWidget(active ? m_content : m_setup);
    if (!active) return;

    m_name->setText(status.primary_name.isEmpty() ? tr("Your CYBOU Identity") : status.primary_name);
    m_name_caption->setText(status.primary_name.isEmpty() ? tr("No CYBOU name yet") : tr("Verified CYBOU name"));
    m_account_id->setText(CybouProduct::shortId(status.account_id));
    m_account_id->setToolTip(status.account_id);
    m_recovery_state->setText(tr("Secured"));
    m_vault_state->setText(status.identity_state == CybouIdentityState::Locked ? tr("Locked") : tr("Unlocked"));
    m_pq_state->setText(tr("Active"));

    ClearLayout(m_names_rows);
    for (const auto& name : m_model->names()) {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel{name.name, m_content};
        label->setObjectName(QStringLiteral("rowTitle"));
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(label);
        if (name.primary) row->addWidget(Pill(tr("Primary"), Tint::Mint, m_content));
        row->addStretch();
        m_names_rows->addLayout(row);
    }
    if (m_model->names().isEmpty()) {
        m_names_rows->addWidget(MutedText(tr("Claim a .cybou name so people can reach you as name.cybou."), m_content));
    }
    // The initial registry supports one name per Identity.
    m_claim->setVisible(m_model->names().isEmpty());
    m_claim->setEnabled(!status.name_claim_pending);
    m_claim_status->setText(status.name_claim_status);
    m_claim_status->setVisible(status.name_claim_pending);

    ClearLayout(m_advanced_rows);
    const auto add = [this](const QString& key, const QString& value) {
        DetailRow(m_advanced_rows, key, m_advanced)->setText(value);
    };
    add(tr("Account ID"), status.account_id);
    add(tr("Key epoch"), status.key_epoch > 0 ? QString::number(status.key_epoch) : tr("Not reported yet"));
    add(tr("Authorization"), tr("Ed25519 + ML-DSA-44 · valid"));
    add(tr("Recovery"), tr("Ed25519 + ML-DSA-65 · secured"));
    add(tr("Key encapsulation"), tr("Hybrid post-quantum KEM · published"));
    add(tr("Created at finalized height"), status.creation_height > 0 ? QString::number(status.creation_height) : tr("Not reported yet"));
}

void IdentityPage::copyAccountId()
{
    QGuiApplication::clipboard()->setText(m_model->status().account_id);
    m_model->notify(tr("Account ID copied"));
}

void IdentityPage::revealRecoveryPhrase()
{
    auto* dialog = new RevealDialog{this};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        QString password = dialog->takePassword();
        m_model->revealRecoveryWordsAsync(password, [this](std::optional<QStringList> words) {
            if (!words) {
                QMessageBox::warning(this, tr("Recovery phrase not shown"),
                    tr("The password is incorrect, or this vault does not store the recovery words."));
                return;
            }
            RecoveryPhraseDialog phrase{RecoveryPhraseDialog::Mode::View, *words, this};
            for (auto& word : *words) word.fill(QChar{0});
            phrase.exec();
        });
        password.fill(QChar{0});
    });
    dialog->open();
}

void IdentityPage::replaceRecoveryPhrase()
{
    const auto choice = QMessageBox::question(this, tr("Replace recovery phrase"),
        tr("CYBOU will create a new recovery phrase and replace all Identity keys. "
           "The old phrase stops working after network confirmation. Continue?"));
    if (choice != QMessageBox::Yes) return;
    bool accepted{false};
    QString password = QInputDialog::getText(this, tr("Confirm with your vault password"),
        tr("Vault password"), QLineEdit::Password, {}, &accepted);
    if (!accepted || password.isEmpty()) return;
    if (!m_model->fixtureMode() && !m_model->requestUnlockIdentity(password)) {
        password.fill(QChar{0});
        QMessageBox::warning(this, tr("Incorrect password"), tr("The vault password is incorrect."));
        return;
    }
    if (m_model->hasPendingRecoveryRotation()) {
        m_model->requestRecoveryRootRotation({}, password, true);
        password.fill(QChar{0});
        return;
    }
    auto words = m_model->generateRotationWords();
    if (!words) {
        password.fill(QChar{0});
        QMessageBox::warning(this, tr("Cannot create a recovery phrase"), tr("Secure randomness is unavailable."));
        return;
    }
    RecoveryPhraseDialog phrase{RecoveryPhraseDialog::Mode::Create, *words, this};
    if (phrase.exec() == QDialog::Accepted) m_model->requestRecoveryRootRotation(*words, password);
    for (auto& word : *words) word.fill(QChar{0});
    password.fill(QChar{0});
}

void IdentityPage::claimName()
{
    bool accepted{false};
    const QString label = QInputDialog::getText(this, tr("Claim CYBOU name"),
        tr("Choose your name (5–32 characters). You will be reachable as name.cybou."),
        QLineEdit::Normal, {}, &accepted).trimmed().toLower().remove(QStringLiteral(".cybou"));
    if (!accepted) return;
    if (const QString problem = m_model->nameLabelProblem(label); !problem.isEmpty()) {
        QMessageBox::warning(this, tr("Choose another name"), problem);
        return;
    }
    QString password;
    if (!m_model->fixtureMode()) {
        password = QInputDialog::getText(this, tr("Confirm with your vault password"),
            tr("Vault password"), QLineEdit::Password, {}, &accepted);
        if (!accepted || password.isEmpty()) return;
    }
    if (!m_model->requestClaimName(label, password)) {
        QMessageBox::warning(this, tr("Name not claimed"), tr("A name claim is already in progress."));
    }
    password.fill(QChar{0});
}
