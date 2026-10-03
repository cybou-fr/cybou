// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_OPERATION_POOL_H
#define CYBOU_OPERATION_POOL_H

/// \file
/// \brief Волатильный пул локально проверенных операций до PoA-финализации.

#include <cybou/protocol_operation.h>
#include <cybou/state_store.h>

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cybou {

/// \brief Глобальный максимум candidate operations в RAM-пуле одного Full Node, шт.
inline constexpr size_t MAX_PENDING_OPERATIONS{256};
/// \brief Глобальный максимум сериализованных байт в RAM-пуле одного Full Node, байты.
inline constexpr size_t MAX_PENDING_OPERATION_BYTES{8U * 1024U * 1024U};
/// \brief Максимум операций, удерживаемых от одного relay-источника, шт.
inline constexpr size_t MAX_PEER_PENDING_OPERATIONS{32};
/// \brief Максимум сериализованных байт, удерживаемых от одного relay-источника, байты.
inline constexpr size_t MAX_PEER_PENDING_BYTES{1U * 1024U * 1024U};

/// \brief Результат допуска операции в локальный кандидатный пул.
enum class PoolAdmission { ACCEPTED, ALREADY_PENDING, ALREADY_FINALIZED, REJECTED };
/// \var PoolAdmission::ACCEPTED
/// \brief Кандидат-операция локально исполнена и добавлена в RAM-пул.
/// \var PoolAdmission::ALREADY_PENDING
/// \brief Идентичный OperationID уже удерживается в этом пуле.
/// \var PoolAdmission::ALREADY_FINALIZED
/// \brief Операция уже присутствует в finalized history и не должна переисполняться как кандидат.
/// \var PoolAdmission::REJECTED
/// \brief Операция отвергнута из-за невалидности, лимитов или невозможности независимого исполнения.

/// \brief Лимиты RAM-пула и квоты на один источник ретрансляции.
struct OperationPoolLimits {
    /// \brief Максимум candidate operations во всём пуле, шт.
    size_t max_count{MAX_PENDING_OPERATIONS};
    /// \brief Максимум сериализованных байт во всём пуле, байты.
    size_t max_bytes{MAX_PENDING_OPERATION_BYTES};
    /// \brief Максимум операций от одного source_peer, шт.
    size_t max_peer_count{MAX_PEER_PENDING_OPERATIONS};
    /// \brief Максимум сериализованных байт от одного source_peer, байты.
    size_t max_peer_bytes{MAX_PEER_PENDING_BYTES};
};

/// \brief Хранит только локально исполненные кандидаты поверх последнего finalized head.
/// \details Класс сам по себе не потокобезопасен; вызывающая сторона должна сериализовать доступ,
///          обычно тем же mutex, что защищает finalized state и candidate execution.
class OperationPool
{
public:
    /// \brief Создаёт RAM-пул кандидатов поверх конкретного state store.
    /// \param store State store, чей finalized head используется для независимого исполнения.
    /// \param limits Глобальные и per-peer лимиты RAM.
    explicit OperationPool(CybouStateStore& store, OperationPoolLimits limits = {})
        : m_store{store}, m_limits{limits} {}

    /// \brief Исполняет и при успехе добавляет операцию в RAM-пул кандидатов.
    /// \param operation Candidate operation в структурированном виде.
    /// \param source_peer Optional идентификатор relay-источника для per-peer quotas.
    /// \return ACCEPTED, ALREADY_PENDING, ALREADY_FINALIZED или REJECTED.
    /// \pre Local finalized state уже загружен и согласован со store.
    /// \post При ACCEPTED операция появится в Snapshot() и будет учитываться в Size()/Bytes().
    PoolAdmission Admit(const ProtocolOperation& operation,
                        std::optional<std::string> source_peer = std::nullopt);
    /// \brief Возвращает копию текущего порядка кандидатов для сборки блока.
    /// \return Копия операций в детерминированном порядке удержания.
    std::vector<ProtocolOperation> Snapshot() const;
    /// \brief Переисполняет все кандидаты на новом finalized head и возвращает удалённые OperationID.
    /// \return OperationID кандидатов, ставших невалидными или вытесненных после revalidation.
    /// \post Все оставшиеся кандидаты снова валидны относительно текущего finalized head.
    std::vector<cybou::Hash256> Revalidate();
    /// \brief Полностью очищает RAM-пул и статистику по источникам.
    /// \post Size() == 0 и Bytes() == 0.
    void Clear();
    /// \brief Число удерживаемых candidate operations.
    size_t Size() const { return m_entries.size(); }
    /// \brief Суммарный размер сериализованных candidate operations, байты.
    size_t Bytes() const { return m_bytes; }
    /// \brief Проверяет наличие candidate operation по OperationID.
    bool Contains(const cybou::Hash256& id) const { return m_ids.contains(id); }
    /// \brief Возвращает OperationID кандидатов в текущем порядке пула.
    std::vector<cybou::Hash256> Ids() const;

private:
    struct Entry {
        ProtocolOperation operation;
        cybou::Hash256 id;
        size_t bytes;
        std::optional<std::string> source_peer;
    };
    struct PeerUsage {
        size_t count{0};
        size_t bytes{0};
    };

    bool FitsGlobalLimits(size_t bytes) const;
    bool FitsPeerLimits(const std::string& peer, size_t bytes) const;
    void RecordPeerUsage(const std::optional<std::string>& peer, size_t bytes);

    CybouStateStore& m_store;
    const OperationPoolLimits m_limits;
    std::vector<Entry> m_entries;
    std::set<cybou::Hash256> m_ids;
    std::map<std::string, PeerUsage> m_peer_usage;
    size_t m_bytes{0};
};

} // namespace cybou

#endif // CYBOU_OPERATION_POOL_H
