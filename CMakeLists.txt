cmake_minimum_required(VERSION 3.25)
project(DK2MacNative LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(APPLE)
  enable_language(OBJCXX)
  add_compile_definitions(GL_SILENCE_DEPRECATION GLM_ENABLE_EXPERIMENTAL)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "macOS deployment target")
endif()

find_package(OpenGL REQUIRED)

# Build portable dependencies with the engine so a Universal build does not
# depend on architecture-specific Homebrew dylibs.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw EXCLUDE_FROM_ALL)

set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glew/build/cmake glew-build EXCLUDE_FROM_ALL)

add_library(glm_headers INTERFACE)
target_include_directories(glm_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")

file(GLOB KEEPER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(APPLE)
  list(APPEND KEEPER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MacMoviePlayer.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MacSoundPlayer.mm")
endif()
add_executable(dk2-native ${KEEPER_SOURCES})
if(APPLE)
  target_sources(dk2-native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/MacMoviePlayer.mm")
endif()
target_include_directories(dk2-native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(dk2-native PRIVATE GLEW_STATIC)
target_link_libraries(dk2-native PRIVATE OpenGL::GL glew_s glfw glm_headers)

if(APPLE)
  target_link_libraries(dk2-native PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreVideo"
    "-framework AVFoundation" "-framework AVKit")
endif()
