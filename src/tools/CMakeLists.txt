set(CMAKE_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(BUILD_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(PREFIX_DIR "${BUILD_OUTPUT_BASE}")
set(INSTALL_RPATH "${PREFIX_DIR}/lib")

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/objs)

################################################################
## example: flvdump
## read a flv file --> flvdemux streamer --> dump flv information
add_executable(flvdump
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/flvdump.cpp)
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
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/mpegts_dump.cpp)
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
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/flv2flv_streamer.cpp)
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
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/flv2mpegts_streamer.cpp)
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
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/mpegts2flv_streamer.cpp)
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
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/rtmpplay2flv_streamer.cpp)
add_dependencies(rtmpplay2flv_streamer uv flvmux rtmpplay)

IF (APPLE)
target_link_libraries(rtmpplay2flv_streamer pthread dl z m ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(rtmpplay2flv_streamer pthread rt dl z m ssl crypto uv)
ENDIF()

################################################################
## example: read a flv file and publish to rtmp server
## read a flv file --> rtmp publish --> rtmp server
add_executable(flv2rtmppublish_streamer
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/flv2rtmppublish_streamer.cpp)
add_dependencies(flv2rtmppublish_streamer uv flvdemux rtmppublish)

IF (APPLE)
target_link_libraries(flv2rtmppublish_streamer pthread dl z m ssl crypto uv)
ELSEIF (UNIX)
target_link_libraries(flv2rtmppublish_streamer pthread rt dl z m ssl crypto uv)
ENDIF()

################################################################
# example: http client demo while don't use streamer module
add_executable(http_client_demo
            ${PROJECT_SOURCE_DIR}/src/net/http/http_client_demo.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_client.cpp)
add_dependencies(http_client_demo openssl uv)
IF (APPLE)
target_link_libraries(http_client_demo dl z m ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(http_client_demo rt dl z m ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: mspush client demo
add_executable(mediasoup_push_demo
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/mediasoup_push_demo.cpp)
add_dependencies(mediasoup_push_demo mspush uv openssl libsrtp)

IF (APPLE)
target_link_libraries(mediasoup_push_demo dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(mediasoup_push_demo rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: mspull client demo
add_executable(mediasoup_pull_demo
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/mediasoup_pull_demo.cpp)
add_dependencies(mediasoup_pull_demo mspull uv openssl libsrtp)

IF (APPLE)
target_link_libraries(mediasoup_pull_demo dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(mediasoup_pull_demo rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: mspull client bench
add_executable(mediasoup_pull_bench
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/mediasoup_pull_bench.cpp)
add_dependencies(mediasoup_pull_bench mspull uv openssl libsrtp)

IF (APPLE)
target_link_libraries(mediasoup_pull_bench dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(mediasoup_pull_bench rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: whip srs demo
add_executable(whip_srs_demo
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/whip_srs_demo.cpp)
add_dependencies(whip_srs_demo whip uv openssl libsrtp)

IF (APPLE)
target_link_libraries(whip_srs_demo dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(whip_srs_demo rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: whip srs bench
add_executable(whip_srs_bench
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/whip_srs_bench.cpp)
add_dependencies(whip_srs_bench whip uv openssl libsrtp)

IF (APPLE)
target_link_libraries(whip_srs_bench dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(whip_srs_bench rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: mediasoup push bench
add_executable(mediasoup_push_bench
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/mediasoup_push_bench.cpp)
add_dependencies(mediasoup_push_bench mspush uv openssl libsrtp)

IF (APPLE)
target_link_libraries(mediasoup_push_bench dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(mediasoup_push_bench rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

###############################################################
# example: whep srs demo
add_executable(whep_srs_demo
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_factory.cpp
            ${PROJECT_SOURCE_DIR}/src/tools/whep_srs_demo.cpp)
add_dependencies(whep_srs_demo whep uv openssl libsrtp)

IF (APPLE)
target_link_libraries(whep_srs_demo dl z m srtp2 ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(whep_srs_demo rt dl z m srtp2 ssl crypto pthread uv)
ENDIF ()

################################################################
# example: udp client demo
add_executable(udp_client_demo
            ${PROJECT_SOURCE_DIR}/src/net/udp/udp_client_echo.cpp)
add_dependencies(udp_client_demo uv openssl)
IF (APPLE)
target_link_libraries(udp_client_demo dl z m ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(udp_client_demo rt dl z m ssl crypto pthread uv)
ENDIF ()

################################################################
# example: udp server demo
add_executable(udp_server_demo
            ${PROJECT_SOURCE_DIR}/src/net/udp/udp_server_echo.cpp)
add_dependencies(udp_server_demo uv openssl)
IF (APPLE)
target_link_libraries(udp_server_demo dl z m ssl crypto pthread uv)
ELSEIF (UNIX)
target_link_libraries(udp_server_demo rt dl z m ssl crypto pthread uv)
ENDIF ()
