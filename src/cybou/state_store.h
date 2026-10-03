// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Хранилище канонического состояния и финализированных блоков CYBOU.

#ifndef CYBOU_STATE_STORE_H
#define CYBOU_STATE_STORE_H

#include <cybou/block.h>
#include <cybou/block_executor.h>
#include <cybou/kv_store.h>
#include <cybou/network_genesis.h>
#include <cybou/poa_conflict_detector.h>
#include <cybou/protocol_operation.h>
#include <cybou/state.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace cybou {

/// \brief Текущий канонический финализированный head: block id плюс высота.
struct FinalizedHead {
    cybou::Hash256 block_id;
    uint64_t height{0};

    friend bool operator==(const FinalizedHead&, const FinalizedHead&) = default;
};

namespace detail {
template <>
struct LocalRecordCodec<FinalizedHead> {
    static std::vector<unsigned char> Encode(const FinalizedHead& value)
    {
        auto out = LocalRecordCodec<cybou::Hash256>::Encode(value.block_id);
        const auto height = LocalRecordCodec<uint64_t>::Encode(value.height);
        out.insert(out.end(), height.begin(), height.end());
        return out;
    }
    static bool Decode(const std::span<const unsigned char> bytes, FinalizedHead& value)
    {
        if (bytes.size() < cybou::Hash256::size() + sizeof(uint64_t)) return false;
        FinalizedHead decoded;
        if (!LocalRecordCodec<cybou::Hash256>::Decode(bytes.first(cybou::Hash256::size()), decoded.block_id) ||
            !LocalRecordCodec<uint64_t>::Decode(bytes.subspan(cybou::Hash256::size()), decoded.height)) return false;
        value = decoded;
        return true;
    }
};
} // namespace detail

/// \brief Ошибки загрузки канонического состояния из локального KV store.
enum class StateLoadError : uint8_t {
    NONE,
    NOT_FOUND,
    CORRUPT,
    NETWORK_MISMATCH,
};

/// \brief Результат загрузки канонического состояния с проверкой hash integrity.
struct StateLoadResult {
    StateLoadError error{StateLoadError::NONE};
    std::optional<CybouState> state;

    explicit operator bool() const { return error == StateLoadError::NONE && state.has_value(); }
};

/// \brief Ошибки первичной инициализации genesis.
enum class GenesisInitError : uint8_t {
    NONE,
    ALREADY_INITIALIZED,
    GENESIS_STATE_MISMATCH,
};

/// \brief Результат записи genesis state в пустое хранилище.
struct GenesisInitResult {
    GenesisInitError error{GenesisInitError::NONE};

    explicit operator bool() const { return error == GenesisInitError::NONE; }
};

/// \brief Ошибки атомарного коммита PoA-finalized блока.
enum class BlockTransitionError : uint8_t {
    NONE,
    INVALID_BLOCK_ID,
    STATE_NOT_INITIALIZED,
    NETWORK_MISMATCH,
    CORRUPT_STATE,
    PARENT_MISMATCH,
    BLOCK_ALREADY_APPLIED,
    INVALID_HEIGHT,
    CORRUPT_HEAD,
    INVALID_OPERATION,
    TOO_MANY_ACCOUNT_CREATES,
    INVALID_CERTIFICATE,
    STATE_ROOT_MISMATCH,
    POA_EQUIVOCATION_DETECTED,
    POA_SAFETY_HALTED,
};

/// \brief Результат проверки и коммита финализированного блока.
struct BlockTransitionResult {
    BlockTransitionError error{BlockTransitionError::NONE};
    BlockExecutionResult op_result{};
    explicit operator bool() const { return error == BlockTransitionError::NONE; }
};

/// \brief Единственный владелец канонического состояния, финализированных блоков и локальных индексов.
class CybouStateStore
{
public:
    CybouStateStore(
        KVStore& db,
        VerifiedNetworkGenesis network_genesis);

    /// \brief Сохраняет genesis state на высоте 0; повторная инициализация запрещена.
    GenesisInitResult InitializeGenesis(const CybouState& genesis_state, bool sync = true);

    /// \brief Загружает каноническое состояние и проверяет его hash integrity.
    StateLoadResult LoadState() const;

    /// \brief Возвращает текущий канонический state root.
    std::optional<cybou::Hash256> GetStateRoot() const;

    /// \brief Возвращает текущий финализированный head.
    std::optional<FinalizedHead> GetFinalizedHead() const;

    /// \brief Возвращает block id последнего финализированного блока.
    std::optional<cybou::Hash256> GetFinalizedTip() const;

    /// \brief Возвращает финализированную высоту, начиная с 0 для genesis.
    std::optional<uint64_t> GetFinalizedHeight() const;

    /// \brief Вычисляет candidate state root поверх канонического родительского состояния без коммита.
    std::optional<cybou::Hash256> ComputeCandidateStateRoot(
        const std::vector<ProtocolOperation>& operations,
        uint64_t height) const;

    /// \brief Возвращает NetworkBinding активной сети.
    const cybou::Hash256& GetNetworkBinding() const { return m_network_binding; }
    /// \brief Возвращает верифицированный genesis активной сети.
    const VerifiedNetworkGenesis& GetNetworkGenesis() const { return m_network_genesis; }
    /// \brief Возвращает underlying KV store.
    KVStore& GetDatabase() const { return m_db; }
    /// \brief Сообщает, переведён ли локальный PoA safety guard в fail-closed состояние.
    bool PoaSafetyHalted() const;
    /// \brief Возвращает локально сохранённые доказательства PoA safety halt.
    PoaEvidenceReadResult ReadPoaSafetyEvidence() const;

    /// \brief Возвращает NetworkBinding, сохранённый при инициализации genesis.
    std::optional<cybou::Hash256> GetStoredNetworkBinding() const;

    /// \brief Проверяет и атомарно коммитит PoA-finalized блок вместе с новым состоянием и индексами.
    BlockTransitionResult CommitFinalizedBlock(
        const FinalizedBlock& finalized_block,
        bool sync = true);

    /// \brief Загружает финализированный блок по его block id.
    std::optional<FinalizedBlock> GetBlock(const cybou::Hash256& block_id) const;

    /// \brief Загружает финализированный блок по канонической высоте.
    std::optional<FinalizedBlock> GetBlockAtHeight(uint64_t height) const;

    /// \brief Проверяет наличие операции в локальном индексе финализированных операций.
    bool HasIndexedFinalizedOperation(const cybou::Hash256& op_id) const;
    /// \brief Возвращает финализированную высоту операции, если локальный индекс и блок валидны.
    std::optional<uint64_t> GetFinalizedOperationHeight(const cybou::Hash256& op_id) const;


private:
    KVStore& m_db;
    const VerifiedNetworkGenesis m_network_genesis;
    const cybou::Hash256 m_network_binding;
    std::unique_ptr<PoaConflictDetector> m_poa_conflict_detector;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
