add_library(sqlite sqlite3.c)
target_compile_definitions(sqlite PRIVATE -DSQLITE_ENABLE_FTS5)
target_include_directories(sqlite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
