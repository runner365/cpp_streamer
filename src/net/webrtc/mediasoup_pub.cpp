#include "mediasoup_pub.hpp"
#include "json.hpp"

namespace cpp_streamer
{

using json = nlohmann::json;

static void GetAudioCodec(json& audio_codec_obj) {
    audio_codec_obj["kind"]                 = "audio";
    audio_codec_obj["mimeType"]             = "audio/opus";
    audio_codec_obj["clockRate"]            = 48000;
    audio_codec_obj["channels"]             = 2;
    audio_codec_obj["preferredPayloadType"] = 111;
    audio_codec_obj["parameters"] = json::object();

    auto audio_rtcpFeedback_array = json::array();
    auto rtcpFeedback_nack = json::object();
    auto rtcpFeedback_tcc  = json::object();

    rtcpFeedback_nack["type"] = "nack";
    rtcpFeedback_nack["parameter"] = "";

    rtcpFeedback_tcc["type"] = "transport-cc";
    rtcpFeedback_tcc["parameter"] = "";
    audio_rtcpFeedback_array.push_back(rtcpFeedback_nack);
    audio_rtcpFeedback_array.push_back(rtcpFeedback_tcc);
    audio_codec_obj["rtcpFeedback"] = audio_rtcpFeedback_array;
}

static void GetH264Codec(json& video_codec_obj, const std::string& profile_level_id, int payload_type) {
    video_codec_obj["kind"]      = "video";
    video_codec_obj["mimeType"]  = "video/H264";
    video_codec_obj["clockRate"] = 90000;

    auto parameters_obj = json::object();
    parameters_obj["level-asymmetry-allowed"] = 1;
    parameters_obj["packetization-mode"]      = 1;
    parameters_obj["profile-level-id"]        = profile_level_id;
    parameters_obj["x-google-start-bitrate"]  = 1000;

    video_codec_obj["parameters"] = parameters_obj;

    auto nack_obj = json::object();
    nack_obj["type"]      = "nack";
    nack_obj["parameter"] = "";

    auto pli_obj = json::object();
    pli_obj["type"]      = "nack";
    pli_obj["parameter"] = "pli";

    auto fir_obj = json::object();
    fir_obj["type"]      = "ccm";
    fir_obj["parameter"] = "fir";

    auto remb_obj = json::object();
    remb_obj["type"]      = "goog-remb";     
    remb_obj["parameter"] = "";

    auto tcc_obj = json::object();
    tcc_obj["type"] = "transport-cc";
    tcc_obj["parameter"] = "";

    auto rtcp_fb_array = json::array();
    rtcp_fb_array.push_back(nack_obj);
    rtcp_fb_array.push_back(pli_obj);
    rtcp_fb_array.push_back(fir_obj);
    rtcp_fb_array.push_back(remb_obj);
    rtcp_fb_array.push_back(tcc_obj);
    video_codec_obj["rtcpFeedback"] = rtcp_fb_array;
    video_codec_obj["preferredPayloadType"] = payload_type;
}

static void GetRtxCodec(json& rtx_codec_obj, int payload_type) {
    rtx_codec_obj["kind"]      = "video";
    rtx_codec_obj["mimeType"]  = "video/rtx";
    rtx_codec_obj["clockRate"] = 90000;
    rtx_codec_obj["preferredPayloadType"] = payload_type;
    auto paramters_obj = json::object();
    paramters_obj["apt"] = 106;
    rtx_codec_obj["parameters"] = paramters_obj;
    rtx_codec_obj["rtcpFeedback"] = json::array();
}

static void GetCodecs(json& codecs_array) {
    auto audio_codec_obj        = json::object();
    auto baseline_codec_obj     = json::object();
    auto main_codec_obj         = json::object();
    auto baseline_rtx_codec_obj = json::object();
    auto main_rtx_codec_obj     = json::object();
 
    std::string baseline = "42e01f";
    std::string main = "4d001f";
    
    GetH264Codec(baseline_codec_obj, baseline, 106);
    GetRtxCodec(baseline_rtx_codec_obj, 107);
    GetH264Codec(main_codec_obj, main, 108);
    GetRtxCodec(main_rtx_codec_obj, 109);

    GetAudioCodec(audio_codec_obj);

    codecs_array = json::array();
    codecs_array.push_back(baseline_codec_obj);
    codecs_array.push_back(main_codec_obj);
    codecs_array.push_back(audio_codec_obj);
    codecs_array.push_back(baseline_rtx_codec_obj);
    codecs_array.push_back(main_rtx_codec_obj);
}

void GetHeaderExts(json& header_exts) {
    header_exts = json::array();
    auto audio_mid_obj = json::object();
    audio_mid_obj["kind"]             = "audio";
    audio_mid_obj["uri"]              = "urn:ietf:params:rtp-hdrext:sdes:mid";
    audio_mid_obj["preferredId"]      = 1;
    audio_mid_obj["preferredEncrypt"] = false;
    audio_mid_obj["direction"]        = "sendrecv";
    header_exts.push_back(audio_mid_obj);

    auto video_mid_obj = json::object();
    video_mid_obj["kind"]             = "audio";
    video_mid_obj["uri"]              = "urn:ietf:params:rtp-hdrext:sdes:mid";
    video_mid_obj["preferredId"]      = 1;
    video_mid_obj["preferredEncrypt"] = false;
    video_mid_obj["direction"]        = "sendrecv";
    header_exts.push_back(video_mid_obj);

    auto rtp_streamid_obj = json::object();
    rtp_streamid_obj["kind"]             = "video";
    rtp_streamid_obj["uri"]              = "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
    rtp_streamid_obj["preferredId"]      = 2;
    rtp_streamid_obj["preferredEncrypt"] = false;
    rtp_streamid_obj["direction"]        = "recvonly";
    header_exts.push_back(rtp_streamid_obj);

    auto repair_streamid_obj = json::object();
    repair_streamid_obj["kind"]             = "video";
    repair_streamid_obj["uri"]              = "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";
    repair_streamid_obj["preferredId"]      = 3;
    repair_streamid_obj["preferredEncrypt"] = false;
    repair_streamid_obj["direction"]        = "recvonly";
    header_exts.push_back(repair_streamid_obj);

    auto audio_abs_time = json::object();
    audio_abs_time["kind"]             = "audio";
    audio_abs_time["uri"]              = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
    audio_abs_time["preferredId"]      = 4;
    audio_abs_time["preferredEncrypt"] = false;
    audio_abs_time["direction"]        = "sendrecv";
    header_exts.push_back(audio_abs_time);

    auto video_abs_time = json::object();
    video_abs_time["kind"]             = "video";
    video_abs_time["uri"]              = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
    video_abs_time["preferredId"]      = 4;
    video_abs_time["preferredEncrypt"] = false;
    video_abs_time["direction"]        = "sendrecv";
    header_exts.push_back(video_abs_time);

    auto audio_twc_obj = json::object();
    audio_twc_obj["kind"]             = "audio";
    audio_twc_obj["uri"]              = "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
    audio_twc_obj["preferredId"]      = 5;
    audio_twc_obj["preferredEncrypt"] = false;
    audio_twc_obj["direction"]        = "recvonly";
    header_exts.push_back(audio_twc_obj);

    auto video_twc_obj = json::object();
    video_twc_obj["kind"]             = "video";
    video_twc_obj["uri"]              = "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
    video_twc_obj["preferredId"]      = 5;
    video_twc_obj["preferredEncrypt"] = false;
    video_twc_obj["direction"]        = "recvonly";
    header_exts.push_back(video_twc_obj);

    auto audio_level_obj = json::object();
    audio_level_obj["kind"]             = "audio";
    audio_level_obj["uri"]              = "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
    audio_level_obj["preferredId"]      = 5;
    audio_level_obj["preferredEncrypt"] = false;
    audio_level_obj["direction"]        = "recvonly";
    header_exts.push_back(audio_level_obj);
}

void GetRtpCapabilities(json& json_obj) {
    auto codecs_array = json::array();
    auto header_ext_array = json::array();

    GetCodecs(codecs_array);
    GetHeaderExts(header_ext_array);

    json_obj["codecs"] = codecs_array;
    json_obj["headerExtensions"] = header_ext_array;
    return;
}

}
