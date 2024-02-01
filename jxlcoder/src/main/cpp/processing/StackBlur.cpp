//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "StackBlur.h"
#include <memory>
#include <vector>

using namespace std;

#define clamp(a, min, max) \
    ({__typeof__ (a) _a__ = (a); \
      __typeof__ (min) _min__ = (min); \
      __typeof__ (max) _max__ = (max); \
      _a__ < _min__ ? _min__ : _a__ > _max__ ? _max__ : _a__; })

template<class T>
inline __attribute__((flatten))
int nclamp(const T value, const T minValue, const T maxValue) {
    return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
}

void stackBlurU8(uint8_t *data, int width, int height, int radius) {
    int wm = width - 1;
    int hm = height - 1;
    int wh = width * height;
    int div = radius + radius + 1;

    vector<uint8_t> r(wh);
    vector<uint8_t> g(wh);
    vector<uint8_t> b(wh);
    int rsum = 0, gsum = 0, bsum = 0, x = 0, y = 0, i = 0, p = 0, yp = 0, yi = 0, yw = 0;
    vector<uint8_t> vmin(min(width, height));

    int divsum = (div + 1) >> 1;
    divsum *= divsum;
    vector<uint8_t> dv(256 * divsum);
    i = 0;
    while (i < 256 * divsum) {
        dv[i] = i / divsum;
        i++;
    }

    vector<vector<uint8_t>> stack(div, vector<uint8_t>(3));
    int stackpointer = 0, stackstart = 0;
    vector<uint8_t> sir;
    int r1 = radius + 1;
    int routsum = 0, goutsum = 0, boutsum = 0, rinsum = 0, ginsum = 0, binsum = 0, rbs = 0;

    y = 0;
    while (y < height) {
        bsum = 0;
        gsum = 0;
        rsum = 0;
        boutsum = 0;
        goutsum = 0;
        routsum = 0;
        binsum = 0;
        ginsum = 0;
        rinsum = 0;
        i = -radius;
        while (i <= radius) {
            p = data[clamp(yi, 0, hm * width) + clamp(i, 0, wm)];
            sir = stack[clamp(i + radius, 0, stack.size() - 1)];
            sir[0] = p & 0xff0000 >> 16;
            sir[1] = p & 0x00ff00 >> 8;
            sir[2] = p & 0x0000ff;
            rbs = r1 - abs(i);
            rsum += sir[0] * rbs;
            gsum += sir[1] * rbs;
            bsum += sir[2] * rbs;
            if (i > 0) {
                rinsum += sir[0];
                ginsum += sir[1];
                binsum += sir[2];
            } else {
                routsum += sir[0];
                goutsum += sir[1];
                boutsum += sir[2];
            }
            i++;
        }
        stackpointer = radius;
        x = 0;
        while (x < width) {
            r[yi] = dv[clamp(rsum, 0, dv.size() - 1)];
            g[yi] = dv[clamp(gsum, 0, dv.size() - 1)];
            b[yi] = dv[clamp(bsum, 0, dv.size() - 1)];
            rsum -= routsum;
            gsum -= goutsum;
            bsum -= boutsum;
            stackstart = stackpointer - radius + div;
            sir = stack[clamp(stackstart % div, 0, stack.size() - 1)];
            routsum -= sir[0];
            goutsum -= sir[1];
            boutsum -= sir[2];
            if (y == 0) {
                vmin[x] = min(x + radius + 1, wm);
            }
            int max = vmin.size() - 1;
            int pp = vmin[nclamp(x, 0, max)];
            p = data[clamp(yw, 0, hm) + clamp(pp, 0, wm)];
            sir[0] = p & 0xff0000 >> 16;
            sir[1] = p & 0x00ff00 >> 8;
            sir[2] = p & 0x0000ff;
            rinsum += sir[0];
            ginsum += sir[1];
            binsum += sir[2];
            rsum += rinsum;
            gsum += ginsum;
            bsum += binsum;
            stackpointer = (stackpointer + 1) % div;
            sir = stack[clamp(stackpointer % div, 0, stack.size() - 1)];
            routsum += sir[0];
            goutsum += sir[1];
            boutsum += sir[2];
            rinsum -= sir[0];
            ginsum -= sir[1];
            binsum -= sir[2];
            yi++;
            x++;
        }
        yw += width;
        y++;
    }
    x = 0;
    while (x < width) {
        bsum = 0;
        gsum = 0;
        rsum = 0;
        boutsum = 0;
        goutsum = 0;
        routsum = 0;
        binsum = 0;
        ginsum = 0;
        rinsum = 0;
        yp = -radius * width;
        i = -radius;
        while (i <= radius) {
            yi = max(yp, 0) + x;
            sir = stack[i + radius];
            sir[0] = r[clamp(yi, 0, r.size() - 1)];
            sir[1] = g[clamp(yi, 0, g.size() - 1)];
            sir[2] = b[clamp(yi, 0, b.size() - 1)];
            rbs = r1 - abs(i);
            rsum += r[clamp(yi, 0, r.size() - 1)] * rbs;
            gsum += g[clamp(yi, 0, g.size() - 1)] * rbs;
            bsum += b[clamp(yi, 0, b.size() - 1)] * rbs;
            if (i > 0) {
                rinsum += sir[0];
                ginsum += sir[1];
                binsum += sir[2];
            } else {
                routsum += sir[0];
                goutsum += sir[1];
                boutsum += sir[2];
            }
            if (i < hm) {
                yp += width;
            }
            i++;
        }
        yi = x;
        stackpointer = radius;
        y = 0;
        while (y < height) {

            // Preserve alpha channel: ( 0xff000000 & pix[yi] )
            data[clamp(yi, 0, hm * width)] =
                    -0x1000000 & data[clamp(yi, 0, hm * width)] | (dv[rsum] << 16) |
                    (dv[gsum] << 8) | dv[bsum];
            rsum -= routsum;
            gsum -= goutsum;
            bsum -= boutsum;
            stackstart = stackpointer - radius + div;
            sir = stack[clamp(stackstart % div, 0, stack.size() - 1)];
            routsum -= sir[0];
            goutsum -= sir[1];
            boutsum -= sir[2];
            if (x == 0) {
                vmin[y] = min(y + r1, hm) * width;
            }
            p = x + vmin[clamp(y, 0, vmin.size() - 1)];
            sir[0] = r[clamp(p, 0, r.size() - 1)];
            sir[1] = g[clamp(p, 0, g.size() - 1)];
            sir[2] = b[clamp(p, 0, b.size() - 1)];
            rinsum += sir[0];
            ginsum += sir[1];
            binsum += sir[2];
            rsum += rinsum;
            gsum += ginsum;
            bsum += binsum;
            stackpointer = (stackpointer + 1) % div;
            sir = stack[clamp(stackpointer, 0, stack.size() - 1)];
            routsum += sir[0];
            goutsum += sir[1];
            boutsum += sir[2];
            rinsum -= sir[0];
            ginsum -= sir[1];
            binsum -= sir[2];
            yi += width;
            y++;
        }
        x++;
    }
}
