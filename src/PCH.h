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

using ExclusiveLock = std::mutex;
using Locker = std::lock_guard<ExclusiveLock>;

using SharedLock = std::shared_mutex;
using ReadLocker = std::shared_lock<SharedLock>;
using WriteLocker = std::unique_lock<SharedLock>;

#define DLLEXPORT __declspec(dllexport)

#include "Plugin.h"
