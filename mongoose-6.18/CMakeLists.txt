cmake_minimum_required(VERSION 3.17)
add_library(mongoose src/mongoose.c)
target_include_directories(mongoose PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
