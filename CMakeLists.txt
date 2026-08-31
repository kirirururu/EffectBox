cmake_minimum_required(VERSION 4.3)

project(EffectBox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JUCE REQUIRED)

add_subdirectory(src)
