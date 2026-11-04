cmake_minimum_required(VERSION 3.16)
project(adsb-aircraft-tracker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ADSB_BUILD_TESTS "Build the unit tests" ON)
option(ADSB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ADSB_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(adsb_options INTERFACE)
target_compile_options(adsb_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
if(ADSB_WARNINGS_AS_ERRORS)
  target_compile_options(adsb_options INTERFACE -Werror)
endif()
if(ADSB_SANITIZE)
  target_compile_options(adsb_options INTERFACE -fsanitize=address,undefined
                         -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  target_link_options(adsb_options INTERFACE -fsanitize=address,undefined)
endif()

add_library(adsb STATIC
  src/crc.cpp
  src/cpr.cpp
  src/message.cpp
  src/demod.cpp
  src/input.cpp
  src/tracker.cpp
  src/output.cpp
  src/sim.cpp)
target_include_directories(adsb PUBLIC include)
target_link_libraries(adsb PUBLIC adsb_options)

add_executable(adsb-tracker apps/tracker_main.cpp)
target_link_libraries(adsb-tracker PRIVATE adsb)

add_executable(adsb-sim apps/sim_main.cpp)
target_link_libraries(adsb-sim PRIVATE adsb)

include(GNUInstallDirs)
install(TARGETS adsb-tracker adsb-sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(ADSB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
