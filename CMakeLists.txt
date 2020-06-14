cmake_minimum_required(VERSION 3.17.0)
project(ppos VERSION 0.1.0)


set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*,-header-filter=.*)

if (USE_SANITIZER STREQUAL "Address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
elseif (USE_SANITIZER STREQUAL "Thread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
elseif (USE_SANITIZER STREQUAL "Undefined")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
elseif (USE_SANITIZER STREQUAL "Leak")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=leak")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
elseif (USE_SANITIZER STREQUAL "Memory")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
endif()

if(MSVC)
    find_package(wxWidgets "3.1")
else()
    find_package(wxWidgets "3.1" REQUIRED COMPONENTS net core base)
endif()

include(${wxWidgets_USE_FILE})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(PPOS_SOURCE_LIST ppos sqlite mongoose wpObject comm)
add_subdirectory(sqlite)
add_subdirectory(mongoose)
add_subdirectory(wpObject)
add_subdirectory(comm)

if(MSVC) 
add_executable(ppos WIN32 main.cpp minimal.cpp)
message(${wxWidgets_LIBRARIES})
target_link_libraries(
    ${PPOS_SOURCE_LIST}
    ${wxWidgets_LIB_DIR}/wxmsw31ud.lib
    ${wxWidgets_LIBRARIES}
)
else()
add_executable(ppos main.cpp minimal.cpp)
message(${wxWidgets_LIBRARIES})
target_link_libraries(
    ${PPOS_SOURCE_LIST}
    ${wxWidgets_LIBRARIES}
    Threads::Threads
)
target_compile_features(ppos PUBLIC cxx_std_17)

endif()
