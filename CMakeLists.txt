cmake_minimum_required(VERSION 3.17.0)
project(ppos VERSION 0.1.0)

find_package(wxWidgets "3.1")
include(${wxWidgets_USE_FILE})

add_subdirectory(sqlite)
add_subdirectory(mongoose)
add_executable(ppos WIN32 main.cpp minimal.cpp)
message(${wxWidgets_LIBRARIES})
target_link_libraries(
    ppos 
    sqlite 
    mongoose
    ${wxWidgets_LIB_DIR}/wxmsw31ud.lib
    ${wxWidgets_LIBRARIES}
)
