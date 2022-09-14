cmake_minimum_required(VERSION 2.8)

project(subtitleclient)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -pthread")

include_directories(
        include
        ../libsubtitle
        ../libsubtitle/include
        ../libsubtitle/ipc/
        ../libsubtitle/io
        ../libsubtitle/subtitle
        ../libsubtitle/utils
        ../libsubtitle/subtitle/parser
        ../libsubtitle/subtitle/parser/dvb/include/am_adp
        ../libsubtitle/subtitle/parser/dvb/include/am_mw
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

target_link_libraries(${TARGET} PUBLIC binder)
target_link_libraries(${TARGET} PUBLIC log)
target_link_libraries(${TARGET} PUBLIC pthread)
target_link_libraries(${TARGET} PUBLIC subtitle)

set(CLIENT_PUBLIC_HEADERS
        SubtitleAPICommon.h
        SubtitleNativeAPI.h
        SubtitleReportAPI.h)
install(
        FILES ${CLIENT_PUBLIC_HEADERS} DESTINATION include
)

install(
        TARGETS ${TARGET}
)
