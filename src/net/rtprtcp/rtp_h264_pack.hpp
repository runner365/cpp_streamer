#ifndef RTP_H264_PACK_HPP
#define RTP_H264_PACK_HPP
#include "net/rtprtcp/rtp_packet.hpp"
#include "format/h264_h265_header.hpp"
#include "rtp_pack.hpp"

#include <vector>
#include <map>
#include <memory>

namespace cpp_streamer
{
// Bit masks for FU (A and B) indicators.
enum NalDefs : uint8_t
{
    kFBit     = 0x80,
    kNriMask  = 0x60,
    kTypeMask = 0x1F
};
// Bit masks for FU (A and B) headers.
enum FuDefs : uint8_t
{
    kSBit = 0x80,
    kEBit = 0x40,
    kRBit = 0x20
};

#define kNalHeaderSize   1
#define kFuAHeaderSize   2
#define kLengthFieldSize 2
#define kStapAHeaderSize 3
#define kPayloadMaxSize  1200

// The size of a full NALU start sequence {0 0 0 1}, used for the first NALU
// of an access unit, and for SPS and PPS blocks.
static const size_t kNaluLongStartSequenceSize = 4;

// The size of a shortened NALU start sequence {0 0 1}, that may be used if
// not the first NALU of an access unit or an SPS or PPS block.
static const size_t kNaluShortStartSequenceSize = 3;

// The size of the NALU type byte (1).
static const size_t kNaluTypeSize = 1;

RtpPacket* GenerateStapAPackets(std::vector<std::pair<unsigned char*, int>> NalUVec, HeaderExtension* ext = nullptr);

std::vector<RtpPacket*> GenerateFuAPackets(uint8_t* data, size_t len, HeaderExtension* ext = nullptr);

std::vector<int> SplitNalu(int payload_len);

}

#endif
