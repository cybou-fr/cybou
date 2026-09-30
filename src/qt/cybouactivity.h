// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUACTIVITY_H
#define BITCOIN_QT_CYBOUACTIVITY_H

#include <QCoreApplication>
#include <QString>
#include <QToolButton>
#include <QVector>

#include <functional>

class CybouDesktopModel;
class QFrame;
class QVBoxLayout;

/** One operation still on its way, or one that needs the user. */
struct CybouActivityOperation {
    enum class Kind { File, Download, Mail, Payment, Name, Recovery } kind{Kind::File};
    QString id;      ///< item id for File/Download/Mail; empty otherwise
    QString title;   ///< "Uploading report.pdf"
    QString status;  ///< "Waiting for confirmation", "Securing 42%"
    bool attention{false};
};

/** Everything in flight or failed, derived from the model's semantic state. */
QVector<CybouActivityOperation> CybouActivityOperations(const CybouDesktopModel& model);

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

    void showPopup();
    void rebuildRows();
    void open(const CybouActivityOperation& operation);
};

#endif // BITCOIN_QT_CYBOUACTIVITY_H
