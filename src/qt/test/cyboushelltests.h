// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_TEST_CYBOUSHELLTESTS_H
#define BITCOIN_QT_TEST_CYBOUSHELLTESTS_H

#include <QObject>

#include <memory>

class CybouMainWindow;
class PlatformStyle;

namespace interfaces {
class Node;
}

/**
 * Desktop shell smoke tests.
 *
 * These tests require no live network and no node model: they verify the
 * CYBOU-native shell structure, navigation, diagnostics window lifetime
 * and capability-driven feature availability.
 */
class CybouShellTests : public QObject
{
    Q_OBJECT

public:
    explicit CybouShellTests(interfaces::Node& node);
    ~CybouShellTests() override;

private Q_SLOTS:
    void mainWindowStarts();
    void homePageIsDefault();
    void navigationSwitchesPages();
    void diagnosticsStaySecondaryWindow();
    void identityCreateFollowsCapabilities();
    void emailPageGatesSending();
    void walletPageShowsBalances();
    void storageAndBackupExposeNoOperations();
    void networkPageReflectsModel();
    void themeResolvesAllTokens();
    void navIconsRender();
    void closingWithoutNodeRequestsQuit();

private:
    interfaces::Node& m_node;
    std::unique_ptr<const PlatformStyle> m_platform_style;

    std::unique_ptr<CybouMainWindow> makeWindow();
};

#endif // BITCOIN_QT_TEST_CYBOUSHELLTESTS_H
