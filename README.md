# cpp_streamer
cpp streamer是基于C++11开发的音视频组件，使用者可以把组件串联起来实现自己的流媒体功能。

支持多种媒体格式，流媒体直播/rtc协议。

当前支持媒体格式与流媒体格式:
* flv mux/demux
* mpegts mux/demux
* rtmp publish/play
* srs whip
* srs whip bench(srs webrtc性能压测)
* mediasoup whip(mediaoup webrtc 性能压测)

网络开发部分，采用高性能，跨平台的libuv网络异步库；

## cpp streamer使用简介
cpp streamer是音视频组件，提供串流方式开发模式。

举例：flv文件转换成mpegts的实现，实现如下图

![cpp_stream flv2mpegts](doc/imgs/flv2mpegts.png)

* 先读取flv文件
* 使用flvdemux组件：source接口导入文件二进制流，解析后，通过sinker接口输出视频+音频的媒体流；
* 使用mpegtsmux组件: source接口导入上游解析后的媒体流后，组件内部进行mpegts的封装，再通过sinker接口输出mpegts格式；
* 通过mpegtsmux组件的sinker接口组件输出，写文件得到mpegts文件；

## cpp streamer应用实例

* [flv转mpegts](doc/flv2mpegts.md)
* [flv转rtmp推流](doc/flv2rtmp.md)
* [mpegts转whip(webrtc http ingest protocol)，向srs webrtc服务推流](doc/mpegts2whip_srs.md)
* [mpegts转whip bench压测，向srs webrtc服务推流压测](doc/mpegts2whip_srs_bench.md)
* [mpegts转mediasoup broadcaster推流压测](doc/mpegts2mediasoup_push_bench.md)

