// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cybouapplication.h>

#include <qt/cyboumainwindow.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
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
    Q_INIT_RESOURCE(cybou);
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
    QCommandLineOption network_option(QStringList{QStringLiteral("network")},
        QObject::tr("Use an explicit network file with an isolated --datadir."), QObject::tr("file"));
    parser.addOption(network_option);
    QCommandLineOption peer_option(QStringList{QStringLiteral("peer")},
        QObject::tr("Connect to this CYP2 IP:port endpoint."), QObject::tr("endpoint"));
    parser.addOption(peer_option);
    parser.process(app);

    if (parser.isSet(peer_option)) {
        const auto peer = parser.value(peer_option);
        const auto colon = peer.lastIndexOf(QLatin1Char(':'));
        bool valid_port{false};
        const auto port = peer.mid(colon+1).toUInt(&valid_port);
        auto host = peer.left(colon);
        if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) host = host.mid(1,host.size()-2);
        if (colon<=0 || host.isEmpty() || !valid_port || port==0 || port>65535) {
            QMessageBox::critical(nullptr, QObject::tr("CYBOU"), QObject::tr("--peer requires IP:port."));
            return 1;
        }
        qputenv("CYBOU_DEV_P2P_HOST",host.toUtf8());
        qputenv("CYBOU_DEV_P2P_PORT",QByteArray::number(port));
    }

    if (parser.isSet(network_option)) {
        if (!parser.isSet(data_dir_option) || !parser.isSet(peer_option)) {
            QMessageBox::critical(nullptr, QObject::tr("CYBOU"), QObject::tr("--network requires an explicit isolated --datadir and --peer."));
            return 1;
        }
        qputenv("CYBOU_NETWORK_FILE", QFileInfo{parser.value(network_option)}.absoluteFilePath().toUtf8());
    }
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
