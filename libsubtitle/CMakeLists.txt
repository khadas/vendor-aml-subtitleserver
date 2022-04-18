cmake_minimum_required(VERSION 2.8)

project(subtitle)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -pthread")

include_directories(subtitle/render/waylandrender/textrender)
set(textrender_SRCS
        subtitle/render/waylandrender/textrender/draw_context.cpp
        subtitle/render/waylandrender/textrender/pen.cpp
        subtitle/render/waylandrender/textrender/shape.cpp
        subtitle/render/waylandrender/textrender/surface.cpp
        subtitle/render/waylandrender/textrender/text.cpp
        subtitle/render/waylandrender/textrender/rectangle.cpp
        subtitle/render/waylandrender/textrender/round_rectangle.cpp
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
        subtitle/parser/dvb/include/am_adp
        subtitle/parser/dvb/include/am_mw
        subtitle/parser/dvb/android/ndk/include
        port
        subtitle
        ../../zvbi/src
        subtitle/render/waylandrender
        subtitle/render/waylandrender/wayland_inc
)

set(subtitle_srcs
        SubtitleService.cpp
        SubtitleServer.cpp
        ipc/SocketServer.cpp
        ipc/IpcBuffer.cpp
        utils/ringbuffer.c
        utils/AndroidCallbackMessageQueue.cpp
        utils/Segment.cpp
        utils/MemoryLeakTrackUtil.cpp
        utils/VideoInfo.cpp
        io/FileSource.cpp
        io/SocketSource.cpp
        io/DeviceSource.cpp
        io/DataSourceFactory.cpp
        io/ExternalDataSource.cpp
        io/DemuxSource.cpp
        io/VbiSource.cpp
        subtitle/Subtitle.cpp
        subtitle/Presentation.cpp
        subtitle/parser/bprint.c
        subtitle/parser/ParserFactory.cpp
        subtitle/parser/AssParser.cpp
        subtitle/parser/PgsParser.cpp
        subtitle/parser/DvbParser.cpp
        subtitle/parser/DvdParser.cpp
        subtitle/parser/TeletextParser.cpp
        subtitle/parser/ClosedCaptionParser.cpp
        subtitle/parser/Scte27Parser.cpp
        subtitle/parser/UserDataAfd.cpp
        subtitle/parser/ExtParser/ExtParser.cpp
        subtitle/parser/ExtParser/StreamReader.cpp
        subtitle/parser/ExtParser/ExtSubFactory.cpp
        subtitle/parser/ExtParser/TextSubtitle.cpp
        subtitle/parser/ExtParser/Aqtitle.cpp
        subtitle/parser/ExtParser/SubStationAlpha.cpp
        subtitle/parser/ExtParser/Subrip.cpp
        subtitle/parser/ExtParser/Jacosub.cpp
        subtitle/parser/ExtParser/Mircodvd.cpp
        subtitle/parser/ExtParser/Mplayer1.cpp
        subtitle/parser/ExtParser/Mplayer2.cpp
        subtitle/parser/ExtParser/Mpsub.cpp
        subtitle/parser/ExtParser/Pjs.cpp
        subtitle/parser/ExtParser/RealText.cpp
        subtitle/parser/ExtParser/Sami.cpp
        subtitle/parser/ExtParser/SubViewer.cpp
        subtitle/parser/ExtParser/Lyrics.cpp
        subtitle/parser/ExtParser/Vplayer.cpp
        subtitle/parser/ExtParser/Subrip09.cpp
        subtitle/parser/ExtParser/SubViewer2.cpp
        subtitle/parser/ExtParser/SubViewer3.cpp
        subtitle/parser/ExtParser/WebVtt.cpp
        subtitle/render/AndroidHidlRemoteRender.cpp
        subtitle/render/waylandrender/WLRender.cpp
        subtitle/render/waylandrender/WLGLDevice.cpp
        subtitle/parser/dvb/am_mw/am_cc/am_cc.c
        subtitle/parser/dvb/am_mw/am_cc/cc_json.c
        subtitle/parser/dvb/am_mw/am_scte27/am_scte27.c
        subtitle/parser/dvb/am_adp/am_time/am_time.c
        subtitle/parser/dvb/am_adp/am_userdata/am_userdata.c
        subtitle/parser/dvb/am_adp/am_userdata/aml/aml.c
        subtitle/parser/dvb/am_adp/am_evt/am_evt.c
        subtitle/parser/dvb/am_adp/am_misc/am_misc.c
        subtitle/parser/dvb/am_adp/am_misc/am_adplock.c
        subtitle/parser/dvb/am_adp/am_misc/am_sig_handler.c
        subtitle/parser/dvb/am_adp/am_dmx/am_dmx.c
        subtitle/parser/dvb/am_adp/am_dmx/linux_dvb/linux_dvb.c
        )

set(TARGET subtitle)
add_library(${TARGET} SHARED ${subtitle_srcs} ${textrender_SRCS})

target_link_libraries(${TARGET} PUBLIC cairo)
target_link_libraries(${TARGET} PUBLIC binder)
target_link_libraries(${TARGET} PUBLIC log)
target_link_libraries(${TARGET} PUBLIC zvbi)
target_link_libraries(${TARGET} PUBLIC pthread)
target_link_libraries(${TARGET} PUBLIC wayland-client)
target_link_libraries(${TARGET} PUBLIC wayland-egl)
target_link_libraries(${TARGET} PUBLIC EGL)
target_link_libraries(${TARGET} PUBLIC GLESv1_CM)
target_link_libraries(${TARGET} PUBLIC mediahal_mediasync)

install(
        TARGETS ${TARGET}
)
