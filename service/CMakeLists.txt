cmake_minimum_required(VERSION 2.8)

project(subtitleservice)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -pthread")

include_directories(
        ../libsubtitle/subtitle
        ../libsubtitle/subtitle/parser
        ../libsubtitle/subtitle/render
        ../libsubtitle/ipc
        ../libsubtitle/io
        ../libsubtitle/utils
        ../libsubtitle/include
        ../libsubtitle/display
        ../libsubtitle/subtitle/parser/ExtParser
        ../libsubtitle/port
        ../libsubtitle
        ../libsubtitle/subtitle
)

set(subtitleservice_srcs
        SubtitleServiceLinux.cpp
        main_subtitleservice.cpp
        )

set(TARGET subtitleservice)
add_executable(${TARGET} ${subtitleservice_srcs})

if (NOT NDEBUG)
    add_definitions(-DNEED_SUBTITLE_DEBUG_INFORMATION)
    add_definitions(-DNEED_SUBTITLE_DEBUG_ERROR)
    target_link_libraries(${TARGET} PUBLIC log)
endif()

target_link_libraries(${TARGET} PUBLIC binder)
target_link_libraries(${TARGET} PUBLIC zvbi)
target_link_libraries(${TARGET} PUBLIC pthread)
target_link_libraries(${TARGET} PUBLIC subtitle)
target_link_libraries(${TARGET} PUBLIC subtitleclient)

install(
        TARGETS ${TARGET}
)
