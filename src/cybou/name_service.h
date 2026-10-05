// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Синхронный workflow claim'а имени .cybou поверх node runtime и keystore.

#ifndef CYBOU_NAME_SERVICE_H
#define CYBOU_NAME_SERVICE_H

#include <cybou/keystore.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/node_runtime.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>

namespace cybou {

/// \brief Фазы синхронного claim workflow для имени `.cybou`.
enum class NameClaimPhase {
    SAVING,             ///< Локальное шифрованное сохранение секрета claim-а.
    COMMITTING,         ///< Отправка `NameCommit`.
    WAITING_FOR_COMMIT, ///< Ожидание финализации `NameCommit`.
    WORKING,            ///< Локальный подбор PoW для `NameReveal`.
    REVEALING,          ///< Отправка `NameReveal`.
    WAITING_FOR_NAME,   ///< Ожидание появления имени в финализированном NameRegistry.
    ACTIVE,             ///< Имя успешно финализировано за текущим аккаунтом.
    FAILED              ///< Workflow завершился ошибкой или отменой.
};
/// \brief Итоговая информация о выполнении claim workflow.
struct NameClaimResult {
    bool success{false}; ///< Итоговый признак успешного claim-а.
    NameClaimPhase phase{NameClaimPhase::FAILED}; ///< Последняя достигнутая фаза.
    std::string message; ///< Краткое сообщение для UI/оператора.
};
using NamePhaseCallback = std::function<void(NameClaimPhase, const std::string&)>;

/// \brief Высокоуровневый orchestrator commit/reveal workflow для назначения имени текущему аккаунту.
class CybouNameService {
public:
    /// \brief Создаёт name service поверх runtime, keystore и пути к identity vault.
    CybouNameService(CybouNodeRuntime& runtime, CybouKeyStore& keystore, std::filesystem::path identity_vault_path);
    /// \brief Выполняет полный синхронный claim имени: persist secret, commit, ожидание, reveal и финализацию.
    /// \param label Желаемая label без суффикса `.cybou`.
    /// \param password Пароль локального vault/claim-файла.
    /// \param on_phase Необязательный callback смены фаз.
    /// \param timeout Максимальное время ожидания финализации commit/reveal.
    /// \return Успех/ошибку workflow; состояние сети при этом меняется только через реально отправленные операции.
    /// \pre Текущий Identity должен быть разблокирован и финализирован в состоянии.
    /// \post При успехе имя закреплено в финализированном `NameRegistry`; при неуспехе локальный секрет claim-а может остаться сохранённым для повтора.
    NameClaimResult ClaimSync(std::string label, std::string password,
        const NamePhaseCallback& on_phase = {},
        std::chrono::milliseconds timeout = std::chrono::seconds(60));
    /// \brief Просит текущий ClaimSync завершиться как можно скорее.
    void Cancel() { m_cancelled.store(true); }

private:
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    IdentityOperationCoordinator& m_operation_coordinator;
    std::filesystem::path m_identity_vault_path;
    std::filesystem::path m_claim_path;
    std::atomic<bool> m_cancelled{false};
};

} // namespace cybou
#endif
