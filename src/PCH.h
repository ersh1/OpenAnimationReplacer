#pragma once

#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#define IMGUI_DEFINE_MATH_OPERATORS

#define USE_BS_LOCKS

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <effolkronium/random.hpp>

#ifdef NDEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)

#include <boost/container_hash/hash.hpp>
#include <ranges>
#include <shared_mutex>

using namespace std::literals;

namespace logger = SKSE::log;

namespace util
{
	using SKSE::stl::report_and_fail;
}

namespace std
{
	template <class T>
	struct hash<RE::BSPointerHandle<T>>
	{
		uint32_t operator()(const RE::BSPointerHandle<T>& a_handle) const
		{
			const uint32_t nativeHandle = const_cast<RE::BSPointerHandle<T>*>(&a_handle)->native_handle();  // ugh
			return nativeHandle;
		}
	};

	template <class T>
	struct hash<RE::hkRefPtr<T>>
	{
		uintptr_t operator()(const RE::hkRefPtr<T>& a_ptr) const
		{
			return reinterpret_cast<uintptr_t>(a_ptr.get());
		}
	};

	template <class T1, class T2>
	struct pointer_pair_hash
	{
		std::size_t operator()(const std::pair<T1*, T2*>& a_p) const
		{
			std::size_t seed = 0;
			boost::hash_combine(seed, a_p.first);
			boost::hash_combine(seed, a_p.second);
			return seed;
		}
	};
}

namespace RE
{
	template <class T>
	bool operator<(const RE::BSPointerHandle<T>& a_lhs, const RE::BSPointerHandle<T>& a_rhs)
	{
		return a_lhs.native_handle() < a_rhs.native_handle();
	}

	template <class T>
	std::size_t hash_value(const BSPointerHandle<T>& a_handle)
	{
		boost::hash<uint32_t> hasher;
		return hasher(a_handle.native_handle());
	};

}

// Hash functor template
template <typename T>
struct KeyHash
{
	size_t operator()(const T& key) const
	{
		// Default hash implementation
		return std::hash<T>{}(key);
	}
};

template <typename T1, typename T2>
struct KeyHash<std::pair<T1, T2>>
{
	std::size_t operator()(const std::pair<T1, T2>& a_p) const
	{
		std::size_t seed = 0;
		boost::hash_combine(seed, a_p.first);
		boost::hash_combine(seed, a_p.second);
		return seed;
	}
};

// Special hash support for std::string_view to enable heterogeneous lookup
template <>
struct KeyHash<std::string>
{
	using is_transparent = void;

	size_t operator()(std::string_view key) const
	{
		return std::hash<std::string_view>{}(key);
	}
};

struct CaseInsensitiveHash
{
	size_t operator()(const std::string& a_key) const
	{
		std::string lowerStr = a_key;
		std::ranges::transform(lowerStr, lowerStr.begin(), [](char c) {
			return static_cast<char>(std::tolower(c));
		});
		return std::hash<std::string>()(lowerStr);
	}
};

struct CaseInsensitiveEqual
{
	bool operator()(const std::string& a, const std::string& b) const
	{
		return std::equal(a.begin(), a.end(), b.begin(), b.end(),
			[](char a, char b) {
				return std::tolower(a) == std::tolower(b);
			});
	}
};

struct CaseInsensitiveCompare
{
	bool operator()(const std::string& a, const std::string& b) const
	{
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
			[](char a, char b) {
				return std::tolower(a) < std::tolower(b);
			});
	}
};

// Case-insensitive hash function for std::filesystem::path
struct CaseInsensitivePathHash
{
	size_t operator()(const std::filesystem::path& a_path) const
	{
		std::string lowerStr = a_path.string();
		std::ranges::transform(lowerStr, lowerStr.begin(), [](char c) {
			return static_cast<char>(std::tolower(c));
		});
		return std::hash<std::string>()(lowerStr);
	}
};

// Case-insensitive key equality function for std::filesystem::path
struct CaseInsensitivePathEqual
{
	bool operator()(const std::filesystem::path& a_lhs, const std::filesystem::path& a_rhs) const
	{
		std::string lhsStr = a_lhs.string();
		std::string rhsStr = a_rhs.string();

		if (lhsStr.length() != rhsStr.length()) {
			return false;
		}

		std::ranges::transform(lhsStr, lhsStr.begin(), [](char c) {
			return static_cast<char>(std::tolower(c));
		});
		std::ranges::transform(rhsStr, rhsStr.begin(), [](char c) {
			return static_cast<char>(std::tolower(c));
		});

		return lhsStr == rhsStr;
	}
};

#ifdef USE_BS_LOCKS
using ExclusiveLock = RE::BSSpinLock;
using Locker = RE::BSSpinLockGuard;

using SharedLock = RE::BSReadWriteLock;
using ReadLocker = RE::BSReadLockGuard;
using WriteLocker = RE::BSWriteLockGuard;
#else
using ExclusiveLock = std::mutex;
using Locker = std::lock_guard<ExclusiveLock>;

using SharedLock = std::shared_mutex;
using ReadLocker = std::shared_lock<SharedLock>;
using WriteLocker = std::unique_lock<SharedLock>;
#endif

#define DLLEXPORT __declspec(dllexport)

#include "Plugin.h"
