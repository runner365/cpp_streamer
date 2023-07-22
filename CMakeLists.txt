cmake_minimum_required(VERSION 3.7.1)
project(cpp_streamer)

#set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wno-deprecated -Wno-deprecated-declarations -Wno-reorder -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wno-deprecated -Wno-deprecated-declarations -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")


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
IF (APPLE)
link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib)
ELSEIF (UNIX)
link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
ENDIF ()


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
            ./src/net/webrtc/rtc_recv_stream.cpp
            ./src/net/webrtc/jitterbuffer.cpp
            ./src/net/webrtc/nack_generator.cpp
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
            ./src/net/webrtc/ms_push.cpp
            ./src/net/webrtc/mediasoup_pub.cpp
            ./src/net/webrtc/peerconnection.cpp
            ./src/net/webrtc/srtp_session.cpp
            ./src/net/webrtc/dtls.cpp
            ./src/net/webrtc/rtc_send_stream.cpp
            ./src/net/webrtc/rtc_recv_stream.cpp
            ./src/net/webrtc/jitterbuffer.cpp
            ./src/net/webrtc/nack_generator.cpp
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
## whep streamer module
add_library(whep SHARED
            ./src/net/webrtc/whep.cpp
            ./src/net/webrtc/peerconnection.cpp
            ./src/net/webrtc/srtp_session.cpp
            ./src/net/webrtc/dtls.cpp
            ./src/net/webrtc/rtc_send_stream.cpp
            ./src/net/webrtc/rtc_recv_stream.cpp
            ./src/net/webrtc/jitterbuffer.cpp
            ./src/net/webrtc/nack_generator.cpp
            ./src/net/webrtc/pack_handle_h264.cpp
            ./src/net/http/http_client.cpp
            ./src/net/rtprtcp/rtp_packet.cpp
            ./src/net/rtprtcp/rtp_h264_pack.cpp
            ./src/format/sdp/sdp.cpp
            ./src/format/opus_header.cpp
            ./src/net/stun/stun.cpp
            ./src/utils/byte_crypto.cpp)

add_dependencies(whep openssl libsrtp)

IF (APPLE)
target_link_libraries(whep pthread dl z m srtp2 ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(whep pthread rt dl z m srtp2 ssl crypto uv)
ENDIF ()

################################################################
## mspull streamer module
add_library(mspull SHARED
            ./src/net/webrtc/ms_pull.cpp
            ./src/net/webrtc/mediasoup_pub.cpp
            ./src/net/webrtc/peerconnection.cpp
            ./src/net/webrtc/srtp_session.cpp
            ./src/net/webrtc/dtls.cpp
            ./src/net/webrtc/rtc_send_stream.cpp
            ./src/net/webrtc/rtc_recv_stream.cpp
            ./src/net/webrtc/jitterbuffer.cpp
            ./src/net/webrtc/nack_generator.cpp
            ./src/net/webrtc/pack_handle_h264.cpp
            ./src/net/http/http_client.cpp
            ./src/net/rtprtcp/rtp_packet.cpp
            ./src/net/rtprtcp/rtp_h264_pack.cpp
            ./src/format/sdp/sdp.cpp
            ./src/format/opus_header.cpp
            ./src/format/h264_h265_header.cpp
            ./src/net/stun/stun.cpp
            ./src/utils/byte_crypto.cpp)

add_dependencies(mspull openssl libsrtp)

IF (APPLE)
target_link_libraries(mspull pthread dl z m srtp2 ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(mspull pthread rt dl z m srtp2 ssl crypto uv)
ENDIF ()


add_subdirectory(src/tools)
