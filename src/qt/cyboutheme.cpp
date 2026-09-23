// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboutheme.h>

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace {

QString iconResource(CybouTheme::NavIcon icon)
{
    switch (icon) {
    case CybouTheme::NavIcon::Home: return QStringLiteral(":/icons/cybou/home.svg");
    case CybouTheme::NavIcon::Identity: return QStringLiteral(":/icons/cybou/identity.svg");
    case CybouTheme::NavIcon::Email: return QStringLiteral(":/icons/cybou/email.svg");
    case CybouTheme::NavIcon::Storage: return QStringLiteral(":/icons/cybou/storage.svg");
    case CybouTheme::NavIcon::Backup: return QStringLiteral(":/icons/cybou/backup.svg");
    case CybouTheme::NavIcon::Network: return QStringLiteral(":/icons/cybou/network.svg");
    case CybouTheme::NavIcon::Settings: return QStringLiteral(":/icons/cybou/settings.svg");
    case CybouTheme::NavIcon::Diagnostics: return QStringLiteral(":/icons/cybou/diagnostics.svg");
    }
    return {};
}

/** Placeholder stroke color used inside the SVG assets. */
constexpr auto SVG_STROKE_PLACEHOLDER = "#6b7280";

} // namespace

QString CybouTheme::applicationStyleSheet()
{
    // The palette constants above are the single source of truth; the sheet
    // is assembled from them so pages can never drift from the site palette.
    const QString subtle = color(SUBTLE).name();
    const QString canvas = color(CANVAS).name();
    const QString surface = color(SURFACE).name();
    const QString border = color(BORDER).name();
    const QString border_medium = color(BORDER_MEDIUM).name();
    const QString text_primary = color(TEXT_PRIMARY).name();
    const QString text_secondary = color(TEXT_SECONDARY).name();
    const QString text_muted = color(TEXT_MUTED).name();
    const QString dim = color(DIM).name();
    const QString mint = color(MINT).name();
    const QString teal = color(BRAND_TEAL).name();
    const QString teal_dark = color(BRAND_TEAL_DARK).name();
    const QString mint_soft = color(MINT_SOFT).name();
    const QString mint_ghost = color(MINT_GHOST).name();

    return QStringLiteral(R"(
        QMainWindow#cybouMainWindow, QWidget#shell, QStackedWidget { background: %1; color: %7; }
        QMenuBar { background: %2; border-bottom: 1px solid %4; padding: 4px 8px; color: %7; }
        QMenuBar::item:selected, QMenu::item:selected { background: %12; color: %11; }
        QMenu { background: %2; border: 1px solid %4; color: %7; }

        QFrame#sidebar { background: %2; border-right: 1px solid %4; }
        QLabel#brand { font-size: 23px; font-weight: 800; letter-spacing: 1px; color: %7; }
        QLabel#brandCaption { font-size: 11px; font-weight: 700; color: %11; }
        QLabel#sidebarFootnote { color: %9; font-size: 12px; line-height: 1.4; }

        QToolButton { border: 0; border-radius: 10px; padding: 9px 12px; text-align: left; color: %8; font-size: 14px; }
        QToolButton:hover { background: %3; color: %11; }
        QToolButton:checked { background: %12; color: %11; font-weight: 700; }
        QFrame#separator { color: %4; }

        QFrame#card { background: %2; border: 1px solid %4; border-radius: 14px; }
        QFrame#iconChip { background: %13; border-radius: 20px; }
        QLabel#heroAccent { background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, stop:0 %12, stop:1 %1); }
        QLabel#eyebrow { color: %9; font-size: 11px; font-weight: 800; letter-spacing: 3px; }
        QLabel#heroTitle { color: %7; font-size: 30px; font-weight: 800; }
        QLabel#heroSubtitle { color: %8; font-size: 14px; }
        QLabel#pageTitle { color: %7; font-size: 28px; font-weight: 800; }
        QLabel#pageSubtitle { color: %9; font-size: 14px; }
        QLabel#sectionTitle { color: %7; font-size: 19px; font-weight: 750; }
        QLabel#cardLabel { color: %9; font-size: 12px; font-weight: 700; }
        QLabel#cardTitle { color: %7; font-size: 21px; font-weight: 800; }
        QLabel#serviceTitle { color: %7; font-size: 16px; font-weight: 750; }
        QLabel#bodyText { color: %8; font-size: 14px; }
        QLabel#mutedText { color: %9; font-size: 13px; }
        QLabel#metric { color: %7; font-size: 26px; font-weight: 800; }
        QLabel#metricCaption { color: %9; font-size: 12px; }
        QLabel#statusBadge { background: %12; color: %11; border-radius: 13px; padding: 6px 12px; font-weight: 700; }
        QLabel#neutralBadge { background: %3; color: %9; border-radius: 11px; padding: 4px 9px; }
        QLabel#warningBadge { background: #fef3c7; color: #92400e; border-radius: 11px; padding: 6px 12px; font-weight: 700; }
        QLabel#phaseLabel { color: %10; font-size: 12px; font-weight: 600; }
        QLabel#phaseLabelActive { color: %11; font-size: 12px; font-weight: 800; }
        QLabel#phaseLabelDone { color: %8; font-size: 12px; font-weight: 600; }

        QPushButton { min-height: 34px; border-radius: 8px; padding: 4px 16px; font-weight: 700; }
        QPushButton#primaryButton { background: %14; color: white; border: 1px solid %14; }
        QPushButton#primaryButton:hover { background: %11; border-color: %11; }
        QPushButton#secondaryButton { background: %2; color: %11; border: 1px solid %5; }
        QPushButton#secondaryButton:hover { background: %13; border-color: %15; }
        QPushButton:disabled { background: %3; color: %10; border-color: %4; }

        QCheckBox { color: %7; font-size: 14px; spacing: 8px; }
        QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1px solid %5; background: %2; }
        QCheckBox::indicator:checked { background: %14; border-color: %14; }
    )")
        .arg(subtle)        //  1 app background
        .arg(canvas)        //  2 card / canvas
        .arg(surface)       //  3 surface
        .arg(border)        //  4 border subtle
        .arg(border_medium) //  5 border medium
        .arg(text_primary)  //  6 (reserved)
        .arg(text_primary)  //  7 primary text
        .arg(text_secondary)//  8 secondary text
        .arg(text_muted)    //  9 muted text
        .arg(dim)           // 10 dim
        .arg(teal_dark)     // 11 teal dark (active ink)
        .arg(mint_soft)     // 12 mint soft (selected bg)
        .arg(mint_ghost)    // 13 mint ghost (hover / chips)
        .arg(teal)          // 14 brand teal (primary action)
        .arg(mint);         // 15 mint accent
}

QPixmap CybouTheme::iconPixmap(NavIcon icon, const QSize& size, const QColor& stroke)
{
    QFile file{iconResource(icon)};
    if (!file.open(QIODevice::ReadOnly)) return {};

    QByteArray svg = file.readAll();
    svg.replace(SVG_STROKE_PLACEHOLDER, stroke.name().toUtf8());

    QSvgRenderer renderer{svg};
    if (!renderer.isValid()) return {};

    // Render at 2x and tag the device pixel ratio so icons stay crisp on HiDPI.
    QPixmap pixmap{size * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    renderer.render(&painter, QRectF{QPointF{0, 0}, QSizeF{size}});
    return pixmap;
}

QIcon CybouTheme::navIcon(NavIcon icon)
{
    const QSize size{22, 22};
    QIcon result;
    result.addPixmap(iconPixmap(icon, size, color(DIM)), QIcon::Normal, QIcon::Off);
    result.addPixmap(iconPixmap(icon, size, color(DIM)), QIcon::Active, QIcon::Off);
    result.addPixmap(iconPixmap(icon, size, color(BRAND_TEAL_DARK)), QIcon::Normal, QIcon::On);
    result.addPixmap(iconPixmap(icon, size, color(BRAND_TEAL_DARK)), QIcon::Active, QIcon::On);
    return result;
}

void CybouTheme::applyTo(QApplication& app)
{
    app.setStyleSheet(applicationStyleSheet());
}
