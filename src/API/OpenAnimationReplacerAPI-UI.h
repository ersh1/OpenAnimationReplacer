#pragma once

/*
* For modders: Copy this file into your own project if you wish to use this API
*/
namespace OAR_API::UI
{
	// Available UI interface versions
	enum class InterfaceVersion : uint8_t
	{
		V1,  // unsupported
		V2,

		Latest = V2
	};

	// Open Animation Replacer's UI interface
	class IUIInterface2
	{
	public:
		/// <summary>
		/// Get Open Animation Replacer's ImGui context.
		/// </summary>
		/// <returns>A pointer to the ImGuiContext* object</returns>
		[[nodiscard]] virtual void* GetImGuiContext() noexcept = 0;

		/// <summary>
		/// Get Open Animation Replacer's ImGui allocator functions.
		/// </summary>
		/// <returns>Returns by reference - ImGuiMemAllocFunc*, ImGuiMemFreeFunc*, void**</returns>
		virtual void GetImGuiAllocatorFunctions(void* a_ptrAllocFunc, void* a_ptrFreeFunc, void** a_ptrUserData) noexcept = 0;

		/// <summary>
		///	Moves the ImGui cursor to the second column in Open Animation Replacer's UI layout.
		///	</summary>
		///	<param name="a_percent">The percentage of the window taken up by the first column</param>
		virtual void SecondColumn(float a_percent) noexcept = 0;

		/// <summary>
		///	Returns the width of the first column in Open Animation Replacer's UI layout.
		///	</summary>
		///	<param name="a_percent">The percentage of the window taken up by the first column</param>
		[[nodiscard]] virtual float GetFirstColumnWidth(float a_percent) noexcept = 0;
	};

	using IUIInterface = IUIInterface2;

	using _RequestPluginAPI_UI = IUIInterface* (*)(InterfaceVersion a_interfaceVersion, const char* a_pluginName, REL::Version a_pluginVersion);

	/// <summary>
	/// Request the Open Animation Replacer UI API interface.
	/// </summary>
	/// <param name="a_interfaceVersion">The interface version to request</param>
	/// <returns>The pointer to the API singleton, or nullptr if request failed</returns>
	IUIInterface* GetAPI(InterfaceVersion a_interfaceVersion = InterfaceVersion::Latest);

	static inline bool bImGuiContextInitialized = false;

	/// <summary>
	/// Call this inside any of the functions that are supposed to use OpenAnimationReplacer's ImGui context, and call InitializeImGuiContext if this returns false.
	/// </summary>
	/// <returns>Whether the context is correctly set</returns>
	inline bool IsImGuiContextInitialized() { return bImGuiContextInitialized; }

	/// <summary>
	/// Call this to set up OpenAnimationReplacer's ImGui context as the current one.
	/// </summary>
	/// <returns>Whether the context is correctly set</returns>
	bool InitializeImGuiContext();
}

extern OAR_API::UI::IUIInterface* g_oarUIInterface;
