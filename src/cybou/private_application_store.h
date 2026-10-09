// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Локальная зашифрованная rebuildable Application DB одной Identity.

#ifndef CYBOU_PRIVATE_APPLICATION_STORE_H
#define CYBOU_PRIVATE_APPLICATION_STORE_H

#include <cybou/account_id.h>
#include <cybou/keystore.h>
#include <cybou/kv_store.h>

#include <array>
#include <stdexcept>
#include <map>
#include <string>
#include <utility>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

/// Исключение несовпадения ключа локальной Application DB и текущей Identity.
class PrivateApplicationStoreKeyMismatch final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Возвращает каталог Identity внутри общей директории данных CYBOU.
/// \return `<data_dir>/identities/<AccountID-hex>`.
std::filesystem::path IdentityDataDirectory(const std::filesystem::path& data_dir, const AccountId& account);

/// Персональная зашифрованная Application DB одной разблокированной Identity.
class PrivateApplicationStore final {
public:
    /// Открывает `<identity_dir>/app.db` для текущей Identity.
    /// \param identity Разблокированный keystore Identity.
    /// \param identity_dir Каталог этой Identity внутри локальной data plane.
    /// \throws std::invalid_argument Если Identity заблокирована или путь пуст.
    /// \throws PrivateApplicationStoreKeyMismatch Если `app.db` зашифрована ключом другой Identity.
    /// \post Key-check либо проверен, либо создан fail-closed до первой пользовательской записи.
    PrivateApplicationStore(CybouKeyStore& identity, const std::filesystem::path& identity_dir,
        std::string_view filename = "app.db", bool preserve_data_key = false);
    /// Durably wrap the same data key for a prepared rotation before submitting it.
    bool PrepareKeyRotation(std::span<const unsigned char, 32> new_recovery_entropy);
    ~PrivateApplicationStore();

    PrivateApplicationStore(const PrivateApplicationStore&) = delete;
    PrivateApplicationStore& operator=(const PrivateApplicationStore&) = delete;

    /// Сохраняет запись; false при блокировке, смене ключа или ошибке хранилища.
    /// \pre `name` не пустое, не зарезервированное и укладывается в лимит.
    /// \post `plaintext` не журналируется; данные на диске всегда шифруются и аутентифицируются.
    bool Put(std::string_view name, std::span<const unsigned char> plaintext);
    /// Возвращает расшифрованную запись по имени.
    /// \return Значение или `std::nullopt`, если запись отсутствует, store заблокирован либо дешифрование не прошло.
    /// \post Возвращённый plaintext считается секретом и должен быть очищен владельцем копий.
    std::optional<std::vector<unsigned char>> Get(std::string_view name) const;
    /// Проверяет наличие записи по имени.
    bool Has(std::string_view name) const;
    /// Удаляет запись по имени.
    /// \return `true`, если удаление подтверждено локально или отложено внутри открытого batch.
    bool Erase(std::string_view name);
    /// Возвращает true, если доступ к store сейчас разблокирован.
    bool IsUnlocked() const;

    /// Одно изменение batch: сохранить значение или удалить запись.
    using Change = std::pair<std::string, std::optional<std::vector<unsigned char>>>;
    /// Атомарно применяет пакет изменений к LevelDB.
    /// \return `true`, если весь batch зафиксирован или `false` при любой ошибке.
    /// \post При неудаче commit fail-closed: частичный plaintext на диск не попадает.
    bool WriteBatch(std::span<const Change> changes);

    /// Группирует зависимые записи в атомарную логическую транзакцию.
    class Batch final {
    public:
        /// Открывает batch или savepoint поверх текущего store.
        /// \thread_safety Batch захватывает `store.m_mutex` на всё время жизни объекта.
        explicit Batch(PrivateApplicationStore& store);
        ~Batch();
        Batch(const Batch&) = delete;
        Batch& operator=(const Batch&) = delete;
        /// Подтверждает batch или savepoint.
        /// \return `true`, если изменения приняты; для outermost batch это означает запись на диск.
        /// \post При провале все staged plaintext текущего уровня очищаются.
        bool Commit();

    private:
        using Staged = std::map<std::string, std::optional<std::vector<unsigned char>>>;
        void RollBack();
        static void Cleanse(Staged& staged);

        PrivateApplicationStore& m_store;
        bool m_outermost{false};
        bool m_done{false};
        /** Enclosing batch state at this savepoint (nested batches only). */
        Staged m_savepoint;
        bool m_savepoint_failed{false};
    };

    /// Возвращает AccountID владельца store.
    const AccountId& Account() const { return m_account; }
    /// Возвращает путь к каталогу `app.db`.
    const std::filesystem::path& Path() const { return m_path; }

private:
    std::optional<std::array<unsigned char, 32>> AccessKey() const;
    std::optional<std::array<unsigned char, 32>> UnwrapKey(std::span<const unsigned char, 32> wrapping_key) const;
    bool WrapKey(std::span<const unsigned char, 32> wrapping_key, std::span<const unsigned char, 32> data_key,
        bool prepared_rotation = false);
    std::optional<std::string> RecordKey(std::span<const unsigned char, 32> key, std::string_view name) const;
    std::optional<std::vector<unsigned char>> Encrypt(std::span<const unsigned char, 32> key,
        std::string_view name, std::span<const unsigned char> plaintext) const;
    std::optional<std::vector<unsigned char>> Decrypt(std::span<const unsigned char, 32> key,
        std::string_view name, std::span<const unsigned char> encoded) const;

    CybouKeyStore& m_identity;
    AccountId m_account;
    std::filesystem::path m_path;
    std::array<unsigned char, 32> m_key_check{};
    std::unique_ptr<KVStore> m_db;
    bool m_preserve_data_key{false};
    mutable std::recursive_mutex m_mutex;
    /// Изменения открытого batch, индексированные по имени записи; plaintext живёт только до Commit/RollBack/деструктора.
    std::optional<std::map<std::string, std::optional<std::vector<unsigned char>>>> m_staged;
    bool m_staged_failed{false};
};

} // namespace cybou

#endif // CYBOU_PRIVATE_APPLICATION_STORE_H
