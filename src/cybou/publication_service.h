// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Исходящие приватные публикации, локальный staging и отслеживание durability.

#ifndef CYBOU_PUBLICATION_SERVICE_H
#define CYBOU_PUBLICATION_SERVICE_H

#include <cybou/identity_operation_coordinator.h>
#include <cybou/private_application_schema.h>
#include <cybou/private_application_store.h>
#include <cybou/encrypted_chunk_tree.h>

#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cybou {

class CybouNodeRuntime;
class StorageService;

/// \brief Settlement-периоды (сутки), оплачиваемые одной автоматической StorageLease публикации.
inline constexpr std::uint32_t DEFAULT_STORAGE_LEASE_PERIODS{30};

/// \brief Уже подготовленный локальный bundle для RootPublication.
struct PreparedPublicationBundle {
    /// \brief ChunkId корневого ROOT-чанка документа.
    ChunkId root_chunk_id{};
    /// \brief ContentKey дерева, необходимый владельцу/получателю для чтения содержимого.
    ContentKey content_key{};
    /// \brief Корень chunk authorization leaves в точном staged-порядке.
    ChunkId chunk_authorization_root{};
    /// \brief Число чанков, закоммиченных в \ref chunk_authorization_root.
    std::uint32_t chunk_count{0};
};

/// \brief Текущая фаза локальной publication job.
enum class PublicationJobPhase : std::uint8_t {
    /// \brief Candidate-операция отправлена или ожидает финализацию.
    WAITING_FINALITY = 1,
    /// \brief Публикация финализована, но удалённая durability ещё не достигла target.
    SECURING = 2,
    /// \brief Требуется локальное вмешательство: retry/resume не дал безопасного результата.
    NEEDS_ATTENTION = 3,
    /// Finalized и каждый chank достиг целевого числа удалённых реплик.
    PROTECTED = 4,
    /// Ожидание более ранней Identity-операции; capsule строятся при отправке.
    QUEUED = 5,
};

/// \brief Наблюдаемое состояние publication job.
struct PublicationJobResult {
    /// \brief Текущее логическое состояние job.
    PublicationJobPhase phase{PublicationJobPhase::NEEDS_ATTENTION};
    /// \brief OperationID RootPublication, если candidate-операция уже была построена.
    cybou::Hash256 operation_id;
    /// \brief Высота блока финализации, известная для finalized job.
    std::uint64_t finalized_height{0};
    /// \brief Последняя диагностическая ошибка или причина ожидания.
    std::string error;
    /// 0..100 на пути к remote replica target при SECURING; -1 если неизвестно.
    int durability_percent{-1};
};

/// \brief Новый локальный источник данных, который нужно зашифровать в отдельное дерево.
struct NewContent {
    /// \brief Потоковый источник plaintext нового вложения/файла.
    EncryptedTreeSource source;
};

/// \brief Генерирует случайный private item id для сообщений, вложений и Files-элементов.
/// \return Новый PrivateItemId либо \c std::nullopt при отказе RNG или генерации
/// запрещённых всех-нулей/всех-0xff.
/// \par Потокобезопасность
/// Не использует разделяемое состояние этого модуля; потокобезопасность RNG
/// определяется криптобиблиотекой.
std::optional<PrivateItemId> NewPrivateItemId();

/// \brief Сервис исходящих приватных публикаций одного unlocked Identity.
///
/// Сервис хранит точный intent RootPublication, локальный порядок leaf-чанков
/// и после finality передаёт управление durability в StorageService.
class PublicationService final {
public:
    /// \brief Создаёт сервис публикаций для unlocked Identity и его Application DB.
    /// \param runtime Локальный Full Node runtime.
    /// \param identity Разблокированная Identity отправителя.
    /// \param application_db Персональный Application DB этой Identity.
    /// \param coordinator Durable coordinator для Identity-операций.
    /// \post Сервис может очистить незавершённый локальный staging предыдущего запуска.
    /// \throw std::runtime_error При обнаружении повреждённого локального staging state.
    /// \par Потокобезопасность
    /// После построения объект сериализует собственные публичные операции внутренним mutex.
    PublicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
        PrivateApplicationStore& application_db, IdentityOperationCoordinator& coordinator,
        PrivateApplicationStore* local_recovery_db = nullptr);
    /// Restore an exact Outbox-owned job; absence returns nullopt, invalid evidence fails closed.
    std::optional<PublicationJobResult> RecoverJob(std::string_view local_job_id,
        const PreparedPublicationBundle& bundle, const Hash256& expected_operation);
    /// \brief Отправляет уже подготовленный bundle через durable Identity journal.
    /// \param local_job_id Локальный ASCII job id.
    /// \param bundle Уже staged bundle с root/content/proof summary.
    /// \param recipient Необязательный recipient AccountId.
    /// \return Текущее состояние job; при ошибке phase обычно `NEEDS_ATTENTION`.
    /// \pre \p local_job_id должен соответствовать локальному формату job id.
    /// \pre `bundle.chunk_count > 0` и локальный ChunkBlobStore уже содержит `bundle.root_chunk_id`.
    /// \post При успехе intent/job записаны в Application DB.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationJobResult SubmitPrepared(std::string_view local_job_id,
        const PreparedPublicationBundle& bundle,
        std::optional<AccountId> recipient = std::nullopt);
    /// \brief Возобновляет существующую publication job по локальному идентификатору.
    /// \param local_job_id Локальный job id.
    /// \return Актуальное состояние job либо `NEEDS_ATTENTION`, если job отсутствует/повреждена.
    /// \post Может продвинуть job из QUEUED/WAITING_FINALITY в более позднюю фазу.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationJobResult Resume(std::string_view local_job_id);
    /// \brief Отменяет только безопасно забываемую job без принятой/finalized неопределённости.
    /// \param local_job_id Локальный job id.
    /// \return \c true, если job безопасно помечена/доведена до отмены; \c false,
    /// если у job уже есть принятой candidate или финализованная неопределённость.
    /// \post При успехе локальные staging pins и saved intent/leaves могут быть очищены.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool CancelPublication(std::string_view local_job_id);
    /// \brief Возвращает текущее состояние локальной publication job.
    /// \param local_job_id Локальный job id.
    /// \return Текущее состояние job либо \c std::nullopt, если job отсутствует
    /// или локальная запись повреждена.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<PublicationJobResult> GetJob(std::string_view local_job_id);
    /// \brief Помечает finalized job как PROTECTED после отчёта StorageService.
    /// \param local_job_id Локальный job id.
    /// \return \c true, если локальное состояние успешно обновлено до PROTECTED.
    /// \pre У job уже должна быть финализация и достигнут target удалённых реплик.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool MarkProtected(std::string_view local_job_id);

    /// \brief Публикует MAIL_MESSAGE с recipient capsule и обязательной self capsule.
    ///
    /// Новые вложения шифруются в дочерние деревья; уже защищённые вложения
    /// переиспользуются по root/key без повторной загрузки.
    /// \param local_job_id Локальный job id.
    /// \param message Приватный Mail document.
    /// \param new_attachments Источники новых вложений, индексированные по позиции в \p message.attachments.
    /// \return Состояние созданной/возобновлённой publication job.
    /// \pre Переиспользуемые вложения уже должны ссылаться на доступный PROTECTED content.
    /// \post При успехе точный intent публикации сохранён до финализации.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationJobResult PublishMail(std::string_view local_job_id, MailMessage message,
        std::vector<std::pair<std::size_t, NewContent>> new_attachments = {});
    /// \brief Публикует FILES_MUTATION_BATCH с self capsule.
    /// \param local_job_id Локальный job id.
    /// \param batch Приватный Files document.
    /// \param new_content Источники нового контента, индексированные по позиции mutation.
    /// \return Состояние созданной/возобновлённой publication job.
    /// \pre \p batch.mutations не должен быть пустым.
    /// \post При успехе точный intent публикации сохранён до финализации.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationJobResult PublishFiles(std::string_view local_job_id, FilesMutationBatch batch,
        std::vector<std::pair<std::size_t, NewContent>> new_content = {});

    /// \brief Публикует IDENTITY_RECOVERY_BRIDGE для безопасного первого шага IdentityRotate.
    /// \param local_job_id Локальный job id.
    /// \param new_recovery_entropy Новый recovery secret, для которого готовится bridge.
    /// \return Состояние созданной/возобновлённой publication job.
    /// \post При успехе bridge ждёт финализацию и дальнейшую durability-проверку.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationJobResult PublishRecoveryBridge(std::string_view local_job_id,
        std::span<const unsigned char, 32> new_recovery_entropy);
    /// \brief Проверяет, что recovery bridge уже PROTECTED и читается новым recovery entropy.
    /// \param local_job_id Локальный job id bridge-публикации.
    /// \param new_recovery_entropy Новый recovery secret, который обязан открывать bridge.
    /// \param storage StorageService для чтения finalized чанков.
    /// \return \c true, если PROTECTED bridge читается и содержит ожидаемые поля.
    /// \pre Job уже должна быть финализована и доведена до PROTECTED.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта, но зависит от внешней потокобезопасности \p storage.
    bool VerifyRecoveryBridge(std::string_view local_job_id, std::span<const unsigned char, 32> new_recovery_entropy,
        StorageService& storage);

    /// \brief Возвращает все известные локальные job id в порядке создания.
    /// \return Список локальных job id.
    /// \par Потокобезопасность
    /// Требует, чтобы вызывающая сторона не модифицировала одновременно тот же Application DB в обход сервиса.
    std::vector<std::string> Jobs();
    /// \brief Продвигает ограниченный набор jobs по кругу: finality, затем placement/durability.
    /// \param storage StorageService для финализованных публикаций.
    /// \param max_jobs Максимальное число jobs за проход; ноль не выполняет работу.
    /// \return Пары `job id -> актуальное состояние` только для обработанных jobs.
    /// \post Может перевести job между WAITING_FINALITY, SECURING, PROTECTED и NEEDS_ATTENTION.
    /// \par Потокобезопасность
    /// Метод сам не удерживает общий mutex на всём проходе; корректность опирается на потокобезопасность этого сервиса и \p storage.
    std::vector<std::pair<std::string, PublicationJobResult>> ProcessDurability(
        StorageService& storage, std::size_t max_jobs = std::numeric_limits<std::size_t>::max());

    /// \brief Решает, нужна ли ещё собственная публикация: OperationID и её authorization leaves.
    using PublicationNeeded = std::function<bool(const cybou::Hash256& operation_id, std::span<const ChunkId> leaves)>;
    /// \brief Отзывает одну собственную финализированную публикацию, которая больше ничему не нужна (DEC-271).
    /// \details Не более одного отзыва в полёте; мосты восстановления не трогаются; отзыв не тратит
    ///          последние операции окна (резерв для действий пользователя). После финализации job удаляется
    ///          локально, а его pin освобождается.
    /// \return OperationID публикации, чей отзыв сейчас в полёте или только что отправлен; иначе nullopt.
    std::optional<cybou::Hash256> RevokeUnreferenced(const PublicationNeeded& needed);

private:
    struct Job;
    /// \brief Неподписанный intent публикации, независимый от будущего nonce.
    struct Intent {
        PreparedPublicationBundle bundle;
        std::optional<AccountId> recipient;
        std::optional<std::pair<XWingPublicKey, std::uint64_t>> future_self;
    };
    std::optional<Intent> LoadIntent(std::string_view local_job_id) const;
    bool SaveIntent(std::string_view local_job_id, const Intent& intent);
    PublicationJobResult BuildAndSubmit(std::string_view local_job_id, const Intent& intent);
    /// \brief Результат локального staging: bundle плюс точный порядок authorization leaves.
    struct Staged {
        PreparedPublicationBundle bundle;
        std::vector<ChunkId> leaves;
    };
    /// \brief Строит private metadata корневого документа из дочерних summary.
    using BuildMetadata = std::function<std::optional<std::vector<unsigned char>>(
        std::span<const EncryptedTreeSummary> children)>;
    std::optional<Job> Load(std::string_view local_job_id) const;
    bool IsCancellationPending(std::string_view local_job_id) const;
    bool FinishCancellation(std::string_view local_job_id);
    bool Save(std::string_view local_job_id, const Job& job);
    PublicationJobResult ResumeLocked(std::string_view local_job_id, Job& job);
    PublicationJobResult SubmitPreparedLocked(std::string_view local_job_id,
        const PreparedPublicationBundle& bundle, std::optional<AccountId> recipient,
        std::optional<std::pair<XWingPublicKey, std::uint64_t>> future_self);
    std::optional<Staged> Stage(std::string_view local_job_id, std::vector<NewContent>& children,
        const BuildMetadata& build_metadata, std::string& error);
    std::optional<std::vector<ChunkId>> LoadLeaves(std::string_view local_job_id) const;
    /// Обеспечивает финализированную аренду перед placement; подаёт StorageLease не более одного в полёте.
    /// \return true, только если аренда уже активна в finalized state.
    bool EnsureStorageLease(std::string_view local_job_id, const cybou::Hash256& publication_id);
    /// \brief Удаляет локальную job после финализированного отзыва её публикации.
    bool ForgetRevokedJob(std::string_view local_job_id);

    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_identity;
    PrivateApplicationStore& m_application_db;
    IdentityOperationCoordinator& m_coordinator;
    PrivateApplicationStore* m_local_recovery_db;
    std::mutex m_mutex;
    /// Volatile round-robin position; never stored in the Application DB.
    std::size_t m_durability_next{0};
};

} // namespace cybou

#endif // CYBOU_PUBLICATION_SERVICE_H
