// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/onboardingview.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QClipboard>
#include <QCompleter>
#include <QGridLayout>
#include <QGuiApplication>
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
        clearPhrase();
        m_restore_password->clear();
        m_restore_confirm->clear();
        updateRestoreState();
        showScreen(Screen::Restore);
        m_word_fields.first()->setFocus();
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
    m_strength = new QLabel{page};
    m_strength->setObjectName(QStringLiteral("passwordStrength"));
    layout->addWidget(m_strength);
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
    // One field per word: autocomplete from the word list, per-word checks,
    // and pasting a whole phrase into any field fills the rest.
    auto* grid_host = new QWidget{page};
    grid_host->setObjectName(QStringLiteral("recoveryPhrase"));
    auto* grid = new QGridLayout{grid_host};
    grid->setContentsMargins(0, 4, 0, 4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    auto* completer = new QCompleter{CybouDesktopModel::recoveryWordList(), grid_host};
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::InlineCompletion);
    constexpr int kColumns = 4;
    for (int i = 0; i < kPhraseWords; ++i) {
        auto* field = new QLineEdit{grid_host};
        field->setObjectName(QStringLiteral("recoveryWord%1").arg(i));
        field->setPlaceholderText(QString::number(i + 1));
        field->setAccessibleName(tr("Word %1").arg(i + 1));
        field->setCompleter(completer);
        field->setMinimumHeight(34);
        m_word_fields.append(field);
        grid->addWidget(field, i % (kPhraseWords / kColumns), i / (kPhraseWords / kColumns));
        connect(field, &QLineEdit::textEdited, this, [this, i](const QString& text) {
            const QStringList parts = SplitWords(text);
            if (parts.size() > 1) distributeWords(i, parts);
            updateRestoreState();
        });
        connect(field, &QLineEdit::textChanged, this, [this] { updateRestoreState(); });
        connect(field, &QLineEdit::returnPressed, this, [this, i] {
            if (i + 1 < m_word_fields.size()) m_word_fields.at(i + 1)->setFocus();
        });
    }
    layout->addWidget(grid_host);
    auto* paste = Button(tr("Paste phrase"), false, page);
    paste->setProperty("cybouId", QStringLiteral("pastePhrase"));
    connect(paste, &QPushButton::clicked, this, [this] {
        QString text = QGuiApplication::clipboard()->text();
        distributeWords(0, SplitWords(text));
        text.fill(QChar{0});
        updateRestoreState();
    });
    layout->addWidget(paste, 0, Qt::AlignLeft);
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

    connect(m_restore_password, &QLineEdit::textChanged, this, [this] { updateRestoreState(); });
    connect(m_restore_confirm, &QLineEdit::textChanged, this, [this] { updateRestoreState(); });
    connect(m_restore_button, &QPushButton::clicked, this, [this] { submitRestore(); });
    connect(back, &QPushButton::clicked, this, [this] {
        clearPhrase();
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
    layout->addSpacing(6);
    m_continue_restoring = Button(tr("Open CYBOU"), true, page);
    m_continue_restoring->setObjectName(QStringLiteral("primaryButton"));
    m_continue_restoring->setProperty("cybouId", QStringLiteral("continueWhileRestoring"));
    m_continue_restoring->setToolTip(tr("Mail and Files keep restoring in the background."));
    connect(m_continue_restoring, &QPushButton::clicked, this, [this] {
        const auto& status = m_model->status();
        m_model->setIdentityState(CybouIdentityState::Syncing, status.account_id, status.creation_height);
    });
    layout->addWidget(m_continue_restoring, 0, Qt::AlignLeft);
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

namespace {
/** 0 empty, 1 weak, 2 fair, 3 strong — length and character variety. */
int PasswordStrength(const QString& password)
{
    if (password.isEmpty()) return 0;
    int classes = 0;
    bool lower = false, upper = false, digit = false, other = false;
    for (const QChar ch : password) {
        if (ch.isLower()) lower = true;
        else if (ch.isUpper()) upper = true;
        else if (ch.isDigit()) digit = true;
        else other = true;
    }
    classes = int{lower} + int{upper} + int{digit} + int{other};
    if (password.size() < 12) return 1;
    if (password.size() >= 16 && classes >= 3) return 3;
    return password.size() >= 20 || classes >= 3 ? 3 : 2;
}
} // namespace

void OnboardingView::updatePasswordState()
{
    const QString password = m_password->text();
    const int strength = PasswordStrength(password);
    const QString labels[] = {QString{}, tr("Weak"), tr("Fair"), tr("Strong")};
    const QRgb colors[] = {CybouTheme::DIM, CybouTheme::ROSE, CybouTheme::AMBER, CybouTheme::BRAND_TEAL_DARK};
    QString bars;
    for (int i = 1; i <= 3; ++i) {
        bars += QStringLiteral("<span style=\"color:%1;\">&#9644;&#9644;&#9644;</span> ")
                    .arg(CybouTheme::color(i <= strength ? colors[strength] : CybouTheme::BORDER).name());
    }
    m_strength->setText(strength == 0 ? QString{}
        : bars + QStringLiteral("<span style=\"color:%1; font-weight:700;\">%2</span>")
                     .arg(CybouTheme::color(colors[strength]).name(), labels[strength]));
    m_strength->setAccessibleName(labels[strength]);
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

QString OnboardingView::enteredPhrase() const
{
    QStringList words;
    for (const auto* field : m_word_fields) words << field->text().trimmed().toLower();
    return words.join(QLatin1Char{' '});
}

void OnboardingView::clearPhrase()
{
    for (auto* field : m_word_fields) field->clear();
}

void OnboardingView::distributeWords(int start, const QStringList& words)
{
    for (int k = 0; k < words.size() && start + k < m_word_fields.size(); ++k) {
        m_word_fields.at(start + k)->setText(words.at(k).toLower());
    }
    const int next = qMin(start + static_cast<int>(words.size()), static_cast<int>(m_word_fields.size()) - 1);
    m_word_fields.at(next)->setFocus();
}

void OnboardingView::updateRestoreState()
{
    int filled = 0;
    int unknown = 0;
    for (auto* field : m_word_fields) {
        const QString word = field->text().trimmed();
        if (word.isEmpty()) {
            field->setStyleSheet({});
            continue;
        }
        ++filled;
        const bool known = CybouDesktopModel::isRecoveryWord(word);
        if (!known) ++unknown;
        field->setStyleSheet(known ? QString{} : QStringLiteral("border-color: %1;").arg(CybouTheme::color(CybouTheme::ROSE).name()));
    }
    m_phrase_count->setText(unknown > 0
        ? tr("%1 / %2 words · %3 not recognised").arg(filled).arg(kPhraseWords).arg(unknown)
        : tr("%1 / %2 words").arg(filled).arg(kPhraseWords));
    const QString password = m_restore_password->text();
    const QString confirmation = m_restore_confirm->text();
    QString hint;
    if (!password.isEmpty() && password.size() < kMinPasswordLength) {
        hint = tr("Use at least %1 characters.").arg(kMinPasswordLength);
    } else if (!confirmation.isEmpty() && password != confirmation) {
        hint = tr("The passwords do not match.");
    }
    m_restore_hint->setText(hint);
    m_restore_button->setEnabled(filled == kPhraseWords && unknown == 0 && password.size() >= kMinPasswordLength &&
        password == confirmation && m_model->capabilities().account_creation);
}

void OnboardingView::submitRestore()
{
    QString phrase = enteredPhrase();
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
    clearPhrase();
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
        // Usable once the Identity, wallet and names are back.
        m_continue_restoring->setVisible(progress.identity == CybouRestoreStepState::Done &&
            progress.wallet == CybouRestoreStepState::Done && progress.names == CybouRestoreStepState::Done &&
            !status.account_id.isEmpty());
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
