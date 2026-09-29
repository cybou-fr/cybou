// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUUIFIXTURES_H
#define BITCOIN_QT_CYBOUUIFIXTURES_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class CybouDesktopModel;

/**
 * Deterministic desktop UI fixtures (CYBOU_UI_FIXTURE=<name>).
 *
 * A fixture only feeds product state into the desktop model so every screen
 * can be developed and screenshotted without a running core. It does not
 * simulate the protocol and it never calls a backend.
 */
namespace CybouUiFixtures {

/** Supported fixture names: empty, active, mail, files, offline, restoring. */
QStringList names();

/** Fixed reference time used by all fixtures (keeps screenshots stable). */
QDateTime referenceTime();

/** Name requested through CYBOU_UI_FIXTURE, or empty for live mode. */
QString requestedFixture();

/**
 * Loads the named fixture into the model and marks it as fixture-fed.
 * Returns false for unknown names (the model is left untouched).
 */
bool apply(CybouDesktopModel& model, const QString& name);

/** Page the fixture opens first: home, mail or files. */
QString initialPage(const QString& name);

/**
 * Fixture-mode stand-in for backend replies. It advances product states on
 * short timers so flows (create, restore, send, upload) can be exercised
 * visually. It is only ever attached when a fixture is active.
 */
class Driver final : public QObject
{
    Q_OBJECT
public:
    explicit Driver(CybouDesktopModel* model, QObject* parent = nullptr);

    /** Step delay in milliseconds (tests use 0). */
    void setStepDelay(int ms) { m_step_ms = ms; }

private:
    CybouDesktopModel* const m_model;
    int m_step_ms{900};

    void later(int steps, std::function<void()> action);
    void runCreate();
    void runRestore();
    void runSend(const QString& id);
};

} // namespace CybouUiFixtures

#endif // BITCOIN_QT_CYBOUUIFIXTURES_H
