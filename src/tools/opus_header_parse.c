#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	char magic[9]; // "OpusHead" + '\0'
	uint8_t version;
	uint8_t channels;
	uint16_t preSkip;
	uint32_t inputSampleRate;
	uint16_t outputGain;
	uint8_t channelMapping;
} OpusHeaderInfo;

int parse_opus_header(const uint8_t* data, size_t size, OpusHeaderInfo* info) {
	if (size < 19) return -1;
	memcpy(info->magic, data + 1, 8); // 跳过第一个字节
	info->magic[8] = '\0';
	info->version = data[9];
	info->channels = data[10];
	info->preSkip = data[11] | (data[12] << 8);
	info->inputSampleRate = data[13] | (data[14] << 8) | (data[15] << 16) | (data[16] << 24);
	info->outputGain = data[17] | (data[18] << 8);
	info->channelMapping = data[19];
	return 0;
}

int main() {
	const uint8_t audio_data[] = {
		0x90, 0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64, 0x01, 0x02, 0x38,
		0x01, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	OpusHeaderInfo info;
	if (parse_opus_header(audio_data, sizeof(audio_data), &info) == 0) {
		printf("Magic: %s\n", info.magic);
		printf("Version: %d\n", info.version);
		printf("Channels: %d\n", info.channels);
		printf("PreSkip: %d\n", info.preSkip);
		printf("InputSampleRate: %u\n", info.inputSampleRate);
		printf("OutputGain: %d\n", info.outputGain);
		printf("ChannelMapping: %d\n", info.channelMapping);
	} else {
		printf("Parse error!\n");
	}
	return 0;
}
