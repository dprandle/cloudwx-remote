cmake_minimum_required(VERSION 3.11.0)

project(cloudwx)

# These have the option to be different since you might want a version attached to the target name but not the app.
# There may be more than a single app/target per project - so we by default set them to the cloudwx we can easily change that later.
set(TARGET_NAME cloudwx)
set(APP_NAME cloudwx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Set the src files for the project
file(GLOB SRC_FILES
  "${SRC_DIR}/*.cpp")

add_executable(${TARGET_NAME} ${SRC_FILES})

# target_link_libraries(${TARGET_NAME} lib1 lib2 ... libn)
