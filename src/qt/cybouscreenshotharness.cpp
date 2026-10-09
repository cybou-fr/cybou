// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Выделенный harness для создания скриншотов окон CYBOU без блокировки production UI.

#include <qt/cybouscreenshotharness.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cybouuifixtures.h>
#include <qt/cybouconsoledialog.h>
#include <qt/authorityreview.h>
#include <QMessageBox>
#include <cybou/official_networks.h>
#include <qt/cybouapplicationbackend.h>

#include <qt/pages/homepage.h>
#include <qt/pages/emailpage.h>
#include <qt/pages/storagepage.h>
#include <qt/pages/networkpage.h>
#include <qt/pages/onboardingview.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QPushButton>
#include <QToolButton>
#include <QDialog>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

namespace cybou::gui {

void RunScreenshotHarness(CybouMainWindow* window, const QString& directory)
{
    if (!window) return;
    // Dev-only QA harness (CYBOU_SCREENSHOT_DIR): captures the named
    // screens for the active fixture and quits. File names are
    // <prefix><screen>.png, e.g. 1280x860-mail-reader.png.
    QDir{}.mkpath(directory);
    const QString prefix = qEnvironmentVariable("CYBOU_SCREENSHOT_PREFIX");
    const QString fixture = CybouUiFixtures::requestedFixture();
    const auto save = [window, directory, prefix](const QString& screen) {
        QEventLoop settle;
        QTimer::singleShot(250, &settle, &QEventLoop::quit);
        settle.exec();
        for (int i = 0; i < 3; ++i) qApp->processEvents();
        window->grab().save(QDir{directory}.filePath(QStringLiteral("%1%2.png").arg(prefix, screen)));
    };
    auto* home = static_cast<HomePage*>(window->page(CybouPage::Home));
    auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
    auto* model = window->desktopModel();

    if (fixture == QLatin1String{"empty"} || fixture.isEmpty()) {
        window->showPage(CybouPage::Home);
        save(QStringLiteral("home-empty"));
        home->onboarding()->showScreen(OnboardingView::Screen::Restore);
        save(QStringLiteral("identity-restore"));
    } else if (fixture == QLatin1String{"restoring"}) {
        window->showPage(CybouPage::Home);
        save(QStringLiteral("identity-restoring"));
    } else if (fixture == QLatin1String{"offline"}) {
        window->showPage(CybouPage::Mail);
        mail->setView(EmailPage::View::Sent);
        mail->openMessage(QStringLiteral("m-outgoing"));
        save(QStringLiteral("mail-offline"));
    } else {
        window->showPage(CybouPage::Home);
        save(QStringLiteral("home-active"));
        window->showPage(CybouPage::Identity);
        save(QStringLiteral("identity-active"));

        window->showPage(CybouPage::Mail);
        mail->setView(EmailPage::View::Inbox);
        save(QStringLiteral("mail-inbox"));
        mail->openMessage(QStringLiteral("m-project"));
        save(QStringLiteral("mail-reader"));
        mail->setView(EmailPage::View::Sent);
        mail->openMessage(QStringLiteral("m-sent-securing"));
        save(QStringLiteral("mail-attachment-progress"));
        mail->openMessage(QStringLiteral("m-sent-validated"));
        save(QStringLiteral("mail-validated"));
        mail->setView(EmailPage::View::Inbox);
        CybouMailItem draft;
        draft.to_name = QStringLiteral("alice.cybou");
        draft.subject = QObject::tr("Project files");
        draft.body = QObject::tr("Hello Alice,\n\nHere are the final files.\n\nStan");
        if (const auto attachment = model->attachmentFromFile(QStringLiteral("f-report"))) draft.attachments = {*attachment};
        mail->openCompose(draft);
        save(QStringLiteral("mail-compose"));

        window->showPage(CybouPage::Files);
        files->setView(StoragePage::View::MyFiles);
        save(QStringLiteral("files-list"));
        if (auto* table = files->findChild<QTreeWidget*>(QStringLiteral("filesTable")); table && table->topLevelItemCount()) {
            table->topLevelItem(0)->setSelected(true);
            save(QStringLiteral("files-selection"));
            table->clearSelection();
        }
        files->setGridMode(true);
        save(QStringLiteral("files-grid"));
        files->setGridMode(false);
        files->showDetails(QStringLiteral("f-report"));
        save(QStringLiteral("files-details"));
        files->showDetails({});
        QTemporaryDir temp;
        const QString upload = temp.filePath(QStringLiteral("presentation.pdf"));
        QFile file{upload};
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QByteArray(2 * 1024 * 1024, 'x'));
            file.close();
            const QString id = model->requestFileUpload(upload);
            model->setFileState(id, CybouContentState::Securing, 42);
        }
        save(QStringLiteral("files-upload"));

        window->showPage(CybouPage::Wallet);
        save(QStringLiteral("wallet"));
        save(QStringLiteral("wallet-pending")); // fixture activity: Waiting / Finalized
        window->showPage(CybouPage::Network);
        // Synthetic local diagnostics only in the explicit screenshot fixture.
        cybou::NodeDiagnosticsSnapshot map_snapshot;
        // The fixture uses the compiled public DEVNET profile. Peer observations
        // remain synthetic and do not constitute live network evidence.
        map_snapshot.network_binding = cybou::ComputeNetworkBinding(
            cybou::RequireOfficialNetwork("devnet").genesis.GetNetworkPublicKey()).GetHex();
        map_snapshot.peers = {{"51.255.46.58:29461", 48213, ""}, {"51.255.46.58:29462", 48212, ""}, {"127.0.0.1:29461", 48213, ""}};
        model->setNetworkDiagnostics(map_snapshot);
        save(QStringLiteral("network"));
        auto* network = static_cast<NetworkPage*>(window->page(CybouPage::Network));
        network->selectPeer(0);
        save(QStringLiteral("network-peer"));
        network->showAdvanced();
        save(QStringLiteral("network-advanced-peer"));
        network->findChild<QTabWidget*>(QStringLiteral("networkAdvancedTabs"))->setCurrentIndex(2);
        save(QStringLiteral("network-storage"));
        network->findChild<QPushButton*>(QStringLiteral("networkAdvancedButton"))->setChecked(false);
        map_snapshot.peers.erase(map_snapshot.peers.begin());
        model->setNetworkDiagnostics(map_snapshot);
        save(QStringLiteral("network-known-peer"));
        if (auto* backend = model->applicationBackend()) {
            Q_EMIT backend->applicationLoadChanged(CybouApplicationLoadState::Loading, 35, 100, {});
            qApp->processEvents();
            if (auto* loading = window->findChild<QDialog*>(QStringLiteral("applicationLoadingDialog"))) {
                loading->grab().save(QDir{directory}.filePath(prefix + QStringLiteral("identity-loading.png")));
                window->showPage(CybouPage::Files);
                if (auto* local_btn = loading->findChild<QPushButton*>(QStringLiteral("applicationLoadingLocalButton"))) {
                    local_btn->click();
                }
            }
            save(QStringLiteral("recovery-background"));
            Q_EMIT backend->applicationLoadChanged(CybouApplicationLoadState::Ready, 0, 0, {});
        }
        window->showNetworkDiagnostics();
        save(QStringLiteral("diagnostics"));
        if (auto* launch = window->findChild<QToolButton*>(QStringLiteral("networkMonitorButton"))) {
            launch->click();
            qApp->processEvents();
            if (auto* table = window->findChild<QWidget*>(QStringLiteral("networkMonitorPeers"))) {
                if (auto* dialog = qobject_cast<QDialog*>(table->window())) {
                    dialog->grab().save(QDir{directory}.filePath(prefix + QStringLiteral("network-monitor.png")));
                    dialog->close();
                }
            }
        }

        {
            CybouConsoleDialog console{model, window};
            console.show();
            console.executeCommand(QStringLiteral("help"));
            console.executeCommand(QStringLiteral("status"));
            console.executeCommand(QStringLiteral("storage"));
            qApp->processEvents();
            console.grab().save(QDir{directory}.filePath(prefix + QStringLiteral("console-user.png")));
        }
        window->showPage(CybouPage::Settings);
        save(QStringLiteral("settings"));

        // Operator console with a representative finalizer snapshot.
        CybouNetworkAuthorityStatus authority;
        authority.proven = true;
        authority.signer_enabled = true;
        authority.finalizer = CybouFinalizerState::Finalizing;
        authority.finalized_height = 48'213;
        authority.candidates = 4;
        authority.identities = 1'284;
        authority.names = 911;
        authority.pending_name_commits = 7;
        authority.total_balance = 12'480'300;
        authority.total_system_balance = 3'902'144;
        authority.storage_escrow = 1'204'500;
        authority.safety_journal_status = QStringLiteral("Synthetic signing journal observation (fixture)");
        authority.settlement_due = true;
        authority.next_settlement_period = 3;
        authority.next_settlement_start_utc = 86400;
        authority.next_settlement_due_utc = 172800;
        model->setNetworkAuthority(authority);
        {
            CybouConsoleDialog console{model, window};
            console.show();
            console.executeCommand(QStringLiteral("help"));
            console.executeCommand(QStringLiteral("authority status"));
            qApp->processEvents();
            console.grab().save(QDir{directory}.filePath(prefix + QStringLiteral("console-authority.png")));
        }
        window->showPage(CybouPage::NetworkAuthority);
        save(QStringLiteral("network-authority"));
        QTimer::singleShot(100,window,[window,directory,prefix] {
            if (auto* review = window->findChild<QMessageBox*>(QStringLiteral("authoritySettlementReview"))) {
                review->grab().save(QDir{directory}.filePath(prefix + QStringLiteral("authority-settlement-review.png")));
                for (auto* button : review->buttons()) {
                    if (review->buttonRole(button) == QMessageBox::ActionRole) button->click();
                }
                qApp->processEvents();
                review->grab().save(QDir{directory}.filePath(prefix + QStringLiteral("authority-settlement-entries.png")));
                review->reject();
            }
        });
        ReviewStorageSettlement(model,3,86400,172800,{{cybou::Hash256{1},cybou::AccountId{cybou::Hash256{2}},250}},window);
        QTimer::singleShot(100, window, [window,directory,prefix] {
            if (auto* review = window->findChild<QMessageBox*>(QStringLiteral("authorityPauseReview"))) {
                review->grab().save(QDir{directory}.filePath(prefix + QStringLiteral("authority-pause-review.png")));
                review->reject();
            }
        });
        for (auto* button : window->findChildren<QPushButton*>()) {
            if (button->property("cybouId").toString() == QStringLiteral("authorityPause")) { button->click(); break; }
        }
        authority.finalizer = CybouFinalizerState::Paused;
        model->setNetworkAuthority(authority);
        save(QStringLiteral("network-authority-paused"));
        authority.finalizer = CybouFinalizerState::SafetyHalt;
        authority.signer_enabled = false;
        model->setNetworkAuthority(authority);
        save(QStringLiteral("network-authority-safety-halt"));
        model->setNetworkAuthority({});
        model->setIdentityState(CybouIdentityState::Locked, model->status().account_id, model->status().creation_height);
        model->setBackgroundFinalizerActive(true);
        save(QStringLiteral("authority-vault-locked"));
        model->setBackgroundFinalizerActive(false);
        model->setIdentityState(CybouIdentityState::Active, QStringLiteral("fixture-account"), 1);

        model->setFileItems({});
        window->showPage(CybouPage::Files);
        save(QStringLiteral("files-empty"));
    }
    qApp->quit();
}

} // namespace cybou::gui
