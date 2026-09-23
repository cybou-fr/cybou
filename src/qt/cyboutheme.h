// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUTHEME_H
#define BITCOIN_QT_CYBOUTHEME_H

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QRgb>

class QApplication;

namespace CybouTheme {

/**
 * Canonical CYBOU desktop palette.
 *
 * Single source of visual truth for the desktop client. The colors mirror
 * the CYBOU website (cybou.fr); pages must not invent their own colors.
 */
inline constexpr QRgb CANVAS         = 0xffffff; // page canvas
inline constexpr QRgb SUBTLE         = 0xf8f9fb; // subtle background
inline constexpr QRgb SURFACE        = 0xf1f3f6; // recessed surface
inline constexpr QRgb CARD           = 0xffffff; // card background
inline constexpr QRgb BORDER         = 0xe5e7eb; // subtle border
inline constexpr QRgb BORDER_MEDIUM  = 0xd1d5db; // medium border
inline constexpr QRgb TEXT_PRIMARY   = 0x111827; // primary text
inline constexpr QRgb TEXT_SECONDARY = 0x4b5563; // secondary text
inline constexpr QRgb TEXT_MUTED     = 0x6b7280; // muted text
inline constexpr QRgb DIM            = 0x9ca3af; // disabled / dim
inline constexpr QRgb LOGO_MINT      = 0x34d399; // logo mint
inline constexpr QRgb MINT           = 0x10b981; // mint accent
inline constexpr QRgb BRAND_TEAL     = 0x059669; // brand teal
inline constexpr QRgb BRAND_TEAL_DARK= 0x047857; // brand teal dark
inline constexpr QRgb BRAND_BLUE     = 0x0284c7; // brand blue
inline constexpr QRgb BRAND_INDIGO   = 0x4f46e5; // brand indigo

/** Derived tints. Kept here so pages never hardcode derived colors. */
inline constexpr QRgb MINT_SOFT  = 0xd1fae5; // selected background
inline constexpr QRgb MINT_GHOST = 0xecfdf5; // hover / chip background

inline QColor color(QRgb rgb) { return QColor{rgb}; }

/** Application stylesheet built from the canonical palette. */
QString applicationStyleSheet();

/** Coherent monochrome CYBOU line-icon set (: /icons/cybou/*.svg). */
enum class NavIcon {
    Home,
    Identity,
    Email,
    Storage,
    Backup,
    Network,
    Settings,
    Diagnostics,
};

/**
 * Render one icon in the given color. The SVG assets are monochrome
 * outlines; the placeholder stroke color is replaced before rendering so
 * the same asset serves inactive (dim) and active (teal) states.
 */
QPixmap iconPixmap(NavIcon icon, const QSize& size, const QColor& stroke);

/** Checkable icon: dim when inactive, brand teal when active/checked. */
QIcon navIcon(NavIcon icon);

/** Apply palette, stylesheet and HiDPI icon defaults to the application. */
void applyTo(QApplication& app);

} // namespace CybouTheme

#endif // BITCOIN_QT_CYBOUTHEME_H
