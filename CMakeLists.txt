cmake_minimum_required(VERSION 3.17.0)
project(ppos VERSION 0.1.0)

if(MSVC)
    find_package(wxWidgets "3.1")
else()
    find_package(wxWidgets "3.1" REQUIRED COMPONENTS net core base)
endif()

include(${wxWidgets_USE_FILE})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(sqlite)
add_subdirectory(mongoose)


if(MSVC) 
add_executable(ppos WIN32 main.cpp minimal.cpp)
message(${wxWidgets_LIBRARIES})
target_link_libraries(
    ppos 
    sqlite 
    mongoose
    ${wxWidgets_LIB_DIR}/wxmsw31ud.lib
    ${wxWidgets_LIBRARIES}
)
else()
add_executable(ppos main.cpp minimal.cpp)
message(${wxWidgets_LIBRARIES})
target_link_libraries(
    ppos 
    sqlite 
    mongoose
    ${wxWidgets_LIBRARIES}
    Threads::Threads
)

endif()
