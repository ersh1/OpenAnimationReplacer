set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(SOURCE_DIR "${ROOT_DIR}/src")
set(SOURCE_FILES
	"${SOURCE_DIR}/ActiveClip.cpp"
	"${SOURCE_DIR}/ActiveClip.h"
	"${SOURCE_DIR}/AnimationFileHashCache.cpp"
	"${SOURCE_DIR}/AnimationFileHashCache.h"
	"${SOURCE_DIR}/AnimationLog.cpp"
	"${SOURCE_DIR}/AnimationLog.h"
	"${SOURCE_DIR}/BaseConditions.cpp"
	"${SOURCE_DIR}/BaseConditions.h"
	"${SOURCE_DIR}/Conditions.cpp"
	"${SOURCE_DIR}/Conditions.h"
	"${SOURCE_DIR}/DetectedProblems.cpp"
	"${SOURCE_DIR}/DetectedProblems.h"
	"${SOURCE_DIR}/Hooks.cpp"
	"${SOURCE_DIR}/Hooks.h"
	"${SOURCE_DIR}/Jobs.h"
	"${SOURCE_DIR}/main.cpp"
	"${SOURCE_DIR}/ModAPI.cpp"
	"${SOURCE_DIR}/ModAPI.h"
	"${SOURCE_DIR}/Offsets.h"
	"${SOURCE_DIR}/OpenAnimationReplacer.cpp"
	"${SOURCE_DIR}/OpenAnimationReplacer.h"
	"${SOURCE_DIR}/Parsing.cpp"
	"${SOURCE_DIR}/Parsing.h"
	"${SOURCE_DIR}/PCH.h"
	"${SOURCE_DIR}/ReplacementAnimation.cpp"
	"${SOURCE_DIR}/ReplacementAnimation.h"
	"${SOURCE_DIR}/ReplacerMods.cpp"
	"${SOURCE_DIR}/ReplacerMods.h"
	"${SOURCE_DIR}/Settings.cpp"
	"${SOURCE_DIR}/Settings.h"
	"${SOURCE_DIR}/Utils.cpp"
	"${SOURCE_DIR}/Utils.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-ConditionTypes.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-ConditionTypes.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Conditions.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Conditions.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-UI.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-UI.h"
	"${SOURCE_DIR}/Havok/Havok.h"
	"${SOURCE_DIR}/UI/UIAnimationLog.cpp"
	"${SOURCE_DIR}/UI/UIAnimationLog.h"
	"${SOURCE_DIR}/UI/UIAnimationQueue.cpp"
	"${SOURCE_DIR}/UI/UIAnimationQueue.h"
	"${SOURCE_DIR}/UI/UICommon.cpp"
	"${SOURCE_DIR}/UI/UICommon.h"
	"${SOURCE_DIR}/UI/UIErrorBanner.cpp"
	"${SOURCE_DIR}/UI/UIErrorBanner.h"
	"${SOURCE_DIR}/UI/UIMain.cpp"
	"${SOURCE_DIR}/UI/UIMain.h"
	"${SOURCE_DIR}/UI/UIManager.cpp"
	"${SOURCE_DIR}/UI/UIManager.h"
	"${SOURCE_DIR}/UI/UIWelcomeBanner.cpp"
	"${SOURCE_DIR}/UI/UIWelcomeBanner.h"
	"${SOURCE_DIR}/UI/UIWindow.cpp"
	"${SOURCE_DIR}/UI/UIWindow.h"
)

source_group(TREE "${ROOT_DIR}" FILES ${SOURCE_FILES})

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/Plugin.h")
configure_file(
	"${ROOT_DIR}/cmake/Plugin.h.in"
	"${VERSION_HEADER}"
	@ONLY
)

source_group("src" FILES "${VERSION_HEADER}")

configure_file(
	"${ROOT_DIR}/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

find_path(MERGEMAPPER_INCLUDE_DIRS "MergeMapperPluginAPI.h")

add_library(
	"${PROJECT_NAME}"
	SHARED
	${SOURCE_FILES}
	"${VERSION_HEADER}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	"${ROOT_DIR}/.clang-format"
	"${ROOT_DIR}/.editorconfig"
	"${MERGEMAPPER_INCLUDE_DIRS}/MergeMapperPluginAPI.cpp"
)

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_20
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			"/sdl"	# Enable Additional Security Checks
			"/utf-8"	# Set Source and Executable character sets to UTF-8
			"/Zi"	# Debug Information Format

			"/await"

			"/permissive-"	# Standards conformance
			"/Zc:preprocessor"	# Enable preprocessor conformance mode

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}/src"
		"${SOURCE_DIR}"
		"${MERGEMAPPER_INCLUDE_DIRS}"
)

set(SKSE_SUPPORT_XBYAK ON)

add_subdirectory("$ENV{CommonLibSSEPath_NG}" CommonLibSSE EXCLUDE_FROM_ALL)

find_package(binary_io REQUIRED CONFIG)
find_package(CryptoPP REQUIRED CONFIG)
find_package(imgui REQUIRED CONFIG)
find_package(mmio REQUIRED CONFIG)
find_package(RapidJSON REQUIRED CONFIG)
find_package(xbyak REQUIRED CONFIG)

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		CommonLibSSE::CommonLibSSE
		binary_io::binary_io
		cryptopp::cryptopp
		imgui::imgui
		mmio::mmio
		rapidjson
		xbyak::xbyak
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
		"${SOURCE_DIR}/PCH.h"
)

install(
	FILES
		"$<TARGET_FILE:${PROJECT_NAME}>"
	DESTINATION "SKSE/Plugins"
	COMPONENT "main"
)

install(
	FILES
		"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
	DESTINATION "/"
	COMPONENT "pdbs"
)

if("${COPY_OUTPUT}")
	add_custom_command(
		TARGET "${PROJECT_NAME}"
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${CompiledPluginsPath}/SKSE/Plugins/"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${CompiledPluginsPath}/SKSE/Plugins/"
		VERBATIM
	)
endif()
