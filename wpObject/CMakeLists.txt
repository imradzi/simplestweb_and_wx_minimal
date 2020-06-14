add_library(wpObject words.cpp wpObject.cpp jsonreader.cpp jsonval.cpp jsonwriter.cpp)
target_include_directories(wpObject PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
