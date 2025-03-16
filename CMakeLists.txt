cmake_minimum_required(VERSION 3.11.0)

project(cloudwx)

message("Compiling for architecture ${CMAKE_HOST_SYSTEM_PROCESSOR}")

# These have the option to be different since you might want a version attached to the target name but not the app.
# There may be more than a single app/target per project - so we by default set them to the cloudwx we can easily change that later.
set(TARGET_NAME cloudwx)
set(APP_NAME cloudwx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)
set(WHISPER_INCLUDE_DIR ${DEPS_DIR}/whisper.cpp/include)

add_subdirectory(${DEPS_DIR}/whisper.cpp)

# Set the src files for the project
file(GLOB SRC_FILES
  "${SRC_DIR}/*.cpp"
  "${SRC_DIR}/*.c")

add_executable(${TARGET_NAME} ${SRC_FILES})

target_include_directories(${TARGET_NAME} PRIVATE
  ${WHISPER_INCLUDE_DIR})

target_link_libraries(${TARGET_NAME}
  whisper)

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  target_compile_options(${TARGET_NAME} PRIVATE -march=armv8-a+fp+simd)
endif()

add_custom_command(
  TARGET ${TARGET_NAME} POST_BUILD
  COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/models ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models)
add_custom_command(
  TARGET ${TARGET_NAME} POST_BUILD
  COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/sample_audio ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sample_audio)

