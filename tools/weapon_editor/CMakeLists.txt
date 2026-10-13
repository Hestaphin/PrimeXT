cmake_minimum_required(VERSION 3.19)
cmake_policy(SET CMP0091 NEW)
project(PrimeXTWeaponEditor LANGUAGES CXX)
# Standalone application: no pxmv / game SDK / engine dependencies.
option(PXWE_BUILD_TESTS "Build native weapon editor backend tests" OFF)
add_library(pxwe_core STATIC pxwe_project.cpp pxwe_properties.cpp pxwe_preview.cpp
    ../../game_shared/modular_weapons.cpp ../../game_shared/ballistic_flight.cpp)
target_compile_features(pxwe_core PUBLIC cxx_std_17)
target_include_directories(pxwe_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../game_shared")

if(WIN32)
    enable_language(RC)
    # pxwe.rc is the sole manifest embedding path. Listing the XML as a
    # target source also sends it to CMake's vs_link_exe --manifests path,
    # even with /MANIFEST:NO, duplicating resource #1 in incremental links.
    add_executable(pxwe WIN32 pxwe_win32.cpp pxwe.rc pxwe_resource.h)
    set_property(SOURCE pxwe.rc APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/pxwe.manifest")
    target_compile_definitions(pxwe PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(pxwe PRIVATE pxwe_core comctl32 comdlg32 shell32 ole32 user32 gdi32 winmm)
    if(MSVC)
        target_compile_options(pxwe PRIVATE /utf-8)
        # The resource embeds the DPI/common-controls manifest; suppress the
        # linker's second default manifest resource rather than merging IDs.
        target_link_options(pxwe PRIVATE /MANIFEST:NO)
    elseif(MINGW)
        target_link_options(pxwe PRIVATE -municode -static-libgcc -static-libstdc++)
    endif()
else()
    # Portable, headless validation interface; the GUI is a Windows application.
    add_executable(pxwe pxwe_cli.cpp)
    target_link_libraries(pxwe PRIVATE pxwe_core)
endif()

# Kept for existing developer tests/legacy Python workflows. pxwe itself never
# invokes this process and needs only its own executable in the game directory.
add_executable(primext-weapon-validate native_validate.cpp)
target_link_libraries(primext-weapon-validate PRIVATE pxwe_core)
add_custom_target(primext-weapon-editor DEPENDS pxwe)
if(MSVC)
    set_target_properties(pxwe_core pxwe primext-weapon-validate PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(PXWE_BUILD_TESTS)
    enable_testing()
    add_executable(pxwe-k11-3-tests tests/native_k11_3_tests.cpp)
    target_link_libraries(pxwe-k11-3-tests PRIVATE pxwe_core)
    add_test(NAME pxwe-k11-3-first-person COMMAND pxwe-k11-3-tests)
    if(MSVC)
        set_target_properties(pxwe-k11-3-tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(pxwe-k11-3-tests PRIVATE /utf-8)
    endif()
    add_executable(pxwe-j11-6-tests tests/native_j11_6_tests.cpp)
    target_link_libraries(pxwe-j11-6-tests PRIVATE pxwe_core)
    add_test(NAME pxwe-j11-6-audit-fixes COMMAND pxwe-j11-6-tests)
    add_executable(pxwe-j11-5-tests tests/native_j11_5_tests.cpp)
    target_link_libraries(pxwe-j11-5-tests PRIVATE pxwe_core)
    add_test(NAME pxwe-j11-5-rename-help COMMAND pxwe-j11-5-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../documentation/examples/modular-v4-j11_1")
    add_executable(pxwe-j11-schema-tests ../../game_shared/tests/modular_j11_1_schema_tests.cpp)
    target_link_libraries(pxwe-j11-schema-tests PRIVATE pxwe_core)
    add_test(NAME pxwe-schema-v4 COMMAND pxwe-j11-schema-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../documentation/examples/modular-v4-j11_1")
    # Portable guard for the native blank-tab regression. Windows painting
    # still requires the documented GUI acceptance check.
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME pxwe-j11-5-win32-wiring
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_j11_5_win32_wiring.py")
        add_test(NAME pxwe-tab-visibility-source
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tab_visibility.py")
        add_test(NAME pxwe-j11-4-win32-source
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_j11_4_ui_wiring.py")
    endif()
    add_executable(pxwe-j11-4-tests tests/native_j11_4_tests.cpp)
    target_link_libraries(pxwe-j11-4-tests PRIVATE pxwe_core)
    add_test(NAME pxwe-j11-4-camera COMMAND pxwe-j11-4-tests)
    add_executable(pxwe-j11-3-tests tests/native_j11_3_tests.cpp)
    target_link_libraries(pxwe-j11-3-tests PRIVATE pxwe_core)
    add_test(NAME pxwe-j11-3 COMMAND pxwe-j11-3-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/../../documentation/examples/modular-v4-j11_1")
    if(MSVC)
        # These targets link the same static-CRT core as pxwe. Keep the
        # J.11 test additions consistent in Debug and Release builds.
        set_target_properties(pxwe-j11-6-tests pxwe-j11-5-tests
            pxwe-j11-schema-tests pxwe-j11-4-tests pxwe-j11-3-tests PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        foreach(pxwe_test_target IN ITEMS pxwe-j11-6-tests pxwe-j11-5-tests
                pxwe-j11-schema-tests pxwe-j11-4-tests pxwe-j11-3-tests)
            target_compile_options(${pxwe_test_target} PRIVATE /utf-8)
        endforeach()
    endif()
    add_executable(pxwe-tests tests/native_project_tests.cpp)
    target_link_libraries(pxwe-tests PRIVATE pxwe_core)
    if(MSVC)
        target_compile_options(pxwe-tests PRIVATE /utf-8)
        set_property(TARGET pxwe-tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME pxwe-native COMMAND pxwe-tests)
    add_executable(pxwe-format-tests tests/native_format_tests.cpp)
    target_link_libraries(pxwe-format-tests PRIVATE pxwe_core)
    if(MSVC)
        target_compile_options(pxwe-format-tests PRIVATE /utf-8)
        set_property(TARGET pxwe-format-tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME pxwe-format3 COMMAND pxwe-format-tests "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_executable(pxwe-usability-tests tests/native_usability_tests.cpp)
    target_link_libraries(pxwe-usability-tests PRIVATE pxwe_core)
    if(MSVC)
        target_compile_options(pxwe-usability-tests PRIVATE /utf-8)
        set_property(TARGET pxwe-usability-tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME pxwe-usability COMMAND pxwe-usability-tests "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_executable(pxwe-preview-tests tests/native_preview_tests.cpp)
    target_link_libraries(pxwe-preview-tests PRIVATE pxwe_core)
    if(MSVC)
        target_compile_options(pxwe-preview-tests PRIVATE /utf-8)
        set_property(TARGET pxwe-preview-tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME pxwe-preview COMMAND pxwe-preview-tests)
    add_executable(pxwe-category-tests tests/native_category_tests.cpp)
    target_link_libraries(pxwe-category-tests PRIVATE pxwe_core)
    if(MSVC)
        target_compile_options(pxwe-category-tests PRIVATE /utf-8)
        set_property(TARGET pxwe-category-tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME pxwe-categories COMMAND pxwe-category-tests)
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone install prefix denotes a game/mod folder.
    install(TARGETS pxwe RUNTIME DESTINATION devkit)
    install(FILES PrimeXT_Weapon_Editor_Beginner_Guide.md DESTINATION devkit)
else()
    install(TARGETS pxwe RUNTIME DESTINATION "${GAMEDIR}/${UTILS_INSTALL_DIR}")
    install(FILES PrimeXT_Weapon_Editor_Beginner_Guide.md DESTINATION "${GAMEDIR}/${UTILS_INSTALL_DIR}")
endif()
