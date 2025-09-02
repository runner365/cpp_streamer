#ifndef WAV_PUB_HPP
#define WAV_PUB_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <sstream>

namespace cpp_streamer
{
// 定义RIFF头结构
typedef struct {
    char chunkId[4];        // "RIFF"
    uint32_t chunkSize;    // 文件大小 - 8
    char format[4];        // "WAVE"
} RIFFHeader;

typedef struct {
    char subchunkId[4];
    uint32_t subchunkSize;
} ChunkHeader;

// 定义格式块结构
typedef struct {
    // char subchunk1Id[4];    // "fmt "
    // uint32_t subchunk1Size; // 通常为16（PCM格式）
    uint16_t audioFormat;   // 编码格式，1表示PCM
    uint16_t numChannels;   // 声道数
    uint32_t sampleRate;    // 采样率
    uint32_t byteRate;      // 每秒字节数
    uint16_t blockAlign;    // 数据块对齐
    uint16_t bitsPerSample; // 位深度
} FormatChunk;

class WavDataCallbackI
{
public:
    virtual void OnWavPacketCallBack(const uint8_t* data, size_t len, const FormatChunk* format) = 0;
};

inline std::string WavFormatChunkDump(const FormatChunk* format) {
    std::ostringstream oss;
    oss << "audioFormat:" << format->audioFormat << ", \r\n"
        << "numChannels:" << format->numChannels << ", \r\n"
        << "sampleRate:" << format->sampleRate << ", \r\n"
        << "byteRate:" << format->byteRate << ", \r\n"
        << "blockAlign:" << format->blockAlign << ", \r\n"
        << "bitsPerSample:" << format->bitsPerSample;
    return oss.str();
}
}
#endif
