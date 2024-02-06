//
// Created by Radzivon Bartoshyk on 14/01/2024.
//

#ifndef JXLCODER_COLORSPACEPROFILE_H
#define JXLCODER_COLORSPACEPROFILE_H

#include <memory>
#include <vector>
#include "Eigen/Eigen"

using namespace std;

static const float Rec709Primaries[3][2] = {{0.640, 0.330},
                                            {0.300, 0.600},
                                            {0.150, 0.060}};

static Eigen::Matrix<float, 3, 2> getRec709Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.640, 0.330, 0.300, 0.600, 0.150, 0.060;
    return xf;
}

static const float Rec2020Primaries[3][2] = {{0.708, 0.292},
                                             {0.170, 0.797},
                                             {0.131, 0.046}};

static Eigen::Matrix<float, 3, 2> getRec2020Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.708, 0.292, 0.170, 0.797, 0.131, 0.046;
    return xf;
}

static const float DisplayP3Primaries[3][2] = {{0.740, 0.270},
                                               {0.220, 0.780},
                                               {0.090, -0.090}};

static Eigen::Matrix<float, 3, 2> getDisplayP3Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.68, 0.32, 0.265, 0.69, 0.15, 0.06;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getDCIP3Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getBT601_525Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.630, 0.340, 0.310, 0.595, 0.155, 0.070;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getBT601_625Primaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.640, 0.330, 0.290, 0.600, 0.150, 0.060;
    return xf;
}

static Eigen::Matrix<float, 3, 2> getAdobeRGBPrimaries() {
    Eigen::Matrix<float, 3, 2> xf;
    xf << 0.6400, 0.3300, 0.2100, 0.7100, 0.1500, 0.0600;
    return xf;
}

static Eigen::Vector2f getIlluminantD65() {
    Eigen::Vector2f xf = {0.3127, 0.3290};
    return xf;
}

static Eigen::Vector2f getIlluminantDCI() {
    return {0.3140, 0.3510};
}

static const float Rec2020LumaPrimaries[3] = {0.2627f, 0.6780f, 0.0593f};
static const float Rec709LumaPrimaries[3] = {0.2126f, 0.7152f, 0.0722f};
static const float DisplayP3LumaPrimaries[3] = {0.299f, 0.587f, 0.114f};
static const float DisplayP3WhitePointNits = 80;
static const float Rec709WhitePointNits = 100;
static const float Rec2020WhitePointNits = 203;

static const float IlluminantD65[2] = {0.3127, 0.3290};

class ColorSpaceProfile {
public:
    ColorSpaceProfile(const float primaries[3][2], const float illuminant[2],
                      const float lumaCoefficients[3], const float whitePointNits) : whitePointNits(
            whitePointNits) {
        memcpy(this->primaries, primaries, sizeof(float) * 3 * 2);
        memcpy(this->illuminant, illuminant, sizeof(float) * 2);
        memcpy(this->lumaCoefficients, lumaCoefficients, sizeof(float) * 3);
    }

    float primaries[3][2];
    float illuminant[2];
    float lumaCoefficients[3];
    float whitePointNits;
};

static ColorSpaceProfile *rec2020Profile = new ColorSpaceProfile(Rec2020Primaries, IlluminantD65,
                                                                 Rec2020LumaPrimaries,
                                                                 Rec2020WhitePointNits);
static ColorSpaceProfile *rec709Profile = new ColorSpaceProfile(Rec709Primaries, IlluminantD65,
                                                                Rec709LumaPrimaries,
                                                                Rec709WhitePointNits);
static ColorSpaceProfile *displayP3Profile = new ColorSpaceProfile(DisplayP3Primaries,
                                                                   IlluminantD65,
                                                                   DisplayP3LumaPrimaries,
                                                                   DisplayP3WhitePointNits);

#endif //JXLCODER_COLORSPACEPROFILE_H
