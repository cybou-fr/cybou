// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

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
