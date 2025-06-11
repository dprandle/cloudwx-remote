# Disable mongo auto cleanup and init - we do that manually
set(ENABLE_AUTOMATIC_INIT_AND_CLEANUP OFF)
set(ENABLE_TESTS OFF)
set(ENABLE_EXAMPLES OFF)
set(ENABLE_SNAPPY OFF)
add_subdirectory(mongo-c-driver)

add_subdirectory(whisper.cpp)

