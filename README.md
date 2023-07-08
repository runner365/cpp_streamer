# cpp_streamer
cpp streamer是基于C++11开发的音视频组件，可以理解成C++版本gstreamer，使用者可以把组件串联起来实现自己的流媒体功能。

支持多种媒体格式，流媒体直播/rtc协议。

当前支持媒体格式与流媒体格式:
* flv mux/demux
* mpegts mux/demux
* rtmp publish/play
* srs whip
* srs whip bench(srs webrtc性能压测)
* mediasoup whip(mediaoup webrtc 性能压测)

## cpp streamer使用简介
cpp streamer是音视频组件，提供串流方式开发模式，可以理解成gstreamer的C++版本。

举例：flv文件转换成mpegts的实现，实现如下图

![cpp_stream flv2mpegts](doc/imgs/flv2mpegts.png)

* 先读取flv文件
* 使用flv demux组件：source接口导入文件二进制流，解析后，通过sinker接口输出视频+音频的媒体流；
* 使用mpegts mux组件: source接口导入上游解析后的媒体流后，组件内部进行mpegts的封装，再通过sinker接口输出mpegts格式；
* 通过mpegts mux组件的sinker接口组件输出，写文件得到mpegts文件；
