//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#ifndef JXLCODER_JXLENCODING_H
#define JXLCODER_JXLENCODING_H

#include <vector>

enum jxl_colorspace {
    rgb = 1,
    rgba = 2
};

enum jxl_compression_option {
    loseless = 1,
    loosy = 2
};

/**
 * Compresses the provided pixels.
 *
 * @param pixels input pixels
 * @param xsize width of the input image
 * @param ysize height of the input image
 * @param compressed will be populated with the compressed bytes
 */
bool EncodeJxlOneshot(const std::vector<uint8_t> &pixels, const uint32_t xsize,
                      const uint32_t ysize, std::vector<uint8_t> *compressed,
                      jxl_colorspace colorspace, jxl_compression_option compression_option,
                      bool useFloat16, std::vector<uint8_t> iccProfile, int effort);

#endif //JXLCODER_JXLENCODING_H
