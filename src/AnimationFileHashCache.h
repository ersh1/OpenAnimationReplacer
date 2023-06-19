#pragma once

#include "Parsing.h"

struct CachedAnimationHash
{
    CachedAnimationHash(uint64_t a_lastWriteTime, uint64_t a_fileSize, std::string_view a_hash) :
        lastWriteTime(a_lastWriteTime),
        fileSize(a_fileSize),
        hash(a_hash) {}

    uint64_t lastWriteTime;
    uint64_t fileSize;
    std::string hash;
};

class AnimationFileHashCache final
{
public:
    static AnimationFileHashCache& GetSingleton()
    {
        static AnimationFileHashCache singleton;
        return singleton;
    }

    void ReadCacheFromDisk();
    void WriteCacheToDisk();
    void DeleteCache();

    static std::string CalculateHash(std::string_view a_fullPath);

    [[nodiscard]] bool IsDirty() const { return _bDirty; }

    [[nodiscard]] bool TryGetCachedHash(std::string_view a_path, uint64_t a_lastWriteTime, uint64_t a_fileSize, std::string& a_outCachedHash) const;

    void SaveHash(std::string_view a_path, uint64_t a_lastWriteTime, uint64_t a_fileSize, std::string_view a_hash)
    {
        WriteLocker locker(_dataLock);

        _cache.emplace(a_path, CachedAnimationHash(a_lastWriteTime, a_fileSize, a_hash));

        _bDirty = true;
    }

private:
    AnimationFileHashCache() = default;
    AnimationFileHashCache(const AnimationFileHashCache&) = delete;
    AnimationFileHashCache(AnimationFileHashCache&&) = delete;
    ~AnimationFileHashCache() = default;

    AnimationFileHashCache& operator=(const AnimationFileHashCache&) = delete;
    AnimationFileHashCache& operator=(AnimationFileHashCache&&) = delete;

    mutable SharedLock _dataLock;
    std::unordered_map<std::string, CachedAnimationHash> _cache;
    bool _bDirty = false;
};
