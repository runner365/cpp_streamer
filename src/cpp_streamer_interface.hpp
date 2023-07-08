#ifndef CPP_STREAMER_INTERFACE_H
#define CPP_STREAMER_INTERFACE_H
#include "media_packet.hpp"
#include "logger.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <map>

namespace cpp_streamer
{

class StreamerReport
{
public:
    StreamerReport() = default;
    virtual ~StreamerReport() = default;

public:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) = 0;
};

class CppStreamerInterface
{
public:
    CppStreamerInterface() = default;
    virtual ~CppStreamerInterface() = default;

public:
    virtual std::string StreamerName() = 0;
    virtual void SetLogger(Logger* logger) = 0;
    virtual int AddSinker(CppStreamerInterface* sinker) = 0;
    virtual int RemoveSinker(const std::string& name) = 0;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) = 0;
    virtual void StartNetwork(const std::string& url, void* loop_handle) = 0;
    virtual void AddOption(const std::string& key, const std::string& value) = 0;
    virtual void SetReporter(StreamerReport* reporter) = 0;

protected:
    Logger* logger_ = nullptr;
    std::string name_;
    std::map<std::string, CppStreamerInterface*> sinkers_;
    std::map<std::string, std::string> options_;
    StreamerReport* report_ = nullptr;
};


}

#endif
