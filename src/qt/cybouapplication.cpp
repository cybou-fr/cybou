// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cybouapplication.h>

#include <qt/cyboumainwindow.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QSettings>

#include <util/translation.h>

#include <filesystem>
#include <string>

extern const TranslateFn G_TRANSLATION_FUN = [](const char* text) {
    return QCoreApplication::translate("bitcoin-core", text).toStdString();
};

namespace {

QString DefaultDataDirectory()
{
#ifdef Q_OS_WIN
    return QDir::home().filePath(QStringLiteral("AppData/Local/CYBOU"));
#elif defined(Q_OS_MACOS)
    return QDir::home().filePath(QStringLiteral("Library/Application Support/CYBOU"));
#else
    return QDir::home().filePath(QStringLiteral(".cybou"));
#endif
}

std::filesystem::path ToFilesystemPath(const QString& path)
{
    const QByteArray utf8 = path.toUtf8();
    const auto* data = reinterpret_cast<const char8_t*>(utf8.constData());
    const std::u8string path_utf8{data, static_cast<std::size_t>(utf8.size())};
    return std::filesystem::path{path_utf8};
}

} // namespace

int CybouQtMain(int argc, char* argv[])
{
    QApplication app{argc, argv};
    QCoreApplication::setOrganizationName(QStringLiteral("CYBOU"));
    QCoreApplication::setApplicationName(QStringLiteral("CYBOU"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));
    app.setApplicationDisplayName(QStringLiteral("CYBOU"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Protected communication infrastructure"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption data_dir_option(QStringList{QStringLiteral("datadir")},
        QObject::tr("Use the specified CYBOU data directory."), QObject::tr("directory"));
    parser.addOption(data_dir_option);
    parser.process(app);

    QString data_dir;
    if (parser.isSet(data_dir_option)) {
        data_dir = QDir::cleanPath(parser.value(data_dir_option));
    } else {
        QSettings settings;
        data_dir = settings.value(QStringLiteral("strDataDir"), DefaultDataDirectory()).toString();
    }
    if (!QDir{}.mkpath(data_dir)) {
        QMessageBox::critical(nullptr, QObject::tr("CYBOU"),
            QObject::tr("Could not create the data directory: %1").arg(data_dir));
        return 1;
    }

    CybouMainWindow window{ToFilesystemPath(data_dir)};
    QObject::connect(&window, &CybouMainWindow::quitRequested, &app, &QCoreApplication::quit);
    window.startRuntime();
    window.show();
    return app.exec();
}
