include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/StapikWarnings.cmake")

set(STAPIK_COMMON_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Directory with the stapik-common CMake modules")
set(STAPIK_APP_SYSTEM_PREFIX "/usr" CACHE PATH "Where launcher, .desktop file and icon are installed (outside the application prefix)")
option(STAPIK_WINDOWS_CONSOLE "Windows: build applications with a console window (for debugging) instead of as GUI programs" OFF)

if(WIN32)
    enable_language(RC)
endif()

function(stapik_add_application)
    cmake_parse_arguments(PARSE_ARGV 0 STAPIK_APP ""
            "NAME;DISPLAY_NAME;DESCRIPTION;VERSION;APPLICATION_ID;RESOURCES_DIRECTORY;PACKAGING_DIRECTORY;MAINTAINER;HOMEPAGE;SECTION"
            "SOURCES;DESKTOP_CATEGORIES;DEPENDS;LINK_LIBRARIES")

    if(STAPIK_APP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "stapik_add_application: unknown arguments: ${STAPIK_APP_UNPARSED_ARGUMENTS}")
    endif()
    if(STAPIK_APP_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "stapik_add_application: missing value for: ${STAPIK_APP_KEYWORDS_MISSING_VALUES}")
    endif()
    if(NOT STAPIK_APP_NAME)
        message(FATAL_ERROR "stapik_add_application: NAME is required")
    endif()
    if(NOT STAPIK_APP_NAME MATCHES "^[a-z0-9][a-z0-9.+-]+$")
        message(FATAL_ERROR "stapik_add_application: NAME '${STAPIK_APP_NAME}' must be a valid Debian package name (lowercase letters, digits, '+', '-', '.', at least two characters)")
    endif()
    if(NOT STAPIK_APP_SOURCES)
        message(FATAL_ERROR "stapik_add_application: SOURCES is required")
    endif()

    if(NOT STAPIK_APP_DISPLAY_NAME)
        set(STAPIK_APP_DISPLAY_NAME "${STAPIK_APP_NAME}")
    endif()
    if(NOT STAPIK_APP_DESCRIPTION)
        set(STAPIK_APP_DESCRIPTION "${STAPIK_APP_DISPLAY_NAME}")
    endif()
    if(NOT STAPIK_APP_VERSION)
        set(STAPIK_APP_VERSION "${PROJECT_VERSION}")
    endif()
    if(NOT STAPIK_APP_VERSION)
        message(FATAL_ERROR "stapik_add_application: VERSION is required when the project has no VERSION")
    endif()
    if(NOT STAPIK_APP_APPLICATION_ID)
        set(STAPIK_APP_APPLICATION_ID "${STAPIK_APP_NAME}")
    endif()
    if(NOT STAPIK_APP_MAINTAINER)
        set(STAPIK_APP_MAINTAINER "Stapik Group")
    endif()
    if(NOT STAPIK_APP_SECTION)
        set(STAPIK_APP_SECTION "utils")
    endif()
    if(NOT STAPIK_APP_DESKTOP_CATEGORIES)
        set(STAPIK_APP_DESKTOP_CATEGORIES "Utility")
    endif()

    set(resourcesDirectory "")
    if(STAPIK_APP_RESOURCES_DIRECTORY)
        cmake_path(ABSOLUTE_PATH STAPIK_APP_RESOURCES_DIRECTORY BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE resourcesDirectory)
        if(NOT IS_DIRECTORY "${resourcesDirectory}")
            message(FATAL_ERROR "stapik_add_application: RESOURCES_DIRECTORY '${resourcesDirectory}' does not exist")
        endif()
    endif()

    set(packagingDirectory "")
    if(STAPIK_APP_PACKAGING_DIRECTORY)
        cmake_path(ABSOLUTE_PATH STAPIK_APP_PACKAGING_DIRECTORY BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE packagingDirectory)
        if(NOT IS_DIRECTORY "${packagingDirectory}")
            message(FATAL_ERROR "stapik_add_application: PACKAGING_DIRECTORY '${packagingDirectory}' does not exist")
        endif()
    endif()

    # --- Executable ---
    add_executable(${STAPIK_APP_NAME} ${STAPIK_APP_SOURCES})
    target_link_libraries(${STAPIK_APP_NAME} PRIVATE stapik::common ${STAPIK_APP_LINK_LIBRARIES})
    target_compile_definitions(${STAPIK_APP_NAME} PRIVATE
            STAPIK_APP_NAME="${STAPIK_APP_NAME}"
            STAPIK_APP_DISPLAY_NAME="${STAPIK_APP_DISPLAY_NAME}"
            STAPIK_APP_VERSION="${STAPIK_APP_VERSION}")
    stapik_enable_warnings(${STAPIK_APP_NAME})

    # --- Resources next to the executable (build tree) ---
    if(NOT STAPIK_COMMON_RESOURCES_DIR OR NOT IS_DIRECTORY "${STAPIK_COMMON_RESOURCES_DIR}")
        message(FATAL_ERROR "stapik_add_application: the stapik-common resources directory '${STAPIK_COMMON_RESOURCES_DIR}' does not exist")
    endif()

    set(resourcesBuildDirectory "$<TARGET_FILE_DIR:${STAPIK_APP_NAME}>/resources")
    set(copyResourcesCommands COMMAND ${CMAKE_COMMAND} -E make_directory "${resourcesBuildDirectory}")
    if(resourcesDirectory)
        list(APPEND copyResourcesCommands
                COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${resourcesDirectory}" "${resourcesBuildDirectory}")
    endif()
    list(APPEND copyResourcesCommands
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${STAPIK_COMMON_RESOURCES_DIR}" "${resourcesBuildDirectory}/stapik-common")
    add_custom_target(${STAPIK_APP_NAME}_resources ALL ${copyResourcesCommands}
            COMMENT "Copying resources of ${STAPIK_APP_NAME}"
            VERBATIM)
    add_dependencies(${STAPIK_APP_NAME} ${STAPIK_APP_NAME}_resources)

    set(generatedDirectory "${CMAKE_CURRENT_BINARY_DIR}/stapik-packaging/${STAPIK_APP_NAME}")

    # --- Windows: GUI executable, version resource, self-contained bundle ---
    if(WIN32)
        if(NOT STAPIK_WINDOWS_CONSOLE)
            set_target_properties(${STAPIK_APP_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
        endif()

        string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" versionMatch "${STAPIK_APP_VERSION}")
        if(versionMatch)
            set(STAPIK_APP_RC_VERSION "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},0")
        else()
            set(STAPIK_APP_RC_VERSION "0,0,0,0")
        endif()

        set(STAPIK_APP_RC_ICON "")
        if(packagingDirectory AND EXISTS "${packagingDirectory}/${STAPIK_APP_NAME}.ico")
            set(STAPIK_APP_RC_ICON "1 ICON \"${packagingDirectory}/${STAPIK_APP_NAME}.ico\"")
        endif()

        # Quotes are doubled inside resource strings.
        foreach(field MAINTAINER DESCRIPTION DISPLAY_NAME)
            string(REPLACE "\"" "\"\"" STAPIK_APP_RC_${field} "${STAPIK_APP_${field}}")
        endforeach()

        configure_file("${STAPIK_COMMON_CMAKE_DIR}/templates/windows/application.rc.in" "${generatedDirectory}/${STAPIK_APP_NAME}.rc" @ONLY)
        target_sources(${STAPIK_APP_NAME} PRIVATE "${generatedDirectory}/${STAPIK_APP_NAME}.rc")

        find_program(STAPIK_BASH bash)
        if(STAPIK_BASH)
            add_custom_target(${STAPIK_APP_NAME}_windows_bundle
                    COMMAND "${STAPIK_BASH}" "${STAPIK_COMMON_CMAKE_DIR}/templates/windows/bundle.sh"
                            --exe "$<TARGET_FILE:${STAPIK_APP_NAME}>"
                            --resources "${resourcesBuildDirectory}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/${STAPIK_APP_NAME}-windows"
                    COMMENT "Assembling the self-contained Windows folder of ${STAPIK_APP_NAME}"
                    VERBATIM)
            add_dependencies(${STAPIK_APP_NAME}_windows_bundle ${STAPIK_APP_NAME} ${STAPIK_APP_NAME}_resources)
        else()
            message(STATUS "stapik_add_application: bash not found, target ${STAPIK_APP_NAME}_windows_bundle is not available")
        endif()

        return()
    endif()

    # --- Install prefix ---
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "/opt/${STAPIK_APP_NAME}" CACHE PATH "Install prefix" FORCE)
    endif()
    set(STAPIK_APP_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

    install(TARGETS ${STAPIK_APP_NAME} RUNTIME DESTINATION bin)
    if(resourcesDirectory)
        install(DIRECTORY "${resourcesDirectory}/" DESTINATION resources)
    endif()

    # Built through FetchContent, stapik-common registers this install rule itself; with an installed
    # package (find_package) nobody does, so the application installs the common resources.
    get_target_property(commonIsImported stapik::common IMPORTED)
    if(commonIsImported)
        install(DIRECTORY "${STAPIK_COMMON_RESOURCES_DIR}/" DESTINATION share/stapik-common)
    endif()

    # --- Launcher, .desktop file, icon ---
    set(launcherTemplate "${STAPIK_COMMON_CMAKE_DIR}/templates/launcher.sh.in")
    set(desktopTemplate "${STAPIK_COMMON_CMAKE_DIR}/templates/application.desktop.in")
    if(packagingDirectory)
        if(EXISTS "${packagingDirectory}/launcher.sh.in")
            set(launcherTemplate "${packagingDirectory}/launcher.sh.in")
        endif()
        if(EXISTS "${packagingDirectory}/application.desktop.in")
            set(desktopTemplate "${packagingDirectory}/application.desktop.in")
        endif()
    endif()

    set(STAPIK_APP_ICON_NAME "application-x-executable")
    if(packagingDirectory AND EXISTS "${packagingDirectory}/${STAPIK_APP_NAME}.svg")
        set(STAPIK_APP_ICON_NAME "${STAPIK_APP_NAME}")
        install(FILES "${packagingDirectory}/${STAPIK_APP_NAME}.svg"
                DESTINATION "${STAPIK_APP_SYSTEM_PREFIX}/share/icons/hicolor/scalable/apps")
    elseif(packagingDirectory AND EXISTS "${packagingDirectory}/${STAPIK_APP_NAME}.png")
        set(STAPIK_APP_ICON_NAME "${STAPIK_APP_NAME}")
        install(FILES "${packagingDirectory}/${STAPIK_APP_NAME}.png"
                DESTINATION "${STAPIK_APP_SYSTEM_PREFIX}/share/icons/hicolor/256x256/apps")
    endif()

    list(JOIN STAPIK_APP_DESKTOP_CATEGORIES ";" desktopCategories)
    set(STAPIK_APP_CATEGORIES "${desktopCategories};")

    configure_file("${launcherTemplate}" "${generatedDirectory}/${STAPIK_APP_NAME}" @ONLY)
    configure_file("${desktopTemplate}" "${generatedDirectory}/${STAPIK_APP_APPLICATION_ID}.desktop" @ONLY)

    install(PROGRAMS "${generatedDirectory}/${STAPIK_APP_NAME}" DESTINATION "${STAPIK_APP_SYSTEM_PREFIX}/bin")
    install(FILES "${generatedDirectory}/${STAPIK_APP_APPLICATION_ID}.desktop"
            DESTINATION "${STAPIK_APP_SYSTEM_PREFIX}/share/applications")

    # --- CPack DEB (only for the top-level project) ---
    if(PROJECT_IS_TOP_LEVEL)
        if(NOT DEFINED CPACK_GENERATOR)
            set(CPACK_GENERATOR "DEB")
        endif()
        if(NOT DEFINED CPACK_DEBIAN_PACKAGE_SHLIBDEPS)
            set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        endif()

        set(CPACK_PACKAGE_NAME "${STAPIK_APP_NAME}")
        set(CPACK_PACKAGE_VERSION "${STAPIK_APP_VERSION}")
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${STAPIK_APP_DESCRIPTION}")
        set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${STAPIK_APP_MAINTAINER}")
        set(CPACK_DEBIAN_PACKAGE_SECTION "${STAPIK_APP_SECTION}")
        if(STAPIK_APP_HOMEPAGE)
            set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${STAPIK_APP_HOMEPAGE}")
        endif()
        if(STAPIK_APP_DEPENDS)
            list(JOIN STAPIK_APP_DEPENDS ", " debianDepends)
            set(CPACK_DEBIAN_PACKAGE_DEPENDS "${debianDepends}")
        endif()

        include(CPack)
    endif()
endfunction()
