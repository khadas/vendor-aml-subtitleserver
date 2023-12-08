cmake_minimum_required(VERSION 2.8)

project(subtitleclient)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17 -pthread")

include_directories(
        include
        ../libsubtitle
        ../libsubtitle/include
        ../libsubtitle/ipc/
        ../libsubtitle/io
        ../libsubtitle/subtitle
        ../libsubtitle/utils
        ../libsubtitle/subtitle/parser
        ../libsubtitle/subtitle/render
        ../libsubtitle/display
        ../client

)

set(subtitleclient_srcs
        SubtitleClientLinux.cpp
        SubtitleNativeAPI.cpp
        ../libsubtitle/ISubtitleCallback.cpp
        SubtitleReportAPI.cpp
        AmSocketClient.cpp
        )

set(TARGET subtitleclient)
add_library(${TARGET} SHARED ${subtitleclient_srcs})

if (NOT NDEBUG)
    add_definitions(-DNEED_SUBTITLE_DEBUG_INFORMATION)
    add_definitions(-DNEED_SUBTITLE_DEBUG_ERROR)
    target_link_libraries(${TARGET} PUBLIC log)
endif()

target_link_libraries(${TARGET} PUBLIC binder)
target_link_libraries(${TARGET} PUBLIC pthread)
target_link_libraries(${TARGET} PUBLIC subtitle)

set(CLIENT_PUBLIC_HEADERS
        SubtitleAPICommon.h
        SubtitleNativeAPI.h
        SubtitleNativeAPIForC.h
        SubtitleReportAPI.h)
install(
        FILES ${CLIENT_PUBLIC_HEADERS} DESTINATION include
)

install(
        TARGETS ${TARGET}
)
