cmake_minimum_required(VERSION 3.16)
project(PrimeXTWeaponEditor LANGUAGES CXX)
# Standalone application: no pxmv / game SDK / engine dependencies.
add_executable(primext-weapon-validate native_validate.cpp ../../game_shared/modular_weapons.cpp)
target_compile_features(primext-weapon-validate PRIVATE cxx_std_17)
target_include_directories(primext-weapon-validate PRIVATE ../../game_shared)
add_custom_target(primext-weapon-editor SOURCES app.py project.py launch.py validator_location.py README.md)
