#include "AnimationFileHashCache.h"

#include <binary_io/binary_io.hpp>
#include <mmio/mmio.hpp>

#include <cryptopp/sha.h>

#include "Settings.h"


void AnimationFileHashCache::ReadCacheFromDisk()
{
    if (!std::filesystem::exists(Settings::animationFileHashCachePath)) {
        return;
    }

    WriteLocker locker(_dataLock);

    binary_io::file_istream in{ Settings::animationFileHashCachePath };
    const auto readString = [&](std::string& a_dst) {
        uint16_t len;
        in.read(len);
        a_dst.resize(len);
        in.read_bytes(std::as_writable_bytes(std::span{ a_dst.data(), a_dst.size() }));
    };

    uint32_t numEntries;
    in.read(numEntries);

    for (uint32_t i = 0; i < numEntries; i++) {
        std::string fullPath;
        uint64_t lastWriteTime;
        uint64_t fileSize;
        std::string hash;

        readString(fullPath);
        in.read(lastWriteTime);
        in.read(fileSize);
        readString(hash);
        _cache.emplace(fullPath, CachedAnimationHash(lastWriteTime, fileSize, hash));
    }

    _bDirty = false;
}

void AnimationFileHashCache::WriteCacheToDisk()
{
    binary_io::file_ostream out{ Settings::animationFileHashCachePath };
    const auto writeString = [&](const std::string_view a_str) {
        out.write(static_cast<uint16_t>(a_str.length()));
        out.write_bytes(std::as_bytes(std::span{ a_str.data(), a_str.length() }));
    };

    ReadLocker locker(_dataLock);

    const auto numEntries = static_cast<uint32_t>(_cache.size());
    out.write(numEntries);

    for (auto& [pathStr, cachedHash] : _cache) {
        writeString(pathStr);
        out.write(cachedHash.lastWriteTime);
        out.write(cachedHash.fileSize);
        writeString(cachedHash.hash);
    }

    _bDirty = false;
}

void AnimationFileHashCache::DeleteCache()
{
    if (std::filesystem::is_regular_file(Settings::animationFileHashCachePath)) {
        std::filesystem::remove(Settings::animationFileHashCachePath);
    }

    WriteLocker locker(_dataLock);
    _cache.clear();

    _bDirty = false;
}

std::string AnimationFileHashCache::CalculateHash(const Parsing::ReplacementAnimationToAdd& a_anim)
{
    // Search cached hashes first
    WIN32_FILE_ATTRIBUTE_DATA fad;
    uint64_t lastWriteTime = 0;
    uint64_t fileSize = 0;
    if (GetFileAttributesEx(a_anim.fullPath.data(), GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER ulTime;
        ulTime.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        ulTime.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        lastWriteTime = ulTime.QuadPart;
        ULARGE_INTEGER ulSize;
        ulSize.HighPart = fad.nFileSizeHigh;
        ulSize.LowPart = fad.nFileSizeLow;
        fileSize = ulSize.QuadPart;
    }

    auto& hashCache = GetSingleton();

    std::string ret;
    if (hashCache.TryGetCachedHash(a_anim.fullPath, lastWriteTime, fileSize, ret)) {
        return ret;
    }

    // Calculate a hash from the animation file
    mmio::mapped_file_source file;
    if (file.open(a_anim.fullPath)) {
        CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
        CryptoPP::SHA256().CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(file.data()), file.size());

        ret = std::string(reinterpret_cast<char*>(digest), CryptoPP::SHA256::DIGESTSIZE);
        hashCache.SaveHash(a_anim.fullPath, lastWriteTime, fileSize, ret);
    }

    return ret;
}

bool AnimationFileHashCache::TryGetCachedHash(const std::string_view a_path, const uint64_t a_lastWriteTime, const uint64_t a_fileSize, std::string& a_outCachedHash) const
{
    ReadLocker locker(_dataLock);

    if (const auto it = _cache.find(a_path.data()); it != _cache.end()) {
        if (it->second.fileSize == a_fileSize && it->second.lastWriteTime == a_lastWriteTime) {
            a_outCachedHash = it->second.hash;
            return true;
        }
    }
    return false;
}
