# SPDX-FileCopyrightText: 2026 Destin Moss <https://youcoded.ai>
# SPDX-License-Identifier: MIT
#
# Vendored copy of the MIT swipe engine of the Swipe Keyboard project,
# https://github.com/itsdestin/swipe-keyboard, commit
# f48099755cd1ebeb3f3a9e40f680a683ccc95b2b (2026-10-24). Only engine/src and
# engine/include are copied; the upstream CMakeLists and its GoogleTest-based
# tests are not part of this copy. See README.md in this directory.

add_library(swipekb_engine STATIC
    src/context.cpp
    src/language_engine.cpp
    src/learning.cpp
    src/ngram.cpp
    src/ngram_predictor.cpp
    src/normalize.cpp
    src/nwp_model.cpp
    src/nwp_predictor.cpp
    src/prediction.cpp
    src/spelling.cpp
    src/swipe_beam.cpp
    src/swipe_engine.cpp
    src/swipe_geo.cpp
    src/swipe_layout.cpp
    src/swipe_lm.cpp
    src/swipe_net.cpp
)

target_include_directories(swipekb_engine PUBLIC include)
target_link_libraries(swipekb_engine PUBLIC Qt6::Core)
target_compile_features(swipekb_engine PUBLIC cxx_std_20)

# The keyboard's CMAKE_CXX_FLAGS disable exceptions; the engine relies on them
# for std::optional and Qt containers (the same reason whisper.cpp gets them
# back in src/CMakeLists.txt).
target_compile_options(swipekb_engine PRIVATE -fexceptions)

# Linked into the keyboard executable, and later into the QML plugin.
set_target_properties(swipekb_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The engine's word lists are cut from the predictive word lists at build time
# (tools/generate-swipe-dictionaries.py): the same FrequencyWords data, the
# most frequent words only, in the AOSP .combined format the engine reads.
find_package(Python3 COMPONENTS Interpreter)
set(_swipe_dictionary_dir ${CMAKE_CURRENT_BINARY_DIR}/dictionaries)
set(_swipe_dictionary_files "")
set(_swipe_languages en ru)
if(Python3_Interpreter_FOUND)
    foreach(_language IN LISTS _swipe_languages)
        set(_output ${_swipe_dictionary_dir}/${_language}.combined)
        add_custom_command(
            OUTPUT ${_output}
            COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate-swipe-dictionaries.py
                --data ${PROJECT_SOURCE_DIR}/src/prediction/data
                --out ${_swipe_dictionary_dir}
                --langs ${_language}
                --top 50000
            DEPENDS ${PROJECT_SOURCE_DIR}/tools/generate-swipe-dictionaries.py
                ${PROJECT_SOURCE_DIR}/src/prediction/data/${_language}.bin
            COMMENT "Generating the ${_language} swipe dictionary"
            VERBATIM
        )
        list(APPEND _swipe_dictionary_files ${_output})
    endforeach()
    add_custom_target(swipe_dictionaries ALL DEPENDS ${_swipe_dictionary_files})
    add_dependencies(swipekb_engine swipe_dictionaries)
else()
    message(WARNING "Python 3 is missing: the swipe dictionaries will not be generated and swipe typing stays disabled at run time")
endif()

install(FILES ${_swipe_dictionary_files}
    DESTINATION ${KDE_INSTALL_DATADIR}/plasma-keyboard-custom/swipe)
install(FILES
    model/swipe-model.bin
    model/NOTICE.md
    model/FUTO-SWIPE-DATA-LICENSE.txt
    DESTINATION ${KDE_INSTALL_DATADIR}/plasma-keyboard-custom/swipe)
