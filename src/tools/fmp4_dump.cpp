#include "format/mp4/mp4_box.hpp"
#include "utils/json.hpp"

#include <string>
#include <stdint.h>
#include <stddef.h>
#include <iostream>
#include <sstream>

using namespace cpp_streamer;

void ReadData(FILE* file) {
    std::vector<uint8_t> buffer;
    uint64_t seek_pos = 0;
    MovInfo mov;
    FtypBox* ftyp_box = nullptr;
    MoovBox* moov_box = nullptr;
    MoofBox* moof_box = nullptr;
    MdatBox* mdat_box = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes;

    while (true) {
        uint8_t header[16];

        fseek(file, seek_pos, 0);
        size_t read_bytes = fread(header, 1, sizeof(header), file);
        if (read_bytes < sizeof(header)) {
            break;
        }
        uint8_t* p = header;

        std::string box_type;
        int mov_offset = 0;
        uint64_t box_size = GetBoxHeaderInfo(p, box_type, mov_offset);
        
        buffer.resize(box_size);
        
        fseek(file, seek_pos, 0);
        fread(buffer.data(), 1, box_size, file);
        p = buffer.data();

        if (box_type == "ftyp") {
            ftyp_box = new FtypBox();
            p = ftyp_box->Parse(p, mov);
            std::cout << "Parsed ftyp box:" << ftyp_box->Dump() << std::endl;
        } else if (box_type == "moov") {
            moov_box = new MoovBox();
            p = moov_box->Parse(p, mov);
            std::cout << "Parsed moov box:" << moov_box->Dump() << std::endl;
        } else if (box_type == "moof") {
            moof_box = new MoofBox();
            p = moof_box->Parse(p, mov);
            std::cout << "Parsed moof box:" << moof_box->Dump() << std::endl;
        } else if (box_type == "mdat") {
            mdat_box = new MdatBox();
            p = mdat_box->Parse(p, mov);
            std::cout << "Parsed mdat box:" << mdat_box->Dump() << std::endl;
        } else {
            Mp4BoxBase* box = new Mp4BoxBase();
            p = box->Parse(p);
            unknown_boxes.push_back(box);
            std::cout << "Parsed unknown box:" << box->Dump() << std::endl;
        }
        seek_pos += box_size;
        std::cout << "Current file position: " << seek_pos << std::endl;
        std::cout << "\r\n";

        if (ftyp_box && moov_box && moof_box && mdat_box) {
            break;
        }
    }
    std::stringstream ss;
    ss << "{";
    if (ftyp_box) {
        ss << "\"ftyp\":" << ftyp_box->Dump();
    }
    if (moov_box) {
        if (ftyp_box) {
            ss << ",";
        }
        ss << "\"moov\":" << moov_box->Dump();
    }
    if (moof_box) {
        if (moov_box || ftyp_box) {
            ss << ",";
        }
        ss << "\"moof\":" << moof_box->Dump();
    }
    if (mdat_box) {
        if (ftyp_box || moov_box || moof_box) {
            ss << ",";
        }
        ss << "\"mdat\":" << mdat_box->Dump();
    }
    if (!unknown_boxes.empty()) {
        if (ftyp_box || moov_box || moof_box || mdat_box) {
            ss << ",";
        }
        ss << "\"unknown\":[";
        for (size_t i = 0; i < unknown_boxes.size(); ++i) {
            ss << unknown_boxes[i]->Dump();
            if (i < unknown_boxes.size() - 1) {
                ss << ",";
            }
        }
        ss << "]";
    }
    ss << "}";

    std::cout << ss.str() << std::endl;
}

int main(int argn, char** argv) {
    if (argn < 2) {
        std::cerr << "Usage: " << argv[0] << " <fmp4_file>" << std::endl;
        return 1;
    }

    const char* fmp4_file = argv[1];

    FILE* file_p = fopen(fmp4_file, "rb");
    if (!file_p) {
        std::cerr << "Failed to open file: " << fmp4_file << std::endl;
        return 1;
    }

    ReadData(file_p);

    fclose(file_p);
    return 0;
}