//
// Created by Radzivon Bartoshyk on 08/01/2024.
//

#ifndef JXLCODER_JXLDEFINITIONS_H
#define JXLCODER_JXLDEFINITIONS_H

enum JxlColorMatrix {
    MATRIX_ITUR_2020_PQ,
    MATRIX_ITUR_709,
    MATRIX_ITUR_2020,
    MATRIX_DISPLAY_P3,
    MATRIX_SRGB_LINEAR,
    MATRIX_ADOBE_RGB,
    MATRIX_DCI_P3,
    MATRIX_UNKNOWN
};

enum JxlColorPixelType {
    rgb = 1,
    rgba = 2
};

enum JxlCompressionOption {
    loseless = 1,
    loosy = 2
};

enum JxlEncodingPixelDataFormat {
    UNSIGNED_8 = 1,
    BINARY_16 = 2
};

#endif //JXLCODER_JXLDEFINITIONS_H
