# SPDX-FileCopyrightText: 2024 Aleix Pol <aleixpol@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.16)

set(PROJECT_VERSION "6.7.90")

project(plasma-keyboard VERSION ${PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pinned to the Arch Linux Archive snapshot of 2025-07-28, which matches SteamOS
# 3.8 (Qt 6.9.1-5, glibc 2.41, KF6 6.16.0, libplasma/Plasma 6.4.3). The release
# is built against exactly that snapshot because the app uses Qt's private Gui
# and WaylandClient APIs, whose ABI is not stable across Qt minor releases:
# building against a newer Qt (e.g. the rolling 6.11) would demand symbols
# SteamOS does not have, and building against an older one (6.7/6.8) produces
# private symbols that were removed or changed by Qt 6.9. The floors below
# therefore declare the versions SteamOS 3.8 actually provides.
set(PROJECT_DEP_VERSION "6.4.3")
set(QT_MIN_VERSION "6.9.0")
set(KF6_MIN_VERSION "6.16.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs) # Should be included first so its variables are available to other modules.

include(ECMDeprecationSettings)
include(ECMInstallIcons)
include(ECMQmlModule)
include(ECMQtDeclareLoggingCategory)
include(ECMSetupVersion)

include(FeatureSummary)

include(KDEClangFormat)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEGitCommitHooks)

option(PLASMA_KEYBOARD_SOUND_ENABLED "Whether to enable sound effect support, requires GPLv3 compliance" ON)
# Upstream spelled this option PLASMA_KEYBOARD_SOUNDS_ENABLED while everything
# else (config-plasma-keyboard.h.cmake, the main.cpp check) uses the name below,
# so with the old name the sounds were always compiled out and force-disabled.
# Keep honouring it so builds passing -DPLASMA_KEYBOARD_SOUNDS_ENABLED=OFF still
# end up without the GPLv3 sound asset.
if(DEFINED PLASMA_KEYBOARD_SOUNDS_ENABLED)
    set(PLASMA_KEYBOARD_SOUND_ENABLED "${PLASMA_KEYBOARD_SOUNDS_ENABLED}")
endif()
option(PLASMA_KEYBOARD_VIBRATION_ENABLED "Whether to enable haptic feedback via vibrations, requires GPLv3 compliance" ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Multimedia
    Network
    Qml
    VirtualKeyboard
    WaylandClient
)

# Qt 6.8+ ships the private modules as standalone CMake packages (Qt 6.7 and
# older defined the Qt6::*Private targets from the base module packages). The
# declared floor is Qt 6.9, so these are always present and can be required.
find_package(Qt6GuiPrivate ${QT_MIN_VERSION} REQUIRED NO_MODULE)
find_package(Qt6CorePrivate ${QT_MIN_VERSION} REQUIRED NO_MODULE)
find_package(Qt6WaylandClientPrivate ${QT_MIN_VERSION} REQUIRED NO_MODULE)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    KCMUtils
    Config
    Crash
    GlobalAccel
)

find_package(PlasmaQuick ${PROJECT_DEP_VERSION} REQUIRED)

# The visible keyboard window is a wlr-layer-shell surface (so that it stays above
# other windows and can be moved freely); LayerShellQt assigns that shell
# integration to the window, so the project does not carry its own copy of the
# protocol.
find_package(LayerShellQt ${PROJECT_DEP_VERSION} REQUIRED)

ecm_set_disabled_deprecation_versions(QT 6.9.0
    KF 6.16.0
)

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX PLASMA_KEYBOARD VERSION_HEADER plasma_keyboard_version.h)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XKBCommon REQUIRED IMPORTED_TARGET xkbcommon)

find_package(Wayland 1.2 REQUIRED COMPONENTS Client)
find_package(QtWaylandScanner REQUIRED ${QT_MIN_VERSION})
find_package(WaylandProtocols 1.19 REQUIRED)

add_subdirectory(kcm)
add_subdirectory(src)

# Panel widget that toggles the keyboard, for tablets without a hardware
# keyboard to press the shortcut on.
install(DIRECTORY plasmoid/org.kde.plasma.keyboard.custom.toggle DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

# Translations: this is a parallel build of plasma-keyboard that coexists with
# the official package, so ki18n_install(po) (which compiles and installs every
# po file) cannot be used: the upstream "plasma-keyboard" and
# "kcm_plasmakeyboard" .mo files would conflict with the official package.
# Only our own domains are installed: "kcm_plasmakeyboardcustom" (the plugin id
# of the renamed KCM) and "plasma-keyboard-custom" (the strings this fork adds
# to the application itself, asked for with i18nd()). The rest of the
# application keeps using the "plasma-keyboard" domain shipped by the official
# package.
find_package(Gettext REQUIRED)
set(_custom_translation_domains
    kcm_plasmakeyboardcustom
    plasma-keyboard-custom
)
set(_custom_mo_files)
foreach (_domain IN LISTS _custom_translation_domains)
    file(GLOB _po_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/po" "po/*/${_domain}.po")
    foreach (_po_file IN LISTS _po_files)
        get_filename_component(_lang "${_po_file}" DIRECTORY)
        set(_mo_file "${CMAKE_CURRENT_BINARY_DIR}/${_lang}/LC_MESSAGES/${_domain}.mo")
        add_custom_command(
            OUTPUT "${_mo_file}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${_lang}/LC_MESSAGES"
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${_mo_file}" "${CMAKE_CURRENT_SOURCE_DIR}/po/${_po_file}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/po/${_po_file}"
            COMMENT "Generating ${_domain} translation for ${_lang}"
            VERBATIM
        )
        install(FILES "${_mo_file}" DESTINATION "${KDE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES")
        list(APPEND _custom_mo_files "${_mo_file}")
    endforeach ()
endforeach ()
if (_custom_mo_files)
    add_custom_target(plasmakeyboard-custom-translations ALL DEPENDS ${_custom_mo_files})
endif ()

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h *.c)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
