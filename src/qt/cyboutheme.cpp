// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboutheme.h>

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QSvgRenderer>

namespace {

QString iconResource(CybouTheme::NavIcon icon)
{
    // NOTE: the .qrc registers these under prefix /icons/cybou with extensionless
    // aliases ("home", not "home.svg"), so the resource paths must not carry ".svg".
    switch (icon) {
    case CybouTheme::NavIcon::Home: return QStringLiteral(":/icons/cybou/home");
    case CybouTheme::NavIcon::Identity: return QStringLiteral(":/icons/cybou/identity");
    case CybouTheme::NavIcon::Email: return QStringLiteral(":/icons/cybou/email");
    case CybouTheme::NavIcon::Storage: return QStringLiteral(":/icons/cybou/storage");
    case CybouTheme::NavIcon::Backup: return QStringLiteral(":/icons/cybou/backup");
    case CybouTheme::NavIcon::Wallet: return QStringLiteral(":/icons/cybou/wallet");
    case CybouTheme::NavIcon::Network: return QStringLiteral(":/icons/cybou/network");
    case CybouTheme::NavIcon::Settings: return QStringLiteral(":/icons/cybou/settings");
    case CybouTheme::NavIcon::Diagnostics: return QStringLiteral(":/icons/cybou/diagnostics");
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
    //
    // Named tokens (@name@) are substituted with literal replace() calls.
    // Do NOT switch this back to QString::arg(%N): every .arg() replaces the
    // lowest-numbered remaining marker, so a single unused marker in the
    // template silently shifts every later value by one position.
    QString sheet{QStringLiteral(R"(
        QMainWindow#cybouMainWindow, QWidget#shell, QStackedWidget { background: @subtle@; color: @text_primary@; }
        QMenuBar { background: @canvas@; border-bottom: 1px solid @border@; padding: 4px 8px; color: @text_primary@; }
        QMenuBar::item:selected, QMenu::item:selected { background: @mint_soft@; color: @teal_dark@; }
        QMenu { background: @canvas@; border: 1px solid @border@; color: @text_primary@; }

        QFrame#sidebar { background: @canvas@; border-right: 1px solid @border@; }
        QFrame#statusStrip { background: @canvas@; border-bottom: 1px solid @border@; }
        QLabel#stripCaption { color: @text_muted@; font-size: 11px; }
        QLabel#stripValue { color: @text_primary@; font-size: 13px; font-weight: 700; }
        QLabel#stripValue[tint="teal"] { color: @teal_dark@; }
        QToolButton#iconButton { border: none; border-radius: 14px; padding: 6px; background: transparent; }
        QToolButton#iconButton:hover { background: @surface@; }
        QToolButton#iconButton:checked { background: @mint_soft@; }
        QLabel#sectionLink { color: @teal_dark@; font-size: 13px; font-weight: 700; }
        QLabel#rowTitle { color: @text_primary@; font-size: 14px; font-weight: 700; background: transparent; border: none; }
        QLabel#rowSub { color: @text_muted@; font-size: 12px; background: transparent; border: none; }
        QLabel#rowMeta { color: @dim@; font-size: 12px; background: transparent; border: none; }
        QProgressBar#usageMeter { border: none; border-radius: 5px; background: @surface@; min-height: 8px; max-height: 8px; }
        QProgressBar#usageMeter::chunk { border-radius: 4px; background: @mint@; }
        QCheckBox#switch { spacing: 10px; }
        QCheckBox#switch::indicator { width: 40px; height: 22px; border-radius: 11px; border: none; background: @border_medium@; }
        QCheckBox#switch::indicator:checked { background: @mint@; }
        QFrame#readerBody { background: @subtle@; border-radius: 10px; }
        QLabel#brand { font-size: 23px; font-weight: 800; letter-spacing: 1px; color: @text_primary@; }
        QLabel#brandCaption { font-size: 11px; font-weight: 700; color: @teal_dark@; }
        QLabel#sidebarFootnote { color: @text_muted@; font-size: 12px; line-height: 1.4; }

        QToolButton { border: 1px solid transparent; border-radius: 10px; padding: 9px 12px; text-align: left; color: @text_secondary@; font-size: 14px; }
        QToolButton:hover { background: @surface@; color: @teal_dark@; }
        QToolButton:checked { background: @mint_soft@; color: @teal_dark@; font-weight: 700; }
        QToolButton:focus { border-color: @teal@; color: @teal_dark@; }
        QFrame#separator { color: @border@; }

        QFrame#card { background: @canvas@; border: 1px solid @border@; border-radius: 14px; }
        QFrame#iconChip { background: @mint_ghost@; border-radius: 20px; }
        QFrame#iconChip[tint="mint"] { background: @mint_soft@; }
        QFrame#iconChip[tint="blue"] { background: @blue_soft@; }
        QFrame#iconChip[tint="indigo"] { background: @indigo_soft@; }
        QFrame#iconChip[tint="violet"] { background: @violet_soft@; }
        QFrame#iconChip[tint="amber"] { background: @amber_soft@; }
        QFrame#iconChip[tint="rose"] { background: @rose_soft@; }
        QLabel#pill { background: @mint_soft@; color: @teal_dark@; border-radius: 11px; padding: 4px 10px; font-size: 12px; font-weight: 700; }
        QLabel#pill[tint="blue"] { background: @blue_soft@; color: @blue@; }
        QLabel#pill[tint="indigo"] { background: @indigo_soft@; color: @indigo@; }
        QLabel#pill[tint="violet"] { background: @violet_soft@; color: @violet@; }
        QLabel#pill[tint="amber"] { background: @amber_soft@; color: @amber@; }
        QLabel#pill[tint="rose"] { background: @rose_soft@; color: @rose@; }
        QLabel#pill[tint="neutral"] { background: @surface@; color: @text_muted@; }
        QLabel#dot { background: @mint@; border-radius: 4px; }
        QLabel#dot[tint="blue"] { background: @blue@; }
        QLabel#dot[tint="amber"] { background: @amber@; }
        QLabel#dot[tint="muted"] { background: @dim@; }
        QLabel#eyebrow { color: @text_muted@; font-size: 11px; font-weight: 800; letter-spacing: 3px; }
        QLabel#heroTitle { color: @text_primary@; font-size: 30px; font-weight: 800; }
        QLabel#heroTitleBig { color: @text_primary@; font-size: 34px; font-weight: 800; }
        QLabel#heroSubtitle { color: @text_secondary@; font-size: 14px; }
        QFrame#heroHeader { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f4fdf8, stop:1 #e5f9ef); border: 1px solid #cdeede; border-radius: 16px; }
        QFrame#heroPanel { background: @canvas@; border: 1px solid @border@; border-radius: 16px; }
        QLabel#pageTitle { color: @text_primary@; font-size: 28px; font-weight: 800; }
        QLabel#pageSubtitle { color: @text_muted@; font-size: 14px; }
        QLabel#sectionTitle { color: @text_primary@; font-size: 19px; font-weight: 750; }
        QLabel#cardLabel { color: @text_muted@; font-size: 12px; font-weight: 700; }
        QLabel#cardTitle { color: @text_primary@; font-size: 21px; font-weight: 800; }
        QLabel#serviceTitle { color: @text_primary@; font-size: 16px; font-weight: 750; }
        QLabel#bodyText { color: @text_secondary@; font-size: 14px; }
        QLabel#mutedText { color: @text_muted@; font-size: 13px; }
        QLabel#metric { color: @text_primary@; font-size: 26px; font-weight: 800; }
        QLabel#metricCaption { color: @text_muted@; font-size: 12px; }
        QLabel#statusBadge { background: @mint_soft@; color: @teal_dark@; border-radius: 13px; padding: 6px 12px; font-weight: 700; }
        QLabel#neutralBadge { background: @surface@; color: @text_muted@; border-radius: 11px; padding: 4px 9px; }
        QLabel#warningBadge { background: #fef3c7; color: #92400e; border-radius: 11px; padding: 6px 12px; font-weight: 700; }
        QLabel#phaseLabel { color: @dim@; font-size: 12px; font-weight: 600; }
        QLabel#phaseLabelActive { color: @teal_dark@; font-size: 12px; font-weight: 800; }
        QLabel#phaseLabelDone { color: @text_secondary@; font-size: 12px; font-weight: 600; }

        QPushButton { min-height: 34px; border-radius: 8px; padding: 4px 16px; font-weight: 700; }
        QPushButton#primaryButton, QPushButton[primary="true"] { background: @mint@; color: white; border: 1px solid @mint@; }
        QPushButton#primaryButton:hover, QPushButton[primary="true"]:hover { background: @teal@; border-color: @teal@; }
        QPushButton#primaryButton:pressed, QPushButton[primary="true"]:pressed { background: @teal_dark@; border-color: @teal_dark@; }
        QPushButton#primaryButton:focus, QPushButton[primary="true"]:focus { border-color: @teal_dark@; }
        QPushButton#secondaryButton { background: @canvas@; color: @teal_dark@; border: 1px solid @border_medium@; }
        QPushButton#secondaryButton:hover { background: @mint_ghost@; border-color: @mint@; }
        QPushButton#secondaryButton:pressed { background: @mint_soft@; }
        QPushButton#secondaryButton:focus { border-color: @teal@; }
        QPushButton#softButton { background: @mint_soft@; color: @teal_dark@; border: 1px solid @mint_soft@; }
        QPushButton#softButton:hover { background: @mint_ghost@; border-color: @mint@; }
        QPushButton#softButton:pressed { background: @mint@; color: white; }
        QPushButton#softButton:disabled { background: @surface@; color: @dim@; border-color: @border@; }
        QPushButton:disabled { background: @surface@; color: @dim@; border-color: @border@; }
        QPushButton#primaryButton:disabled, QPushButton[primary="true"]:disabled { background: @surface@; color: @dim@; border-color: @border@; }
        QPushButton#secondaryButton:disabled { background: @surface@; color: @dim@; border-color: @border@; }

        QLineEdit, QSpinBox { background: @canvas@; border: 1px solid @border_medium@; border-radius: 8px; padding: 6px 10px; color: @text_primary@; font-size: 14px; min-height: 22px; }
        QLineEdit:focus, QSpinBox:focus { border-color: @teal@; }
        QLineEdit:disabled, QSpinBox:disabled { background: @surface@; color: @dim@; }
        QSpinBox::up-button, QSpinBox::down-button { width: 18px; border-left: 1px solid @border@; background: @surface@; position: absolute; right: 0; }
        QSpinBox::up-button { top: 0; bottom: 50%; border-top-right-radius: 7px; }
        QSpinBox::down-button { top: 50%; bottom: 0; border-bottom-right-radius: 7px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: @mint_ghost@; }
        QSpinBox::up-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 5px solid @text_muted@; }
        QSpinBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid @text_muted@; }

        QCheckBox { color: @text_primary@; font-size: 14px; spacing: 8px; }
        QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1px solid @border_medium@; background: @canvas@; }
        QCheckBox::indicator:hover { border-color: @teal@; }
        QCheckBox::indicator:checked { background: @teal@; border-color: @teal@; }
        QCheckBox::indicator:checked:hover { background: @teal_dark@; border-color: @teal_dark@; }
        QCheckBox:focus { color: @teal_dark@; }
        QCheckBox:focus::indicator { border-color: @teal@; }

        /* Secondary windows (node diagnostics) and any future dialogs must
           not fall back to the platform dark palette. */
        QDialog { background: @subtle@; color: @text_primary@; }
        QWidget#RPCConsole { background: @subtle@; color: @text_primary@; }
        QWidget#RPCConsole QLabel { color: @text_secondary@; }
        QDialog QLabel { color: @text_secondary@; }
        QToolTip { background: @canvas@; color: @text_primary@; border: 1px solid @border@; padding: 4px 8px; }
        QTabWidget::pane { border: 1px solid @border@; border-radius: 10px; background: @canvas@; top: -1px; }
        QTabBar::tab { background: @surface@; color: @text_secondary@; padding: 8px 18px; margin-right: 4px;
                       border-top-left-radius: 8px; border-top-right-radius: 8px; }
        QTabBar::tab:hover:!selected { background: @mint_ghost@; color: @teal_dark@; }
        QTabBar::tab:selected { background: @canvas@; color: @teal_dark@; font-weight: 700; }
        QTabBar::tab:disabled { color: @dim@; }
        QTableView, QTreeView, QListView { background: @canvas@; alternate-background-color: @subtle@;
            border: 1px solid @border@; border-radius: 10px; color: @text_primary@; gridline-color: @border@; }
        QHeaderView { background: @surface@; border: none; }
        QHeaderView::section { background: @surface@; color: @text_muted@; border: none;
                               border-bottom: 1px solid @border@; border-right: 1px solid @border@;
                               padding: 6px 10px; font-weight: 700; }
        QTableView::item, QTreeView::item { color: @text_secondary@; padding: 4px 6px; }
        QTableView::item:selected, QTreeView::item:selected, QListView::item:selected { background: @mint_soft@; color: @teal_dark@; }
        QTextEdit, QPlainTextEdit, QTextBrowser { background: @canvas@; border: 1px solid @border_medium@;
            border-radius: 8px; padding: 8px 10px; color: @text_primary@; font-size: 14px; }
        QTextEdit:focus, QPlainTextEdit:focus, QTextBrowser:focus { border-color: @teal@; }
        QListWidget { background: @canvas@; border: 1px solid @border@; border-radius: 12px; outline: none; }
        QListWidget::item { padding: 8px 12px; border-bottom: 1px solid @border@; }
        QListWidget#folderList::item { padding: 0px; }
        QListWidget::item:hover { background: @surface@; }
        QListWidget::item:selected { background: @mint_soft@; }
        QProgressBar#sizeMeter { border: 1px solid @border@; border-radius: 5px; background: @surface@; }
        QProgressBar#sizeMeter::chunk { border-radius: 4px; background: @teal@; }
        QProgressBar#sizeMeter[overLimit="true"]::chunk { background: #dc2626; }
        QComboBox { background: @canvas@; border: 1px solid @border_medium@; border-radius: 8px;
                    padding: 5px 10px; color: @text_primary@; min-height: 20px; }
        QComboBox:focus { border-color: @teal@; }
        QComboBox::drop-down { border: none; width: 24px; }
        QComboBox QAbstractItemView { background: @canvas@; border: 1px solid @border@; selection-background-color: @mint_soft@; }
        QRadioButton { color: @text_primary@; spacing: 6px; }
        QRadioButton::indicator { width: 16px; height: 16px; border-radius: 8px; border: 1px solid @border_medium@; background: @canvas@; }
        QRadioButton::indicator:checked { border-color: @teal@; background: @teal@; }
        QSlider::groove:horizontal { height: 4px; background: @border@; border-radius: 2px; }
        QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; background: @teal@; }
        QSlider::handle:horizontal:hover { background: @teal_dark@; }
        QGroupBox { color: @text_primary@; border: 1px solid @border@; border-radius: 10px; margin-top: 12px; padding-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; color: @text_muted@; }

        /* The page scroll surface must blend into the shell background. */
        QScrollArea#pageScroll { background: @subtle@; border: none; }
        QScrollArea#pageScroll > QWidget > QWidget { background: @subtle@; }
        QScrollBar:vertical { background: @subtle@; width: 12px; margin: 2px; border-radius: 6px; }
        QScrollBar::handle:vertical { background: @border_medium@; min-height: 40px; border-radius: 5px; }
        QScrollBar::handle:vertical:hover { background: @text_muted@; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QScrollBar:horizontal { background: @subtle@; height: 12px; margin: 2px; border-radius: 6px; }
        QScrollBar::handle:horizontal { background: @border_medium@; min-width: 40px; border-radius: 5px; }
        QScrollBar::handle:horizontal:hover { background: @text_muted@; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
    )")};

    const auto set = [&sheet](const QString& token, const QString& value) {
        sheet.replace(QStringLiteral("@") + token + QStringLiteral("@"), value);
    };
    set(QStringLiteral("subtle"), color(SUBTLE).name());
    set(QStringLiteral("canvas"), color(CANVAS).name());
    set(QStringLiteral("surface"), color(SURFACE).name());
    set(QStringLiteral("border"), color(BORDER).name());
    set(QStringLiteral("border_medium"), color(BORDER_MEDIUM).name());
    set(QStringLiteral("text_primary"), color(TEXT_PRIMARY).name());
    set(QStringLiteral("text_secondary"), color(TEXT_SECONDARY).name());
    set(QStringLiteral("text_muted"), color(TEXT_MUTED).name());
    set(QStringLiteral("dim"), color(DIM).name());
    set(QStringLiteral("mint"), color(MINT).name());
    set(QStringLiteral("teal"), color(BRAND_TEAL).name());
    set(QStringLiteral("teal_dark"), color(BRAND_TEAL_DARK).name());
    set(QStringLiteral("mint_soft"), color(MINT_SOFT).name());
    set(QStringLiteral("mint_ghost"), color(MINT_GHOST).name());
    set(QStringLiteral("blue_soft"), color(BLUE_SOFT).name());
    set(QStringLiteral("indigo_soft"), color(INDIGO_SOFT).name());
    set(QStringLiteral("violet_soft"), color(VIOLET_SOFT).name());
    set(QStringLiteral("amber_soft"), color(AMBER_SOFT).name());
    set(QStringLiteral("rose_soft"), color(ROSE_SOFT).name());
    set(QStringLiteral("blue"), color(BLUE).name());
    set(QStringLiteral("indigo"), color(INDIGO).name());
    set(QStringLiteral("violet"), color(VIOLET).name());
    set(QStringLiteral("amber"), color(AMBER).name());
    set(QStringLiteral("rose"), color(ROSE).name());
    // Dark squircle tile behind the white CYBOU logomark, mirroring the
    // website's .logo-tile (white-on-transparent art needs a dark base even
    // on the light desktop shell).
    set(QStringLiteral("logo_tile_bg"), color(LOGO_TILE_BG).name());
    set(QStringLiteral("logo_tile_border"), color(LOGO_TILE_BORDER).name());
    return sheet;
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

QPixmap CybouTheme::logoTile(const QSize& tile_size, int radius, const QSize& logo_size)
{
    // Render at 2x for crisp HiDPI output.
    QPixmap pixmap{tile_size * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);

    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF tile_rect{QPointF{0, 0}, QSizeF{tile_size}};
    painter.setPen(Qt::NoPen);
    painter.setBrush(color(LOGO_TILE_BG));
    painter.drawRoundedRect(tile_rect, radius, radius);
    painter.setPen(QPen{color(LOGO_TILE_BORDER), 1});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(tile_rect.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5, radius - 0.5);

    QPixmap logo = QPixmap{QStringLiteral(":/icons/logo")}
                       .scaled(logo_size * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    logo.setDevicePixelRatio(2); // logical size becomes logo_size
    const QPointF top_left{
        (tile_size.width() - logo_size.width()) / 2.0,
        (tile_size.height() - logo_size.height()) / 2.0};
    painter.drawPixmap(top_left, logo);
    return pixmap;
}

void CybouTheme::applyTo(QApplication& app)
{
    app.setStyleSheet(applicationStyleSheet());
}
