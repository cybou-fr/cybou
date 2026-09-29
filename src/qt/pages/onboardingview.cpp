// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/onboardingview.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

using namespace CybouUi;

namespace {

constexpr int kMinPasswordLength = 12;
constexpr int kPhraseWords = 24;

/** Centers a fixed-width card inside the page. */
QWidget* CenteredCard(QWidget* parent, QVBoxLayout*& layout_out, int max_width = 600)
{
    auto* host = new QWidget{parent};
    auto* outer = new QVBoxLayout{host};
    outer->setContentsMargins(24, 36, 24, 36);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    auto* card = Card(host);
    card->setMaximumWidth(max_width);
    card->setMinimumWidth(qMin(max_width, 420));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    layout_out = new QVBoxLayout{card};
    layout_out->setContentsMargins(36, 32, 36, 32);
    layout_out->setSpacing(14);
    row->addWidget(card, 4);
    row->addStretch(1);
    outer->addLayout(row);
    outer->addStretch(1);
    return host;
}

QLineEdit* PasswordField(const QString& placeholder, QWidget* parent)
{
    auto* edit = new QLineEdit{parent};
    edit->setEchoMode(QLineEdit::Password);
    edit->setPlaceholderText(placeholder);
    edit->setMinimumHeight(38);
    return edit;
}

QLabel* FieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("cardLabel"));
    return label;
}

QPushButton* Button(const QString& text, bool primary, QWidget* parent)
{
    auto* button = new QPushButton{text, parent};
    button->setObjectName(primary ? QStringLiteral("primaryButton") : QStringLiteral("secondaryButton"));
    button->setMinimumHeight(40);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

/** Progress row: [state glyph] text. State: 0 pending, 1 running, 2 done. */
QLabel* StepRow(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("bodyText"));
    label->setProperty("stepText", text);
    label->setTextFormat(Qt::RichText);
    return label;
}

void SetStep(QLabel* label, int state)
{
    const QString text = label->property("stepText").toString().toHtmlEscaped();
    const QString mark = state == 2 ? QStringLiteral("&#10003;") : state == 1 ? QStringLiteral("&#10227;") : QStringLiteral("&#8226;");
    const QString color = CybouTheme::color(state == 2 ? CybouTheme::BRAND_TEAL_DARK
        : state == 1 ? CybouTheme::TEXT_PRIMARY : CybouTheme::DIM).name();
    label->setText(QStringLiteral("<span style=\"color:%1; font-weight:700;\">%2</span>&nbsp;&nbsp;"
                                  "<span style=\"color:%1;\">%3</span>").arg(color, mark, text));
    label->setAccessibleName(label->property("stepText").toString());
}

int RestoreStepState(CybouRestoreStepState state)
{
    switch (state) {
    case CybouRestoreStepState::Pending: return 0;
    case CybouRestoreStepState::Running: return 1;
    case CybouRestoreStepState::Done: return 2;
    }
    return 0;
}

QStringList SplitWords(const QString& phrase)
{
    return phrase.trimmed().split(QRegularExpression{QStringLiteral("\\s+")}, Qt::SkipEmptyParts);
}

} // namespace

OnboardingView::OnboardingView(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_stack{new QStackedWidget{this}}
{
    setObjectName(QStringLiteral("onboarding"));
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);
    m_stack->addWidget(buildWelcome());
    m_stack->addWidget(buildPassword());
    m_stack->addWidget(buildRecoveryWords());
    m_stack->addWidget(buildConfirmWords());
    m_stack->addWidget(buildCreating());
    m_stack->addWidget(buildRestore());
    m_stack->addWidget(buildRestoring());
    m_stack->addWidget(buildUnlock());

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::identityCreationFailed, this, [this](const QString& reason) {
        showScreen(Screen::Creating);
        m_create_error->setText(reason.isEmpty() ? tr("Your Identity could not be created. Please try again.") : reason);
        m_create_error->setVisible(true);
        m_create_retry->setVisible(true);
    });
    refresh();
}

OnboardingView::~OnboardingView()
{
    clearSecrets();
}

OnboardingView::Screen OnboardingView::screen() const
{
    return static_cast<Screen>(m_stack->currentIndex());
}

void OnboardingView::showScreen(Screen screen)
{
    m_stack->setCurrentIndex(static_cast<int>(screen));
}

QWidget* OnboardingView::buildWelcome()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout, 560);
    auto* logo = new QLabel{page};
    logo->setPixmap(CybouTheme::logoTile({64, 64}, 16, {44, 44}));
    logo->setFixedSize(64, 64);
    layout->addWidget(logo, 0, Qt::AlignHCenter);
    layout->addSpacing(6);
    auto* title = HeroTitle(tr("Welcome to CYBOU"), page, true);
    title->setAlignment(Qt::AlignHCenter);
    layout->addWidget(title);
    auto* subtitle = HeroSubtitle(tr("One identity for your private\nMail, Files, Names and Wallet."), page);
    subtitle->setAlignment(Qt::AlignHCenter);
    layout->addWidget(subtitle);
    layout->addSpacing(12);
    auto* create = Button(tr("Create Identity"), true, page);
    create->setObjectName(QStringLiteral("primaryButton"));
    create->setProperty("cybouId", QStringLiteral("createIdentity"));
    auto* restore = Button(tr("Restore from mnemonic"), false, page);
    restore->setProperty("cybouId", QStringLiteral("restoreIdentity"));
    layout->addWidget(create);
    layout->addWidget(restore);
    layout->addSpacing(8);
    auto* note = new QLabel{page};
    note->setPixmap(glyphPixmap(Glyph::ShieldCheck, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    auto* note_text = MutedText(tr("Post-quantum protected"), page);
    note_text->setWordWrap(false);
    auto* note_row = new QHBoxLayout;
    note_row->addStretch();
    note_row->addWidget(note);
    note_row->addWidget(note_text);
    note_row->addStretch();
    layout->addLayout(note_row);

    connect(create, &QPushButton::clicked, this, [this] { startCreate(); });
    connect(restore, &QPushButton::clicked, this, [this] {
        m_phrase->clear();
        m_restore_password->clear();
        m_restore_confirm->clear();
        updateRestoreState();
        showScreen(Screen::Restore);
        m_phrase->setFocus();
    });
    // Buttons follow the account-creation capability from the model.
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, create, [this, create, restore] {
        create->setEnabled(m_model->capabilities().account_creation);
        restore->setEnabled(m_model->capabilities().account_creation);
    });
    create->setEnabled(m_model->capabilities().account_creation);
    restore->setEnabled(m_model->capabilities().account_creation);
    return page;
}

QWidget* OnboardingView::buildPassword()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout);
    layout->addWidget(Eyebrow(tr("STEP 1 OF 3"), page));
    layout->addWidget(HeroTitle(tr("Create a local vault password"), page));
    layout->addWidget(MutedText(tr("This password unlocks CYBOU on this computer. It never leaves your device. "
                                   "Your recovery phrase, not this password, restores your Identity elsewhere."), page));
    layout->addSpacing(4);
    layout->addWidget(FieldLabel(tr("Vault password"), page));
    m_password = PasswordField(tr("At least %1 characters").arg(kMinPasswordLength), page);
    m_password->setObjectName(QStringLiteral("vaultPassword"));
    layout->addWidget(m_password);
    layout->addWidget(FieldLabel(tr("Repeat password"), page));
    m_password_confirm = PasswordField(tr("Repeat the password"), page);
    m_password_confirm->setObjectName(QStringLiteral("vaultPasswordConfirm"));
    layout->addWidget(m_password_confirm);
    m_password_hint = MutedText({}, page);
    layout->addWidget(m_password_hint);
    auto* buttons = new QHBoxLayout;
    auto* back = Button(tr("Back"), false, page);
    m_password_next = Button(tr("Continue"), true, page);
    m_password_next->setObjectName(QStringLiteral("primaryButton"));
    m_password_next->setProperty("cybouId", QStringLiteral("passwordContinue"));
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(m_password_next);
    layout->addLayout(buttons);

    connect(m_password, &QLineEdit::textChanged, this, [this] { updatePasswordState(); });
    connect(m_password_confirm, &QLineEdit::textChanged, this, [this] { updatePasswordState(); });
    connect(m_password_confirm, &QLineEdit::returnPressed, this, [this] {
        if (m_password_next->isEnabled()) acceptPassword();
    });
    connect(m_password_next, &QPushButton::clicked, this, [this] { acceptPassword(); });
    connect(back, &QPushButton::clicked, this, [this] { cancelCreate(); });
    updatePasswordState();
    return page;
}

QWidget* OnboardingView::buildRecoveryWords()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout, 680);
    layout->addWidget(Eyebrow(tr("STEP 2 OF 3"), page));
    layout->addWidget(HeroTitle(tr("Write down your 24 recovery words"), page));
    layout->addWidget(MutedText(tr("These words restore your Identity, Mail, Files, Names and Wallet on any computer. "
                                   "Anyone who has them controls your Identity. Write them on paper, in order, "
                                   "and keep them somewhere safe. CYBOU cannot recover them for you."), page));
    m_words_grid = new QWidget{page};
    m_words_grid->setObjectName(QStringLiteral("recoveryWords"));
    auto* grid = new QGridLayout{m_words_grid};
    grid->setContentsMargins(0, 6, 0, 6);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    layout->addWidget(m_words_grid);
    auto* buttons = new QHBoxLayout;
    auto* back = Button(tr("Cancel"), false, page);
    auto* next = Button(tr("I have written them down"), true, page);
    next->setObjectName(QStringLiteral("primaryButton"));
    next->setProperty("cybouId", QStringLiteral("wordsContinue"));
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(next);
    layout->addLayout(buttons);
    connect(back, &QPushButton::clicked, this, [this] { cancelCreate(); });
    connect(next, &QPushButton::clicked, this, [this] {
        prepareConfirmation();
        showScreen(Screen::ConfirmWords);
        if (!m_confirm_inputs.isEmpty()) m_confirm_inputs.first()->setFocus();
    });
    return page;
}

QWidget* OnboardingView::buildConfirmWords()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout);
    layout->addWidget(Eyebrow(tr("STEP 3 OF 3"), page));
    layout->addWidget(HeroTitle(tr("Confirm your recovery words"), page));
    layout->addWidget(MutedText(tr("Enter the requested words to confirm you saved the phrase correctly."), page));
    for (int i = 0; i < 3; ++i) {
        auto* label = FieldLabel({}, page);
        auto* input = new QLineEdit{page};
        input->setMinimumHeight(38);
        input->setObjectName(QStringLiteral("confirmWord%1").arg(i));
        m_confirm_labels.append(label);
        m_confirm_inputs.append(input);
        layout->addWidget(label);
        layout->addWidget(input);
        connect(input, &QLineEdit::textChanged, this, [this] {
            bool all = true;
            for (int k = 0; k < m_confirm_inputs.size(); ++k) {
                if (m_confirm_inputs.at(k)->text().trimmed().isEmpty()) all = false;
            }
            m_confirm_next->setEnabled(all);
            m_confirm_hint->clear();
        });
        connect(input, &QLineEdit::returnPressed, this, [this] {
            if (m_confirm_next->isEnabled()) acceptConfirmation();
        });
    }
    m_confirm_hint = MutedText({}, page);
    layout->addWidget(m_confirm_hint);
    auto* buttons = new QHBoxLayout;
    auto* back = Button(tr("Show words again"), false, page);
    m_confirm_next = Button(tr("Create Identity"), true, page);
    m_confirm_next->setObjectName(QStringLiteral("primaryButton"));
    m_confirm_next->setProperty("cybouId", QStringLiteral("confirmCreate"));
    m_confirm_next->setEnabled(false);
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(m_confirm_next);
    layout->addLayout(buttons);
    connect(back, &QPushButton::clicked, this, [this] { showScreen(Screen::RecoveryWords); });
    connect(m_confirm_next, &QPushButton::clicked, this, [this] { acceptConfirmation(); });
    return page;
}

QWidget* OnboardingView::buildCreating()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout, 520);
    layout->addWidget(HeroTitle(tr("Creating your Identity"), page));
    layout->addWidget(MutedText(tr("This takes a moment. You can keep CYBOU open while it finishes."), page));
    layout->addSpacing(6);
    for (const QString& text : {tr("Preparing keys"), tr("Creating Identity"),
             tr("Waiting for network confirmation"), tr("Identity active")}) {
        auto* row = StepRow(text, page);
        m_create_steps.append(row);
        layout->addWidget(row);
    }
    m_create_error = MutedText({}, page);
    m_create_error->setStyleSheet(QStringLiteral("color: %1;").arg(CybouTheme::color(CybouTheme::ROSE).name()));
    m_create_error->setVisible(false);
    layout->addWidget(m_create_error);
    m_create_retry = Button(tr("Start again"), false, page);
    m_create_retry->setVisible(false);
    layout->addWidget(m_create_retry, 0, Qt::AlignLeft);
    connect(m_create_retry, &QPushButton::clicked, this, [this] {
        m_create_error->setVisible(false);
        m_create_retry->setVisible(false);
        showScreen(Screen::Welcome);
    });
    return page;
}

QWidget* OnboardingView::buildRestore()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout, 640);
    layout->addWidget(HeroTitle(tr("Restore your Identity"), page));
    layout->addWidget(MutedText(tr("Enter your 24-word recovery phrase"), page));
    m_phrase = new QPlainTextEdit{page};
    m_phrase->setObjectName(QStringLiteral("recoveryPhrase"));
    m_phrase->setPlaceholderText(tr("word1 word2 word3 …"));
    m_phrase->setTabChangesFocus(true);
    m_phrase->setFixedHeight(110);
    layout->addWidget(m_phrase);
    m_phrase_count = MutedText({}, page);
    layout->addWidget(m_phrase_count);
    layout->addWidget(FieldLabel(tr("Local vault password"), page));
    m_restore_password = PasswordField(tr("At least %1 characters").arg(kMinPasswordLength), page);
    m_restore_password->setObjectName(QStringLiteral("restorePassword"));
    layout->addWidget(m_restore_password);
    m_restore_confirm = PasswordField(tr("Repeat the password"), page);
    m_restore_confirm->setObjectName(QStringLiteral("restorePasswordConfirm"));
    layout->addWidget(m_restore_confirm);
    m_restore_hint = MutedText({}, page);
    layout->addWidget(m_restore_hint);
    auto* buttons = new QHBoxLayout;
    auto* back = Button(tr("Back"), false, page);
    m_restore_button = Button(tr("Restore Identity"), true, page);
    m_restore_button->setObjectName(QStringLiteral("primaryButton"));
    m_restore_button->setProperty("cybouId", QStringLiteral("restoreSubmit"));
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(m_restore_button);
    layout->addLayout(buttons);

    connect(m_phrase, &QPlainTextEdit::textChanged, this, [this] { updateRestoreState(); });
    connect(m_restore_password, &QLineEdit::textChanged, this, [this] { updateRestoreState(); });
    connect(m_restore_confirm, &QLineEdit::textChanged, this, [this] { updateRestoreState(); });
    connect(m_restore_button, &QPushButton::clicked, this, [this] { submitRestore(); });
    connect(back, &QPushButton::clicked, this, [this] {
        m_phrase->clear();
        m_restore_password->clear();
        m_restore_confirm->clear();
        showScreen(Screen::Welcome);
    });
    updateRestoreState();
    return page;
}

QWidget* OnboardingView::buildRestoring()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout, 520);
    layout->addWidget(HeroTitle(tr("Restoring your Identity"), page));
    layout->addWidget(MutedText(tr("CYBOU is rebuilding your data from the network. "
                                   "Mail and Files appear as they are verified."), page));
    layout->addSpacing(6);
    for (const QString& text : {tr("Identity recovered"), tr("Wallet state recovered"), tr("Names recovered"),
             tr("Mail"), tr("Files")}) {
        auto* row = StepRow(text, page);
        m_restore_steps.append(row);
        layout->addWidget(row);
    }
    return page;
}

QWidget* OnboardingView::buildUnlock()
{
    QVBoxLayout* layout{nullptr};
    auto* page = CenteredCard(this, layout, 480);
    auto* logo = new QLabel{page};
    logo->setPixmap(CybouTheme::logoTile({56, 56}, 14, {38, 38}));
    logo->setFixedSize(56, 56);
    layout->addWidget(logo, 0, Qt::AlignHCenter);
    m_unlock_title = HeroTitle(tr("Welcome back"), page);
    m_unlock_title->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_unlock_title);
    auto* subtitle = MutedText(tr("Enter your local vault password to unlock CYBOU on this computer."), page);
    subtitle->setAlignment(Qt::AlignHCenter);
    layout->addWidget(subtitle);
    m_unlock_password = PasswordField(tr("Vault password"), page);
    m_unlock_password->setObjectName(QStringLiteral("unlockPassword"));
    layout->addWidget(m_unlock_password);
    m_unlock_hint = MutedText({}, page);
    layout->addWidget(m_unlock_hint);
    auto* unlock = Button(tr("Unlock"), true, page);
    unlock->setObjectName(QStringLiteral("primaryButton"));
    unlock->setProperty("cybouId", QStringLiteral("unlockSubmit"));
    layout->addWidget(unlock);
    auto* restore = Button(tr("Restore a different Identity"), false, page);
    layout->addWidget(restore);
    connect(unlock, &QPushButton::clicked, this, [this] { submitUnlock(); });
    connect(m_unlock_password, &QLineEdit::returnPressed, this, [this] { submitUnlock(); });
    connect(restore, &QPushButton::clicked, this, [this] { showScreen(Screen::Restore); });
    return page;
}

void OnboardingView::startCreate()
{
    // Existing local vault: unlock it (and resume creation) instead.
    if (m_model->hasLocalVault()) {
        showScreen(Screen::Unlock);
        m_unlock_password->setFocus();
        return;
    }
    clearSecrets();
    const auto words = m_model->prepareNewIdentityWords();
    if (!words || words->size() != kPhraseWords) {
        m_create_error->setText(tr("Your keys could not be prepared on this computer."));
        m_create_error->setVisible(true);
        m_create_retry->setVisible(true);
        showScreen(Screen::Creating);
        return;
    }
    m_words = *words;
    populateWords();
    m_password->clear();
    m_password_confirm->clear();
    showScreen(Screen::Password);
    m_password->setFocus();
}

void OnboardingView::updatePasswordState()
{
    const QString password = m_password->text();
    const QString confirmation = m_password_confirm->text();
    QString hint;
    if (!password.isEmpty() && password.size() < kMinPasswordLength) {
        hint = tr("Use at least %1 characters.").arg(kMinPasswordLength);
    } else if (!confirmation.isEmpty() && password != confirmation) {
        hint = tr("The passwords do not match.");
    }
    m_password_hint->setText(hint);
    m_password_next->setEnabled(password.size() >= kMinPasswordLength && password == confirmation);
}

void OnboardingView::acceptPassword()
{
    m_pending_password = m_password->text();
    m_password->clear();
    m_password_confirm->clear();
    showScreen(Screen::RecoveryWords);
}

void OnboardingView::populateWords()
{
    auto* grid = qobject_cast<QGridLayout*>(m_words_grid->layout());
    while (QLayoutItem* item = grid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    constexpr int kColumns = 4;
    const int rows = (m_words.size() + kColumns - 1) / kColumns;
    for (int i = 0; i < m_words.size(); ++i) {
        auto* cell = new QLabel{QStringLiteral("<span style=\"color:%1;\">%2</span>&nbsp;&nbsp;<b>%3</b>")
            .arg(CybouTheme::color(CybouTheme::TEXT_MUTED).name())
            .arg(i + 1, 2)
            .arg(m_words.at(i).toHtmlEscaped()), m_words_grid};
        cell->setObjectName(QStringLiteral("pill"));
        cell->setProperty("tint", "neutral");
        cell->setTextFormat(Qt::RichText);
        cell->setMinimumHeight(34);
        // Column-major order reads 1–6 down the first column.
        grid->addWidget(cell, i % rows, i / rows);
    }
}

void OnboardingView::prepareConfirmation()
{
    m_confirm_positions.clear();
    if (m_model->fixtureMode()) {
        m_confirm_positions = {3, 11, 20};
    } else {
        while (m_confirm_positions.size() < 3) {
            const int position = static_cast<int>(QRandomGenerator::system()->bounded(kPhraseWords));
            if (!m_confirm_positions.contains(position)) m_confirm_positions.append(position);
        }
        std::sort(m_confirm_positions.begin(), m_confirm_positions.end());
    }
    for (int i = 0; i < m_confirm_inputs.size(); ++i) {
        m_confirm_labels.at(i)->setText(tr("Word #%1").arg(m_confirm_positions.at(i) + 1));
        m_confirm_inputs.at(i)->clear();
    }
    m_confirm_hint->clear();
    m_confirm_next->setEnabled(false);
}

void OnboardingView::acceptConfirmation()
{
    for (int i = 0; i < m_confirm_inputs.size(); ++i) {
        const QString typed = m_confirm_inputs.at(i)->text().trimmed().toLower();
        if (typed != m_words.at(m_confirm_positions.at(i))) {
            m_confirm_hint->setText(tr("Word #%1 does not match. Check your written copy.")
                .arg(m_confirm_positions.at(i) + 1));
            return;
        }
    }
    for (auto* input : m_confirm_inputs) input->clear();
    const QString password = m_pending_password;
    for (auto& word : m_words) word.fill(QChar{0});
    m_words.clear();
    m_pending_password.fill(QChar{0});
    m_pending_password.clear();
    populateWords();
    m_create_error->setVisible(false);
    m_create_retry->setVisible(false);
    showScreen(Screen::Creating);
    m_model->requestCreateIdentity(password);
    refresh();
}

void OnboardingView::updateRestoreState()
{
    const int count = SplitWords(m_phrase->toPlainText()).size();
    m_phrase_count->setText(tr("%1 / %2 words").arg(count).arg(kPhraseWords));
    const QString password = m_restore_password->text();
    const QString confirmation = m_restore_confirm->text();
    QString hint;
    if (count > kPhraseWords) {
        hint = tr("The phrase has more than %1 words.").arg(kPhraseWords);
    } else if (!password.isEmpty() && password.size() < kMinPasswordLength) {
        hint = tr("Use at least %1 characters.").arg(kMinPasswordLength);
    } else if (!confirmation.isEmpty() && password != confirmation) {
        hint = tr("The passwords do not match.");
    }
    m_restore_hint->setText(hint);
    m_restore_button->setEnabled(count == kPhraseWords && password.size() >= kMinPasswordLength &&
        password == confirmation && m_model->capabilities().account_creation);
}

void OnboardingView::submitRestore()
{
    QString phrase = m_phrase->toPlainText();
    QString password = m_restore_password->text();
    if (!m_model->recoveryPhraseValid(phrase)) {
        phrase.fill(QChar{0});
        password.fill(QChar{0});
        m_restore_hint->setText(tr("These words are not a valid CYBOU recovery phrase. Check the spelling and order."));
        return;
    }
    const bool started = m_model->requestRestoreIdentity(phrase, password);
    phrase.fill(QChar{0});
    password.fill(QChar{0});
    if (!started) {
        m_restore_hint->setText(tr("Restore could not start. Check the phrase and try again."));
        return;
    }
    m_phrase->clear();
    m_restore_password->clear();
    m_restore_confirm->clear();
    showScreen(Screen::Restoring);
}

void OnboardingView::submitUnlock()
{
    QString password = m_unlock_password->text();
    m_unlock_password->clear();
    if (password.isEmpty()) return;
    m_unlock_password->setEnabled(false);
    m_unlock_hint->setText(tr("Unlocking…"));
    // The vault KDF runs off the GUI thread.
    m_model->requestUnlockIdentityAsync(password, [this](bool ok) {
        m_unlock_password->setEnabled(true);
        if (!ok) {
            m_unlock_hint->setText(tr("The password is incorrect."));
            m_unlock_password->setFocus();
            return;
        }
        m_unlock_hint->clear();
        // A vault whose Identity was not confirmed yet resumes creation.
        if (m_model->status().identity_state != CybouIdentityState::Active) {
            showScreen(Screen::Creating);
            m_model->requestCreateIdentity({});
        }
    });
    password.fill(QChar{0});
}

void OnboardingView::cancelCreate()
{
    clearSecrets();
    m_model->discardPreparedIdentity();
    showScreen(Screen::Welcome);
}

void OnboardingView::clearSecrets()
{
    for (auto& word : m_words) word.fill(QChar{0});
    m_words.clear();
    m_pending_password.fill(QChar{0});
    m_pending_password.clear();
    if (m_words_grid) populateWords();
}

void OnboardingView::refresh()
{
    const auto& status = m_model->status();
    switch (status.identity_state) {
    case CybouIdentityState::Creating: {
        showScreen(Screen::Creating);
        const int step = static_cast<int>(status.identity_step);
        for (int i = 0; i < m_create_steps.size(); ++i) {
            SetStep(m_create_steps.at(i), i < step ? 2 : i == step ? 1 : 0);
        }
        break;
    }
    case CybouIdentityState::Restoring: {
        showScreen(Screen::Restoring);
        const auto& progress = m_model->restoreProgress();
        const CybouRestoreStepState states[] = {progress.identity, progress.wallet, progress.names,
            progress.mail, progress.files};
        for (int i = 0; i < m_restore_steps.size(); ++i) SetStep(m_restore_steps.at(i), RestoreStepState(states[i]));
        break;
    }
    case CybouIdentityState::Locked:
        m_unlock_title->setText(status.primary_name.isEmpty() ? tr("Welcome back")
            : tr("Welcome back, %1").arg(status.primary_name));
        if (screen() != Screen::Restore && screen() != Screen::Restoring) showScreen(Screen::Unlock);
        break;
    case CybouIdentityState::None:
        if (screen() == Screen::Creating && m_model->identityCreationRequestPending()) {
            for (int i = 0; i < m_create_steps.size(); ++i) SetStep(m_create_steps.at(i), i == 0 ? 1 : 0);
        } else if (screen() == Screen::Creating && !m_create_error->isVisible()) {
            showScreen(Screen::Welcome);
        } else if (screen() == Screen::Restoring) {
            showScreen(Screen::Restore);
            m_restore_hint->setText(tr("Restore did not complete. Check the phrase and try again."));
        }
        break;
    case CybouIdentityState::Syncing:
    case CybouIdentityState::Active:
    case CybouIdentityState::NeedsAttention:
        for (auto* row : m_create_steps) SetStep(row, 2);
        break;
    }
    updateRestoreState();
}
