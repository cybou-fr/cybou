// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Исходящие приватные публикации, локальный staging и отслеживание durability.

#ifndef CYBOU_PUBLICATION_SERVICE_H
#define CYBOU_PUBLICATION_SERVICE_H

#include <cybou/identity_operation_coordinator.h>
#include <cybou/private_application_schema.h>
#include <cybou/private_application_store.h>
#include <cybou/encrypted_chunk_tree.h>

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cybou {

class CybouNodeRuntime;
class StorageService;

/// \brief Уже подготовленный локальный bundle для RootPublication.
struct PreparedPublicationBundle {
    ChunkId root_chunk_id{};
    ContentKey content_key{};
    ChunkId chunk_authorization_root{};
    std::uint32_t chunk_count{0};
};

/// \brief Текущая фаза локальной publication job.
enum class PublicationJobPhase : std::uint8_t {
    WAITING_FINALITY = 1,
    SECURING = 2,
    NEEDS_ATTENTION = 3,
    /// Finalized и каждый chank достиг целевого числа удалённых реплик.
    PROTECTED = 4,
    /// Ожидание более ранней Identity-операции; capsule строятся при отправке.
    QUEUED = 5,
};

/// \brief Наблюдаемое состояние publication job.
struct PublicationJobResult {
    PublicationJobPhase phase{PublicationJobPhase::NEEDS_ATTENTION};
    cybou::Hash256 operation_id;
    std::uint64_t finalized_height{0};
    std::string error;
    /// 0..100 на пути к remote replica target при SECURING; -1 если неизвестно.
    int durability_percent{-1};
};

/// \brief Новый локальный источник данных, который нужно зашифровать в отдельное дерево.
struct NewContent {
    EncryptedTreeSource source;
};

/// \brief Генерирует случайный private item id для сообщений, вложений и Files-элементов.
std::optional<PrivateItemId> NewPrivateItemId();

/// \brief Сервис исходящих приватных публикаций одного unlocked Identity.
///
/// Сервис хранит точный intent RootPublication, локальный порядок leaf-чанков
/// и после finality передаёт управление durability в StorageService.
class PublicationService final {
public:
    /// \brief Создаёт сервис публикаций для unlocked Identity и его Application DB.
    PublicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
        PrivateApplicationStore& application_db, IdentityOperationCoordinator& coordinator);
    /// \brief Отправляет уже подготовленный bundle через durable Identity journal.
    PublicationJobResult SubmitPrepared(std::string_view local_job_id,
        const PreparedPublicationBundle& bundle,
        std::optional<AccountId> recipient = std::nullopt);
    /// \brief Возобновляет существующую publication job по локальному идентификатору.
    PublicationJobResult Resume(std::string_view local_job_id);
    /// \brief Отменяет только безопасно забываемую job без принятой/finalized неопределённости.
    bool CancelPublication(std::string_view local_job_id);
    /// \brief Возвращает текущее состояние локальной publication job.
    std::optional<PublicationJobResult> GetJob(std::string_view local_job_id);
    /// \brief Помечает finalized job как PROTECTED после отчёта StorageService.
    bool MarkProtected(std::string_view local_job_id);

    /// \brief Публикует MAIL_MESSAGE с recipient capsule и обязательной self capsule.
    ///
    /// Новые вложения шифруются в дочерние деревья; уже защищённые вложения
    /// переиспользуются по root/key без повторной загрузки.
    PublicationJobResult PublishMail(std::string_view local_job_id, MailMessage message,
        std::vector<std::pair<std::size_t, NewContent>> new_attachments = {});
    /// \brief Публикует FILES_MUTATION_BATCH с self capsule.
    PublicationJobResult PublishFiles(std::string_view local_job_id, FilesMutationBatch batch,
        std::vector<std::pair<std::size_t, NewContent>> new_content = {});

    /// \brief Публикует IDENTITY_RECOVERY_BRIDGE для безопасного первого шага IdentityRotate.
    PublicationJobResult PublishRecoveryBridge(std::string_view local_job_id,
        std::span<const unsigned char, 32> new_recovery_entropy);
    /// \brief Проверяет, что recovery bridge уже PROTECTED и читается новым recovery entropy.
    bool VerifyRecoveryBridge(std::string_view local_job_id, std::span<const unsigned char, 32> new_recovery_entropy,
        StorageService& storage);

    /// \brief Возвращает все известные локальные job id в порядке создания.
    std::vector<std::string> Jobs();
    /// \brief Продвигает каждую незавершённую job: finality, затем placement/durability.
    std::vector<std::pair<std::string, PublicationJobResult>> ProcessDurability(StorageService& storage);

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

    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_identity;
    PrivateApplicationStore& m_application_db;
    IdentityOperationCoordinator& m_coordinator;
    std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_PUBLICATION_SERVICE_H
