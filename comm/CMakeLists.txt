add_library(comm comm.cpp connection.cpp)
target_include_directories(comm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
