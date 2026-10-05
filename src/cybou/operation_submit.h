// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_OPERATION_SUBMIT_H
#define CYBOU_OPERATION_SUBMIT_H

/// \file
/// \brief Итог локальной подачи операции в Full Node runtime.

#include <cybou/hash256.h>
#include <cstdint>
namespace cybou {
/// \brief Высокоуровневый статус попытки подать операцию в runtime и/или relay.
enum class OperationSubmitStatus : uint8_t {
    /// \brief Операция отвергнута локальной проверкой или не удалось безопасно продолжить.
    REJECTED = 0x00,
    /// \brief Кандидат-операция локально принята и удерживается узлом.
    ACCEPTED = 0x01,
    /// \brief Идентичная кандидат-операция уже есть в локальном candidate pool.
    ALREADY_PENDING = 0x02,
    /// \brief Операция уже найдена в finalized history и не требует повторной подачи.
    ALREADY_FINALIZED = 0x03,
    /// \brief Байты не декодируются в canonical ProtocolOperation.
    INVALID_PAYLOAD = 0x04,
    /// \brief Локальное состояние принадлежит другой сети.
    NETWORK_MISMATCH = 0x05,
    /// \brief Exact bytes поставлены в hop-by-hop relay и будут повторяться позже.
    RELAY_QUEUED = 0x06,
    /// \brief Операция требует локального PoA signer, но он недоступен.
    POA_SIGNER_UNAVAILABLE = 0x07,
    /// \brief Relay-очередь переполнена и exact bytes не были удержаны.
    RELAY_QUEUE_FULL = 0x08,
};

/// \brief Результат локальной подачи операции и сопутствующие transport hints.
struct OperationSubmitResult {
    /// \brief Высокоуровневый итог локального исполнения и/или relay.
    OperationSubmitStatus status{OperationSubmitStatus::REJECTED};
    /// \brief Вычисленный OperationID; может быть нулевым только если операция не сериализовалась канонически.
    cybou::Hash256 op_id;
    /// \brief Локальная transport-подсказка: отсутствие ack не доказывает отказ relay-пира.
    bool delivery_uncertain{false};

    /// \brief Удобный предикат для сценариев, где accepted/pending/finalized считаются успешным итогом.
    explicit operator bool() const {
        return status == OperationSubmitStatus::ACCEPTED ||
               status == OperationSubmitStatus::ALREADY_PENDING ||
               status == OperationSubmitStatus::ALREADY_FINALIZED ||
               status == OperationSubmitStatus::RELAY_QUEUED;
    }
};

} // namespace cybou
#endif
