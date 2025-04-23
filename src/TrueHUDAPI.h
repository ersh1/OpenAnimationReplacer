#pragma once
#include <functional>
#include <queue>
#include <stdint.h>

/*
* For modders: Copy this file into your own project if you wish to use this API
*/
namespace TRUEHUD_API
{
	constexpr const auto TrueHUDPluginName = "TrueHUD";

	// Available True HUD interface versions
	enum class InterfaceVersion : uint8_t
	{
		V1,
		V2,
		V3,
		V4
	};

	// Error types that may be returned by the True HUD
	enum class APIResult : uint8_t
	{
		// Your API call was successful
		OK,

		// You tried to release a resource that was not allocated to you
		// Do not attempt to manipulate the requested resource if you receive this response
		NotOwner,

		// True HUD currently must keep control of this resource for proper functionality
		// Do not attempt to manipulate the requested resource if you receive this response
		MustKeep,

		// You have already been given control of this resource
		AlreadyGiven,

		// Another mod has been given control of this resource at the present time
		// Do not attempt to manipulate the requested resource if you receive this response
		AlreadyTaken,

		// The widget clip failed to load
		WidgetFailedToLoad,

		// You sent a command on a thread that could cause a data race were it to be processed
		// Do not attempt to manipulate the requested resource if you receive this response
		BadThread,
	};

	// Removal modes of the widget when requesting an info bar to be removed
	enum class WidgetRemovalMode : std::uint8_t
	{
		// The widget will be removed instantly
		Immediate,

		// A short fade to zero opacity will start instantly, after which the widget will be removed
		Normal,

		// The fade to zero opacity will start after a short delay (normally used on dead targets)
		Delayed
	};

	// Player widget bar types
	enum class PlayerWidgetBarType : std::uint8_t
	{
		HealthBar,
		MagickaBar,
		StaminaBar,
		SpecialBar
	};

	// Bar color types
	enum class BarColorType : std::uint8_t
	{
		BarColor,
		PhantomColor,
		BackgroundColor,
		PenaltyColor,
		FlashColor
	};

	using SpecialResourceCallback = std::function<float(RE::Actor* a_actor)>;
	using APIResultCallback = std::function<void(APIResult)>;

	// Widget base class
	class WidgetBase
	{
	public:
		using WidgetTask = std::function<void()>;
		using Lock = std::recursive_mutex;
		using Locker = std::lock_guard<Lock>;

		enum WidgetState : std::uint8_t
		{
			kActive = 0,
			kPendingHide = 1,
			kHidden = 2,
			kDelayedRemoval = 3,
			kPendingRemoval = 4,
			kRemoved = 5
		};

		WidgetBase() = default;
		virtual ~WidgetBase() = default;

		WidgetBase(RE::GPtr<RE::GFxMovieView> a_view) :
			_view(a_view),
			_widgetID(0)
		{}

		WidgetBase(uint32_t a_widgetID) :
			_widgetID(a_widgetID)
		{}

		WidgetBase(RE::GPtr<RE::GFxMovieView> a_view, uint32_t a_widgetID) :
			_view(a_view),
			_widgetID(a_widgetID)
		{}

		virtual void Update(float a_deltaTime) = 0;
		virtual void Initialize() = 0;
		virtual void Dispose() = 0;
		virtual void SetWidgetState(WidgetState a_newWidgetState)
		{
			_widgetState = a_newWidgetState;
		}

		void AddWidgetTask(WidgetTask a_task)
		{
			Locker locker(_lock);
			_taskQueue.push(std::move(a_task));
		}

		void ProcessDelegates()
		{
			while (!_taskQueue.empty()) {
				auto& task = _taskQueue.front();
				task();
				_taskQueue.pop();
			}
		}

		RE::GPtr<RE::GFxMovieView> _view;
		RE::GFxValue _object;
		uint32_t _widgetID;

		mutable Lock _lock;
		std::queue<WidgetTask> _taskQueue;

		WidgetState _widgetState = WidgetState::kHidden;

		float _depth = 0;
	};

	// True HUD's modder interface
	class IVTrueHUD1
	{
	public:
		/// <summary>
		/// Get the thread ID True HUD Movement is running in.
		/// You may compare this with the result of GetCurrentThreadId() to help determine
		/// if you are using the correct thread.
		/// </summary>
		/// <returns>TID</returns>
		[[nodiscard]] virtual unsigned long GetTrueHUDThreadId() const noexcept = 0;

		/// <summary>
		/// Request control of the current target (affects actor info bar mode)
		/// If granted, you may manipulate the current target and soft target in whatever ways you wish for the duration of your control.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <returns>OK, AlreadyGiven, AlreadyTaken</returns>
		[[nodiscard]] virtual APIResult RequestTargetControl(SKSE::PluginHandle a_myPluginHandle) noexcept = 0;

		/// <summary>
		/// Request control over the special resource bars.
		/// If granted, you may provide the functions that will be used to get the special bars' fill percent.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <returns>OK, AlreadyGiven, AlreadyTaken</returns>
		[[nodiscard]] virtual APIResult RequestSpecialResourceBarsControl(SKSE::PluginHandle a_myPluginHandle) noexcept = 0;

		/// <summary>
		/// Tries to set the current target to the given actor handle. Will only do so if granted target control.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <returns>OK, NotOwner</returns>
		virtual APIResult SetTarget(SKSE::PluginHandle a_myPluginHandle, RE::ActorHandle a_actorHandle) noexcept = 0;

		/// <summary>
		/// Tries to set the current soft target to the given actor handle. Will only do so if granted target control.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <returns>OK, NotOwner</returns>
		virtual APIResult SetSoftTarget(SKSE::PluginHandle a_myPluginHandle, RE::ActorHandle a_actorHandle) noexcept = 0;

		/// <summary>
		/// Tries to create an info bar widget for the given actor handle.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		virtual void AddActorInfoBar(RE::ActorHandle a_actorHandle) noexcept = 0;

		/// <summary>
		/// Tries to remove an info bar widget for the given actor handle.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_removalMode">How quickly should the widget be removed</param>
		virtual void RemoveActorInfoBar(RE::ActorHandle a_actorHandle, WidgetRemovalMode a_removalMode) noexcept = 0;

		/// <summary>
		/// Adds an actor handle to the boss queue.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		virtual void AddBoss(RE::ActorHandle a_actorHandle) noexcept = 0;

		/// <summary>
		/// Tries to remove an actor handle from the boss queue / a boss bar widget, if exists.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_removalMode">How quickly should the widget be removed</param>
		virtual void RemoveBoss(RE::ActorHandle a_actorHandle, WidgetRemovalMode a_removalMode) noexcept = 0;

		/// <summary>
		/// Tries to send a visual flash event related to the given actor value on a widget related to the given actor handle (similar to vanilla stamina bar flashing when trying to sprint while it's empty). Will only succeed if such a target exists and is supported by the widget.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_actorValue">Actor value represented on the bar you want to flash</param>
		/// <param name="a_bLong">Play longer flash animation</param>
		virtual void FlashActorValue(RE::ActorHandle a_actorHandle, RE::ActorValue a_actorValue, bool a_bLong) noexcept = 0;

		/// <summary>
		/// Tries to send a visual flash event on a special bar related to the given actor handle. Will only do so if granted control.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_bLong">Play longer flash animation</param>
		/// <returns>OK, NotOwner</returns>
		virtual APIResult FlashActorSpecialBar(SKSE::PluginHandle a_myPluginHandle, RE::ActorHandle a_actorHandle, bool a_bLong) noexcept = 0;

		/// <summary>
		/// Registers the special resource functions
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_getCurrentSpecialResource">Function that will return current special resource value</param>
		/// <param name="a_getMaxSpecialResource">Function that will return max special resource value</param>
		/// <param name="a_bSpecialMode">Whether the max value is default and 0 is the flash threshold (true), or the other way around (false)</param>
		/// <param name="a_bDisplaySpecialForPlayer">Whether the special bar should be displayed for the player as well</param>
		/// <returns>OK, NotOwner</returns>
		virtual APIResult RegisterSpecialResourceFunctions(SKSE::PluginHandle a_myPluginHandle, SpecialResourceCallback&& a_getCurrentSpecialResource, SpecialResourceCallback&& a_getMaxSpecialResource, bool a_bSpecialMode, bool a_bDisplaySpecialForPlayer = true) noexcept = 0;

		/// <summary>
		/// Loads a custom widget swf. First step in registering a custom widget.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_filePath">File path to the .swf file, relative to the Data/Interface folder</param>
		/// <param name="a_successCallback">Function that will be called back when done. Check the result before proceeding.</param>
		virtual void LoadCustomWidgets(SKSE::PluginHandle a_myPluginHandle, std::string_view a_filePath, APIResultCallback&& a_successCallback) noexcept = 0;

		/// <summary>
		/// Registers a new widget type. Second step in registering a custom widget.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_widgetType">A pluginwide unique ID of the widget type.</param>
		virtual void RegisterNewWidgetType(SKSE::PluginHandle a_myPluginHandle, uint32_t a_widgetType) noexcept = 0;

		/// <summary>
		/// Adds a custom widget. Will succeed only if prepared correctly by calling the previous two functions.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_widgetType">A pluginwide unique ID of the widget type.</param>
		/// <param name="a_widgetID">An unique ID of the widget.</param>
		/// <param name="a_symbolIdentifier">The ActionScript linkage name of the movieclip</param>
		/// <param name="a_widget">The shared pointer to the widget (based on the WidgetBase class) created in your plugin</param>
		virtual void AddWidget(SKSE::PluginHandle a_myPluginHandle, uint32_t a_widgetType, uint32_t a_widgetID, std::string_view a_symbolIdentifier, std::shared_ptr<WidgetBase> a_widget) noexcept = 0;

		/// <summary>
		/// Removes a custom widget.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <param name="a_widgetType">A pluginwide unique ID of the widget type.</param>
		/// <param name="a_widgetID">An unique ID of the widget.</param>
		/// <param name="a_removalMode">Indicates whether the widget should be removed instantly. Modes other than Immediate have to be handled inside the widget</param>
		virtual void RemoveWidget(SKSE::PluginHandle a_myPluginHandle, uint32_t a_widgetType, uint32_t a_widgetID, WidgetRemovalMode a_removalMode) noexcept = 0;

		/// <summary>
		/// Returns the plugin handle of the plugin controlling the current target resource.
		/// </summary>
		/// <returns>Handle or kSKSE::PluginHandle_Invalid if no one currently owns the resource</returns>
		virtual SKSE::PluginHandle GetTargetControlOwner() const noexcept = 0;

		/// <summary>
		/// Returns the plugin handle of the plugin controlling player widget bar colors.
		/// </summary>
		/// <returns>Handle or kSKSE::PluginHandle_Invalid if no one currently owns the resource</returns>
		virtual SKSE::PluginHandle GetPlayerWidgetBarColorsControlOwner() const noexcept = 0;

		/// <summary>
		/// Returns the plugin handle of the plugin controlling special resource bars.
		/// </summary>
		/// <returns>Handle or kSKSE::PluginHandle_Invalid if no one currently owns the resource</returns>
		virtual SKSE::PluginHandle GetSpecialResourceBarControlOwner() const noexcept = 0;

		/// <summary>
		/// Release your control of the target resource.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <returns>OK, NotOwner</returns>
		virtual APIResult ReleaseTargetControl(SKSE::PluginHandle a_myPluginHandle) noexcept = 0;

		/// <summary>
		/// Release your control of the special resource bars.
		/// </summary>
		/// <param name="a_myPluginHandle">Your assigned plugin handle</param>
		/// <returns>OK, NotOwner</returns>
		virtual APIResult ReleaseSpecialResourceBarControl(SKSE::PluginHandle a_myPluginHandle) noexcept = 0;
	};

	class IVTrueHUD2 : public IVTrueHUD1
	{
	public:
		/// <summary>
		/// Overrides the bar color for the given actor handle and color type.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_actorValue">Actor value represented on the bar you want to override</param>
		/// <param name="a_colorType">Which color you want to override</param>
		/// <param name="a_color">The color in hex</param>
		virtual void OverrideBarColor(RE::ActorHandle a_actorHandle, RE::ActorValue a_actorValue, BarColorType a_colorType, uint32_t a_color) noexcept = 0;

		/// <summary>
		/// Overrides the special bar color for the given actor handle and color type.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_colorType">Which color you want to override</param>
		/// <param name="a_color">The color in hex</param>
		virtual void OverrideSpecialBarColor(RE::ActorHandle a_actorHandle, BarColorType a_colorType, uint32_t a_color) noexcept = 0;

		/// <summary>
		/// Reverts the bar color for the given actor handle and color type.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_actorValue">Actor value represented on the bar you want to revert</param>
		/// <param name="a_colorType">Which color you want to revert</param>
		virtual void RevertBarColor(RE::ActorHandle a_actorHandle, RE::ActorValue a_actorValue, BarColorType a_colorType) noexcept = 0;

		/// <summary>
		/// Reverts the special bar color for the given actor handle and color type.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_colorType">Which color you want to revert</param>
		virtual void RevertSpecialBarColor(RE::ActorHandle a_actorHandle, BarColorType a_colorType) noexcept = 0;
	};

	class IVTrueHUD3 : public IVTrueHUD2
	{
	public:
		// Debug drawing API functions
		virtual void DrawLine(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawPoint(const RE::NiPoint3& a_position, float a_size, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF) noexcept = 0;
		virtual void DrawArrow(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, float a_size = 10.f, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawBox(const RE::NiPoint3& a_center, const RE::NiPoint3& a_extent, const RE::NiQuaternion& a_rotation, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawCircle(const RE::NiPoint3& a_center, const RE::NiPoint3& a_x, const RE::NiPoint3& a_y, float a_radius, uint32_t a_segments, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawHalfCircle(const RE::NiPoint3& a_center, const RE::NiPoint3& a_x, const RE::NiPoint3& a_y, float a_radius, uint32_t a_segments, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawSphere(const RE::NiPoint3& a_origin, float a_radius, uint32_t a_segments = 16, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawCylinder(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, float a_radius, uint32_t a_segments, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawCone(const RE::NiPoint3& a_origin, const RE::NiPoint3& a_direction, float a_length, float a_angleWidth, float a_angleHeight, uint32_t a_segments, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
		virtual void DrawCapsule(const RE::NiPoint3& a_origin, float a_halfHeight, float a_radius, const RE::NiQuaternion& a_rotation, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;

		/// <summary>
		/// Get whether an info bar already exists for this actor.
		/// </summary>
		/// <param name="a_actorHandle">Actor handle</param>
		/// <param name="a_bFloatingOnly">Only return true if the bar is a floating one</param>
		/// <returns>Whether the bar exists or not</returns>
		[[nodiscard]] virtual bool HasInfoBar(RE::ActorHandle a_actorHandle, bool a_bFloatingOnly = false) const noexcept = 0;
	};

	class IVTrueHUD4 : public IVTrueHUD3
	{
	public:
		// Debug drawing API functions
		virtual void DrawCapsule2(const RE::NiPoint3& a_vertexA, const RE::NiPoint3& a_vertexB, float a_radius, float a_duration = 0.f, uint32_t a_color = 0xFF0000FF, float a_thickness = 1.f) noexcept = 0;
	};

	typedef void* (*_RequestPluginAPI)(const InterfaceVersion interfaceVersion);

	/// <summary>
	/// Request the True HUD API interface.
	/// Recommended: Send your request during or after SKSEMessagingInterface::kMessage_PostLoad to make sure the dll has already been loaded
	/// </summary>
	/// <param name="a_interfaceVersion">The interface version to request</param>
	/// <returns>The pointer to the API singleton, or nullptr if request failed</returns>
	[[nodiscard]] inline void* RequestPluginAPI(const InterfaceVersion a_interfaceVersion = InterfaceVersion::V4)
	{
		auto pluginHandle = GetModuleHandle("TrueHUD.dll");
		_RequestPluginAPI requestAPIFunction = (_RequestPluginAPI)(void*)GetProcAddress(pluginHandle, "RequestPluginAPI");
		if (requestAPIFunction) {
			return requestAPIFunction(a_interfaceVersion);
		}
		return nullptr;
	}
}
