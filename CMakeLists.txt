cmake_minimum_required(VERSION 3.7.1)
project(cpp_streamer)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wno-deprecated -Wno-deprecated-declarations -Wno-reorder -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")


set(CMAKE_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(BUILD_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(PREFIX_DIR "${BUILD_OUTPUT_BASE}")
set(INSTALL_RPATH "${PREFIX_DIR}/lib")

# set output static libary
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/lib)

# set pkgconfig path
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/output/lib/pkgconfig")

# set include path
include_directories(${CMAKE_BINARY_DIR}/output/include
                    ${CMAKE_BINARY_DIR}/output/include/srtp2
                    ${CMAKE_BINARY_DIR}/output/include/openssl
                    src
                    src/format
                    src/format/flv
                    src/format/mpegts
                    src/format/amf
                    src/format/sdp
                    src/net
                    src/net/rtprtcp
                    src/net/http
                    src/net/tcp
                    src/net/udp
                    src/net/stun
                    src/net/rtmp
                    src/net/webrtc
                    src/utils
                    src/utils/av
                    3rdparty/libuv/include
                    /usr/local/include)

# set lib path
link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)

add_subdirectory(3rdparty)

################################################################
## flvdemux streamer module
add_library(flvdemux SHARED
            ./src/format/audio_header.cpp
            ./src/format/h264_h265_header.cpp
            ./src/format/flv/flv_demux.cpp)
IF (APPLE)
target_link_libraries(flvdemux pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(flvdemux pthread rt dl z m)
ENDIF ()

################################################################
## flvmux streamer module
add_library(flvmux SHARED
            ./src/format/flv/flv_mux.cpp)
IF (APPLE)
target_link_libraries(flvmux pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(flvmux pthread rt dl z m)
ENDIF ()

################################################################
## mpegtsmux streamer module
add_library(mpegtsmux SHARED
            ./src/format/audio_header.cpp
            ./src/format/h264_h265_header.cpp
            ./src/format/mpegts/mpegts_mux.cpp)
IF (APPLE)
target_link_libraries(mpegtsmux pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(mpegtsmux pthread rt dl z m)
ENDIF ()

################################################################
## mpegtsdemux streamer module
add_library(mpegtsdemux SHARED
            ./src/format/audio_header.cpp
            ./src/format/h264_h265_header.cpp
            ./src/format/opus_header.cpp
            ./src/format/mpegts/mpegts_demux.cpp)
IF (APPLE)
target_link_libraries(mpegtsdemux pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(mpegtsdemux pthread rt dl z m)
ENDIF ()

################################################################
## rtmpplay streamer module
add_library(rtmpplay SHARED
            ./src/net/rtmp/chunk_stream.cpp
            ./src/net/rtmp/rtmp_client_session.cpp
            ./src/net/rtmp/rtmp_control_handler.cpp
            ./src/net/rtmp/rtmp_handshake.cpp
            ./src/net/rtmp/rtmp_play.cpp
            ./src/net/rtmp/rtmp_session_base.cpp)
add_dependencies(rtmpplay openssl)

IF (APPLE)
target_link_libraries(rtmpplay pthread dl z m ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(rtmpplay pthread rt dl z m ssl crypto uv)
ENDIF ()

################################################################
## rtmppublish streamer module
add_library(rtmppublish SHARED
            ./src/format/h264_h265_header.cpp
            ./src/net/rtmp/chunk_stream.cpp
            ./src/net/rtmp/rtmp_client_session.cpp
            ./src/net/rtmp/rtmp_control_handler.cpp
            ./src/net/rtmp/rtmp_handshake.cpp
            ./src/net/rtmp/rtmp_publish.cpp
            ./src/net/rtmp/rtmp_session_base.cpp)
add_dependencies(rtmppublish openssl)

IF (APPLE)
target_link_libraries(rtmppublish pthread dl z m ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(rtmppublish pthread rt dl z m ssl crypto uv)
ENDIF ()

################################################################
## whip streamer module
add_library(whip SHARED
            ./src/net/webrtc/whip.cpp
            ./src/net/webrtc/peerconnection.cpp
            ./src/net/webrtc/srtp_session.cpp
            ./src/net/webrtc/dtls.cpp
            ./src/net/webrtc/rtc_send_stream.cpp
            ./src/net/http/http_client.cpp
            ./src/net/rtprtcp/rtp_packet.cpp
            ./src/net/rtprtcp/rtp_h264_pack.cpp
            ./src/format/sdp/sdp.cpp
            ./src/format/opus_header.cpp
            ./src/net/stun/stun.cpp
            ./src/utils/byte_crypto.cpp)

add_dependencies(whip openssl libsrtp)

IF (APPLE)
target_link_libraries(whip pthread dl z m srtp2 ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(whip pthread rt dl z m srtp2 ssl crypto uv)
ENDIF ()

################################################################
## mspush streamer module
add_library(mspush SHARED
            ./src/net/webrtc/mediasoup_broadcaster.cpp
            ./src/net/webrtc/peerconnection.cpp
            ./src/net/webrtc/srtp_session.cpp
            ./src/net/webrtc/dtls.cpp
            ./src/net/webrtc/rtc_send_stream.cpp
            ./src/net/http/http_client.cpp
            ./src/net/rtprtcp/rtp_packet.cpp
            ./src/net/rtprtcp/rtp_h264_pack.cpp
            ./src/format/sdp/sdp.cpp
            ./src/format/opus_header.cpp
            ./src/net/stun/stun.cpp
            ./src/utils/byte_crypto.cpp)

add_dependencies(mspush openssl libsrtp)

IF (APPLE)
target_link_libraries(mspush pthread dl z m srtp2 ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(mspush pthread rt dl z m srtp2 ssl crypto uv)
ENDIF ()

################################################################
## example: flvdump
## read a flv file --> flvdemux streamer --> dump flv information
add_executable(flvdump
            src/cpp_streamer_factory.cpp
            src/tools/flvdump.cpp)
add_dependencies(flvdump flvdemux)
IF (APPLE)
target_link_libraries(flvdump pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(flvdump pthread rt dl z m)
ENDIF ()

################################################################
## example: mpegtsdump
## read a mpegts file --> mpegtsdemux streamer --> dump mpegts information
add_executable(mpegtsdump
            src/cpp_streamer_factory.cpp
            src/tools/mpegts_dump.cpp)
add_dependencies(mpegtsdump mpegtsdemux)
IF (APPLE)
target_link_libraries(mpegtsdump pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(mpegtsdump pthread rt dl z m)
ENDIF ()


################################################################
## example: flv2flv
## read a flv file --> flvdemux streamer --> flvmux streamer --> write a flv file
add_executable(flv2flv_streamer
            src/cpp_streamer_factory.cpp
            src/tools/flv2flv_streamer.cpp)
add_dependencies(flv2flv_streamer flvdemux flvmux)
IF (APPLE)
target_link_libraries(flv2flv_streamer pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(flv2flv_streamer pthread rt dl z m)
ENDIF ()

################################################################
## example: flv2ts
## read a flv file --> flvdemux streamer --> mpegtsmux streamer --> write a mpegts file
add_executable(flv2ts_streamer
            src/cpp_streamer_factory.cpp
            src/tools/flv2mpegts_streamer.cpp)
add_dependencies(flv2ts_streamer flvdemux mpegtsmux)
IF (APPLE)
target_link_libraries(flv2ts_streamer pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(flv2ts_streamer pthread rt dl z m)
ENDIF ()

################################################################
## example: ts2flv
## read a ts file --> mpegtsdemux streamer --> flvmux streamer --> write a flv file
add_executable(ts2flv_streamer
            src/cpp_streamer_factory.cpp
            src/tools/mpegts2flv_streamer.cpp)
add_dependencies(ts2flv_streamer flvmux mpegtsdemux)
IF (APPLE)
target_link_libraries(ts2flv_streamer pthread dl z m)
ELSEIF (UNIX)
target_link_libraries(ts2flv_streamer pthread rt dl z m)
ENDIF ()

################################################################
## example: rtmp play a url to flv file 
## rtmp pull a stream from a rtmp url --> rtmp play --> flvmux streamer --> write a flv file
add_executable(rtmpplay2flv_streamer
            src/cpp_streamer_factory.cpp
            src/tools/rtmpplay2flv_streamer.cpp)
add_dependencies(rtmpplay2flv_streamer flvmux rtmpplay)

IF (APPLE)
target_link_libraries(rtmpplay2flv_streamer pthread dl z m ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(rtmpplay2flv_streamer pthread rt dl z m ssl crypto uv)
ENDIF()

################################################################
## example: read a flv file and publish to rtmp server
## read a flv file --> rtmp publish --> rtmp server
add_executable(flv2rtmppublish_streamer
            src/cpp_streamer_factory.cpp
            src/tools/flv2rtmppublish_streamer.cpp)
add_dependencies(flv2rtmppublish_streamer flvdemux rtmppublish)

IF (APPLE)
target_link_libraries(flv2rtmppublish_streamer pthread dl z m ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(flv2rtmppublish_streamer pthread rt dl z m ssl crypto uv)
ENDIF()

################################################################
# example: http client demo while don't use streamer module
add_executable(http_client_demo
            src/net/http/http_client_demo.cpp
            src/net/http/http_client.cpp)
add_dependencies(http_client_demo openssl uv)
IF (APPLE)
target_link_libraries(http_client_demo dl z m ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(http_client_demo rt dl z m ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: mspush client demo
add_executable(mediasoup_push_demo
            src/cpp_streamer_factory.cpp
            src/tools/mediasoup_push_demo.cpp)
add_dependencies(mediasoup_push_demo mspush openssl libsrtp)

IF (APPLE)
target_link_libraries(mediasoup_push_demo dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(mediasoup_push_demo rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()


###############################################################
# example: whip client demo
add_executable(whip_demo
            src/cpp_streamer_factory.cpp
            src/tools/whip_demo.cpp)
add_dependencies(whip_demo whip openssl libsrtp)

IF (APPLE)
target_link_libraries(whip_demo dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(whip_demo rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: whip bench
add_executable(whip_bench
            src/cpp_streamer_factory.cpp
            src/tools/whip_bench.cpp)
add_dependencies(whip_bench whip openssl libsrtp)

IF (APPLE)
target_link_libraries(whip_bench dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(whip_bench rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: mediasoup push bench
add_executable(mediasoup_push_bench
            src/cpp_streamer_factory.cpp
            src/tools/mediasoup_push_bench.cpp)
add_dependencies(mediasoup_push_bench mspush openssl libsrtp)

IF (APPLE)
target_link_libraries(mediasoup_push_bench dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(mediasoup_push_bench rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()


################################################################
# example: udp client demo
add_executable(udp_client_demo
            src/net/udp/udp_client_echo.cpp)
add_dependencies(udp_client_demo uv)
IF (APPLE)
target_link_libraries(udp_client_demo dl z m ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(udp_client_demo rt dl z m ssl crypto pthread uv)
ENDIF ()

################################################################
# example: udp server demo
add_executable(udp_server_demo
            src/net/udp/udp_server_echo.cpp)
add_dependencies(udp_server_demo uv)
IF (APPLE)
target_link_libraries(udp_server_demo dl z m ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(udp_server_demo rt dl z m ssl crypto pthread uv)
ENDIF ()
