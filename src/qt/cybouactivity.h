// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_CYBOUACTIVITY_H
#define CYBOU_QT_CYBOUACTIVITY_H

#include <QCoreApplication>
#include <QString>
#include <QHash>
#include <QToolButton>
#include <QVector>

#include <functional>

class CybouDesktopModel;
struct CybouActivityItem;
struct CybouContact;
class QFrame;
class QVBoxLayout;
class QScrollArea;

/** One operation still on its way, or one that needs the user. */
struct CybouActivityOperation {
    enum class Kind { File, Download, Mail, LocalMail, LocalFiles, Payment, Name, Recovery } kind{Kind::File};
    QString id;      ///< item id for File/Download/Mail; empty otherwise
    QString title;   ///< "Uploading report.pdf"
    QString status;  ///< "Waiting for confirmation", "Securing 42%"
    bool attention{false};
    QString key; ///< stable local command key; other rows use kind/item
};

/** Everything in flight or failed, derived from the model's semantic state. */
QVector<CybouActivityOperation> CybouActivityOperations(const CybouDesktopModel& model);

/** Live mode: Recent activity derived from Mail, Files and Wallet state. */
QVector<CybouActivityItem> CybouBuildActivityItems(const CybouDesktopModel& model,
    const QVector<CybouActivityItem>& extra_activity);

/** Live mode: people this Identity mailed, heard from or paid, most recent first. */
QVector<CybouContact> CybouBuildContacts(const CybouDesktopModel& model);

/**
 * Header control for the one place that shows every running or failed
 * operation. Hidden when nothing is happening; a click opens the list, where
 * each row opens its item and failed rows offer Try again.
 */
class CybouActivityButton : public QToolButton
{
    Q_DECLARE_TR_FUNCTIONS(CybouActivityButton)

public:
    explicit CybouActivityButton(CybouDesktopModel* model, QWidget* parent = nullptr);

    std::function<void(const QString& file_id)> onOpenFile;
    std::function<void(const QString& mail_id)> onOpenMail;
    std::function<void()> onOpenWallet;
    std::function<void()> onOpenIdentity;

    void refresh();

private:
    CybouDesktopModel* const m_model;
    QFrame* m_popup{nullptr};
    QVBoxLayout* m_rows{nullptr};
    QScrollArea* m_scroll{nullptr};
    QHash<QString, QWidget*> m_operation_rows;
    QHash<QString, CybouActivityOperation> m_shown_operations;

    void showPopup();
    void rebuildRows();
    void open(const CybouActivityOperation& operation);
};

#endif // CYBOU_QT_CYBOUACTIVITY_H
