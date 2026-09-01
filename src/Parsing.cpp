#include "Parsing.h"

#include <future>
#include <mmio/mmio.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <thread>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <type_traits>

#include "AnimationFileHashCache.h"
#include "OpenAnimationReplacer.h"
#include "Settings.h"

#ifdef _WIN32
constexpr ULONG ThreadIoPriority = 22;
constexpr ULONG LowThreadIoPriority = 0;
constexpr ULONG HighThreadIoPriority = 3;
using NtSetInformationThreadFn = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG);

static std::atomic<DWORD> g_directoryCacheThreadId{ 0 };

static void SetIOPriority(HANDLE a_thread, ULONG a_priority)
{
	if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
		if (const auto ntSetInformationThread = reinterpret_cast<NtSetInformationThreadFn>(GetProcAddress(ntdll, "NtSetInformationThread"))) {
			ntSetInformationThread(a_thread, ThreadIoPriority, &a_priority, sizeof(a_priority));
		}
	}
}

static void SetLowIOPriority()
{
	SetIOPriority(GetCurrentThread(), LowThreadIoPriority);
}
#endif

static std::atomic<uint32_t> g_precachedHashCount{ 0 };

namespace Parsing
{
	namespace
	{
		constexpr auto timingBucketCount = static_cast<size_t>(TimingBucket::kTotal);
		constexpr auto timingCounterCount = static_cast<size_t>(TimingCounter::kTotal);

		std::array<std::atomic<int64_t>, timingBucketCount> timingNanoseconds;
		std::array<std::atomic<uint64_t>, timingBucketCount> timingCounts;
		std::array<std::atomic<uint64_t>, timingCounterCount> timingCounters;

		constexpr std::string_view GetTimingBucketName(TimingBucket a_bucket)
		{
			switch (a_bucket) {
			case TimingBucket::kModJson:
				return "Mod JSON";
			case TimingBucket::kSubModJson:
				return "Submod JSON";
			case TimingBucket::kConditionsTxt:
				return "Legacy conditions.txt";
			case TimingBucket::kAnimationDirectoryScan:
				return "Animation directory scans";
			case TimingBucket::kAnimationFileHash:
				return "Animation file hash/read";
			case TimingBucket::kSetAnimationFiles:
				return "Set animation files";
			case TimingBucket::kCacheAnimationPathSubMods:
				return "Cache animation path map";
			default:
				return "Unknown";
			}
		}

		double ToMilliseconds(const int64_t a_nanoseconds)
		{
			return std::chrono::duration<double, std::milli>(std::chrono::nanoseconds(a_nanoseconds)).count();
		}

		class WorkerPool
		{
		public:
			explicit WorkerPool(uint32_t a_workerCount)
			{
				a_workerCount = std::max(1u, a_workerCount);
				_workers.reserve(a_workerCount);

				for (uint32_t i = 0; i < a_workerCount; ++i) {
					_workers.emplace_back([this]() {
						while (true) {
							std::function<void()> task;

							{
								std::unique_lock lock(_queueLock);
								_condition.wait(lock, [this]() {
									return _bStop || !_tasks.empty();
								});

								if (_bStop && _tasks.empty()) {
									return;
								}

								task = std::move(_tasks.front());
								_tasks.pop();
							}

							task();
						}
					});
				}
			}

			WorkerPool(const WorkerPool&) = delete;
			WorkerPool& operator=(const WorkerPool&) = delete;

			~WorkerPool()
			{
				{
					std::scoped_lock lock(_queueLock);
					_bStop = true;
				}

				_condition.notify_all();

				for (auto& worker : _workers) {
					if (worker.joinable()) {
						worker.join();
					}
				}
			}

			template <class F>
			auto Enqueue(F&& a_task)
			{
				using ReturnType = std::invoke_result_t<std::decay_t<F>>;

				auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(a_task));
				auto future = task->get_future();

				{
					std::scoped_lock lock(_queueLock);
					_tasks.emplace([task]() {
						(*task)();
					});
				}

				_condition.notify_one();

				return future;
			}

		private:
			std::vector<std::thread> _workers;
			std::queue<std::function<void()>> _tasks;
			std::mutex _queueLock;
			std::condition_variable _condition;
			bool _bStop = false;
		};
	}

	void ResetTimingStats()
	{
		if constexpr (bEnableParseTiming) {
			for (auto& timing : timingNanoseconds) {
				timing.store(0, std::memory_order_relaxed);
			}
			for (auto& count : timingCounts) {
				count.store(0, std::memory_order_relaxed);
			}
			for (auto& counter : timingCounters) {
				counter.store(0, std::memory_order_relaxed);
			}
		}
	}

	void LogTimingStats()
	{
		if constexpr (bEnableParseTiming) {
			logger::info("Accumulated parsing timings (worker-time, may exceed wall time with async parsing):");
			for (size_t i = 0; i < timingBucketCount; ++i) {
				const auto duration = timingNanoseconds[i].load(std::memory_order_relaxed);
				const auto count = timingCounts[i].load(std::memory_order_relaxed);
				if (count > 0) {
					logger::info("  {}: {:.3f}ms ({} calls)", GetTimingBucketName(static_cast<TimingBucket>(i)), ToMilliseconds(duration), count);
				}
			}

			const auto hashCalculated = timingCounters[static_cast<size_t>(TimingCounter::kAnimationHashCalculated)].load(std::memory_order_relaxed);
			const auto hashCacheHits = timingCounters[static_cast<size_t>(TimingCounter::kAnimationHashCacheHit)].load(std::memory_order_relaxed);
			const auto hashFailures = timingCounters[static_cast<size_t>(TimingCounter::kAnimationHashFailed)].load(std::memory_order_relaxed);
			if (hashCalculated || hashCacheHits || hashFailures) {
				logger::info("  Animation hash details: {} calculated, {} cache hits, {} failures", hashCalculated, hashCacheHits, hashFailures);
			}

			const auto directoryEntries = timingCounters[static_cast<size_t>(TimingCounter::kDirectoryEntriesSeen)].load(std::memory_order_relaxed);
			const auto directoryDirectories = timingCounters[static_cast<size_t>(TimingCounter::kDirectoryDirectoriesSeen)].load(std::memory_order_relaxed);
			const auto directoryFiles = timingCounters[static_cast<size_t>(TimingCounter::kDirectoryFilesSeen)].load(std::memory_order_relaxed);
			const auto directoryHkxFiles = timingCounters[static_cast<size_t>(TimingCounter::kDirectoryHkxFilesFound)].load(std::memory_order_relaxed);
			const auto directoryInvalidPaths = timingCounters[static_cast<size_t>(TimingCounter::kDirectoryInvalidPaths)].load(std::memory_order_relaxed);
			const auto directoryHiddenRecursionSkips = timingCounters[static_cast<size_t>(TimingCounter::kDirectoryHiddenRecursionSkips)].load(std::memory_order_relaxed);
			if (directoryEntries || directoryDirectories || directoryFiles || directoryHkxFiles || directoryInvalidPaths || directoryHiddenRecursionSkips) {
				logger::info("  Directory scan details: {} entries, {} directories, {} files, {} hkx files, {} invalid paths, {} hidden recursion skips", directoryEntries, directoryDirectories, directoryFiles, directoryHkxFiles, directoryInvalidPaths, directoryHiddenRecursionSkips);
			}
		}
	}

	void AddTiming(TimingBucket a_bucket, std::chrono::nanoseconds a_duration)
	{
		if constexpr (bEnableParseTiming) {
			const auto index = static_cast<size_t>(a_bucket);
			timingNanoseconds[index].fetch_add(a_duration.count(), std::memory_order_relaxed);
			timingCounts[index].fetch_add(1, std::memory_order_relaxed);
		}
	}

	void AddTimingCounter(TimingCounter a_counter)
	{
		if constexpr (bEnableParseTiming) {
			timingCounters[static_cast<size_t>(a_counter)].fetch_add(1, std::memory_order_relaxed);
		}
	}

	ConditionsTxtFile::ConditionsTxtFile(const std::filesystem::path& a_fileName) :
		file(a_fileName),
		filename(a_fileName.string())
	{
		if (!file.is_open()) {
			//util::report_and_fail("Error opening _conditions.txt file");
			logger::error("Error opening {} file", a_fileName.string());
		}
	}

	ConditionsTxtFile::~ConditionsTxtFile()
	{
		file.close();
	}

	std::unique_ptr<Conditions::ConditionSet> ConditionsTxtFile::GetConditions(std::string& a_currentLine, bool a_bInOrBlock /*= false*/)
	{
		auto conditions = std::make_unique<Conditions::ConditionSet>();

		do {
			if (file.fail()) {
				if (file.eof()) {
					break;
				}
				//util::report_and_fail("Error reading from _conditions.txt file");
				logger::error("Error reading from {} file", filename);
				return std::move(conditions);
			}

			a_currentLine = Utils::TrimWhitespace(a_currentLine);
			if (!a_currentLine.empty()) {
				const bool bEndsWithOR = a_currentLine.ends_with("OR"sv);
				if (bEndsWithOR && !a_bInOrBlock) {
					// start an OR block - create an OR condition and add all conditions to it until we reach AND
					auto orCondition = OpenAnimationReplacer::GetSingleton().CreateCondition("OR");
					static_cast<Conditions::ORCondition*>(orCondition.get())->conditionsComponent->conditionSet = GetConditions(a_currentLine, true);
					conditions->Add(orCondition);
				} else {
					// create and add a new condition
					if (auto newCondition = Conditions::CreateConditionFromString(a_currentLine)) {
						conditions->Add(newCondition);

						if (!bEndsWithOR && a_bInOrBlock) {
							// end an OR block
							return std::move(conditions);
						}
					}
				}
			}
		} while (std::getline(file, a_currentLine));

		return std::move(conditions);
	}

	std::unique_ptr<Conditions::ConditionSet> ParseConditionsTxt(const std::filesystem::path& a_txtPath)
	{
		ScopedTimer timer(TimingBucket::kConditionsTxt);

		ConditionsTxtFile txt(a_txtPath);

		std::string line;
		std::getline(txt.file, line);

		return txt.GetConditions(line);
	}

	bool DeserializeMod(const std::filesystem::path& a_jsonPath, DeserializeMode a_deserializeMode, ModParseResult& a_outParseResult)
	{
		ScopedTimer timer(TimingBucket::kModJson);

		mmio::mapped_file_source file;
		if (file.open(a_jsonPath)) {
			//rapidjson::StringStream stream{ reinterpret_cast<const char*>(file.data()) };
			rapidjson::MemoryStream stream{ reinterpret_cast<const char*>(file.data()), file.size() };

			rapidjson::Document doc;
			doc.ParseStream(stream);

			if (doc.HasParseError()) {
				logger::error("Failed to parse file: {}", a_jsonPath.string());
				return false;
			}

			if (a_deserializeMode != DeserializeMode::kDataOnly) {
				// read mod name (required)
				if (const auto nameIt = doc.FindMember("name"); nameIt != doc.MemberEnd() && nameIt->value.IsString()) {
					a_outParseResult.name = nameIt->value.GetString();
				} else {
					logger::error("Failed to find mod name in file: {}", a_jsonPath.string());
					return false;
				}

				// read mod author (optional)
				if (const auto authorIt = doc.FindMember("author"); authorIt != doc.MemberEnd() && authorIt->value.IsString()) {
					a_outParseResult.author = authorIt->value.GetString();
				}

				// read mod description (optional)
				if (const auto descriptionIt = doc.FindMember("description"); descriptionIt != doc.MemberEnd() && descriptionIt->value.IsString()) {
					a_outParseResult.description = descriptionIt->value.GetString();
				}
			}

			if (a_deserializeMode == DeserializeMode::kInfoOnly) {
				// we're only here to get the info, so we're done
				return true;
			}

			// read condition presets (optional)
			if (const auto presetIt = doc.FindMember("conditionPresets"); presetIt != doc.MemberEnd() && presetIt->value.IsArray()) {
				for (auto& conditionPresetValue : presetIt->value.GetArray()) {
					if (conditionPresetValue.IsObject()) {
						const auto conditionPresetObject = conditionPresetValue.GetObj();

						if (const auto conditionPresetNameIt = conditionPresetObject.FindMember("name"); conditionPresetNameIt != conditionPresetObject.MemberEnd() && conditionPresetNameIt->value.IsString()) {
							std::string conditionPresetName = conditionPresetNameIt->value.GetString();

							std::string conditionPresetDescription = "";  // optional
							if (const auto conditionPresetDescriptionIt = conditionPresetObject.FindMember("description"); conditionPresetDescriptionIt != conditionPresetObject.MemberEnd() && conditionPresetDescriptionIt->value.IsString()) {
								conditionPresetDescription = conditionPresetDescriptionIt->value.GetString();
							}

							if (const auto conditionPresetConditionSetIt = conditionPresetObject.FindMember("conditions"); conditionPresetConditionSetIt != conditionPresetObject.MemberEnd() && conditionPresetConditionSetIt->value.IsArray()) {
								auto conditionPreset = std::make_unique<Conditions::ConditionPreset>(conditionPresetName, conditionPresetDescription);
								for (auto& conditionValue : conditionPresetConditionSetIt->value.GetArray()) {
									auto condition = Conditions::CreateConditionFromJson(conditionValue);
									if (!condition->IsValid()) {
										logger::error("Failed to parse condition in file: {}", a_jsonPath.string());

										rapidjson::StringBuffer buffer;
										rapidjson::PrettyWriter writer(buffer);
										doc.Accept(writer);

										logger::error("Dumping entire json file from memory: {}", buffer.GetString());
									}

									conditionPreset->Add(condition);
								}

								a_outParseResult.conditionPresets.push_back(std::move(conditionPreset));
							}
						}
					}
				}
			}

			a_outParseResult.path = a_jsonPath.parent_path().string();
			a_outParseResult.bSuccess = true;

			return true;
		}

		logger::error("Failed to open file: {}", a_jsonPath.string());
		return false;
	}

	bool DeserializeSubMod(std::filesystem::path a_jsonPath, DeserializeMode a_deserializeMode, SubModParseResult& a_outParseResult)
	{
		ScopedTimer timer(TimingBucket::kSubModJson);

		mmio::mapped_file_source file;
		if (file.open(a_jsonPath)) {
			//rapidjson::StringStream stream{ reinterpret_cast<const char*>(file.data()) };
			rapidjson::MemoryStream stream{ reinterpret_cast<const char*>(file.data()), file.size() };

			rapidjson::Document doc;
			doc.ParseStream(stream);

			if (doc.HasParseError()) {
				logger::error("Failed to parse file: {}", a_jsonPath.string());
				return false;
			}

			if (a_deserializeMode != DeserializeMode::kDataOnly) {
				// read submod name (required)
				if (auto nameIt = doc.FindMember("name"); nameIt != doc.MemberEnd() && nameIt->value.IsString()) {
					a_outParseResult.name = nameIt->value.GetString();
				} else {
					logger::error("Failed to find mod name in file: {}", a_jsonPath.string());
					return false;
				}

				// read submod description (optional)
				if (auto descriptionIt = doc.FindMember("description"); descriptionIt != doc.MemberEnd() && descriptionIt->value.IsString()) {
					a_outParseResult.description = descriptionIt->value.GetString();
				}
			}

			if (a_deserializeMode == DeserializeMode::kInfoOnly) {
				// we're only here to get the info, so we're done
				return true;
			}

			// read submod priority (required)
			if (auto it = doc.FindMember("priority"); it != doc.MemberEnd() && it->value.IsInt()) {
				a_outParseResult.priority = it->value.GetInt();
			} else {
				logger::error("Failed to find submod priority in file: {}", a_jsonPath.string());
				return false;
			}

			// read submod disabled (optional)
			if (auto it = doc.FindMember("disabled"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bDisabled = it->value.GetBool();
			}

			// read disabled animations (optional, json field deprecated and replaced by replacementAnimDatas - reading it kept for compatibility with older config versions)
			if (auto it = doc.FindMember("disabledAnimations"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& disabledAnimation : it->value.GetArray()) {
					if (disabledAnimation.IsObject()) {
						auto projectNameIt = disabledAnimation.FindMember("projectName");
						if (auto pathIt = disabledAnimation.FindMember("path"); projectNameIt != disabledAnimation.MemberEnd() && projectNameIt->value.IsString() && pathIt != disabledAnimation.MemberEnd() && pathIt->value.IsString()) {
							a_outParseResult.replacementAnimDatas.emplace_back(projectNameIt->value.GetString(), pathIt->value.GetString(), true);
						}
					}
				}
			}

			// read deprecated settings - for backwards compatibility
			if (auto it = doc.FindMember("keepRandomResultsOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bKeepRandomResultsOnLoop_DEPRECATED = it->value.GetBool();
			}
			if (auto it = doc.FindMember("shareRandomResults"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bShareRandomResults_DEPRECATED = it->value.GetBool();
			}

			// read replacement animation datas (optional)
			if (auto it = doc.FindMember("replacementAnimDatas"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& replacementAnimData : it->value.GetArray()) {
					if (replacementAnimData.IsObject()) {
						auto projectNameIt = replacementAnimData.FindMember("projectName");
						if (auto pathIt = replacementAnimData.FindMember("path"); projectNameIt != replacementAnimData.MemberEnd() && projectNameIt->value.IsString() && pathIt != replacementAnimData.MemberEnd() && pathIt->value.IsString()) {
							bool bDisabled = false;
							if (auto disabledIt = replacementAnimData.FindMember("disabled"); disabledIt != replacementAnimData.MemberEnd() && disabledIt->value.IsBool()) {
								bDisabled = disabledIt->value.GetBool();
							}

							// read replacement animation variants
							std::optional<std::vector<ReplacementAnimData::Variant>> variants = std::nullopt;
							std::optional<VariantMode> variantMode = std::nullopt;
							std::optional<Conditions::StateDataScope> variantStateScope = std::nullopt;
							if (a_outParseResult.bShareRandomResults_DEPRECATED) {
								variantStateScope = Conditions::StateDataScope::kSubMod;
							}
							bool bBlendBetweenVariants = true;
							bool bResetRandomOnLoopOrEcho = !a_outParseResult.bKeepRandomResultsOnLoop_DEPRECATED;
							bool bSharePlayedHistory = false;

							if (auto variantsIt = replacementAnimData.FindMember("variants"); variantsIt != replacementAnimData.MemberEnd() && variantsIt->value.IsArray()) {
								int32_t variantIndex = 0;
								for (auto& variantObj : variantsIt->value.GetArray()) {
									if (variantObj.IsObject()) {
										if (auto variantFilenameIt = variantObj.FindMember("filename"); variantFilenameIt != variantObj.MemberEnd() && variantFilenameIt->value.IsString()) {
											bool bVariantDisabled = false;
											float variantWeight = 1.f;
											bool bVariantPlayOnce = false;

											if (auto variantDisabledIt = variantObj.FindMember("disabled"); variantDisabledIt != variantObj.MemberEnd() && variantDisabledIt->value.IsBool()) {
												bVariantDisabled = variantDisabledIt->value.GetBool();
											}
											if (auto weightIt = variantObj.FindMember("weight"); weightIt != variantObj.MemberEnd() && weightIt->value.IsNumber()) {
												variantWeight = weightIt->value.GetFloat();
											}
											if (auto variantPlayOnceIt = variantObj.FindMember("playOnce"); variantPlayOnceIt != variantObj.MemberEnd() && variantPlayOnceIt->value.IsBool()) {
												bVariantPlayOnce = variantPlayOnceIt->value.GetBool();
											}

											ReplacementAnimData::Variant variant(variantFilenameIt->value.GetString(), bVariantDisabled, variantWeight, variantIndex++, bVariantPlayOnce);

											if (!variants.has_value()) {
												variants.emplace();
											}

											variants->emplace_back(variant);
										}
									}
								}
							}

							if (auto variantModeIt = replacementAnimData.FindMember("variantMode"); variantModeIt != replacementAnimData.MemberEnd() && variantModeIt->value.IsNumber()) {
								variantMode = static_cast<VariantMode>(variantModeIt->value.GetInt());
							}

							if (auto variantStateScopeIt = replacementAnimData.FindMember("variantStateScope"); variantStateScopeIt != replacementAnimData.MemberEnd() && variantStateScopeIt->value.IsNumber()) {
								variantStateScope = static_cast<Conditions::StateDataScope>(variantStateScopeIt->value.GetInt());
							}

							if (auto blendIt = replacementAnimData.FindMember("blendBetweenVariants"); blendIt != replacementAnimData.MemberEnd() && blendIt->value.IsBool()) {
								bBlendBetweenVariants = blendIt->value.GetBool();
							}

							if (auto resetRandomIt = replacementAnimData.FindMember("resetRandomOnLoopOrEcho"); resetRandomIt != replacementAnimData.MemberEnd() && resetRandomIt->value.IsBool()) {
								bResetRandomOnLoopOrEcho = resetRandomIt->value.GetBool();
							}

							if (auto sharePlayedHistoryIt = replacementAnimData.FindMember("sharePlayedHistory"); sharePlayedHistoryIt != replacementAnimData.MemberEnd() && sharePlayedHistoryIt->value.IsBool()) {
								bSharePlayedHistory = sharePlayedHistoryIt->value.GetBool();
							}

							a_outParseResult.replacementAnimDatas.emplace_back(projectNameIt->value.GetString(), pathIt->value.GetString(), bDisabled, variants, variantMode, variantStateScope, bBlendBetweenVariants, bResetRandomOnLoopOrEcho, bSharePlayedHistory);
						}
					}
				}
			}

			// read override animations folder (optional)
			if (auto it = doc.FindMember("overrideAnimationsFolder"); it != doc.MemberEnd() && it->value.IsString()) {
				a_outParseResult.overrideAnimationsFolder = it->value.GetString();
			}

			// read required project name (optional)
			if (auto it = doc.FindMember("requiredBehaviorProjectName"); it != doc.MemberEnd() && it->value.IsString()) {
				a_outParseResult.requiredProjectName = it->value.GetString();
			}

			// read ignore no triggers flag (optional)
			if (auto it = doc.FindMember("ignoreDontConvertAnnotationsToTriggersFlag"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bIgnoreDontConvertAnnotationsToTriggersFlag = it->value.GetBool();
			} else if (auto oldNameIt = doc.FindMember("ignoreNoTriggersFlag"); oldNameIt != doc.MemberEnd() && oldNameIt->value.IsBool()) {  // old name
				a_outParseResult.bIgnoreDontConvertAnnotationsToTriggersFlag = oldNameIt->value.GetBool();
			}

			// read triggersOnly (optional)
			if (auto it = doc.FindMember("triggersFromAnnotationsOnly"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bTriggersFromAnnotationsOnly = it->value.GetBool();
			}

			// read interruptible (optional)
			if (auto it = doc.FindMember("interruptible"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bInterruptible = it->value.GetBool();
			}

			// read custom blend time on interrupt (optional) - only if interruptible is true
			if (a_outParseResult.bInterruptible) {
				if (auto it = doc.FindMember("hasCustomBlendTimeOnInterrupt"); it != doc.MemberEnd() && it->value.IsBool()) {
					a_outParseResult.bCustomBlendTimeOnInterrupt = it->value.GetBool();
				}
				if (a_outParseResult.bCustomBlendTimeOnInterrupt) {
					if (auto it = doc.FindMember("blendTimeOnInterrupt"); it != doc.MemberEnd() && it->value.IsNumber()) {
						a_outParseResult.blendTimeOnInterrupt = it->value.GetFloat();
					}
				}
			}

			// read replace on loop (optional)
			if (auto it = doc.FindMember("replaceOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bReplaceOnLoop = it->value.GetBool();
			}

			// read custom blend time on loop (optional) - only if replace on loop is true
			if (a_outParseResult.bReplaceOnLoop) {
				if (auto it = doc.FindMember("hasCustomBlendTimeOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
					a_outParseResult.bCustomBlendTimeOnLoop = it->value.GetBool();
				}
				if (a_outParseResult.bCustomBlendTimeOnLoop) {
					if (auto it = doc.FindMember("blendTimeOnLoop"); it != doc.MemberEnd() && it->value.IsNumber()) {
						a_outParseResult.blendTimeOnLoop = it->value.GetFloat();
					}
				}
			}

			// read replace on echo (optional)
			if (auto it = doc.FindMember("replaceOnEcho"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bReplaceOnEcho = it->value.GetBool();
			}

			// read custom blend time on echo (optional) - only if replace on echo is true
			if (a_outParseResult.bReplaceOnEcho) {
				if (auto it = doc.FindMember("hasCustomBlendTimeOnEcho"); it != doc.MemberEnd() && it->value.IsBool()) {
					a_outParseResult.bCustomBlendTimeOnEcho = it->value.GetBool();
				}
				if (a_outParseResult.bCustomBlendTimeOnEcho) {
					if (auto it = doc.FindMember("blendTimeOnEcho"); it != doc.MemberEnd() && it->value.IsNumber()) {
						a_outParseResult.blendTimeOnEcho = it->value.GetFloat();
					}
				}
			}

			// read run functions on loop (optional)
			if (auto it = doc.FindMember("runFunctionsOnLoop"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bRunFunctionsOnLoop = it->value.GetBool();
			}

			// read run functions on echo (optional)
			if (auto it = doc.FindMember("runFunctionsOnEcho"); it != doc.MemberEnd() && it->value.IsBool()) {
				a_outParseResult.bRunFunctionsOnEcho = it->value.GetBool();
			}

			// read conditions
			if (auto it = doc.FindMember("conditions"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& conditionValue : it->value.GetArray()) {
					auto condition = Conditions::CreateConditionFromJson(conditionValue, a_outParseResult.conditionSet.get());
					if (!Utils::ConditionHasPresetCondition(condition.get()) && !condition->IsValid()) {
						logger::error("Failed to parse condition in file: {}", a_jsonPath.string());

						rapidjson::StringBuffer buffer;
						rapidjson::PrettyWriter writer(buffer);
						doc.Accept(writer);

						logger::error("Dumping entire json file from memory: {}", buffer.GetString());
					}

					// backwards compatibility with deprecated setting
					if (condition->GetName() == "Random") {
						auto randomCondition = static_cast<Conditions::RandomCondition*>(condition.get());
						if (a_outParseResult.bKeepRandomResultsOnLoop_DEPRECATED) {
							randomCondition->stateComponent->SetShouldResetOnLoopOrEcho(false);
						}
						if (a_outParseResult.bShareRandomResults_DEPRECATED) {
							randomCondition->stateComponent->SetStateDataScope(Conditions::StateDataScope::kSubMod);
						}
					}

					a_outParseResult.conditionSet->Add(condition);
				}
			}

			if (auto it = doc.FindMember("pairedConditions"); it != doc.MemberEnd() && it->value.IsArray()) {
				for (auto& conditionValue : it->value.GetArray()) {
					auto condition = Conditions::CreateConditionFromJson(conditionValue, a_outParseResult.synchronizedConditionSet.get());
					if (!condition->IsValid()) {
						logger::error("Failed to parse paired condition in file: {}", a_jsonPath.string());

						rapidjson::StringBuffer buffer;
						rapidjson::PrettyWriter writer(buffer);
						doc.Accept(writer);

						logger::error("Dumping entire json file from memory: {}", buffer.GetString());
					}

					if (!a_outParseResult.synchronizedConditionSet) {
						a_outParseResult.synchronizedConditionSet = std::make_unique<Conditions::ConditionSet>();
					}

					a_outParseResult.synchronizedConditionSet->Add(condition);
				}
			}

			// read functions
			auto parseFunctionSet = [&](std::unique_ptr<Functions::FunctionSet>& functionSet, Functions::FunctionSetType a_functionSetType) {
				std::string_view memberName;
				switch (a_functionSetType) {
				case Functions::FunctionSetType::kOnActivate:
					memberName = "functionsOnActivate"sv;
					break;
				case Functions::FunctionSetType::kOnDeactivate:
					memberName = "functionsOnDeactivate"sv;
					break;
				case Functions::FunctionSetType::kOnTrigger:
					memberName = "functionsOnTrigger"sv;
					break;
				}

				if (auto it = doc.FindMember(memberName.data()); it != doc.MemberEnd() && it->value.IsArray()) {
					for (auto& functionValue : it->value.GetArray()) {
						auto function = Functions::CreateFunctionFromJson(functionValue, functionSet.get());
						if (!function->IsValid()) {
							logger::error("Failed to parse function in file: {}", a_jsonPath.string());

							rapidjson::StringBuffer buffer;
							rapidjson::PrettyWriter writer(buffer);
							doc.Accept(writer);

							logger::error("Dumping entire json file from memory: {}", buffer.GetString());
						}

						if (!functionSet) {
							functionSet = std::make_unique<Functions::FunctionSet>(a_functionSetType);
						}

						functionSet->Add(function);
					}
				}
			};

			parseFunctionSet(a_outParseResult.functionSetOnActivate, Functions::FunctionSetType::kOnActivate);
			parseFunctionSet(a_outParseResult.functionSetOnDeactivate, Functions::FunctionSetType::kOnDeactivate);
			parseFunctionSet(a_outParseResult.functionSetOnTrigger, Functions::FunctionSetType::kOnTrigger);

			a_outParseResult.path = a_jsonPath.parent_path().string();
			a_outParseResult.bSuccess = true;

			return true;
		}

		logger::error("Failed to open file: {}", a_jsonPath.string());
		return false;
	}

	bool SerializeJson(std::filesystem::path a_jsonPath, const rapidjson::Document& a_doc)
	{
		errno_t err = 0;
		const std::unique_ptr<FILE, decltype(&fclose)> fp{
			[&a_jsonPath, &err] {
				FILE* fp = nullptr;
				err = _wfopen_s(&fp, a_jsonPath.c_str(), L"w");
				return fp;
			}(),
			&fclose
		};

		if (err != 0) {
			logger::error("Failed to open file: {}", a_jsonPath.string());
			return false;
		}

		char writeBuffer[256]{};
		rapidjson::FileWriteStream os{ fp.get(), writeBuffer, sizeof(writeBuffer) };

		rapidjson::PrettyWriter writer(os);
		//writer.SetMaxDecimalPlaces(3);

		a_doc.Accept(writer);

		return true;
	}

	std::string SerializeJsonToString(const rapidjson::Document& a_doc)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter writer(buffer);
		//writer.SetMaxDecimalPlaces(3);

		a_doc.Accept(writer);

		return buffer.GetString();
	}

	std::string StripProjectPath(std::string_view a_path)
	{
		// strips the beginning of the path (Actors\Character\)
		constexpr auto rootPathEnd = "Animations\\";
		const auto rootPathEndPos = a_path.find(rootPathEnd);

		return a_path.substr(rootPathEndPos).data();
	}

	std::string StripReplacerPath(std::string_view a_path)
	{
		// strips the OAR/DAR substring ([Open/Dynamic]AnimationReplacer\subdirectory\subdirectory")
		constexpr auto separator = "\\";

		std::size_t substringStartPos = Utils::FindStringIgnoreCase(a_path, "OpenAnimationReplacer"sv);
		if (substringStartPos == std::string::npos) {
			substringStartPos = Utils::FindStringIgnoreCase(a_path, "DynamicAnimationReplacer"sv);
			if (substringStartPos == std::string::npos) {
				return a_path.data();
			}
		}

		std::size_t substringEndPos = substringStartPos + a_path.substr(substringStartPos).find(separator) + 1;
		substringEndPos = substringEndPos + a_path.substr(substringEndPos).find(separator) + 1;
		substringEndPos = substringEndPos + a_path.substr(substringEndPos).find(separator) + 1;

		std::string ret(a_path.substr(0, substringStartPos));
		ret.append(a_path.substr(substringEndPos));

		return ret;
	}

	std::string ConvertVariantsPath(std::string_view a_path)
	{
		// removes the variants substring "_variants_" and appends ".hkx" to the end
		constexpr std::string_view substring = "_variants_"sv;

		const std::size_t substringStartPos = a_path.find(substring);
		if (substringStartPos == std::string::npos) {
			return a_path.data();
		}

		const std::size_t substringEndPos = substringStartPos + substring.length();

		std::string ret(a_path.substr(0, substringStartPos));
		ret.append(a_path.substr(substringEndPos));
		ret.append(".hkx");

		return ret;
	}

	uint16_t GetOriginalAnimationBindingIndex(RE::hkbCharacterStringData* a_stringData, std::string_view a_animationName)
	{
		if (a_stringData) {
			auto& animationBundleNames = a_stringData->animationNames;
			if (!animationBundleNames.empty()) {
				for (uint16_t id = 0; id < animationBundleNames.size(); ++id) {
					if (Utils::CompareStringsIgnoreCase(animationBundleNames[id].data(), a_animationName)) {
						return id;
					}
				}
			}
		}

		return static_cast<uint16_t>(-1);
	}

	template <typename T>
	static std::future<T> MakeFuture(T& a_t)
	{
		std::promise<T> p;
		p.set_value(std::forward<T>(a_t));
		return p.get_future();
	}

	static DirectoryCache g_directoryCache;

	static bool IsDirectoryCacheReady();
	static void WaitForDirectoryCache();

	static void ParseCachedOARDirectory(const CachedOARDirectory& a_cachedOAR, ParseResults& a_outParseResults, WorkerPool& a_workerPool)
	{
		for (const auto& cachedMod : a_cachedOAR.modDirectories) {
			if (!IsPathValid(cachedMod.path)) {
				continue;
			}

			if (!cachedMod.subModDirectories.empty()) {
				const CachedModDirectory* cachedModPtr = &cachedMod;
				a_outParseResults.modParseResultFutures.emplace_back(a_workerPool.Enqueue([cachedModPtr]() {
					return ParseModDirectory(*cachedModPtr);
				}));
				continue;
			}

			std::filesystem::directory_entry modEntry(cachedMod.path);
			if (!Utils::IsDirectory(modEntry)) {
				continue;
			}

			a_outParseResults.modParseResultFutures.emplace_back(a_workerPool.Enqueue([modEntry]() {
				return ParseModDirectory(modEntry);
			}));
		}
	}

	static void ParseCachedLegacyDirectory(const CachedLegacyDirectory& a_cachedLegacy, ParseResults& a_outParseResults, WorkerPool& a_workerPool)
	{
		if (!a_cachedLegacy.detailedEntries.empty()) {
			for (const auto& detailedEntry : a_cachedLegacy.detailedEntries) {
				if (!IsPathValid(detailedEntry.path)) {
					continue;
				}

				if (detailedEntry.isCustomConditions) {
					for (const auto& cachedSubMod : detailedEntry.subMods) {
						const CachedLegacySubMod* cachedSubModPtr = &cachedSubMod;
						a_outParseResults.legacyParseResultFutures.emplace_back(a_workerPool.Enqueue([cachedSubModPtr]() {
							return ParseLegacyCustomConditionsDirectory(*cachedSubModPtr);
						}));
					}
				} else {
					for (auto subModParseResults = ParseLegacyPluginDirectory(detailedEntry); auto& subModParseResult : subModParseResults) {
						if (subModParseResult.bSuccess) {
							a_outParseResults.legacyParseResultFutures.emplace_back(MakeFuture(subModParseResult));
						}
					}
				}
			}
			return;
		}

		for (const auto& cachedEntry : a_cachedLegacy.entries) {
			if (!cachedEntry.isDirectory) {
				continue;
			}

			std::filesystem::directory_entry subEntry(cachedEntry.path);
			if (!Utils::IsDirectory(subEntry) || !IsPathValid(subEntry.path())) {
				continue;
			}

			if (Utils::CompareStringsIgnoreCase(subEntry.path().stem().string(), "_CustomConditions"sv)) {
				for (const auto& subSubEntry : std::filesystem::directory_iterator(subEntry)) {
					if (Utils::IsDirectory(subSubEntry)) {
						a_outParseResults.legacyParseResultFutures.emplace_back(a_workerPool.Enqueue([subSubEntry]() {
							return ParseLegacyCustomConditionsDirectory(subSubEntry);
						}));
					}
				}
			} else {
				for (auto subModParseResults = ParseLegacyPluginDirectory(subEntry); auto& subModParseResult : subModParseResults) {
					if (subModParseResult.bSuccess) {
						a_outParseResults.legacyParseResultFutures.emplace_back(MakeFuture(subModParseResult));
					}
				}
			}
		}
	}

	void ParseDirectory(const std::filesystem::directory_entry& a_directory, ParseResults& a_outParseResults)
	{
		if (!Utils::Exists(a_directory)) {
			return;
		}

		if (!IsDirectoryCacheReady()) {
			logger::info("Waiting for directory cache to complete...");
			auto startWaitingTime = std::chrono::high_resolution_clock::now();
			WaitForDirectoryCache();
			auto endWaitingTime = std::chrono::high_resolution_clock::now();
			a_outParseResults.waitCacheDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endWaitingTime - startWaitingTime);
		}

		const auto workerCount = std::clamp(Settings::uParsingWorkerCount, 1u, 32u);
		logger::info("Using {} parsing worker(s).", workerCount);
		{
			std::shared_lock lock(g_directoryCache.cacheLock);
			WorkerPool workerPool(workerCount);
			for (const auto& cachedOAR : g_directoryCache.oarDirectories) {
				ParseCachedOARDirectory(cachedOAR, a_outParseResults, workerPool);
			}
			for (const auto& cachedLegacy : g_directoryCache.legacyDirectories) {
				ParseCachedLegacyDirectory(cachedLegacy, a_outParseResults, workerPool);
			}
		}
		return;
	}
	ModParseResult ParseModDirectory(const std::filesystem::directory_entry& a_directory)
	{
		ModParseResult result;

		if (IsPathValid(a_directory.path())) {
			bool bDeserializeSuccess = false;

			// check whether the config json file exists first
			const auto configJsonPath = a_directory.path() / "config.json"sv;
			if (Utils::IsRegularFile(configJsonPath)) {
				result.configSource = ConfigSource::kAuthor;

				// check whether user json exists
				const auto userJsonPath = a_directory.path() / "user.json"sv;
				if (Utils::IsRegularFile(userJsonPath)) {
					result.configSource = ConfigSource::kUser;

					// read info from the author json
					if (!DeserializeMod(configJsonPath, DeserializeMode::kInfoOnly, result)) {
						return result;
					}
				}

				// parse the config json file
				if (result.configSource == ConfigSource::kUser) {
					bDeserializeSuccess = DeserializeMod(userJsonPath, DeserializeMode::kDataOnly, result);
				} else {
					bDeserializeSuccess = DeserializeMod(configJsonPath, DeserializeMode::kFull, result);
				}
			}

			if (bDeserializeSuccess) {
				// parse the subfolders
				for (const auto& entry : std::filesystem::directory_iterator(a_directory)) {
					if (Utils::IsDirectory(entry)) {
						// we're in a mod subfolder. we have the animations here and a json.
						auto subModParseResult = ParseModSubdirectory(entry);
						if (subModParseResult.bSuccess) {
							result.subModParseResults.emplace_back(std::move(subModParseResult));
						}
					}
				}
			}
		}
		return result;
	}
	ModParseResult ParseModDirectory(const CachedModDirectory& a_cachedMod)
	{
		ModParseResult result;

		if (IsPathValid(a_cachedMod.path)) {
			bool bDeserializeSuccess = false;

			const auto configJsonPath = a_cachedMod.path / "config.json"sv;
			if (Utils::IsRegularFile(configJsonPath)) {
				result.configSource = ConfigSource::kAuthor;

				const auto userJsonPath = a_cachedMod.path / "user.json"sv;
				if (Utils::IsRegularFile(userJsonPath)) {
					result.configSource = ConfigSource::kUser;

					if (!DeserializeMod(configJsonPath, DeserializeMode::kInfoOnly, result)) {
						return result;
					}
				}

				if (result.configSource == ConfigSource::kUser) {
					bDeserializeSuccess = DeserializeMod(userJsonPath, DeserializeMode::kDataOnly, result);
				} else {
					bDeserializeSuccess = DeserializeMod(configJsonPath, DeserializeMode::kFull, result);
				}
			}

			if (bDeserializeSuccess) {
				for (const auto& cachedSubMod : a_cachedMod.subModDirectories) {
					auto subModParseResult = ParseModSubdirectory(cachedSubMod, false);
					if (subModParseResult.bSuccess) {
						result.subModParseResults.emplace_back(std::move(subModParseResult));
					}
				}
			}
		}

		return result;
	}

	SubModParseResult ParseModSubdirectory(const std::filesystem::directory_entry& a_subDirectory, bool a_bIsLegacy)
	{
		SubModParseResult result;

		if (IsPathValid(a_subDirectory.path())) {
			bool bDeserializeSuccess = false;

			if (a_bIsLegacy) {
				const auto userJsonPath = a_subDirectory.path() / "user.json"sv;
				if (Utils::IsRegularFile(userJsonPath)) {
					result.configSource = ConfigSource::kUser;

					// parse json
					bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kDataOnly, result);
				} else {
					return result;
				}
			} else {
				// check whether the config json file exists first
				const auto configJsonPath = a_subDirectory.path() / "config.json"sv;
				if (Utils::IsRegularFile(configJsonPath)) {
					result.configSource = ConfigSource::kAuthor;

					// check whether user json exists
					const auto userJsonPath = a_subDirectory.path() / "user.json"sv;
					if (Utils::IsRegularFile(userJsonPath)) {
						result.configSource = ConfigSource::kUser;

						// read info from the author json
						if (!DeserializeSubMod(configJsonPath, DeserializeMode::kInfoOnly, result)) {
							return result;
						}
					}

					// parse json
					if (result.configSource == ConfigSource::kUser) {
						bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kDataOnly, result);
					} else {
						bDeserializeSuccess = DeserializeSubMod(configJsonPath, DeserializeMode::kFull, result);
					}
				} else {
					return result;
				}
			}

			if (bDeserializeSuccess) {
				if (result.overrideAnimationsFolder.empty()) {
					result.animationFiles = ParseAnimationsInDirectory(a_subDirectory, a_bIsLegacy);
				} else {
					const auto overridePath = a_subDirectory.path().parent_path() / result.overrideAnimationsFolder;
					const auto overrideDirectory = std::filesystem::directory_entry(overridePath);
					if (Utils::IsDirectory(overrideDirectory)) {
						result.animationFiles = ParseAnimationsInDirectory(overrideDirectory, a_bIsLegacy);
					} else {
						result.bSuccess = false;
					}
				}
			}
		}

		return result;
	}

	SubModParseResult ParseModSubdirectory(const CachedSubModDirectory& a_cachedSubMod, bool a_bIsLegacy)
	{
		SubModParseResult result;

		if (IsPathValid(a_cachedSubMod.path)) {
			bool bDeserializeSuccess = false;

			if (a_bIsLegacy) {
				const auto userJsonPath = a_cachedSubMod.path / "user.json"sv;
				if (Utils::IsRegularFile(userJsonPath)) {
					result.configSource = ConfigSource::kUser;
					bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kDataOnly, result);
				} else {
					return result;
				}
			} else {
				const auto configJsonPath = a_cachedSubMod.path / "config.json"sv;
				if (Utils::IsRegularFile(configJsonPath)) {
					result.configSource = ConfigSource::kAuthor;

					const auto userJsonPath = a_cachedSubMod.path / "user.json"sv;
					if (Utils::IsRegularFile(userJsonPath)) {
						result.configSource = ConfigSource::kUser;

						if (!DeserializeSubMod(configJsonPath, DeserializeMode::kInfoOnly, result)) {
							return result;
						}
					}

					if (result.configSource == ConfigSource::kUser) {
						bDeserializeSuccess = DeserializeSubMod(userJsonPath, DeserializeMode::kDataOnly, result);
					} else {
						bDeserializeSuccess = DeserializeSubMod(configJsonPath, DeserializeMode::kFull, result);
					}
				} else {
					return result;
				}
			}

			if (bDeserializeSuccess) {
				if (result.overrideAnimationsFolder.empty()) {
					result.animationFiles = ParseAnimationsFromCache(a_cachedSubMod, a_bIsLegacy);
				} else {
					const auto overridePath = a_cachedSubMod.path.parent_path() / result.overrideAnimationsFolder;
					const auto overrideDirectory = std::filesystem::directory_entry(overridePath);
					if (Utils::IsDirectory(overrideDirectory)) {
						result.animationFiles = ParseAnimationsInDirectory(overrideDirectory, a_bIsLegacy);
					} else {
						result.bSuccess = false;
					}
				}
			}
		}

		return result;
	}

	SubModParseResult ParseLegacyCustomConditionsDirectory(const std::filesystem::directory_entry& a_directory)
	{
		SubModParseResult result;

		if (IsPathValid(a_directory.path())) {
			// check whether _conditions.txt file exists first
			const std::filesystem::path txtPath = a_directory.path() / "_conditions.txt"sv;
			if (Utils::Exists(txtPath)) {
				// check whether the user json file exists, if yes, treat it as a OAR submod
				const auto jsonPath = a_directory.path() / "user.json"sv;
				if (Utils::IsRegularFile(jsonPath)) {
					result = ParseModSubdirectory(a_directory, true);
					result.name = std::to_string(result.priority);
					return result;
				}
			}

			int32_t priority = 0;
			std::string directoryName = a_directory.path().filename().string();

			if (directoryName.find_first_not_of("-0123456789"sv) == std::string::npos) {
				auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), priority) };
				if (ec == std::errc()) {
					if (Utils::Exists(txtPath)) {
						result.configSource = ConfigSource::kLegacy;
						result.name = std::to_string(priority);
						result.priority = priority;
						result.conditionSet = ParseConditionsTxt(txtPath);  // parse conditions.txt
						result.animationFiles = ParseAnimationsInDirectory(a_directory, true);
						result.bSuccess = true;
					} else {
						const auto subEntryPath = a_directory.path().u8string();
						std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
						logger::warn("directory at {} is missing the _conditions.txt file, skipping", subEntryPathSv);
					}
				} else {
					const auto subEntryPath = a_directory.path().u8string();
					std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
					logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
				}
			} else {
				const auto subEntryPath = a_directory.path().u8string();
				std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
				logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
			}

			result.path = a_directory.path().string();
		}

		return result;
	}

	static std::vector<ReplacementAnimationFile> ParseAnimationsFromLegacyCache(const CachedLegacySubMod& a_cachedSubMod)
	{
		std::vector<ReplacementAnimationFile> result;

		for (const auto& cachedAnim : a_cachedSubMod.animationFiles) {
			if (auto anim = ParseReplacementAnimationEntry(cachedAnim.path.string())) {
				result.emplace_back(*anim);
			}
		}

		return result;
	}

	SubModParseResult ParseLegacyCustomConditionsDirectory(const CachedLegacySubMod& a_cachedSubMod)
	{
		SubModParseResult result;

		if (IsPathValid(a_cachedSubMod.path)) {
			const std::filesystem::path txtPath = a_cachedSubMod.path / "_conditions.txt"sv;
			if (Utils::Exists(txtPath)) {
				const auto jsonPath = a_cachedSubMod.path / "user.json"sv;
				if (Utils::IsRegularFile(jsonPath)) {
					CachedSubModDirectory cachedOARSubMod;
					cachedOARSubMod.path = a_cachedSubMod.path;
					cachedOARSubMod.animationFiles = a_cachedSubMod.animationFiles;
					result = ParseModSubdirectory(cachedOARSubMod, true);
					result.name = std::to_string(result.priority);
					return result;
				}
			}

			int32_t priority = 0;
			std::string directoryName = a_cachedSubMod.path.filename().string();

			if (directoryName.find_first_not_of("-0123456789"sv) == std::string::npos) {
				auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), priority) };
				if (ec == std::errc()) {
					if (Utils::Exists(txtPath)) {
						result.configSource = ConfigSource::kLegacy;
						result.name = std::to_string(priority);
						result.priority = priority;
						result.conditionSet = ParseConditionsTxt(txtPath);
						result.animationFiles = ParseAnimationsFromLegacyCache(a_cachedSubMod);
						result.bSuccess = true;
					} else {
						const auto subEntryPath = a_cachedSubMod.path.u8string();
						std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
						logger::warn("directory at {} is missing the _conditions.txt file, skipping", subEntryPathSv);
					}
				} else {
					const auto subEntryPath = a_cachedSubMod.path.u8string();
					std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
					logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
				}
			} else {
				const auto subEntryPath = a_cachedSubMod.path.u8string();
				std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
				logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
			}

			result.path = a_cachedSubMod.path.string();
		}

		return result;
	}

	std::vector<SubModParseResult> ParseLegacyPluginDirectory(const CachedLegacyEntry& a_cachedEntry)
	{
		std::vector<SubModParseResult> results;

		for (const auto& cachedSubMod : a_cachedEntry.subMods) {
			if (!IsPathValid(cachedSubMod.path)) {
				continue;
			}

			auto jsonPath = cachedSubMod.path / "user.json"sv;
			if (Utils::IsRegularFile(jsonPath)) {
				CachedSubModDirectory cachedOARSubMod;
				cachedOARSubMod.path = cachedSubMod.path;
				cachedOARSubMod.animationFiles = cachedSubMod.animationFiles;
				auto result = ParseModSubdirectory(cachedOARSubMod, true);

				const std::string fileString = a_cachedEntry.path.stem().string();
				const std::string extensionString = a_cachedEntry.path.extension().string();

				const std::string modName = fileString + extensionString;
				const std::string formIDString = cachedSubMod.path.filename().string();
				result.name = modName;
				result.name += '|';
				result.name += formIDString;

				results.emplace_back(std::move(result));
				continue;
			}

			if (cachedSubMod.animationFiles.empty()) {
				continue;
			}

			RE::FormID formID;
			std::string directoryName = cachedSubMod.path.filename().string();

			auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), formID, 16) };
			if (ec == std::errc()) {
				std::string fileString = a_cachedEntry.path.stem().string();
				std::string extensionString = a_cachedEntry.path.extension().string();

				std::string modName = fileString + extensionString;
				if (auto form = Utils::LookupForm(formID, modName)) {
					auto conditionSet = std::make_unique<Conditions::ConditionSet>();
					std::string argument = modName;
					argument += '|';
					argument += directoryName;
					auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition("IsActorBase");
					static_cast<Conditions::IsActorBaseCondition*>(condition.get())->formComponent->SetTESFormValue(form);
					conditionSet->Add(condition);

					SubModParseResult result;

					result.configSource = ConfigSource::kLegacyActorBase;
					result.name = argument;
					result.priority = 0;
					result.conditionSet = std::move(conditionSet);
					result.animationFiles = ParseAnimationsFromLegacyCache(cachedSubMod);
					result.bSuccess = true;
					result.path = cachedSubMod.path.string();

					results.emplace_back(std::move(result));
				}
			} else {
				auto subEntryPath = cachedSubMod.path.u8string();
				std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
				logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
			}
		}

		return results;
	}

	std::vector<SubModParseResult> ParseLegacyPluginDirectory(const std::filesystem::directory_entry& a_directory)
	{
		std::vector<SubModParseResult> results;

		for (const auto& subEntry : std::filesystem::directory_iterator(a_directory)) {
			if (Utils::IsDirectory(subEntry)) {
				// check whether there's any file here
				if (is_empty(subEntry)) {
					continue;
				}

				// check whether the user json file exists, if yes, treat it as a OAR submod
				auto jsonPath = subEntry.path() / "user.json"sv;
				if (Utils::IsRegularFile(jsonPath)) {
					auto result = ParseModSubdirectory(subEntry, true);

					const std::string fileString = a_directory.path().stem().string();
					const std::string extensionString = a_directory.path().extension().string();

					const std::string modName = fileString + extensionString;
					const std::string formIDString = subEntry.path().filename().string();
					result.name = modName;
					result.name += '|';
					result.name += formIDString;

					results.emplace_back(std::move(result));
					continue;
				}

				if (IsPathValid(subEntry.path())) {
					RE::FormID formID;
					std::string directoryName = subEntry.path().filename().string();

					auto [ptr, ec]{ std::from_chars(directoryName.data(), directoryName.data() + directoryName.size(), formID, 16) };
					if (ec == std::errc()) {
						std::string fileString = a_directory.path().stem().string();
						std::string extensionString = a_directory.path().extension().string();

						std::string modName = fileString + extensionString;
						if (auto form = Utils::LookupForm(formID, modName)) {
							auto conditionSet = std::make_unique<Conditions::ConditionSet>();
							std::string argument = modName;
							argument += '|';
							argument += directoryName;
							auto condition = OpenAnimationReplacer::GetSingleton().CreateCondition("IsActorBase");
							static_cast<Conditions::IsActorBaseCondition*>(condition.get())->formComponent->SetTESFormValue(form);
							conditionSet->Add(condition);

							SubModParseResult result;

							result.configSource = ConfigSource::kLegacyActorBase;
							result.name = argument;
							result.priority = 0;
							result.conditionSet = std::move(conditionSet);
							result.animationFiles = ParseAnimationsInDirectory(subEntry, true);
							result.bSuccess = true;
							result.path = subEntry.path().string();

							results.emplace_back(std::move(result));
						}
					} else {
						auto subEntryPath = subEntry.path().u8string();
						std::string_view subEntryPathSv(reinterpret_cast<const char*>(subEntryPath.data()), subEntryPath.size());
						logger::warn("invalid directory name at {}, skipping", subEntryPathSv);
					}
				}
			}
		}

		return results;
	}

	struct AnimationDirectoryEntries
	{
		struct Entry
		{
			std::filesystem::path path;
			std::string pathString;
			std::string filename;
			std::string extension;
		};

		std::vector<Entry> directories;
		std::vector<Entry> hkxFiles;
	};

	std::optional<std::string> TryConvertPathToString(const std::filesystem::path& a_path)
	{
		try {
			return a_path.string();
		} catch (const std::system_error&) {
			auto pathU8String = a_path.u8string();
			std::string_view pathSv(reinterpret_cast<const char*>(pathU8String.data()), pathU8String.size());
			logger::warn("invalid path at {}, skipping", pathSv);
			return std::nullopt;
		}
	}

	bool IsHiddenDirectoryName(std::string_view a_filename)
	{
		static constexpr auto mohiddenFolderName = ".mohidden"sv;
		return Utils::ContainsStringIgnoreCase(a_filename, mohiddenFolderName);
	}

	void ScanAnimationDirectoryFileSystem(const std::filesystem::path& a_directory, bool a_bReadHkxFilenames, AnimationDirectoryEntries& a_outEntries, bool a_bCountHiddenDirectorySkips = false)
	{
		for (const auto& directoryEntry : std::filesystem::directory_iterator(a_directory)) {
			AddTimingCounter(TimingCounter::kDirectoryEntriesSeen);

			if (directoryEntry.is_directory()) {
				auto filename = TryConvertPathToString(directoryEntry.path().filename());
				if (!filename) {
					AddTimingCounter(TimingCounter::kDirectoryInvalidPaths);
					continue;
				}

				if (IsHiddenDirectoryName(*filename)) {
					AddTimingCounter(TimingCounter::kDirectoryInvalidPaths);
					if (a_bCountHiddenDirectorySkips) {
						AddTimingCounter(TimingCounter::kDirectoryHiddenRecursionSkips);
					}
					continue;
				}

				AnimationDirectoryEntries::Entry entry;
				entry.path = directoryEntry.path();
				entry.pathString = TryConvertPathToString(entry.path).value_or(std::string());
				entry.filename = std::move(*filename);

				AddTimingCounter(TimingCounter::kDirectoryDirectoriesSeen);
				a_outEntries.directories.emplace_back(std::move(entry));
			} else if (directoryEntry.is_regular_file()) {
				AddTimingCounter(TimingCounter::kDirectoryFilesSeen);
				if (!Utils::CompareStringsIgnoreCase(directoryEntry.path().extension().string(), ".hkx"sv)) {
					continue;
				}

				AnimationDirectoryEntries::Entry entry;
				entry.path = directoryEntry.path();
				entry.pathString = TryConvertPathToString(entry.path).value_or(std::string());
				entry.extension = ".hkx";

				if (a_bReadHkxFilenames) {
					auto filename = TryConvertPathToString(directoryEntry.path().filename());
					if (!filename) {
						AddTimingCounter(TimingCounter::kDirectoryInvalidPaths);
						continue;
					}
					entry.filename = std::move(*filename);
				}

				AddTimingCounter(TimingCounter::kDirectoryHkxFilesFound);
				a_outEntries.hkxFiles.emplace_back(std::move(entry));
			}
		}
	}

	AnimationDirectoryEntries ScanAnimationDirectory(const std::filesystem::path& a_directory, bool a_bReadHkxFilenames = true)
	{
		AnimationDirectoryEntries result;
		ScopedTimer timer(TimingBucket::kAnimationDirectoryScan);

		ScanAnimationDirectoryFileSystem(a_directory, a_bReadHkxFilenames, result);

		return result;
	}

	std::optional<ReplacementAnimationFile> ParseReplacementAnimationEntry(std::string_view a_fullPath)
	{
		return ReplacementAnimationFile(a_fullPath);
	}

	std::optional<ReplacementAnimationFile> ParseReplacementAnimationVariants(std::string_view a_fullVariantsPath)
	{
		std::vector<ReplacementAnimationFile::Variant> variants;

		for (const auto& fileEntry : ScanAnimationDirectory(std::filesystem::path(a_fullVariantsPath), false).hkxFiles) {
			variants.emplace_back(fileEntry.pathString);
		}

		if (variants.empty()) {
			return std::nullopt;
		}

		return ReplacementAnimationFile(a_fullVariantsPath, variants);
	}

	std::optional<ReplacementAnimationFile> ParseReplacementAnimationVariants(const CachedAnimationFile& a_cachedVariants)
	{
		if (!a_cachedVariants.isVariantsDirectory || a_cachedVariants.variantPaths.empty()) {
			return std::nullopt;
		}

		std::vector<ReplacementAnimationFile::Variant> variants;
		variants.reserve(a_cachedVariants.variantPaths.size());

		for (const auto& variantPath : a_cachedVariants.variantPaths) {
			variants.emplace_back(variantPath.string());
		}

		return ReplacementAnimationFile(a_cachedVariants.path.string(), variants);
	}

	std::vector<ReplacementAnimationFile> ParseNonLegacyAnimationsInDirectory(const std::filesystem::directory_entry& a_directory)
	{
		std::vector<ReplacementAnimationFile> result;
		auto entries = ScanAnimationDirectory(a_directory.path());

		std::vector<std::string> filenamesToSkip{};

		for (const auto& fileEntry : entries.directories) {
			if (fileEntry.filename.starts_with("_variants_"sv)) {
				// parse variants directory
				filenamesToSkip.emplace_back(ConvertVariantsPath(fileEntry.filename));

				if (auto anim = ParseReplacementAnimationVariants(fileEntry.pathString)) {
					result.emplace_back(*anim);
				}
			} else {
				// parse child directory normally
				// append result
				auto res = ParseNonLegacyAnimationsInDirectory(std::filesystem::directory_entry(fileEntry.path));
				result.reserve(result.size() + res.size());
				result.insert(result.end(), std::make_move_iterator(res.begin()), std::make_move_iterator(res.end()));
			}
		}

		for (const auto& fileEntry : entries.hkxFiles) {
			// check if we should skip this file because the variants directory exists
			const bool bSkip = std::ranges::any_of(filenamesToSkip, [&](const auto& a_filename) {
				return fileEntry.filename == a_filename;
			});

			if (bSkip) {
				logger::warn("skipping {}{} at {} because a variants directory exists for this animation", fileEntry.filename, fileEntry.extension, fileEntry.pathString);
				continue;
			}

			if (auto anim = ParseReplacementAnimationEntry(fileEntry.pathString)) {
				result.emplace_back(*anim);
			}
		}

		return result;
	}

	std::vector<ReplacementAnimationFile> ParseLegacyAnimationsInDirectory(const std::filesystem::directory_entry& a_directory)
	{
		std::vector<ReplacementAnimationFile> result;
		std::vector<AnimationDirectoryEntries::Entry> hkxFiles;

		{
			ScopedTimer timer(TimingBucket::kAnimationDirectoryScan);
			std::vector<std::filesystem::path> directoriesToScan{ a_directory.path() };

			while (!directoriesToScan.empty()) {
				auto directory = std::move(directoriesToScan.back());
				directoriesToScan.pop_back();

				AnimationDirectoryEntries entries;
				ScanAnimationDirectoryFileSystem(directory, false, entries, true);

				for (auto& subDirectory : entries.directories) {
					directoriesToScan.emplace_back(std::move(subDirectory.path));
				}

				hkxFiles.reserve(hkxFiles.size() + entries.hkxFiles.size());
				hkxFiles.insert(hkxFiles.end(), std::make_move_iterator(entries.hkxFiles.begin()), std::make_move_iterator(entries.hkxFiles.end()));
			}
		}

		for (const auto& fileEntry : hkxFiles) {
			if (auto anim = ParseReplacementAnimationEntry(fileEntry.pathString)) {
				result.emplace_back(*anim);
			}
		}
		return result;
	}

	std::vector<ReplacementAnimationFile> ParseAnimationsFromCache(const CachedSubModDirectory& a_cachedSubMod, bool a_bIsLegacy /* = false*/)
	{
		std::vector<ReplacementAnimationFile> result;

		for (const auto& cachedAnim : a_cachedSubMod.animationFiles) {
			if (cachedAnim.isVariantsDirectory) {
				if (auto anim = ParseReplacementAnimationVariants(cachedAnim)) {
					result.emplace_back(*anim);
				}
			} else if (auto anim = ParseReplacementAnimationEntry(cachedAnim.path.string())) {
				result.emplace_back(*anim);
			}
		}

		if (!a_bIsLegacy) {
			for (const auto& subdirectory : a_cachedSubMod.subdirectories) {
				auto res = ParseAnimationsFromCache(subdirectory, a_bIsLegacy);
				result.reserve(result.size() + res.size());
				result.insert(result.end(), std::make_move_iterator(res.begin()), std::make_move_iterator(res.end()));
			}
		}

		return result;
	}

	std::vector<ReplacementAnimationFile> ParseAnimationsInDirectory(const std::filesystem::directory_entry& a_directory, bool a_bIsLegacy /* = false*/)
	{
		if (a_bIsLegacy) {
			return ParseLegacyAnimationsInDirectory(a_directory);
		}

		return ParseNonLegacyAnimationsInDirectory(a_directory);
	}

	bool IsPathValid(const std::filesystem::path& a_path)
	{
		// skip invalid paths
		std::string pathString;
		try {
			pathString = a_path.string();
		} catch (const std::system_error&) {
			auto pathU8String = a_path.u8string();
			std::string_view pathSv(reinterpret_cast<const char*>(pathU8String.data()), pathU8String.size());
			logger::warn("invalid path at {}, skipping", pathSv);
			return false;
		}

		// skip hidden folders
		static constexpr auto mohiddenFolderName = ".mohidden"sv;
		if (Utils::ContainsStringIgnoreCase(pathString, mohiddenFolderName)) {
			return false;
		}

		return true;
	}

	static void PrecacheAnimationHash(const std::filesystem::path& a_path)
	{
		if (!Settings::bFilterOutDuplicateAnimations) {
			return;
		}

		auto filename = TryConvertPathToString(a_path);
		if (!filename) {
			return;
		}

		AnimationFileHashCache::CalculateHash(*filename);
		g_precachedHashCount.fetch_add(1, std::memory_order_relaxed);
	}

	static bool IsDirectoryCacheReady()
	{
		return g_directoryCache.bIsComplete.load(std::memory_order_acquire);
	}

	static void WaitForDirectoryCache()
	{
#ifdef _WIN32
		bool bBoostedCacheThread = false;
#endif
		while (!g_directoryCache.bIsComplete.load(std::memory_order_acquire)) {
#ifdef _WIN32
			if (!bBoostedCacheThread) {
				const auto cacheThreadId = g_directoryCacheThreadId.load(std::memory_order_acquire);
				if (cacheThreadId != 0) {
					if (const auto cacheThread = OpenThread(THREAD_SET_INFORMATION, false, cacheThreadId)) {
						SetThreadPriority(cacheThread, THREAD_PRIORITY_HIGHEST);
						SetIOPriority(cacheThread, HighThreadIoPriority);
						CloseHandle(cacheThread);
						bBoostedCacheThread = true;
					}
				}
			}
#endif
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	static void CacheSubModDirectoryContents(const std::filesystem::path& a_path, CachedSubModDirectory& a_outCached, bool a_bIsLegacy);

	static void CacheAnimationFilesInDirectory(const std::filesystem::path& a_directory, CachedSubModDirectory& a_outCached, bool a_bIsLegacy)
	{
		if (!a_bIsLegacy) {
			std::vector<std::string> variantDirectoryNames;
			std::vector<CachedAnimationFile> regularAnimationFiles;

			for (const auto& entry : std::filesystem::directory_iterator(a_directory)) {
				if (!IsPathValid(entry.path())) {
					continue;
				}

				if (!Utils::IsDirectory(entry)) {
					if (Utils::IsRegularFile(entry) && Utils::CompareStringsIgnoreCase(entry.path().extension().string(), ".hkx"sv)) {
						CachedAnimationFile cachedAnim;
						cachedAnim.path = entry.path();
						regularAnimationFiles.push_back(std::move(cachedAnim));
					}
					continue;
				}

				std::string directoryName = entry.path().filename().string();
				if (directoryName.starts_with("_variants_"sv)) {
					variantDirectoryNames.emplace_back(ConvertVariantsPath(directoryName));

					CachedAnimationFile cachedAnim;
					cachedAnim.path = entry.path();
					cachedAnim.isVariantsDirectory = true;

					for (const auto& variantEntry : std::filesystem::directory_iterator(entry)) {
						if (IsPathValid(variantEntry.path()) &&
							Utils::IsRegularFile(variantEntry) &&
							Utils::CompareStringsIgnoreCase(variantEntry.path().extension().string(), ".hkx"sv)) {
							cachedAnim.variantPaths.push_back(variantEntry.path());
							PrecacheAnimationHash(variantEntry.path());
						}
					}

					if (!cachedAnim.variantPaths.empty()) {
						a_outCached.animationFiles.push_back(std::move(cachedAnim));
					}
					continue;
				}

				CachedSubModDirectory subdirectory;
				CacheSubModDirectoryContents(entry.path(), subdirectory, a_bIsLegacy);
				if (!subdirectory.animationFiles.empty() || !subdirectory.subdirectories.empty()) {
					a_outCached.subdirectories.push_back(std::move(subdirectory));
				}
			}

			for (auto& cachedAnim : regularAnimationFiles) {
				std::string filename = cachedAnim.path.filename().string();
				const bool bSkip = std::ranges::any_of(variantDirectoryNames, [&](const auto& name) {
					return filename == name;
				});

				if (!bSkip) {
					PrecacheAnimationHash(cachedAnim.path);
					a_outCached.animationFiles.push_back(std::move(cachedAnim));
				}
			}
		} else {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(a_directory)) {
				if (IsPathValid(entry.path()) &&
					Utils::IsRegularFile(entry) &&
					Utils::CompareStringsIgnoreCase(entry.path().extension().string(), ".hkx"sv)) {
					CachedAnimationFile cachedAnim;
					cachedAnim.path = entry.path();
					PrecacheAnimationHash(cachedAnim.path);
					a_outCached.animationFiles.push_back(std::move(cachedAnim));
				}
			}
		}
	}

	static void CacheSubModDirectoryContents(const std::filesystem::path& a_path, CachedSubModDirectory& a_outCached, bool a_bIsLegacy)
	{
		a_outCached.path = a_path;
		CacheAnimationFilesInDirectory(a_path, a_outCached, a_bIsLegacy);
	}

	static void CacheOARDirectoryContents(const std::filesystem::directory_entry& a_oarEntry, CachedOARDirectory& a_outCached)
	{
		a_outCached.path = a_oarEntry.path();

		for (const auto& modEntry : std::filesystem::directory_iterator(a_oarEntry)) {
			if (!Utils::IsDirectory(modEntry) || !IsPathValid(modEntry.path())) {
				continue;
			}

			CachedModDirectory cachedMod;
			cachedMod.path = modEntry.path();

			for (const auto& subEntry : std::filesystem::directory_iterator(modEntry)) {
				if (!IsPathValid(subEntry.path())) {
					continue;
				}

				if (Utils::IsDirectory(subEntry)) {
					CachedSubModDirectory cachedSubMod;
					CacheSubModDirectoryContents(subEntry.path(), cachedSubMod, false);
					cachedMod.subModDirectories.push_back(std::move(cachedSubMod));
				}
			}

			a_outCached.modDirectories.push_back(std::move(cachedMod));
		}
	}

	static void CacheLegacyAnimationFiles(const std::filesystem::path& a_directory, CachedLegacySubMod& a_outCached)
	{
		a_outCached.path = a_directory;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(a_directory)) {
			if (IsPathValid(entry.path()) &&
				Utils::IsRegularFile(entry) &&
				Utils::CompareStringsIgnoreCase(entry.path().extension().string(), ".hkx"sv)) {
				CachedAnimationFile cachedAnim;
				cachedAnim.path = entry.path();
				PrecacheAnimationHash(cachedAnim.path);
				a_outCached.animationFiles.push_back(std::move(cachedAnim));
			}
		}
	}

	static void CacheLegacyDirectoryContents(const std::filesystem::directory_entry& a_legacyEntry, CachedLegacyDirectory& a_outCached)
	{
		a_outCached.path = a_legacyEntry.path();

		for (const auto& subEntry : std::filesystem::directory_iterator(a_legacyEntry)) {
			if (!Utils::IsDirectory(subEntry) || !IsPathValid(subEntry.path())) {
				continue;
			}

			a_outCached.entries.push_back({ subEntry.path(), true });

			CachedLegacyEntry detailedEntry;
			detailedEntry.path = subEntry.path();

			if (Utils::CompareStringsIgnoreCase(subEntry.path().stem().string(), "_CustomConditions"sv)) {
				detailedEntry.isCustomConditions = true;

				for (const auto& customConditionEntry : std::filesystem::directory_iterator(subEntry)) {
					if (Utils::IsDirectory(customConditionEntry) && IsPathValid(customConditionEntry.path())) {
						CachedLegacySubMod cachedSubMod;
						CacheLegacyAnimationFiles(customConditionEntry.path(), cachedSubMod);
						detailedEntry.subMods.push_back(std::move(cachedSubMod));
					}
				}
			} else {
				for (const auto& formIdEntry : std::filesystem::directory_iterator(subEntry)) {
					if (Utils::IsDirectory(formIdEntry) && IsPathValid(formIdEntry.path())) {
						CachedLegacySubMod cachedSubMod;
						CacheLegacyAnimationFiles(formIdEntry.path(), cachedSubMod);
						detailedEntry.subMods.push_back(std::move(cachedSubMod));
					}
				}
			}

			a_outCached.detailedEntries.push_back(std::move(detailedEntry));
		}
	}

	static void CacheDirectoriesInternal()
	{
		// We do NOT want to overly optimize/parallelize this thread - we want the lowest IO priority possible
		// Otherwise all perf gain is lost due to other loading stuff choking (i.e .esp loading, etc)
#ifdef _WIN32
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
		SetLowIOPriority();
#endif

		logger::info("Starting directory cache...");
		const auto startTime = std::chrono::high_resolution_clock::now();

		static constexpr auto oarFolderName = "openanimationreplacer"sv;
		static constexpr auto legacyFolderName = "dynamicanimationreplacer"sv;
		static constexpr auto meshesPath = "data\\meshes\\"sv;

		const std::filesystem::directory_entry meshesDir(meshesPath);
		if (!Utils::Exists(meshesDir)) {
			logger::warn("Meshes directory not found, skipping directory cache");
			g_directoryCache.bIsComplete.store(true, std::memory_order_release);
			return;
		}

		std::vector<CachedOARDirectory> oarDirs;
		std::vector<CachedLegacyDirectory> legacyDirs;

		try {
			for (std::filesystem::recursive_directory_iterator i(meshesDir), end; i != end; ++i) {
				const auto& entry = *i;
				if (!Utils::IsDirectory(entry)) {
					continue;
				}

				if (!IsPathValid(entry.path())) {
					i.disable_recursion_pending();
					continue;
				}

				std::string stemString = entry.path().stem().string();
				if (Utils::CompareStringsIgnoreCase(stemString, oarFolderName)) {
					CachedOARDirectory cachedOAR;
					CacheOARDirectoryContents(entry, cachedOAR);
					oarDirs.push_back(std::move(cachedOAR));
					i.disable_recursion_pending();
				} else if (Utils::CompareStringsIgnoreCase(stemString, legacyFolderName)) {
					CachedLegacyDirectory cachedLegacy;
					CacheLegacyDirectoryContents(entry, cachedLegacy);
					legacyDirs.push_back(std::move(cachedLegacy));
					i.disable_recursion_pending();
				}
			}
		} catch (const std::filesystem::filesystem_error& e) {
			logger::warn("Error while caching directories: {}", e.what());
		}

		{
			std::unique_lock lock(g_directoryCache.cacheLock);
			g_directoryCache.oarDirectories = std::move(oarDirs);
			g_directoryCache.legacyDirectories = std::move(legacyDirs);
		}

		const auto endTime = std::chrono::high_resolution_clock::now();
		const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		const auto hashCount = g_precachedHashCount.load(std::memory_order_relaxed);

		if (Settings::bFilterOutDuplicateAnimations) {
			logger::info("Directory cache complete: {} OAR directories, {} legacy directories, {} animation hashes ({}ms)",
				g_directoryCache.oarDirectories.size(),
				g_directoryCache.legacyDirectories.size(),
				hashCount,
				duration);
		} else {
			logger::info("Directory cache complete: {} OAR directories, {} legacy directories ({}ms)",
				g_directoryCache.oarDirectories.size(),
				g_directoryCache.legacyDirectories.size(),
				duration);
		}

		g_directoryCache.bIsComplete.store(true, std::memory_order_release);
	}

	void StartDirectoryCaching()
	{
		std::thread([]() {
#ifdef _WIN32
			g_directoryCacheThreadId.store(GetCurrentThreadId(), std::memory_order_release);
#endif
			CacheDirectoriesInternal();
#ifdef _WIN32
			g_directoryCacheThreadId.store(0, std::memory_order_release);
#endif
		}).detach();
	}
}
