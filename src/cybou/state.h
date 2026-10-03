// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Каноническое состояние цепочки CYBOU и детерминированные state-transition helpers.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/economics.h>
#include <cybou/identity_registry.h>
#include <cybou/name_registry.h>
#include <cybou/root_publication.h>
#include <cybou/protocol_params.h>

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cybou {

/// \brief Каноническое состояние аккаунта, коммитящееся в корень состояния.
struct AccountState {
    uint64_t balance{0};          ///< Spendable Balance в CYBOU.
    uint64_t system_balance{0};   ///< Непереводимый System Balance для сетевых комиссий и сервисного бюджета.
    uint64_t authority{0};        ///< Канонический нетрансферабельный AUTH; не входит в total supply CYBOU.
    uint64_t creation_height{0};  ///< Высота блока, на которой аккаунт был финализирован через `AccountCreate`.
    uint64_t creation_epoch{0};   ///< Детерминированный epoch, вычисленный из `creation_height` и protocol params.

    friend bool operator==(const AccountState&, const AccountState&) = default;
};

/// \brief Неизменяемое genesis-выделение для recovery-ключа, заявляемое ровно одним AccountCreate.
struct GenesisAllocation {
    uint64_t balance{0};                   ///< Начальный Balance, зарезервированный под один recovery key id.
    uint64_t authority{0};                 ///< Начальный AUTH, заявляемый тем же самым `AccountCreate`.
    std::string label;                     ///< Необязательная genesis-метка/имя, резервируемая без отдельного reveal.
    std::optional<AccountId> claimed_by;   ///< Аккаунт, единожды заявивший allocation; `nullopt` до claim-а.

    friend bool operator==(const GenesisAllocation&, const GenesisAllocation&) = default;
};

inline constexpr size_t MAX_GENESIS_ALLOCATIONS{16};

/// \brief Детерминированный учёт ресурсов одной Identity для лимитов уровня AUTH (DEC-272).
/// \details Запись существует только пока хотя бы один счётчик ненулевой; устаревшие окна
///          блока и эпохи обнуляются в начале исполнения каждого блока.
struct AccountUsage {
    uint64_t stored_chunks{0};    ///< Chunk-и всех действующих публикаций Identity (квота хранения).
    uint64_t epoch{0};            ///< Эпоха, к которой относится `epoch_operations`.
    uint32_t epoch_operations{0}; ///< Метрируемые операции в эпохе `epoch`.
    uint64_t block_height{0};     ///< Высота, к которой относится `block_operations`.
    uint32_t block_operations{0}; ///< Метрируемые операции в блоке `block_height`.

    bool Empty() const { return stored_chunks == 0 && epoch_operations == 0 && block_operations == 0; }
    friend bool operator==(const AccountUsage&, const AccountUsage&) = default;
};

/// \brief Запись Notarial Register о действующей финализированной RootPublication.
struct PublicationRecord {
    AccountId owner;                   ///< Авторизовавшая публикацию Identity.
    ChunkId chunk_authorization_root{}; ///< Merkle root авторизации chunk-ов для storage admission и purge.
    uint32_t chunk_count{0};           ///< Учитываемый в квоте объём, chunk-и.
    uint64_t height{0};                ///< Высота финализации.

    friend bool operator==(const PublicationRecord&, const PublicationRecord&) = default;
};

/// \brief Полный консенсусный снимок CYBOU, из которого вычисляется state root.
struct CybouState {
    uint64_t onboarding_pool{0};   ///< Остаток DEV OnboardingPool в CYBOU.
    std::map<AccountId, AccountState> accounts; ///< Канонические аккаунтные значения, отсортированные по `AccountId`.
    IdentityRegistry identities;   ///< Финализированный Identity registry, согласованный 1:1 с `accounts`.
    NameRegistry names;            ///< Реестр `.cybou` имён и pending commit-ов.
    /** Keyed by recovery key id; only claim and pre-claim Central Authority fees mutate it. */
    std::map<IdentityKeyId, GenesisAllocation> genesis_allocations;
    /// \brief Ненулевые счётчики ресурсов по аккаунтам (DEC-272).
    std::map<AccountId, AccountUsage> usage;
    /// \brief Действующие публикации по OperationID их RootPublication (DEC-271).
    std::map<cybou::Hash256, PublicationRecord> publications;
};

/// \brief Ищет уникальное genesis-выделение Central Authority; дубликаты считаются некорректным состоянием.
/// \param state Состояние для поиска.
/// \return Указатель на единственную allocation с меткой `cybou`, либо `nullptr`, если её нет или если найден дубликат.
/// \note Потокобезопасно для неизменяемого доступа; не изменяет состояние и детерминировано.
GenesisAllocation* FindCentralAuthorityAllocation(CybouState& state);
/// \copydoc FindCentralAuthorityAllocation(CybouState&)
const GenesisAllocation* FindCentralAuthorityAllocation(const CybouState& state);
/// \brief Проверяет, можно ли безопасно зачислить комиссию Central Authority без переполнения.
/// \param state Каноническое состояние.
/// \param fee Комиссия в CYBOU, списываемая из `System Balance`.
/// \return `true`, если место назначения комиссии однозначно определено и прибавление не переполняет `uint64_t`.
/// \post Никаких изменений состояния.
bool CanCreditCentralAuthorityFee(const CybouState& state, uint64_t fee);
/// \brief Атомарно зачисляет комиссию Central Authority в текущее место учёта комиссии.
/// \param state Изменяемое кандидатное состояние.
/// \param fee Комиссия в CYBOU.
/// \return `true` при успешном зачёте; `false`, если destination невалиден или произошло бы переполнение.
/// \pre Перед вызовом обычно проверяют `CanCreditCentralAuthorityFee`.
/// \post При успехе изменяется только баланс текущего получателя комиссии; при неуспехе состояние остаётся без изменений.
bool CreditCentralAuthorityFee(CybouState& state, uint64_t fee);

enum class AccountCreateStateError : uint8_t {
    NONE,                       ///< Переход выполнен детерминированно.
    INVALID_CREATE,             ///< `AccountCreate` не прошёл форматную/криптографическую или registry-проверку.
    ACCOUNT_EXISTS,             ///< `account_id` уже присутствует в каноническом состоянии.
    RECOVERY_KEY_EXISTS,        ///< Recovery binding уже заявлен другим аккаунтом.
    ACCOUNT_LIMIT,              ///< Достигнут лимит числа аккаунтов в состоянии.
    INSUFFICIENT_ONBOARDING_POOL, ///< В OnboardingPool недостаточно CYBOU для onboarding bonus.
    INCONSISTENT_STATE,         ///< Нарушены внутренние инварианты `accounts`/`identities`/fee destination.
};

/// \brief Применяет `AccountCreate` к кандидатному состоянию без коммита в хранилище.
/// \param op Кандидат-операция `AccountCreate`.
/// \param network_binding Привязка текущей сети.
/// \param block_height Высота финализируемого блока-контейнера.
/// \param params Активные protocol parameters.
/// \param state Кандидатное состояние, модифицируемое только при успехе.
/// \return Код причины отказа либо `NONE`.
/// \pre `state` должно быть внутренне согласованным; функция не исправляет уже испорченное состояние.
/// \post При успехе атомарно создаются Identity и AccountState, списывается onboarding bonus и,
///       при наличии matching genesis allocation, она заявляется ровно один раз.
/// \note Детерминированно на всех Full Node; потокобезопасность не гарантируется для совместного доступа к `state`.
AccountCreateStateError ApplyAccountCreate(const AccountCreateOp& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Применяет NameCommit к кандидатному состоянию.
NameCommitError ApplyNameCommit(const AuthorizedNameCommit& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Применяет NameReveal к кандидатному состоянию.
NameRevealError ApplyNameReveal(const AuthorizedNameReveal& op,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params, CybouState& state);

/// \brief Результаты валидации и применения RootPublication.
enum class RootPublicationError : uint8_t {
    NONE,                      ///< Переход выполнен.
    INVALID_PAYLOAD,           ///< Payload неканоничен либо не удалось вычислить/сопоставить комиссию.
    INVALID_AUTHORIZATION,     ///< Identity authorization не совпала с payload или с финализированным Identity registry.
    SENDER_NOT_FOUND,          ///< Авторизующий аккаунт отсутствует в состоянии.
    INSUFFICIENT_SYSTEM_BALANCE, ///< Недостаточно `System Balance` для комиссии публикации.
    FEE_TRANSFER_FAILED,       ///< Комиссия не может быть безопасно зачислена Central Authority fail-closed.
};

/// \brief Применяет RootPublication и маршрутизацию её комиссии.
RootPublicationError ApplyRootPublication(const AuthorizedRootPublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state);

/// \brief Результаты применения RevokePublication.
enum class RevokePublicationError : uint8_t {
    NONE,                        ///< Публикация отозвана, квота освобождена.
    INVALID_PAYLOAD,             ///< Payload неканоничен.
    INVALID_AUTHORIZATION,       ///< Identity authorization невалидна.
    SENDER_NOT_FOUND,            ///< Авторизующий аккаунт отсутствует.
    PUBLICATION_NOT_FOUND,       ///< Действующей публикации с таким OperationID нет.
    NOT_OWNER,                   ///< Публикация принадлежит другой Identity.
    INSUFFICIENT_SYSTEM_BALANCE, ///< Недостаточно `System Balance` для комиссии.
    FEE_TRANSFER_FAILED,         ///< Комиссия не может быть безопасно зачислена.
    INCONSISTENT_STATE,          ///< Учёт квоты расходится с регистром публикаций.
};

/// \brief Отзывает собственную публикацию; комиссия равна `payment_fee`.
RevokePublicationError ApplyRevokePublication(const AuthorizedRevokePublication& op,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params, CybouState& state);

/// \brief Регистрирует финализируемую RootPublication и учитывает её chunk-и в квоте автора.
/// \return false, если запись уже существует или учёт переполнился (состояние не меняется).
bool RecordPublication(CybouState& state, const cybou::Hash256& publication_id, const AccountId& owner,
    const ChunkId& chunk_authorization_root, uint32_t chunk_count, uint64_t height);

/// \brief Ошибки детерминированной валидации канонического состояния.
enum class StateValidationError : uint8_t {
    NONE,                           ///< Все инварианты канонического состояния соблюдены.
    ACCOUNT_LIMIT_EXCEEDED,         ///< Число аккаунтов превысило консенсусный лимит.
    ACCOUNT_IDENTITY_COUNT_MISMATCH, ///< `accounts` и `identities` потеряли взаимно-однозначность.
    MISSING_IDENTITY,               ///< Для AccountState нет matching Identity record.
    DUPLICATE_RECOVERY_BINDING,     ///< RecoveryKeyId неоднозначен или не индексируется обратно.
    BALANCE_OVERFLOW,               ///< TotalSupply переполнен либо превысил `MAX_SUPPLY`.
    INVALID_NAME_REGISTRY,          ///< Нарушены правила имён, pending commit-ов или genesis-label binding.
    INVALID_RESOURCE_USAGE,         ///< Учёт ресурсов или регистр публикаций неканоничен либо рассогласован.
};

/// \brief Проверяет внутренние инварианты консенсусного состояния.
/// \param state Полный снимок состояния.
/// \return Детализированный код ошибки; `NONE` только для канонически допустимого состояния.
/// \post Состояние не изменяется.
/// \note Потокобезопасно при неизменяемом доступе; детерминировано и fail-closed.
StateValidationError ValidateCybouState(const CybouState& state, uint64_t* out_total_supply = nullptr);
/// \brief Считает канонический total supply, исключая AUTH и обнаруживая переполнения.
/// \param state Полный снимок состояния.
/// \return Сумма OnboardingPool + незаявленных genesis allocation + `Balance` + `System Balance`;
///         при переполнении возвращает `std::numeric_limits<uint64_t>::max()`.
/// \post AUTH сознательно не включается в вычисление в соответствии с `docs/cybou/57_IDENTITY_AUTHORITY.md`.
uint64_t TotalSupply(const CybouState& state);

/// \brief Сериализует каноническое состояние в детерминированный бинарный формат.
/// \param state Валидное каноническое состояние.
/// \param validate Если true, предварительно выполняет ValidateCybouState.
/// \return Байты единственного поддерживаемого wire/storage-формата либо `std::nullopt`, если
///         состояние нарушает инварианты или не сериализуется без неоднозначности.
/// \pre `ValidateCybouState(state) == StateValidationError::NONE`.
/// \post При успехе порядок байтов полностью каноничен: все map уже отсортированы по ключу.
std::optional<std::vector<unsigned char>> SerializeCybouState(const CybouState& state, bool validate = true);
/// \brief Десериализует и валидирует каноническое состояние.
/// \param bytes Полный сериализованный state snapshot.
/// \return `CybouState`, если вход точен, все поля каноничны и итоговый снимок проходит `ValidateCybouState`;
///         иначе `std::nullopt`.
/// \note Потокобезопасно; legacy-декодеры отсутствуют по инварианту single-current-baseline.
std::optional<CybouState> DeserializeCybouState(std::span<const unsigned char> bytes);
/// \brief Вычисляет domain-separated hash канонического состояния.
/// \param state Валидное состояние.
/// \param validate Если true, сериализация выполняет предварительную ValidateCybouState.
/// \return `state root` либо `std::nullopt`, если состояние не сериализуется канонически.
/// \post При успехе hash зависит только от канонических байтов состояния.
/// \note Потокобезопасно и детерминировано.
std::optional<cybou::Hash256> CybouStateHash(const CybouState& state, bool validate = true);

} // namespace cybou
#endif // CYBOU_STATE_H
