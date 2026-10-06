# SPDX-FileCopyrightText: 2024 Aleix Pol <aleixpol@kde.org>
# SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
# SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>
#
# SPDX-License-Identifier: BSD-2-Clause

configure_file(config-plasma-keyboard.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-plasma-keyboard.h)

# The speech recognition engines come from whisper.cpp: it provides libwhisper
# and libparakeet (the Parakeet engine lives in the same repository) under the
# MIT license. It is built together with the keyboard, so the package does not
# depend on the whisper-cpp package, which does not exist on every target
# system (the Arch snapshot the release is built against does not ship it).
# WHISPER_CPP_SOURCE_DIR lets the package build use a source archive instead of
# a git checkout, which keeps the release build offline.
#
# The subproject is excluded from the install rules (CMake 3.28 and newer): it
# carries install() rules of its own for the headers, the CMake package files
# and the libraries, and none of them belong to this package. Its libraries are
# still built, because the keyboard links against them.
#
# They are linked in statically, which is why BUILD_SHARED_LIBS is turned off
# before the subproject is added: KDECMakeSettings turns shared libraries on
# globally, and a shared build would leave the keyboard looking for
# libwhisper.so and libggml*.so at run time, where the package has none.
# The variable is set for the subproject only and unset again right after it:
# a cache-wide "FORCE" would also turn this project's own libraries
# (libkeyboardlib, libbreezestyle) into static archives.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF)
set(WHISPER_CPP_SOURCE_DIR "" CACHE PATH "Local whisper.cpp source tree used instead of downloading one")
if(WHISPER_CPP_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_WHISPER "${WHISPER_CPP_SOURCE_DIR}" CACHE PATH "" FORCE)
endif()
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_IS_DEV OFF CACHE BOOL "" FORCE)
set(WHISPER_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(_whisper_exclude_from_all)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    set(_whisper_exclude_from_all EXCLUDE_FROM_ALL)
endif()
FetchContent_Declare(whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG v1.9.3
    GIT_SHALLOW TRUE
    ${_whisper_exclude_from_all}
)
FetchContent_MakeAvailable(whisper)
unset(BUILD_SHARED_LIBS)

# KDECompilerSettings builds the project without C++ exceptions, but the GGUF
# reader inside whisper.cpp uses try/catch blocks, so the libraries that come
# from there need the exceptions back.
foreach(whisper_target ggml ggml-base ggml-cpu ggml-blas ggml-openmp whisper parakeet)
    if(TARGET ${whisper_target})
        target_compile_options(${whisper_target} PRIVATE -fexceptions)
    endif()
endforeach()

# The GigaAM engine uses transcribe.cpp (MIT), which is built as a library of
# its own instead of being added as a subproject: it vendors its own copy of
# ggml, whose target and symbol names would collide with the ggml whisper.cpp
# brings into the keyboard. The library is loaded while the application runs
# (src/stt/gigaamengine.cpp) and installed next to the keyboard, so the engine
# reports itself unavailable when it is not there.
#
# TRANSCRIBE_CPP_SOURCE_DIR lets the package build use a source archive instead
# of a git checkout, the same way WHISPER_CPP_SOURCE_DIR does.
include(ExternalProject)
set(TRANSCRIBE_CPP_SOURCE_DIR "" CACHE PATH "Local transcribe.cpp source tree used instead of downloading one")
set(_transcribe_cpp_prefix ${CMAKE_CURRENT_BINARY_DIR}/transcribe-cpp)
set(_transcribe_cpp_install ${_transcribe_cpp_prefix}/install)
if(TRANSCRIBE_CPP_SOURCE_DIR)
    set(_transcribe_cpp_source SOURCE_DIR ${TRANSCRIBE_CPP_SOURCE_DIR})
else()
    set(_transcribe_cpp_source
        GIT_REPOSITORY https://github.com/handy-computer/transcribe.cpp.git
        GIT_TAG 5bb2deb2a4afb1fd50534ecb51cfcb521ef94944
        GIT_SHALLOW FALSE
    )
endif()
ExternalProject_Add(transcribe_cpp
    ${_transcribe_cpp_source}
    PREFIX ${_transcribe_cpp_prefix}
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${_transcribe_cpp_install}
        "-DCMAKE_INSTALL_RPATH=\$ORIGIN"
        -DTRANSCRIBE_BUILD_SHARED=ON
        -DTRANSCRIBE_INSTALL=ON
        -DTRANSCRIBE_BUILD_TESTS=OFF
        -DTRANSCRIBE_BUILD_EXAMPLES=OFF
        -DTRANSCRIBE_BUILD_TOOLS=OFF
        # The system BLAS probe of transcribe.cpp finds the reference libblas
        # and then cannot resolve the cblas symbols, so the decoder runs
        # without system BLAS. That is fast enough for the keyboard.
        -DTRANSCRIBE_USE_SYSTEM_BLAS=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target transcribe
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
    UPDATE_COMMAND ""
    LOG_BUILD ON
)

add_executable(plasma-keyboard-custom)

ecm_add_qtwayland_client_protocol(plasma-keyboard-custom PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml BASENAME input-method-unstable-v1)
ecm_add_qtwayland_client_protocol(plasma-keyboard-custom PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml BASENAME text-input-unstable-v1)
ecm_add_qtwayland_client_protocol(plasma-keyboard-custom PROTOCOL "${Wayland_DATADIR}/wayland.xml" BASENAME wayland)

target_sources(plasma-keyboard-custom PRIVATE
    main.cpp
    inputpanelintegration.cpp
    inputpanelintegration.h
    inputpanelrole.h
    clipboard/clipboardhistory.cpp
    clipboard/clipboardhistory.h
    gamepadhandler.cpp
    gamepadhandler.h
    inputlisteneritem.cpp
    inputlisteneritem.h
    inputmethod.cpp
    inputmethod_p.h
    inputpanelwindow.cpp
    inputpanelwindow.h
    inputplugin.cpp
    restartwatcher.cpp
    restartwatcher.h
    settingsreloader.cpp
    settingsreloader.h
    touchholdwatcher.cpp
    inputplugin.h
    theme/thememanager.cpp
    theme/thememanager.h
    qwaylandinputpanelshellintegration.cpp
    qwaylandinputpanelshellintegration_p.h
    qwaylandinputpanelsurface.cpp
    qwaylandinputpanelsurface_p.h
    overlay/overlaycontroller.cpp
    overlay/overlaycontroller.h
    overlay/candidatemodel.cpp
    overlay/candidatemodel.h
    overlay/overlaytrigger.cpp
    overlay/overlaytrigger.h
    overlay/longpresstrigger.cpp
    overlay/longpresstrigger.h
    overlay/diacriticsdataloader.cpp
    overlay/diacriticsdataloader.h
    overlay/diacritics.qrc
    overlay/prefixquerytrigger.cpp
    overlay/prefixquerytrigger.h
    overlay/textexpansiontrigger.cpp
    overlay/textexpansiontrigger.h
    prediction.qrc
    icons.qrc
    windowrestore.qrc
    prediction/predictivedictionary.cpp
    prediction/predictivedictionary.h
    prediction/hunspelldictionary.cpp
    prediction/hunspelldictionary.h
    prediction/wordatcursor.cpp
    prediction/wordatcursor.h
    prediction/wordlookup.cpp
    prediction/wordlookup.h
    prediction/wordpredictor.cpp
    prediction/wordpredictor.h
    stt/sttaudiorecorder.cpp
    stt/sttaudiorecorder.h
    stt/sttengine.cpp
    stt/sttengine.h
    stt/sttmodelcatalog.cpp
    stt/sttmodelcatalog.h
    stt/stt.qrc
    stt/gigaamengine.cpp
    stt/gigaamengine.h
    stt/parakeetengine.cpp
    stt/parakeetengine.h
    stt/whisperengine.cpp
    stt/whisperengine.h
    stt/voskengine.cpp
    stt/voskengine.h
    stt/sttmanager.cpp
    stt/sttmanager.h
)

if(PLASMA_KEYBOARD_VIBRATION_ENABLED)
    set_source_files_properties(dbus/org.sigxcpu.Feedback.Haptic.xml PROPERTIES INCLUDE vibrationevent.h)
    qt_add_dbus_interfaces(dbusinterface_SRCS
        dbus/org.sigxcpu.Feedback.Haptic.xml)

    target_sources(plasma-keyboard-custom PRIVATE
        vibration.cpp
        vibration.h
        vibrationevent.h
        ${dbusinterface_SRCS}
    )
endif()

if(PLASMA_KEYBOARD_SOUND_ENABLED)
    target_sources(plasma-keyboard-custom PRIVATE
        soundresources.qrc
    )
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/70-plasma-keyboard-touchscreen.rules
        DESTINATION ${KDE_INSTALL_LIBDIR}/udev/rules.d)

ecm_qt_declare_logging_category(plasma-keyboard-custom
    HEADER logging.h
    IDENTIFIER "PlasmaKeyboard"
    CATEGORY_NAME "org.kde.plasma.keyboard.custom"
    DESCRIPTION "Plasma Keyboard"
    DEFAULT_SEVERITY Debug
)

ecm_add_qml_module(plasma-keyboard-custom
    URI "org.kde.plasma.keyboard.custom"
    GENERATE_PLUGIN_SOURCE
    DEPENDENCIES
    QtQuick
)

ecm_target_qml_sources(plasma-keyboard-custom SOURCES
    qml/main.qml
    qml/LanguagePopup.qml
    qml/LanguagePopupDelegate.qml
    qml/DiacriticsOverlay.qml
    qml/AlternatesList.qml
    qml/OverlayWindow.qml
    qml/VoicePanel.qml
    qml/GamepadBadge.qml
)
ecm_finalize_qml_module(plasma-keyboard-custom)

kconfig_add_kcfg_files(plasma-keyboard-custom GENERATE_MOC plasmakeyboardsettings.kcfgc)

target_include_directories(plasma-keyboard-custom PRIVATE
    ${CMAKE_BINARY_DIR}
    # Add directories to include path for QML type registration
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay
    ${CMAKE_CURRENT_SOURCE_DIR}/prediction
    ${CMAKE_CURRENT_SOURCE_DIR}/stt
    ${CMAKE_CURRENT_SOURCE_DIR}/theme
)

target_link_libraries(plasma-keyboard-custom
    PRIVATE
    KF6::CoreAddons
    KF6::I18n
    KF6::I18nQml
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::Crash
    KF6::GlobalAccel
    PkgConfig::XKBCommon
    LayerShellQt::Interface
    Plasma::PlasmaQuick
    Qt::DBus
    Qt::Gui
    Qt::GuiPrivate
    Qt::Multimedia
    Qt::VirtualKeyboard
    parakeet
    whisper
    Qt::WaylandClient
    Qt::WaylandClientPrivate
    Wayland::Client
)

# The layer-shell API grew over time: setActivateOnShow arrived in 6.5 and
# setWantsToBeOnActiveScreen in 6.6. The package is also built against an older
# layer-shell-qt (the Arch snapshot used for SteamOS 3.8 ships 6.4.3), so these
# calls are compiled only where the interface provides them.
if(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.5.0)
    target_compile_definitions(plasma-keyboard-custom PRIVATE HAVE_LAYERSHELLQT_ACTIVATE_ON_SHOW)
endif()
if(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.6.0)
    target_compile_definitions(plasma-keyboard-custom PRIVATE HAVE_LAYERSHELLQT_WANTS_ACTIVE_SCREEN)
endif()

# libhunspell is an optional dependency: the engine loads it while the
# application runs, so nothing is linked against it here.

# The GigaAM engine loads libtranscribe while it runs and looks for it in the
# directory the keyboard itself is installed into first: a local build keeps
# the library in the same prefix, which is not part of the loader search path.
target_compile_definitions(plasma-keyboard-custom PRIVATE TRANSCRIBE_LIBRARY_DIR="${KDE_INSTALL_FULL_LIBDIR}")
add_dependencies(plasma-keyboard-custom transcribe_cpp)

install(TARGETS plasma-keyboard-custom ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.plasma.keyboard.custom.desktop DESTINATION ${KDE_INSTALL_APPDIR})

# The transcribe.cpp libraries (libtranscribe and the ggml it carries) belong
# to the keyboard: they are installed next to the binary.
install(DIRECTORY ${_transcribe_cpp_install}/lib/
    DESTINATION ${KDE_INSTALL_LIBDIR}
    FILES_MATCHING
    PATTERN "libtranscribe.so*"
    PATTERN "libggml*.so*"
)

add_subdirectory(styles)
add_subdirectory(qmlplugin)

install(DIRECTORY layouts DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/keyboard-custom)
install(DIRECTORY overlay/diacritics DESTINATION ${CMAKE_INSTALL_PREFIX}/share/plasma/keyboard-custom)

install(FILES plasmakeyboardsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR} RENAME plasmakeyboardcustomsettings.kcfg)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.plasma.keyboard.custom.metainfo.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)
