# flv to mpegts开发示例
## 1. 简介
cpp streamer是音视频组件，提供串流方式开发模式。


flv文件转换成mpegts的实现，使用两个组件:
* flvdemux组件
* mpegtsmux组件

实现如下图

![cpp_stream exampe1](imgs/flv2mpegts.png)

* 先读取flv文件
* 使用flvdemux组件：source接口导入文件二进制流，解析后，通过sinker接口输出视频+音频的媒体流；
* 使用mpegtsmux组件: source接口导入上游解析后的媒体流后，组件内部进行mpegts的封装，再通过sinker接口输出mpegts格式；
* 通过mpegts mux组件的sinker接口组件输出，写文件得到mpegts文件；

## 2. 代码开发实现
代码实现在: src/tools/flv2rtmppublish_streamer.cpp

### 2.1 cpp streamer组件接口简介
每个媒体组件，都采用接口类来访问，如下:
```
class CppStreamerInterface
{
public:
    virtual std::string StreamerName() = 0;
    virtual void SetLogger(Logger* logger) = 0;
    virtual int AddSinker(CppStreamerInterface* sinker) = 0;
    virtual int RemoveSinker(const std::string& name) = 0;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) = 0;
    virtual void StartNetwork(const std::string& url, void* loop_handle) = 0;
    virtual void AddOption(const std::string& key, const std::string& value) = 0;
    virtual void SetReporter(StreamerReport* reporter) = 0;
};
```
* StreamerName: 返回字符串，唯一的组件名
* SetLogger: 设置日志输出，如果不设置，组件内部不产生日志；
* AddSinker：加入组件输出的下一跳接口；
* RemoveSinker：删除组件输出的下一跳接口；
* SourceData：组件接受上一跳传来数据的接口；
* StartNetwork：如果是网络组件，如rtmp，webrtc的whip，需要输入url，开始网络协议的运行；
* AddOption：设置特定的选项；
* SetReporter：设置组件上报消息的接口，如组件内部错误信息，或网络组件传输媒体流的bitrate，帧率等信息；

# 2.2 创建组件
创建组件代码，[详细代码flv2rtmppublish_streamer.cpp](../src/tools/flv2rtmppublish_streamer.cpp)
```
class Flv2TsStreamerMgr : public CppStreamerInterface, public StreamerReport
{
    int MakeStreamers() {
        CppStreamerFactory::SetLogger(s_logger);//设置日志输出
        CppStreamerFactory::SetLibPath("./output/lib");//设置组件动态库的路径
    
        flv_demux_streamer_ = CppStreamerFactory::MakeStreamer("flvdemux");//创建flvdemux的组件
        flv_demux_streamer_->SetLogger(logger_);//设置模块日志输出
        flv_demux_streamer_->SetReporter(this);//设置消息报告
        
        ts_mux_streamer_ = CppStreamerFactory::MakeStreamer("mpegtsmux");//创建mpegtsmux的组件
        ts_mux_streamer_->SetLogger(logger_);//设置模块日志输出
        ts_mux_streamer_->SetReporter(this);//设置消息报告
        
        flv_demux_streamer_->AddSinker(ts_mux_streamer_);//flvdemux组件对象设置下游为：mpegtsmux的组件
        ts_mux_streamer_->AddSinker(this);//mpegtsmux组件设置下游(写mpegts文件)
        return 0;
    }
}
```

# 2.3 flv文件输入
文件读取:
```
    uint8_t read_data[2048];
    size_t read_n = 0;
    do {
        read_n = fread(read_data, 1, sizeof(read_data), file_p);
        if (read_n > 0) {
            streamer_mgr_ptr->InputFlvData(read_data, read_n);
        }
    } while (read_n > 0);
```
文件通过flvdemux组件的sourceData接口输入：
```
    int InputFlvData(uint8_t* data, size_t data_len) {
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();
        pkt_ptr->buffer_ptr_->AppendData((char*)data, data_len);

        flv_demux_streamer_->SourceData(pkt_ptr);//导入flv数据
        return 0;
    }
```

# 2.4 mpegts文件输出
文件输出
```
class Flv2TsStreamerMgr : public CppStreamerInterface, public StreamerReport
{
    int MakeStreamers() {
        //......
        flv_demux_streamer_->AddSinker(ts_mux_streamer_);//flvdemux组件对象设置下游为：mpegtsmux的组件
        //......
        return 0;
    }
public:
    //接受mpegmux组件的sinker输出接口数据
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override {
        FILE* file_p = fopen(filename_.c_str(), "ab+");
        if (file_p) {
            fwrite(pkt_ptr->buffer_ptr_->Data(), 1, pkt_ptr->buffer_ptr_->DataLen(), file_p);
            fclose(file_p);
        }
        return 0;
    }

}
```
