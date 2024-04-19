cmake_minimum_required(VERSION 2.8)

project(subtitle)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17 -pthread")

include_directories(subtitle/render/textrender)
set(textrender_SRCS
        subtitle/render/textrender/draw_context.cpp
        subtitle/render/textrender/pen.cpp
        subtitle/render/textrender/shape.cpp
        subtitle/render/textrender/surface.cpp
        subtitle/render/textrender/text.cpp
        subtitle/render/textrender/rectangle.cpp
        subtitle/render/textrender/round_rectangle.cpp
        )

include_directories(
        subtitle
        subtitle/parser
        subtitle/render
        ipc
        io
        utils
        include
        display
        subtitle/parser/ExtParser
        port
        subtitle
        ../../zvbi/src
        subtitle/render/waylandrender
        subtitle/render/directfbrender
)

set(subtitle_common_srcs
        SubtitleService.cpp
        SubtitleServer.cpp
        ipc/SocketServer.cpp
        ipc/IpcBuffer.cpp
        ipc/IpcSocket.cpp
        ipc/FmqReceiver.cpp

        utils/AmlogicEvent.c
        utils/AmlogicTime.c
        utils/AmlogicUtil.c
        utils/AndroidCallbackMessageQueue.cpp
        utils/bprint.c
        utils/cJSON.c
        utils/CCJsonParser.cpp
        utils/ClosedCaptionJson.c
        utils/MemoryLeakTrackUtil.cpp
        utils/ringbuffer.c
        utils/Segment.cpp
        utils/SubtitleSignalHandler.cpp
        utils/VideoInfo.cpp
        utils/Watchdog.cpp

        io/DataSourceFactory.cpp
        io/Demux.c
        io/DemuxDriver.c
        io/DemuxSource.cpp
        io/DeviceSource.cpp
        io/ExternalDataSource.cpp
        io/FileSource.cpp
        io/SocketSource.cpp
        io/Userdata.c
        io/UserdataDriver.c
        io/VbiSource.cpp

        subtitle/Subtitle.cpp
        subtitle/Presentation.cpp
        subtitle/parser/ParserFactory.cpp
        subtitle/parser/AribB24Parser.cpp
        subtitle/parser/AssParser.cpp
        subtitle/parser/ClosedCaption.c
        subtitle/parser/ClosedCaptionParser.cpp
        subtitle/parser/DvbParser.cpp
        subtitle/parser/DvdParser.cpp
        subtitle/parser/PgsParser.cpp
        subtitle/parser/TeletextParser.cpp
        subtitle/parser/TtmlParser.cpp
        subtitle/parser/SmpteTtmlParser.cpp
        subtitle/parser/Scte27Parser.cpp
        subtitle/parser/UserDataAfd.cpp

        subtitle/parser/ExtParser/Aqtitle.cpp
        subtitle/parser/ExtParser/ExtParser.cpp
        subtitle/parser/ExtParser/ExtSubFactory.cpp
        subtitle/parser/ExtParser/ExtSubStreamReader.cpp
        subtitle/parser/ExtParser/Jacosub.cpp
        subtitle/parser/ExtParser/Lyrics.cpp
        subtitle/parser/ExtParser/Mircodvd.cpp
        subtitle/parser/ExtParser/Mplayer1.cpp
        subtitle/parser/ExtParser/Mplayer2.cpp
        subtitle/parser/ExtParser/Mpsub.cpp
        subtitle/parser/ExtParser/Pjs.cpp
        subtitle/parser/ExtParser/RealText.cpp
        subtitle/parser/ExtParser/Sami.cpp
        subtitle/parser/ExtParser/Subrip09.cpp
        subtitle/parser/ExtParser/Subrip.cpp
        subtitle/parser/ExtParser/SubStationAlpha.cpp
        subtitle/parser/ExtParser/SubViewer2.cpp
        subtitle/parser/ExtParser/SubViewer3.cpp
        subtitle/parser/ExtParser/SubViewer.cpp
        subtitle/parser/ExtParser/TextSubtitle.cpp
        subtitle/parser/ExtParser/Ttml.cpp
        subtitle/parser/ExtParser/VobSubIndex.cpp
        subtitle/parser/ExtParser/Vplayer.cpp
        subtitle/parser/ExtParser/WebVtt.cpp
        subtitle/parser/ExtParser/XmlSubtitle.cpp

        subtitle/render/AndroidHidlRemoteRender.cpp
        )

set(subtitle_srcs ${subtitle_common_srcs})

if (USE_WAYLAND)
    list(APPEND subtitle_srcs
            subtitle/render/waylandrender/WLRender.cpp
            subtitle/render/waylandrender/WLGLDevice.cpp
            )
endif()

if (USE_DFB)
    list(APPEND subtitle_srcs
            subtitle/render/directfbrender/DFBDevice.cpp
            subtitle/render/directfbrender/DFBRender.cpp
            )
endif()

if (USE_FB)
    list(APPEND subtitle_srcs
            subtitle/render/framebufferrender/FBDevice.cpp
            subtitle/render/framebufferrender/FBRender.cpp
            )
endif()


set(TARGET subtitle)
add_library(${TARGET} SHARED ${subtitle_srcs} ${textrender_SRCS})

if (MEDIASYNC_FOR_SUBTITLE)
    add_definitions(-DMEDIASYNC_FOR_SUBTITLE) # Macro definition SEE_FOR_SUBTITLE
endif()

if (USE_WAYLAND)
    add_definitions(-DUSE_WAYLAND) # Macro definition USE_SEE
endif()

if (USE_DFB)
    add_definitions(-DUSE_DFB) # Macro definition USE_SEE
endif()

if (USE_FB)
    add_definitions(-DUSE_FB) # Macro definition USE_SEE
endif()

if (NOT NDEBUG)
    add_definitions(-DNEED_SUBTITLE_DEBUG_INFORMATION)
    add_definitions(-DNEED_SUBTITLE_DEBUG_ERROR)
    target_link_libraries(${TARGET} PUBLIC log)
endif()

#Organize compilation macro control
add_definitions(-DNEED_MIN_DVB)
add_definitions(-DNEED_ARIBCC_STYLE)
add_definitions(-DNEED_TELETEXT_GRAPHICS_SUBTITLE_PAGENUMBER_BLACKGROUND)
#add_definitions(-DNEED_TELETEXT_CACHE_ZVBI_STATUS)
#add_definitions(-DREMOVE_TELETEXT_SEARCH_FOOTER)    #for zapper 4k
#add_definitions(-DNEED_TELETEXT_SUPPORT_LOAD_ANIMATION)    #teletext graphics support load animation
#add_definitions(-DNEED_TELETEXT_UNREMOVED_MIX_VIDEO_MODE_FOUR_COLOR_KEY_BACKGROUND)
#add_definitions(-DNEED_TELETEXT_SUPPORT_GRAPHICS_MODE_SUBTITLE_PAGE_FULL_SCREEN)

#Linux platform currently does not support watchdog function
#add_definitions(-DNEED_WATCHDOG)


target_link_libraries(${TARGET} PUBLIC binder)
target_link_libraries(${TARGET} PUBLIC zvbi)
target_link_libraries(${TARGET} PUBLIC pthread)
target_link_libraries(${TARGET} PUBLIC cairo)
target_link_libraries(${TARGET} PUBLIC tinyxml2)
target_link_libraries(${TARGET} PUBLIC png)

if (USE_WAYLAND)
    target_link_libraries(${TARGET} PUBLIC wayland-client)
    target_link_libraries(${TARGET} PUBLIC wayland-egl)
    target_link_libraries(${TARGET} PUBLIC EGL)
    target_link_libraries(${TARGET} PUBLIC GLESv1_CM)
endif()

if (USE_DFB)
    target_link_libraries(${TARGET} PUBLIC directfb)
endif()

if (MEDIASYNC_FOR_SUBTITLE)
    target_link_libraries(${TARGET} PUBLIC mediahal_mediasync dl)
endif()

install(
        TARGETS ${TARGET}
)
