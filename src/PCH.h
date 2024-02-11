#pragma once

#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#define IMGUI_DEFINE_MATH_OPERATORS

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
}

namespace RE
{
	template <class T>
	bool operator<(const RE::BSPointerHandle<T>& a_lhs, const RE::BSPointerHandle<T>& a_rhs)
	{
		return a_lhs.native_handle() < a_rhs.native_handle();
	}
}

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

using ExclusiveLock = std::mutex;
using Locker = std::lock_guard<ExclusiveLock>;

using SharedLock = std::shared_mutex;
using ReadLocker = std::shared_lock<SharedLock>;
using WriteLocker = std::unique_lock<SharedLock>;

#define DLLEXPORT __declspec(dllexport)

#include "Plugin.h"
