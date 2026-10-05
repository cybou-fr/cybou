// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/secret_file.h>
#include <cybou/crypto/cleanse.h>
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace cybou {
#ifdef _WIN32
namespace {
/// \brief Проверяет, что файл принадлежит текущему пользователю и разрешает доступ только owner/SYSTEM.
bool OwnedPrivate(HANDLE file) {
    PSID owner=nullptr; PACL acl=nullptr; PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(GetSecurityInfo(file,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&descriptor)!=ERROR_SUCCESS) return false;
    HANDLE token=nullptr; DWORD size=0; bool ok=false;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) {
        GetTokenInformation(token,TokenUser,nullptr,0,&size);
        std::vector<unsigned char> info(size);
        if(GetTokenInformation(token,TokenUser,info.data(),size,&size)&&owner&&acl&&EqualSid(owner,reinterpret_cast<TOKEN_USER*>(info.data())->User.Sid)) {
            PSID system=nullptr;PSID owner_rights=nullptr;
            ConvertStringSidToSidW(L"S-1-5-18",&system);ConvertStringSidToSidW(L"S-1-3-4",&owner_rights);
            ok=system&&owner_rights;
            for(DWORD i=0;ok&&i<acl->AceCount;++i) {
                void* raw=nullptr;if(!GetAce(acl,i,&raw)) {ok=false;break;}
                const auto* header=static_cast<ACE_HEADER*>(raw);
                if(header->AceType==ACCESS_ALLOWED_ACE_TYPE) {
                    const auto* ace=static_cast<ACCESS_ALLOWED_ACE*>(raw);const PSID sid=const_cast<DWORD*>(&ace->SidStart);
                    if(!EqualSid(sid,owner)&&!EqualSid(sid,system)&&!EqualSid(sid,owner_rights)) ok=false;
                } else if(header->AceType!=ACCESS_DENIED_ACE_TYPE) ok=false;
            }
            if(system)LocalFree(system);if(owner_rights)LocalFree(owner_rights);
        }
        CloseHandle(token);
    }
    LocalFree(descriptor);return ok;
}
}
#endif
std::FILE* OpenPrivateAppendFile(const std::filesystem::path& path) {
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)(A;;FA;;;SY)",SDDL_REVISION_1,&descriptor,nullptr))return nullptr;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
    HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA|READ_CONTROL,FILE_SHARE_READ,&attributes,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    LocalFree(descriptor);if(file==INVALID_HANDLE_VALUE)return nullptr;
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(file,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||GetFileType(file)!=FILE_TYPE_DISK||!OwnedPrivate(file)){CloseHandle(file);return nullptr;}
    const int fd=_open_osfhandle(reinterpret_cast<intptr_t>(file),_O_APPEND|_O_BINARY);
    if(fd<0){CloseHandle(file);return nullptr;}auto* stream=_fdopen(fd,"ab");if(!stream)_close(fd);return stream;
#else
    const int fd=open(path.c_str(),O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return nullptr;
    struct stat info{};
    if(fstat(fd,&info)!=0||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()||(info.st_mode&0077)){close(fd);return nullptr;}
    auto* stream=fdopen(fd,"ab");if(!stream)close(fd);return stream;
#endif
}

bool CreateSecretFile(const std::filesystem::path& path,std::span<const unsigned char> bytes) {
    if(bytes.empty()||bytes.size()>(1U<<20)) return false;
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)(A;;FA;;;SY)",SDDL_REVISION_1,&descriptor,nullptr)) return false;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE|READ_CONTROL,0,&attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    LocalFree(descriptor);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;const bool ok=OwnedPrivate(file)&&WriteFile(file,bytes.data(),bytes.size(),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
    CloseHandle(file);if(!ok)DeleteFileW(path.c_str());return ok;
#else
    const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return false;
    size_t offset=0;while(offset<bytes.size()) {const auto n=write(fd,bytes.data()+offset,bytes.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)break;offset+=n;}
    bool ok=offset==bytes.size()&&fsync(fd)==0;if(close(fd)!=0)ok=false;
    if(!ok) {unlink(path.c_str());return false;}
    // Отдельный fsync каталога делает durable не только содержимое, но и сам
    // факт появления нового секретного файла после сбоя питания.
    const auto parent=path.parent_path().empty()?std::filesystem::path{"."}:path.parent_path();
    const int dir=open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0)return false;ok=fsync(dir)==0;close(dir);return ok;
#endif
}
std::optional<std::vector<unsigned char>> ReadSecretFile(const std::filesystem::path& path,size_t max_bytes) {
    if(max_bytes==0||max_bytes>(1U<<20))return std::nullopt;
#ifdef _WIN32
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ|READ_CONTROL,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(file==INVALID_HANDLE_VALUE)return std::nullopt;
    BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
    if(!GetFileInformationByHandle(file,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||GetFileType(file)!=FILE_TYPE_DISK||!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>max_bytes||!OwnedPrivate(file)) {CloseHandle(file);return std::nullopt;}
    std::vector<unsigned char> bytes(size.QuadPart);DWORD read=0;
    const bool ok=ReadFile(file,bytes.data(),bytes.size(),&read,nullptr)&&read==bytes.size();CloseHandle(file);
#else
    const int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return std::nullopt;
    struct stat info{};if(fstat(fd,&info)!=0||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()||(info.st_mode&0077)||info.st_size<=0||info.st_size>max_bytes) {close(fd);return std::nullopt;}
    std::vector<unsigned char> bytes(info.st_size);size_t offset=0;
    while(offset<bytes.size()) {const auto n=read(fd,bytes.data()+offset,bytes.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)break;offset+=n;}close(fd);const bool ok=offset==bytes.size();
#endif
    if(!ok) {crypto::CleanseMemory(bytes.data(),bytes.size());return std::nullopt;}return bytes;
}
}
