# SPDX-FileCopyrightText: 2026 Aleix Pol <aleixpol@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test WaylandCompositor)
include(ECMAddTests)

file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mockinputmethodcompositor_config.h
    CONTENT "#pragma once\n#define PLASMA_KEYBOARD_BINARY_PATH \"$<TARGET_FILE:plasma-keyboard-custom>\"\n"
)

ecm_add_test(mockinputmethodcompositor.cpp LINK_LIBRARIES
    KF6::ConfigCore
    PkgConfig::XKBCommon
    Qt::Core
    Qt::Gui
    Qt::GuiPrivate
    Qt::Test
    Qt::WaylandCompositor
    Wayland::Server
)

qt6_generate_wayland_protocol_server_sources(mockinputmethodcompositor
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
        ${Wayland_DATADIR}/wayland.xml
)

# The predictive text engine is compiled into the test directly: it is a part
# of the application sources, and the test only needs the word lists.
ecm_add_test(predictivedictionarytest.cpp
    ../src/prediction/predictivedictionary.cpp
    ../src/prediction/hunspelldictionary.cpp
    ../src/prediction/wordlookup.cpp
    TEST_NAME predictivedictionarytest
    LINK_LIBRARIES
        Qt::Core
        Qt::Qml
        Qt::Test
)

target_include_directories(predictivedictionarytest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction
)

# The word lists are normally compiled into the application (src/prediction.qrc);
# the test reads the same files from the resources.
qt_add_resources(predictivedictionarytest "prediction_dictionaries"
    PREFIX "/prediction"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction/data"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction/data/en.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction/data/ru.bin"
)

# The bigram engine is compiled into the test directly as well, together with
# the bigram lists it looks the predictions up in.
ecm_add_test(wordpredictortest.cpp
    ../src/prediction/wordpredictor.cpp
    ../src/prediction/wordlookup.cpp
    TEST_NAME wordpredictortest
    LINK_LIBRARIES
        Qt::Core
        Qt::Qml
        Qt::Test
)

target_include_directories(wordpredictortest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction
)

qt_add_resources(wordpredictortest "prediction_ngrams"
    PREFIX "/prediction"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction/data"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction/data/en.ngrams"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction/data/ru.ngrams"
)

# Cutting the word being typed out of the text in front of the cursor.
ecm_add_test(wordatcursortest.cpp
    ../src/prediction/wordatcursor.cpp
    TEST_NAME wordatcursortest
    LINK_LIBRARIES
        Qt::Core
        Qt::Test
)

target_include_directories(wordatcursortest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction
)

# The hunspell dictionaries of the system: the test asks the ones that are
# installed and skips what cannot be checked without them.
ecm_add_test(hunspelldictionarytest.cpp
    ../src/prediction/hunspelldictionary.cpp
    ../src/prediction/wordlookup.cpp
    TEST_NAME hunspelldictionarytest
    LINK_LIBRARIES
        Qt::Core
        Qt::Test
)

target_include_directories(hunspelldictionarytest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/prediction
)

# The speech recognition engines. The models are not in the repository, so the
# recognition itself is skipped unless the test environment points at them; the
# engines and the recorder are still checked.
ecm_add_test(sttenginestest.cpp
    ../src/stt/sttengine.cpp
    ../src/stt/parakeetengine.cpp
    ../src/stt/whisperengine.cpp
    ../src/stt/gigaamengine.cpp
    ../src/stt/voskengine.cpp
    ../src/stt/sttaudiorecorder.cpp
    TEST_NAME sttenginestest
    LINK_LIBRARIES
        KF6::I18n
        Qt::Core
        Qt::Multimedia
        Qt::Test
        parakeet
        whisper
)

target_include_directories(sttenginestest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/stt
)
