//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#ifndef JXLCODER_JXLDECODING_H
#define JXLCODER_JXLDECODING_H

#include <vector>

bool DecodeJpegXlOneShot(const uint8_t *jxl, size_t size,
                         std::vector<uint8_t> *pixels, size_t *xsize,
                         size_t *ysize, std::vector<uint8_t> *icc_profile,
                         bool *useFloats,
                         bool *alphaPremultiplied);
bool DecodeBasicInfo(const uint8_t *jxl, size_t size,
                     std::vector<uint8_t> *pixels, size_t *xsize,
                     size_t *ysize);

#endif //JXLCODER_JXLDECODING_H
