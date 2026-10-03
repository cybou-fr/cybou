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
enum class MailFolder : std::uint8_t {
    /// Входящие письма.
    INBOX = 1,
    /// Исходящие письма текущей Identity.
    SENT = 2,
    /// Локально архивированные письма.
    ARCHIVE = 3,
    /// Локальная корзина перед окончательным удалением.
    TRASH = 4,
    /// Логически удалённые записи, скрываемые обычным UX.
    DELETED = 5
};

/// Локально проиндексированная запись письма.
struct MailRecord {
    /// OperationID внешней финализированной публикации.
    cybou::Hash256 operation_id;
    /// Высота финализированного блока публикации.
    std::uint64_t finalized_height{0};
    /// Порядок операции внутри блока.
    std::uint32_t operation_index{0};
    /// Отправитель из внешней авторизованной публикации, а не из расшифрованного тела.
    AccountId sender;
    /// `true`, если это собственная исходящая публикация текущей Identity.
    bool outgoing{false};
    /// Расшифрованное каноническое тело письма.
    MailMessage message;
    /// Локальная папка отображения.
    MailFolder folder{MailFolder::INBOX};
    /// Локальный флаг прочтения.
    bool read{false};
    /// Локальный флаг избранного.
    bool starred{false};
};

/// Канонический порядок публикации мутации, породившей текущее состояние объекта.
struct PrivateOrder {
    /// Высота блока финализации.
    std::uint64_t height{0};
    /// Индекс операции в блоке.
    std::uint32_t operation_index{0};
    /// Индекс мутации внутри приватного batch.
    std::uint32_t mutation_index{0};
    auto operator<=>(const PrivateOrder&) const = default;
};

/// Локально проиндексированная запись Files.
struct FileRecord {
    /// Локально актуальное состояние объекта Files.
    FileItem item;
    /// OperationID публикации, породившей текущее состояние.
    cybou::Hash256 operation_id;
    /// Канонический порядок последней мутации.
    PrivateOrder order;
    /// Логическое удаление из индекса.
    bool deleted{false};
    /// Локальное зашифрованное состояние Identity, не публикуемое в сеть.
    bool starred{false};
};

/// Состояние локальной доступности и индексации корня публикации.
enum class AccessibleRootState : std::uint8_t {
    /// Публикация обнаружена в финализированной истории, но ещё не проиндексирована.
    DISCOVERED = 1,
    /// Корень успешно открыт, прочитан и применён к локальному индексу.
    INDEXED = 2,
    /// Контент временно недоступен и будет повторно запрошен позже.
    TEMPORARILY_UNAVAILABLE = 3,
    /// Капсула открылась, но документ недопустим для этой Identity.
    INVALID = 4,
};

/// Локальное вложение черновика: путь на устройстве или ссылка на Files.
struct DraftAttachment {
    /// Отображаемое имя вложения.
    std::string name;
    /// Логический размер plaintext-вложения.
    std::uint64_t logical_size{0};
    std::string source_path;   ///< Локальный файл, выбранный на этом устройстве.
    std::string reference_id;  ///< Ссылка Files, например `ref-<item>`.
    bool operator==(const DraftAttachment&) const = default;
};

/// Непубликованный compose state письма в локальной Application DB.
struct MailDraft {
    std::string draft_id; ///< `[a-z0-9-]`, не более 64 символов.
    /// Текстовое поле адресата в UX.
    std::string to;
    /// Локально редактируемая тема.
    std::string subject;
    /// Локально редактируемое тело письма.
    std::string body;
    /// Время последнего изменения локального черновика, Unix ms.
    std::uint64_t updated_ms{0};
    /// Локальные вложения, ещё не опубликованные в сеть.
    std::vector<DraftAttachment> attachments;
    bool operator==(const MailDraft&) const = default;
};

/// Текущий прогресс сканирования финализированной истории приложения.
struct ApplicationScanProgress {
    /// Последняя полностью обработанная финализированная высота.
    std::uint64_t scanned_height{0};
    /// Текущая известная финализированная высота runtime.
    std::uint64_t finalized_height{0};
    /// Число корней, ожидающих повторной попытки из-за временной недоступности.
    std::uint32_t unavailable_roots{0};
    bool Complete() const { return scanned_height >= finalized_height && unavailable_roots == 0; }
};


/// Входное обнаружение и приватная индексация для одной разблокированной Identity.
class ApplicationService final {
public:
    /// Создаёт сервис индексации поверх runtime, keystore, Application DB и StorageService.
    /// \thread_safety Публичные методы сериализуют доступ внутренним `m_mutex`; возвращают копии локальных записей.
    ApplicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
        PrivateApplicationStore& application_db, StorageService& storage);

    /// Сканирует до `max_blocks` новых финализированных блоков и повторяет недоступные корни.
    /// \param max_blocks Верхняя граница числа новых блоков за один проход.
    /// \return Обновлённый прогресс локальной индексации.
    ApplicationScanProgress Scan(std::uint64_t max_blocks = 256);
    /// Возвращает текущий прогресс сканирования.
    /// \return Текущая точка локального rebuildable индекса.
    ApplicationScanProgress Progress();

    /// Возвращает локально доступные письма.
    /// \return Копия локально проиндексированных писем.
    std::vector<MailRecord> ListMail();
    /// Возвращает письмо по его PrivateItemId.
    /// \return Письмо либо `std::nullopt`, если запись отсутствует или повреждена.
    std::optional<MailRecord> GetMail(const PrivateItemId& message_id);
    /// Обновляет локальный флаг прочтения письма.
    /// \return `true`, если запись письма найдена и сохранена в локальной Application DB.
    bool SetMailRead(const PrivateItemId& message_id, bool read);
    /// Обновляет локальный флаг звезды письма.
    /// \return `true`, если запись письма найдена и сохранена.
    bool SetMailStarred(const PrivateItemId& message_id, bool starred);
    /// Сохраняет локальный флаг звезды существующего объекта Files.
    /// \return `true`, если запись Files найдена и сохранена.
    bool SetFileStarred(const PrivateItemId& item_id, bool starred);
    /// Перемещает письмо между локальными папками.
    /// \return `true`, если письмо найдено и новая папка сохранена локально.
    bool MoveMail(const PrivateItemId& message_id, MailFolder folder);

    /// Возвращает текущий каталог Files без удалённых элементов.
    /// \return Копия локально проиндексированных элементов Files.
    std::vector<FileRecord> ListFiles();
    /// Возвращает один объект Files по его PrivateItemId.
    /// \return Запись либо `std::nullopt`, если объект отсутствует или удалён из индекса.
    std::optional<FileRecord> GetFile(const PrivateItemId& item_id);

    /// Сохраняет локальный черновик письма.
    /// \return `true`, если черновик принят локальной Application DB.
    bool SaveDraft(const MailDraft& draft);
    /// Возвращает локальные черновики, начиная с самых новых.
    /// \return Копия локальных черновиков, отсортированная по `updated_ms`.
    std::vector<MailDraft> ListDrafts();
    /// Удаляет локальный черновик по `draft_id`.
    /// \return `true`, если черновик удалён или отсутствовал.
    bool DeleteDraft(std::string_view draft_id);

    /// Возвращает собственные RecoveryBridge в каноническом порядке.
    /// \return Список мостов, пригодных для восстановления исторических KEM epoch.
    std::vector<IdentityRecoveryBridge> RecoveryBridges();

    /// Возвращает локальное состояние индексации публикации по её OperationID.
    /// \return `std::nullopt`, если публикация ещё не известна локальному индексу.
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
