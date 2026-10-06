// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_TEST_CYBOUSHELLTESTS_H
#define CYBOU_QT_TEST_CYBOUSHELLTESTS_H

#include <QObject>

#include <memory>

class CybouMainWindow;

/**
 * Desktop shell smoke tests.
 *
 * These tests require no live network: they verify the CYBOU-native shell,
 * capability-driven feature availability, and desktop runtime lifecycle.
 */
class CybouShellTests : public QObject
{
    Q_OBJECT

public:
    CybouShellTests() = default;
    ~CybouShellTests() override;

private Q_SLOTS:
    void mainWindowStarts();
    void homePageIsDefault();
    void navigationSwitchesPages();
    void diagnosticsStaySecondaryWindow();
    void identityCreateFollowsFeatureAvailability();
    void restoreFlowValidatesPhrase();
    void identityPageHidesSecrets();
    void mailNavigationAndSearch();
    void composeGatesAndSends();
    void replyUsesCompleteIdentityAddress();
    void composeSelectsProtectedCybouFiles();
    void mailFilesCrossProduct();
    void walletPageShowsBalances();
    void walletLocksBalanceIntoSystemBalance();
    void contactsComeFromMailAndPayments();
    void activityListsRunningAndFailedOperations();
    void filesGateActions();
    void filesNavigationAndViews();
    void networkPageReflectsModel();
    void networkMonitorUsesCoreSnapshot();
    void authorityDashboardUsesLocalHeightObservation();
    void adapterSettersDrivePages();
    void productCollectionsDriveModel();
    void fixturesLoadDeterministically();
    void normalUiAvoidsProtocolVocabulary();
    void layoutsFitWithoutHorizontalScroll();
    void keyboardAndAsyncUnlock();
    void notificationsOfferUndo();
    void homeFirstStepsAndQuickActions();
    void globalSearchFindsMailAndFiles();
    void darkAppearanceResolvesTokens();
    void languageSwitchRebuildsShell();
    void appearanceAndLanguageSwitchPreserveMailCompose();
    void mailContextMenuAndMoves();
    void filesDropIntoFolders();
    void themeResolvesAllTokens();
    void navIconsRender();
    void closingWithoutNodeRequestsQuit();
    void runtimeStartupFailureCanBeRetried();
    void runtimeRetiresStateFromAnotherNetwork();
    void backendCommandsDriveProjection();
    void localMailCommandsWaitForCommit();
    void composerKeepsTextOnSaveAndSendFailure();
    void fileAdvancedSurvivesRefresh();
    void fileRowsRetainInteractionAcrossUpdates();
    void mailRowsRetainContextAndReplacement();
    void mailReaderKeepsContextAndClearsOnLock();
    void largeFileCatalogUpdatesInPlace();
    void folderImportIsCancellableAndPreservesStructure();
    void activityRefreshPreservesRows();
    void localRefreshRejectsStaleReplies();
    void liveFeatureAvailabilityStayHonest();
    void fixtureLifecycleFollowsBackend();
    void filesShowLocalAvailability();
    void lockHidesPrivateContent();
    void restoreFillsInProgressively();
    void liveMailAndFilesThroughCoreAdapter();
    void rotationKeepsLiveSessionWorking();
    void searchScopeAndIncrementalIndex();
    void walletAndAuthorityPreserveRowsWithoutChurn();
    void relativeTimeLocalization();
    void appearanceSwitchPreservesFullContext();
    void filesProtectionAndOfflineDownload();
    void walletForecastAndTransferReview();
    void assuranceAndRestoreResponsiveness();
    void networkPageAndSchematicFranceMap();
    void authorityExplorerAndEvidenceWorkspace();
    void ownContentInspectorAndBoundedConsole();
    void assuranceLifecycleAndRecoveryGates();

private:
    std::unique_ptr<CybouMainWindow> makeWindow();
};

#endif // CYBOU_QT_TEST_CYBOUSHELLTESTS_H
