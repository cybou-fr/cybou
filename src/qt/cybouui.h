// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUUI_H
#define BITCOIN_QT_CYBOUUI_H

#include <qt/cyboutheme.h>

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

/**
 * Shared CYBOU desktop widget factories.
 *
 * Every page composes its surface from these helpers so the redesign stays
 * consistent: cards, hero headers, tinted icon chips, initial avatars, pills,
 * rows and the line-glyph set. Text passed in is already translated by the
 * caller; helpers never call tr() themselves.
 */
namespace CybouUi {

/* ------------------------------------------------------------------ *
 * Line glyph set (24x24 stroke icons, recolored on render).
 * ------------------------------------------------------------------ */
enum class Glyph {
    Bell, Send, Receive, Download, Upload, Share, Star, Search, Sliders,
    Plus, DotsH, DotsV, Reply, Forward, Folder, File, FileText, Image, Lock,
    ShieldCheck, Check, CheckCircle, Clock, Globe, Wifi, Monitor, Smartphone,
    Palette, User, Users, Key, Refresh, Trash, Archive, Inbox, ChevronRight,
    ChevronLeft, Copy, Info, Database, CloudUp, Camera, Gear, Sparkles,
    ArrowUpRight, ArrowDownLeft, ArrowRight, WalletCard, GridView, ListView,
    Compose, Transfer, Filter, History, MapPin, Server, Eye, Envelope,
};

inline QString glyphPaths(Glyph glyph)
{
    switch (glyph) {
    case Glyph::Bell:
        return QStringLiteral(R"(<path d="M6 9a6 6 0 0 1 12 0c0 5 2 6 2 6H4s2-1 2-6"/><path d="M10 20a2 2 0 0 0 4 0"/>)");
    case Glyph::Send:
        return QStringLiteral(R"(<path d="M22 2 11 13"/><path d="M22 2l-7 20-4-9-9-4 20-7z"/>)");
    case Glyph::Receive:
        return QStringLiteral(R"(<path d="M17 7 7 17"/><path d="M16 17H7V8"/>)");
    case Glyph::Download:
        return QStringLiteral(R"(<path d="M12 3v12"/><path d="m7 10 5 5 5-5"/><path d="M4 21h16"/>)");
    case Glyph::Upload:
        return QStringLiteral(R"(<path d="M12 21V9"/><path d="m7 14 5-5 5 5"/><path d="M4 3h16"/>)");
    case Glyph::Share:
        return QStringLiteral(R"(<circle cx="6" cy="12" r="3"/><circle cx="18" cy="6" r="3"/><circle cx="18" cy="18" r="3"/><path d="m8.7 10.8 6.6-3.6"/><path d="m8.7 13.2 6.6 3.6"/>)");
    case Glyph::Star:
        return QStringLiteral(R"(<path d="m12 3 2.5 6 6.5.5-5 4.5 1.5 6.5L12 17l-5.5 3.5L8 14 3 9.5 9.5 9z"/>)");
    case Glyph::Search:
        return QStringLiteral(R"(<circle cx="11" cy="11" r="7"/><path d="m21 21-4.5-4.5"/>)");
    case Glyph::Sliders:
        return QStringLiteral(R"(<path d="M4 8h10"/><path d="M18 8h2"/><circle cx="16" cy="8" r="2.2"/><path d="M4 16h2"/><path d="M10 16h10"/><circle cx="8" cy="16" r="2.2"/>)");
    case Glyph::Plus:
        return QStringLiteral(R"(<path d="M12 5v14"/><path d="M5 12h14"/>)");
    case Glyph::DotsH:
        return QStringLiteral(R"(<circle cx="5" cy="12" r="1.3" fill="#6b7280" stroke="none"/><circle cx="12" cy="12" r="1.3" fill="#6b7280" stroke="none"/><circle cx="19" cy="12" r="1.3" fill="#6b7280" stroke="none"/>)");
    case Glyph::DotsV:
        return QStringLiteral(R"(<circle cx="12" cy="5" r="1.3" fill="#6b7280" stroke="none"/><circle cx="12" cy="12" r="1.3" fill="#6b7280" stroke="none"/><circle cx="12" cy="19" r="1.3" fill="#6b7280" stroke="none"/>)");
    case Glyph::Reply:
        return QStringLiteral(R"(<path d="M9 14 4 9l5-5"/><path d="M4 9h10a6 6 0 0 1 6 6v4"/>)");
    case Glyph::Forward:
        return QStringLiteral(R"(<path d="m15 14 5-5-5-5"/><path d="M20 9H10a6 6 0 0 0-6 6v4"/>)");
    case Glyph::Folder:
        return QStringLiteral(R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)");
    case Glyph::File:
        return QStringLiteral(R"(<path d="M6 2h8l4 4v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2z"/><path d="M14 2v4h4"/>)");
    case Glyph::FileText:
        return QStringLiteral(R"(<path d="M6 2h8l4 4v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2z"/><path d="M14 2v4h4"/><path d="M8 12h8"/><path d="M8 16h5"/>)");
    case Glyph::Image:
        return QStringLiteral(R"(<rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="8.5" cy="9.5" r="1.5"/><path d="m21 15-5-5-9 9"/>)");
    case Glyph::Lock:
        return QStringLiteral(R"(<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>)");
    case Glyph::ShieldCheck:
        return QStringLiteral(R"(<path d="M12 3l7 3v5c0 5-3.5 8-7 10-3.5-2-7-5-7-10V6z"/><path d="m9 12 2 2 4-4"/>)");
    case Glyph::Check:
        return QStringLiteral(R"(<path d="m5 12 5 5L20 7"/>)");
    case Glyph::CheckCircle:
        return QStringLiteral(R"(<circle cx="12" cy="12" r="9"/><path d="m8.5 12.5 2.5 2.5 5-5"/>)");
    case Glyph::Clock:
        return QStringLiteral(R"(<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 3"/>)");
    case Glyph::Globe:
        return QStringLiteral(R"(<circle cx="12" cy="12" r="9"/><path d="M3 12h18"/><path d="M12 3c3 3.5 3 14.5 0 18"/><path d="M12 3c-3 3.5-3 14.5 0 18"/>)");
    case Glyph::Wifi:
        return QStringLiteral(R"(<path d="M5 12a10 10 0 0 1 14 0"/><path d="M8.5 15.5a5.5 5.5 0 0 1 7 0"/><circle cx="12" cy="19" r="1" fill="#6b7280" stroke="none"/>)");
    case Glyph::Monitor:
        return QStringLiteral(R"(<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M9 20h6"/><path d="M12 16v4"/>)");
    case Glyph::Smartphone:
        return QStringLiteral(R"(<rect x="7" y="2" width="10" height="20" rx="2"/><path d="M11 5h2"/><circle cx="12" cy="18" r="1" fill="#6b7280" stroke="none"/>)");
    case Glyph::Palette:
        return QStringLiteral(R"(<path d="M12 3a9 9 0 1 0 0 18h1.2a2.4 2.4 0 0 0 0-4.8H12a2 2 0 0 1 0-4h5.8a3 3 0 0 0 3-3c0-3.4-4.3-6.2-8.8-6.2z"/><circle cx="7.5" cy="10.5" r="1" fill="#6b7280" stroke="none"/><circle cx="12" cy="7.5" r="1" fill="#6b7280" stroke="none"/><circle cx="16.5" cy="10.5" r="1" fill="#6b7280" stroke="none"/>)");
    case Glyph::User:
        return QStringLiteral(R"(<circle cx="12" cy="8" r="4"/><path d="M4 21c0-4 3.6-6 8-6s8 2 8 6"/>)");
    case Glyph::Users:
        return QStringLiteral(R"(<circle cx="9" cy="8" r="3.5"/><path d="M3 19c0-3.4 2.8-5 6-5 1.4 0 2.8.4 3.8 1.2"/><circle cx="17" cy="9" r="3"/><path d="M16 14.6c2.9.4 5 1.9 5 4.4"/>)");
    case Glyph::Key:
        return QStringLiteral(R"(<circle cx="8" cy="15" r="4.5"/><path d="m11.5 11.5 8.5-8.5"/><path d="m16 7 3 3"/><path d="m13 10 2.5 2.5"/>)");
    case Glyph::Refresh:
        return QStringLiteral(R"(<path d="M20 12a8 8 0 1 1-2.5-5.8"/><path d="M20 4v4h-4"/>)");
    case Glyph::Trash:
        return QStringLiteral(R"(<path d="M4 7h16"/><path d="M9 7V5a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"/><path d="m6 7 1 13a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2l1-13"/><path d="M10 11v6"/><path d="M14 11v6"/>)");
    case Glyph::Archive:
        return QStringLiteral(R"(<path d="M4 4h16v4H4z"/><path d="M6 8v10a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V8"/><path d="M10 12h4"/>)");
    case Glyph::Inbox:
        return QStringLiteral(R"(<path d="M3 13l3-8h12l3 8v5a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><path d="M3 13h6a3 3 0 0 0 6 0h6"/>)");
    case Glyph::ChevronRight:
        return QStringLiteral(R"(<path d="m9 6 6 6-6 6"/>)");
    case Glyph::ChevronLeft:
        return QStringLiteral(R"(<path d="m15 6-6 6 6 6"/>)");
    case Glyph::Copy:
        return QStringLiteral(R"(<rect x="9" y="9" width="12" height="12" rx="2"/><path d="M5 15V5a2 2 0 0 1 2-2h10"/>)");
    case Glyph::Info:
        return QStringLiteral(R"(<circle cx="12" cy="12" r="9"/><path d="M12 11v5"/><circle cx="12" cy="8" r="1" fill="#6b7280" stroke="none"/>)");
    case Glyph::Database:
        return QStringLiteral(R"(<ellipse cx="12" cy="5" rx="8" ry="3"/><path d="M4 5v14c0 1.7 3.6 3 8 3s8-1.3 8-3V5"/><path d="M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3"/>)");
    case Glyph::CloudUp:
        return QStringLiteral(R"(<path d="M6.5 19a4.5 4.5 0 0 1-.4-9A6 6 0 0 1 18 8.6 4 4 0 0 1 17.5 19z"/><path d="M12 12v6"/><path d="m9 15 3 3 3-3"/>)");
    case Glyph::Camera:
        return QStringLiteral(R"(<path d="M3 8a2 2 0 0 1 2-2h2l1.5-2h7L17 6h2a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><circle cx="12" cy="13" r="3.5"/>)");
    case Glyph::Gear:
        return QStringLiteral(R"(<circle cx="12" cy="12" r="3"/><path d="M12 2v3"/><path d="M12 19v3"/><path d="M2 12h3"/><path d="M19 12h3"/><path d="m4.9 4.9 2.1 2.1"/><path d="m17 17 2.1 2.1"/><path d="m19.1 4.9-2.1 2.1"/><path d="m7 17-2.1 2.1"/>)");
    case Glyph::Sparkles:
        return QStringLiteral(R"(<path d="m12 4 1.8 4.2L18 10l-4.2 1.8L12 16l-1.8-4.2L6 10l4.2-1.8z"/><path d="m19 15 .9 2.1L22 18l-2.1.9L19 21l-.9-2.1L16 18l2.1-.9z"/>)");
    case Glyph::ArrowUpRight:
        return QStringLiteral(R"(<path d="M7 17 17 7"/><path d="M8 7h9v9"/>)");
    case Glyph::ArrowDownLeft:
        return QStringLiteral(R"(<path d="M17 7 7 17"/><path d="M16 17H7V8"/>)");
    case Glyph::ArrowRight:
        return QStringLiteral(R"(<path d="M4 12h16"/><path d="m14 6 6 6-6 6"/>)");
    case Glyph::WalletCard:
        return QStringLiteral(R"(<rect x="3" y="6" width="18" height="13" rx="2"/><path d="M3 10h18"/><circle cx="17" cy="15" r="1.2" fill="#6b7280" stroke="none"/>)");
    case Glyph::GridView:
        return QStringLiteral(R"(<rect x="4" y="4" width="7" height="7" rx="1.5"/><rect x="13" y="4" width="7" height="7" rx="1.5"/><rect x="4" y="13" width="7" height="7" rx="1.5"/><rect x="13" y="13" width="7" height="7" rx="1.5"/>)");
    case Glyph::ListView:
        return QStringLiteral(R"(<path d="M4 6h16"/><path d="M4 12h16"/><path d="M4 18h16"/>)");
    case Glyph::Compose:
        return QStringLiteral(R"(<path d="M12 20h9"/><path d="M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z"/>)");
    case Glyph::Transfer:
        return QStringLiteral(R"(<path d="M4 8h13"/><path d="m13 4 4 4-4 4"/><path d="M20 16H7"/><path d="m11 12-4 4 4 4"/>)");
    case Glyph::Filter:
        return QStringLiteral(R"(<path d="M4 5h16l-6 7v6l-4 2v-8z"/>)");
    case Glyph::History:
        return QStringLiteral(R"(<path d="M4 12a8 8 0 1 0 2.5-5.8"/><path d="M4 4v4h4"/><path d="M12 8v4l3 2"/>)");
    case Glyph::MapPin:
        return QStringLiteral(R"(<path d="M12 21s-7-5.4-7-11a7 7 0 0 1 14 0c0 5.6-7 11-7 11z"/><circle cx="12" cy="10" r="2.5"/>)");
    case Glyph::Server:
        return QStringLiteral(R"(<rect x="4" y="4" width="16" height="6" rx="1.5"/><rect x="4" y="14" width="16" height="6" rx="1.5"/><circle cx="8" cy="7" r="1" fill="#6b7280" stroke="none"/><circle cx="8" cy="17" r="1" fill="#6b7280" stroke="none"/>)");
    case Glyph::Eye:
        return QStringLiteral(R"(<path d="M2 12s3.5-6 10-6 10 6 10 6-3.5 6-10 6-10-6-10-6z"/><circle cx="12" cy="12" r="2.5"/>)");
    case Glyph::Envelope:
        return QStringLiteral(R"(<rect x="3" y="5" width="18" height="14" rx="2"/><path d="m3 7 9 6 9-6"/>)");
    }
    return {};
}

constexpr auto GLYPH_STROKE_PLACEHOLDER = "#6b7280";

inline QPixmap glyphPixmap(Glyph glyph, const QSize& size, const QColor& stroke)
{
    QString svg{QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
        "stroke='#6b7280' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>")};
    svg += glyphPaths(glyph);
    svg += QStringLiteral("</svg>");
    svg.replace(GLYPH_STROKE_PLACEHOLDER, stroke.name());

    QSvgRenderer renderer{svg.toUtf8()};
    if (!renderer.isValid()) return {};

    QPixmap pixmap{size * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    renderer.render(&painter, QRectF{QPointF{0, 0}, QSizeF{size}});
    return pixmap;
}

/** Glyph rendered with a red numeric badge in the top-right corner. */
inline QPixmap glyphWithBadge(Glyph glyph, const QSize& size, const QColor& stroke, int badge)
{
    QPixmap base = glyphPixmap(glyph, size, stroke);
    if (badge <= 0) return base;

    const QSize canvas = size + QSize{10, 10};
    QPixmap pixmap{canvas * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 8, base);

    const QRectF badge_rect{QPointF{size.width() - 6, 0}, QSizeF{16, 16}};
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor{CybouTheme::BADGE_RED});
    painter.drawEllipse(badge_rect);
    painter.setPen(QPen{QColor{0xffffff}});
    QFont font{painter.font()};
    font.setPixelSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(badge_rect, Qt::AlignCenter, QString::number(badge));
    return pixmap;
}

/* ------------------------------------------------------------------ *
 * Tints
 * ------------------------------------------------------------------ */
enum class Tint { Mint, Blue, Indigo, Violet, Amber, Rose, Neutral };

inline QRgb tintSoft(Tint tint)
{
    switch (tint) {
    case Tint::Mint: return CybouTheme::MINT_SOFT;
    case Tint::Blue: return CybouTheme::BLUE_SOFT;
    case Tint::Indigo: return CybouTheme::INDIGO_SOFT;
    case Tint::Violet: return CybouTheme::VIOLET_SOFT;
    case Tint::Amber: return CybouTheme::AMBER_SOFT;
    case Tint::Rose: return CybouTheme::ROSE_SOFT;
    case Tint::Neutral: return CybouTheme::SURFACE;
    }
    return CybouTheme::SURFACE;
}

inline QRgb tintInk(Tint tint)
{
    switch (tint) {
    case Tint::Mint: return CybouTheme::BRAND_TEAL_DARK;
    case Tint::Blue: return CybouTheme::BLUE;
    case Tint::Indigo: return CybouTheme::INDIGO;
    case Tint::Violet: return CybouTheme::VIOLET;
    case Tint::Amber: return CybouTheme::AMBER;
    case Tint::Rose: return CybouTheme::ROSE;
    case Tint::Neutral: return CybouTheme::TEXT_MUTED;
    }
    return CybouTheme::TEXT_MUTED;
}

inline const char* tintName(Tint tint)
{
    switch (tint) {
    case Tint::Mint: return "mint";
    case Tint::Blue: return "blue";
    case Tint::Indigo: return "indigo";
    case Tint::Violet: return "violet";
    case Tint::Amber: return "amber";
    case Tint::Rose: return "rose";
    case Tint::Neutral: return "neutral";
    }
    return "neutral";
}

/* ------------------------------------------------------------------ *
 * Factories
 * ------------------------------------------------------------------ */
inline QFrame* Card(QWidget* parent)
{
    auto* card = new QFrame{parent};
    card->setObjectName(QStringLiteral("card"));
    return card;
}

/** Soft tinted rounded square/circle hosting a line glyph. */
inline QLabel* Chip(Glyph glyph, Tint tint, QWidget* parent, int size = 40, int glyph_size = 20)
{
    auto* chip = new QLabel{parent};
    chip->setObjectName(QStringLiteral("iconChip"));
    chip->setProperty("tint", tintName(tint));
    chip->setFixedSize(size, size);
    chip->setAlignment(Qt::AlignCenter);
    chip->setPixmap(glyphPixmap(glyph, {glyph_size, glyph_size}, CybouTheme::color(tintInk(tint))));
    return chip;
}

inline QLabel* NavChip(CybouTheme::NavIcon icon, Tint tint, QWidget* parent, int size = 40, int glyph_size = 20)
{
    auto* chip = new QLabel{parent};
    chip->setObjectName(QStringLiteral("iconChip"));
    chip->setProperty("tint", tintName(tint));
    chip->setFixedSize(size, size);
    chip->setAlignment(Qt::AlignCenter);
    chip->setPixmap(CybouTheme::iconPixmap(icon, {glyph_size, glyph_size}, CybouTheme::color(tintInk(tint))));
    return chip;
}

/** Round initial avatar pixmap (colored disc + white letter), crisp at HiDPI. */
inline QPixmap avatarPixmap(const QString& initials, QRgb background, int size)
{
    QPixmap pixmap{QSize{size, size} * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(CybouTheme::color(background));
    painter.drawEllipse(QRectF{QPointF{0, 0}, QSizeF{size, size}}.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.setPen(QPen{QColor{0xffffff}});
    QFont font{painter.font()};
    font.setPixelSize(size * 0.42);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect{0, 0, size, size}, Qt::AlignCenter, initials.left(2).toUpper());
    return pixmap;
}

/** Round initial avatar label built on avatarPixmap. */
inline QLabel* Avatar(const QString& initials, QRgb background, QWidget* parent, int size = 36)
{
    auto* avatar = new QLabel{parent};
    avatar->setFixedSize(size, size);
    avatar->setPixmap(avatarPixmap(initials, background, size));
    return avatar;
}

inline QLabel* Pill(const QString& text, Tint tint, QWidget* parent)
{
    auto* pill = new QLabel{text, parent};
    pill->setObjectName(QStringLiteral("pill"));
    pill->setProperty("tint", tintName(tint));
    return pill;
}

inline QLabel* Dot(Tint tint, QWidget* parent, int size = 8)
{
    auto* dot = new QLabel{parent};
    dot->setObjectName(QStringLiteral("dot"));
    dot->setProperty("tint", tintName(tint));
    dot->setFixedSize(size, size);
    return dot;
}

/** Flat circular icon button (search options, view toggle, more, star…). */
inline QToolButton* IconButton(Glyph glyph, QWidget* parent, const QString& tooltip = {})
{
    auto* button = new QToolButton{parent};
    button->setObjectName(QStringLiteral("iconButton"));
    button->setAutoRaise(true);
    const int extent = 28;
    button->setFixedSize(extent, extent);
    button->setIconSize(QSize{18, 18});
    button->setIcon(QIcon{glyphPixmap(glyph, {18, 18}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))});
    if (!tooltip.isEmpty()) button->setToolTip(tooltip);
    return button;
}

inline QLabel* Eyebrow(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("eyebrow"));
    return label;
}

inline QLabel* HeroTitle(const QString& text, QWidget* parent, bool big = false)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(big ? QStringLiteral("heroTitleBig") : QStringLiteral("heroTitle"));
    return label;
}

inline QLabel* HeroSubtitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("heroSubtitle"));
    label->setWordWrap(true);
    return label;
}

inline QLabel* SectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

inline QLabel* MutedText(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    return label;
}

inline QLabel* BodyText(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("bodyText"));
    label->setWordWrap(true);
    return label;
}

/** "Section title ……… View all ›" header row; returns the link label (may be nullptr). */
inline QHBoxLayout* SectionHeader(const QString& title, const QString& link_text, QLabel*& link_out, QWidget* parent)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    auto* heading = SectionTitle(title, parent);
    row->addWidget(heading);
    row->addStretch();
    link_out = nullptr;
    if (!link_text.isEmpty()) {
        auto* link = new QLabel{link_text, parent};
        link->setObjectName(QStringLiteral("sectionLink"));
        link->setCursor(Qt::PointingHandCursor);
        link_out = link;
        row->addWidget(link, 0, Qt::AlignVCenter);
    }
    return row;
}

/** Icon-chip activity row: [chip] title / subtitle ……… meta [dot]. */
inline QFrame* ActivityRow(Glyph glyph, Tint tint, const QString& title, const QString& subtitle,
    const QString& meta, QWidget* parent, bool fresh = false)
{
    auto* row = new QFrame{parent};
    auto* layout = new QHBoxLayout{row};
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(12);
    layout->addWidget(Chip(glyph, tint, row, 36, 18), 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* title_label = new QLabel{title, row};
    title_label->setObjectName(QStringLiteral("rowTitle"));
    title_label->setStyleSheet(QStringLiteral("font-weight: 700;"));
    text->addWidget(title_label);
    if (!subtitle.isEmpty()) {
        auto* sub = new QLabel{subtitle, row};
        sub->setObjectName(QStringLiteral("rowSub"));
        sub->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        text->addWidget(sub);
    }
    layout->addLayout(text, 1);
    if (!meta.isEmpty()) {
        auto* meta_label = new QLabel{meta, row};
        meta_label->setObjectName(QStringLiteral("rowMeta"));
        meta_label->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        layout->addWidget(meta_label, 0, Qt::AlignTop);
    }
    if (fresh) layout->addWidget(Dot(Tint::Mint, row), 0, Qt::AlignTop | Qt::AlignHCenter);
    return row;
}

/** Small caption/value column used in stat cards and status panels. */
inline QVBoxLayout* StatColumn(const QString& caption, const QString& value, QWidget* parent)
{
    auto* column = new QVBoxLayout;
    column->setSpacing(2);
    auto* caption_label = new QLabel{caption, parent};
    caption_label->setObjectName(QStringLiteral("metricCaption"));
    auto* value_label = new QLabel{value, parent};
    value_label->setObjectName(QStringLiteral("metric"));
    column->addWidget(caption_label);
    column->addWidget(value_label);
    return column;
}

inline QString relTime(const QDateTime& when, const QDateTime& now = QDateTime::currentDateTime())
{
    if (!when.isValid()) return {};
    const qint64 seconds = when.secsTo(now);
    if (seconds < 60) return QStringLiteral("just now");
    if (seconds < 3600) return QStringLiteral("%1 min ago").arg(seconds / 60);
    if (seconds < 86400) return QStringLiteral("%1 h ago").arg(seconds / 3600);
    if (seconds < 86400 * 7) return QStringLiteral("%1 d ago").arg(seconds / 86400);
    return when.toString(QStringLiteral("d MMM"));
}

} // namespace CybouUi

#endif // BITCOIN_QT_CYBOUUI_H
