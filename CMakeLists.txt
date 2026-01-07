cmake_minimum_required(VERSION 3.15)
project(TodoApi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE PATH "Asio include directory" FORCE)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(crow)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

include_directories(${crow_SOURCE_DIR}/include)
include_directories(${asio_SOURCE_DIR}/asio/include)

add_definitions(-DASIO_STANDALONE)
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

add_executable(todo_app main.cpp)

if(WIN32)
    target_link_libraries(todo_app PRIVATE 
        Threads::Threads 
        ws2_32 
        mswsock 
        SQLite::SQLite3
    )
else()
    target_link_libraries(todo_app PRIVATE 
        Threads::Threads 
        SQLite::SQLite3
    )
endif()