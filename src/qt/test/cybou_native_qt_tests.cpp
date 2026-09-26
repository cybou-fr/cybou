// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <QApplication>
#include <QCoreApplication>
#include <QTest>

int main(int argc, char* argv[])
{
    // Force CYBOU's own icon resources out of the cybou_qt static archive.
    Q_INIT_RESOURCE(cybou);
    QApplication app{argc, argv};
    QCoreApplication::setOrganizationName(QStringLiteral("CYBOU"));
    QCoreApplication::setApplicationName(QStringLiteral("CYBOU-native-qt-test"));

    CybouShellTests tests;
    return QTest::qExec(&tests, argc, argv);
}
