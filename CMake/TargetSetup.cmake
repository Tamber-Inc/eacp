include(AppleSetup)

function(set_default_warnings_level target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endfunction()

function(set_default_target_setting target)
    set_default_warnings_level(${target})
    set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    if (IOS)
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${EACP_IOS_PLIST}")
    elseif (APPLE)
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${EACP_MACOS_PLIST}")
    endif ()
endfunction()

function(eacp_register_catalog_app target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "eacp_register_catalog_app(${target}): target does not exist")
    endif ()

    get_target_property(CATALOG_EXCLUDE ${target} EACP_CATALOG_EXCLUDE)
    if (CATALOG_EXCLUDE)
        return()
    endif ()

    get_target_property(IS_BUNDLE ${target} MACOSX_BUNDLE)
    if (NOT IS_BUNDLE)
        return()
    endif ()

    get_target_property(PRODUCT_ID ${target} EACP_CATALOG_PRODUCT_ID)
    if (NOT PRODUCT_ID)
        get_target_property(PRODUCT_ID ${target} MACOSX_BUNDLE_GUI_IDENTIFIER)
    endif ()
    if (NOT PRODUCT_ID)
        return()
    endif ()

    get_target_property(DISPLAY_NAME ${target} EACP_CATALOG_NAME)
    if (NOT DISPLAY_NAME)
        get_target_property(DISPLAY_NAME ${target} MACOSX_BUNDLE_BUNDLE_NAME)
    endif ()
    if (NOT DISPLAY_NAME)
        set(DISPLAY_NAME "${target}")
    endif ()

    get_target_property(BUNDLE_NAME ${target} EACP_CATALOG_BUNDLE_NAME)
    if (NOT BUNDLE_NAME)
        set(BUNDLE_NAME "${DISPLAY_NAME}.app")
    endif ()

    get_target_property(VERSION ${target} EACP_CATALOG_VERSION)
    if (NOT VERSION)
        get_target_property(VERSION ${target} MACOSX_BUNDLE_SHORT_VERSION_STRING)
    endif ()
    if (NOT VERSION)
        if (EACP_CATALOG_DEFAULT_VERSION)
            set(VERSION "${EACP_CATALOG_DEFAULT_VERSION}")
        elseif (PROJECT_VERSION)
            set(VERSION "${PROJECT_VERSION}")
        else ()
            set(VERSION "1.0.0")
        endif ()
    endif ()

    get_target_property(KIND ${target} EACP_CATALOG_KIND)
    if (NOT KIND)
        set(KIND "App")
    endif ()

    get_target_property(DEPS ${target} EACP_CATALOG_DEPENDENCIES)
    if (NOT DEPS)
        set(DEPS "")
    endif ()

    set_target_properties(${target} PROPERTIES
            EACP_CATALOG_PRODUCT_ID "${PRODUCT_ID}"
            EACP_CATALOG_NAME "${DISPLAY_NAME}"
            EACP_CATALOG_BUNDLE_NAME "${BUNDLE_NAME}"
            EACP_CATALOG_VERSION "${VERSION}"
            EACP_CATALOG_KIND "${KIND}"
            EACP_CATALOG_DEPENDENCIES "${DEPS}")

    get_property(CATALOG_TARGETS GLOBAL PROPERTY EACP_CATALOG_APP_TARGETS)
    list(FIND CATALOG_TARGETS "${target}" EXISTING_INDEX)
    if (EXISTING_INDEX EQUAL -1)
        set_property(GLOBAL APPEND PROPERTY EACP_CATALOG_APP_TARGETS "${target}")
    endif ()
endfunction()

function(eacp_add_app target)
    set(options CATALOG_EXCLUDE)
    set(oneValueArgs PRODUCT_ID NAME VERSION KIND)
    set(multiValueArgs SOURCES LINK DEPENDENCIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_SOURCES)
        message(FATAL_ERROR "eacp_add_app(${target}): SOURCES is required")
    endif ()
    if (NOT ARG_PRODUCT_ID)
        message(FATAL_ERROR "eacp_add_app(${target}): PRODUCT_ID is required")
    endif ()
    if (NOT ARG_NAME)
        set(ARG_NAME "${target}")
    endif ()
    if (NOT ARG_VERSION)
        if (EACP_CATALOG_DEFAULT_VERSION)
            set(ARG_VERSION "${EACP_CATALOG_DEFAULT_VERSION}")
        elseif (PROJECT_VERSION)
            set(ARG_VERSION "${PROJECT_VERSION}")
        else ()
            set(ARG_VERSION "1.0.0")
        endif ()
    endif ()
    if (NOT ARG_KIND)
        set(ARG_KIND "App")
    endif ()

    add_executable(${target} ${ARG_SOURCES})
    if (ARG_LINK)
        target_link_libraries(${target} PRIVATE ${ARG_LINK})
    endif ()

    set_target_properties(${target} PROPERTIES
            EACP_CATALOG_PRODUCT_ID "${ARG_PRODUCT_ID}"
            EACP_CATALOG_NAME "${ARG_NAME}"
            EACP_CATALOG_BUNDLE_NAME "${ARG_NAME}.app"
            EACP_CATALOG_VERSION "${ARG_VERSION}"
            EACP_CATALOG_KIND "${ARG_KIND}"
            EACP_CATALOG_DEPENDENCIES "${ARG_DEPENDENCIES}"
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${ARG_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${ARG_PRODUCT_ID}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${ARG_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${ARG_VERSION}"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ARG_PRODUCT_ID}")
    if (ARG_CATALOG_EXCLUDE)
        set_target_properties(${target} PROPERTIES EACP_CATALOG_EXCLUDE TRUE)
    endif ()

    eacp_set_gui_subsystem(${target})
    set_default_target_setting(${target})
endfunction()

function(eacp_generate_apphub_catalog)
    set(oneValueArgs TARGET OUTPUT ARTIFACT_DIR CHANNEL VERSION ROOT_DIR)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if (NOT ARG_TARGET)
        set(ARG_TARGET eacp-apphub-local-catalog)
    endif ()
    if (NOT ARG_OUTPUT)
        set(ARG_OUTPUT "${CMAKE_BINARY_DIR}/apphub/catalog.json")
    endif ()
    if (NOT ARG_ARTIFACT_DIR)
        set(ARG_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/apphub/artifacts")
    endif ()
    if (NOT ARG_CHANNEL)
        set(ARG_CHANNEL "stable")
    endif ()
    if (NOT ARG_VERSION)
        set(ARG_VERSION "1")
    endif ()
    if (NOT ARG_ROOT_DIR)
        set(ARG_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif ()

    get_property(CATALOG_TARGETS GLOBAL PROPERTY EACP_CATALOG_APP_TARGETS)
    eacp_collect_catalog_apps("${ARG_ROOT_DIR}" DISCOVERED_CATALOG_TARGETS)
    list(APPEND CATALOG_TARGETS ${DISCOVERED_CATALOG_TARGETS})
    if (NOT CATALOG_TARGETS)
        return()
    endif ()
    list(REMOVE_DUPLICATES CATALOG_TARGETS)

    set(PRODUCT_ARGS)
    set(INCLUDED_CATALOG_TARGETS)
    foreach (CATALOG_TARGET IN LISTS CATALOG_TARGETS)
        get_target_property(CATALOG_EXCLUDE ${CATALOG_TARGET} EACP_CATALOG_EXCLUDE)
        if (CATALOG_EXCLUDE)
            continue()
        endif ()

        get_target_property(PRODUCT_ID ${CATALOG_TARGET} EACP_CATALOG_PRODUCT_ID)
        get_target_property(DISPLAY_NAME ${CATALOG_TARGET} EACP_CATALOG_NAME)
        get_target_property(BUNDLE_NAME ${CATALOG_TARGET} EACP_CATALOG_BUNDLE_NAME)
        get_target_property(PRODUCT_VERSION ${CATALOG_TARGET} EACP_CATALOG_VERSION)
        get_target_property(KIND ${CATALOG_TARGET} EACP_CATALOG_KIND)
        get_target_property(DEPS ${CATALOG_TARGET} EACP_CATALOG_DEPENDENCIES)
        if (NOT DEPS)
            set(DEPS "")
        endif ()
        list(JOIN DEPS "," DEPS_CSV)
        list(APPEND INCLUDED_CATALOG_TARGETS "${CATALOG_TARGET}")
        list(APPEND PRODUCT_ARGS
                --product
                "${CATALOG_TARGET}|${PRODUCT_ID}|${DISPLAY_NAME}|${BUNDLE_NAME}|${PRODUCT_VERSION}|${KIND}|${DEPS_CSV}|$<TARGET_BUNDLE_DIR:${CATALOG_TARGET}>")
    endforeach ()

    if (NOT INCLUDED_CATALOG_TARGETS)
        return()
    endif ()

    add_custom_command(
            OUTPUT "${ARG_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_ARTIFACT_DIR}"
            COMMAND node
                    "${CMAKE_SOURCE_DIR}/Scripts/generate-apphub-local-catalog.mjs"
                    --catalog "${ARG_OUTPUT}"
                    --artifact-dir "${ARG_ARTIFACT_DIR}"
                    --catalog-version "${ARG_VERSION}"
                    --channel "${ARG_CHANNEL}"
                    ${PRODUCT_ARGS}
            DEPENDS ${INCLUDED_CATALOG_TARGETS}
                    "${CMAKE_SOURCE_DIR}/Scripts/generate-apphub-local-catalog.mjs"
            VERBATIM)
    add_custom_target(${ARG_TARGET} DEPENDS "${ARG_OUTPUT}")

    set(EACP_APPHUB_LOCAL_CATALOG_TARGET "${ARG_TARGET}" PARENT_SCOPE)
    set(EACP_APPHUB_LOCAL_CATALOG_PATH "${ARG_OUTPUT}" PARENT_SCOPE)
endfunction()

function(eacp_collect_catalog_apps directory out_var)
    set(RESULT)
    get_property(DIR_TARGETS DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach (DIR_TARGET IN LISTS DIR_TARGETS)
        get_target_property(IS_BUNDLE ${DIR_TARGET} MACOSX_BUNDLE)
        if (NOT IS_BUNDLE)
            continue()
        endif ()

        get_target_property(CATALOG_EXCLUDE ${DIR_TARGET} EACP_CATALOG_EXCLUDE)
        if (CATALOG_EXCLUDE)
            continue()
        endif ()

        get_target_property(PRODUCT_ID ${DIR_TARGET} EACP_CATALOG_PRODUCT_ID)
        if (NOT PRODUCT_ID)
            get_target_property(PRODUCT_ID ${DIR_TARGET} MACOSX_BUNDLE_GUI_IDENTIFIER)
        endif ()
        if (PRODUCT_ID)
            eacp_register_catalog_app(${DIR_TARGET})
            list(APPEND RESULT "${DIR_TARGET}")
        endif ()
    endforeach ()

    get_property(SUBDIRECTORIES DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach (SUBDIRECTORY IN LISTS SUBDIRECTORIES)
        eacp_collect_catalog_apps("${SUBDIRECTORY}" CHILD_RESULT)
        list(APPEND RESULT ${CHILD_RESULT})
    endforeach ()

    set(${out_var} ${RESULT} PARENT_SCOPE)
endfunction()

function(eacp_enable_unity_build target)
    if (EACP_UNITY_BUILD)
        set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
    endif ()
endfunction()

# Mark an executable as a windowed GUI app so launching it never pops a console
# window on Windows — the cross-platform analogue of MACOSX_BUNDLE on Apple. A
# no-op everywhere but Windows. Call this on any eacp app that owns a window
# (graphics/tray/webview apps) rather than a console.
function(eacp_set_gui_subsystem target)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
    # WIN32_EXECUTABLE selects /SUBSYSTEM:WINDOWS, whose default MSVC entry point
    # is WinMainCRTStartup (it expects WinMain). eacp apps use a standard
    # int main(), so redirect the entry to the console CRT startup — it still
    # parses argv and calls main(), just without a console attached.
    if (MSVC)
        target_link_options(${target} PRIVATE "/ENTRY:mainCRTStartup")
    endif ()
endfunction()

function(add_ide_sources target)
    file(GLOB_RECURSE ALL_HEADERS
            "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    )

    if (ALL_HEADERS)
        target_sources(${target} PRIVATE ${ALL_HEADERS})
        set_source_files_properties(${ALL_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
    endif ()
endfunction()

function(eacp_default_setup)
    eacp_setup_apple()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_COMPILE_OPTIONS_IPO "-flto=full")
    endif ()
endfunction()
