// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Восстановимый локальный индекс Mail/Files поверх финализированных RootPublication.

#ifndef CYBOU_APPLICATION_SERVICE_H
#define CYBOU_APPLICATION_SERVICE_H

#include <cybou/private_application_schema.h>
#include <cybou/private_application_store.h>
#include <cybou/root_publication.h>

#include <cybou/hash256.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

namespace cybou {

class CybouKeyStore;
class CybouNodeRuntime;
class StorageService;

/// Локальная папка почтового сообщения в Application DB.
enum class MailFolder : std::uint8_t { INBOX = 1, SENT = 2, ARCHIVE = 3, TRASH = 4, DELETED = 5 };

/// Локально проиндексированная запись письма.
struct MailRecord {
    cybou::Hash256 operation_id;
    std::uint64_t finalized_height{0};
    std::uint32_t operation_index{0};
    /// Отправитель из внешней авторизованной публикации, а не из расшифрованного тела.
    AccountId sender;
    bool outgoing{false};
    MailMessage message;
    MailFolder folder{MailFolder::INBOX};
    bool read{false};
    bool starred{false};
};

/// Канонический порядок публикации мутации, породившей текущее состояние объекта.
struct PrivateOrder {
    std::uint64_t height{0};
    std::uint32_t operation_index{0};
    std::uint32_t mutation_index{0};
    auto operator<=>(const PrivateOrder&) const = default;
};

/// Локально проиндексированная запись Files.
struct FileRecord {
    FileItem item;
    cybou::Hash256 operation_id;
    PrivateOrder order;
    bool deleted{false};
    /// Локальное зашифрованное состояние Identity, не публикуемое в сеть.
    bool starred{false};
};

/// Состояние локальной доступности и индексации корня публикации.
enum class AccessibleRootState : std::uint8_t {
    DISCOVERED = 1,
    INDEXED = 2,
    /// Контент временно недоступен и будет повторно запрошен позже.
    TEMPORARILY_UNAVAILABLE = 3,
    /// Капсула открылась, но документ недопустим для этой Identity.
    INVALID = 4,
};

/// Локальное вложение черновика: путь на устройстве или ссылка на Files.
struct DraftAttachment {
    std::string name;
    std::uint64_t logical_size{0};
    std::string source_path;   ///< Локальный файл, выбранный на этом устройстве.
    std::string reference_id;  ///< Ссылка Files, например `ref-<item>`.
    bool operator==(const DraftAttachment&) const = default;
};

/// Непубликованный compose state письма в локальной Application DB.
struct MailDraft {
    std::string draft_id; ///< `[a-z0-9-]`, не более 64 символов.
    std::string to;
    std::string subject;
    std::string body;
    std::uint64_t updated_ms{0};
    std::vector<DraftAttachment> attachments;
    bool operator==(const MailDraft&) const = default;
};

/// Текущий прогресс сканирования финализированной истории приложения.
struct ApplicationScanProgress {
    std::uint64_t scanned_height{0};
    std::uint64_t finalized_height{0};
    std::uint32_t unavailable_roots{0};
    bool Complete() const { return scanned_height >= finalized_height && unavailable_roots == 0; }
};


/// Входное обнаружение и приватная индексация для одной разблокированной Identity.
class ApplicationService final {
public:
    /// Создаёт сервис индексации поверх runtime, keystore, Application DB и StorageService.
    ApplicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
        PrivateApplicationStore& application_db, StorageService& storage);

    /// Сканирует до `max_blocks` новых финализированных блоков и повторяет недоступные корни.
    ApplicationScanProgress Scan(std::uint64_t max_blocks = 256);
    /// Возвращает текущий прогресс сканирования.
    ApplicationScanProgress Progress();

    /// Возвращает локально доступные письма.
    std::vector<MailRecord> ListMail();
    /// Возвращает письмо по его PrivateItemId.
    std::optional<MailRecord> GetMail(const PrivateItemId& message_id);
    /// Обновляет локальный флаг прочтения письма.
    bool SetMailRead(const PrivateItemId& message_id, bool read);
    /// Обновляет локальный флаг звезды письма.
    bool SetMailStarred(const PrivateItemId& message_id, bool starred);
    /// Сохраняет локальный флаг звезды существующего объекта Files.
    bool SetFileStarred(const PrivateItemId& item_id, bool starred);
    /// Перемещает письмо между локальными папками.
    bool MoveMail(const PrivateItemId& message_id, MailFolder folder);

    /// Возвращает текущий каталог Files без удалённых элементов.
    std::vector<FileRecord> ListFiles();
    /// Возвращает один объект Files по его PrivateItemId.
    std::optional<FileRecord> GetFile(const PrivateItemId& item_id);

    /// Сохраняет локальный черновик письма.
    bool SaveDraft(const MailDraft& draft);
    /// Возвращает локальные черновики, начиная с самых новых.
    std::vector<MailDraft> ListDrafts();
    /// Удаляет локальный черновик по `draft_id`.
    bool DeleteDraft(std::string_view draft_id);

    /// Возвращает собственные RecoveryBridge в каноническом порядке.
    std::vector<IdentityRecoveryBridge> RecoveryBridges();

    /// Возвращает локальное состояние индексации публикации по её OperationID.
    std::optional<AccessibleRootState> PublicationState(const cybou::Hash256& operation_id);

private:
    struct Accessible;
    bool ProcessBlock(std::uint64_t height, std::uint64_t my_key_epoch);
    bool ProcessPublication(std::uint64_t height, std::uint32_t index, const cybou::Hash256& operation_id,
        const AuthorizedRootPublication& publication, std::uint64_t my_key_epoch);
    void RecoverOwnPublications(std::uint32_t max_publications);
    bool RecoverPlacement(const cybou::Hash256& operation_id, Accessible& accessible);
    AccessibleRootState Index(const cybou::Hash256& operation_id, Accessible& accessible);
    bool ApplyMail(const cybou::Hash256& operation_id, const Accessible& accessible, const MailMessage& message);
    bool ApplyFiles(const cybou::Hash256& operation_id, const Accessible& accessible, const FilesMutationBatch& batch);
    bool ApplyBridge(const cybou::Hash256& operation_id, const Accessible& accessible, const IdentityRecoveryBridge& bridge);
    std::optional<Accessible> LoadAccessible(const cybou::Hash256& operation_id) const;
    bool SaveAccessible(const cybou::Hash256& operation_id, const Accessible& accessible);
    std::optional<MailRecord> LoadMail(const PrivateItemId& id) const;
    bool SaveMail(const MailRecord& record);
    std::optional<FileRecord> LoadFile(const PrivateItemId& id) const;
    bool SaveFile(const FileRecord& record);
    std::uint64_t Checkpoint() const;
    /// Импортирует проверенные исторические KEM seed из RecoveryBridge.
    bool ImportBridgeSeeds(const AccountId& me, std::uint64_t my_key_epoch);

    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_identity;
    PrivateApplicationStore& m_application_db;
    StorageService& m_storage;
    std::mutex m_mutex;
    /// Установлен на однократном проходе ремонта старых индексов.
    bool m_repairing{false};
};

} // namespace cybou

#endif // CYBOU_APPLICATION_SERVICE_H
