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
            p = data[yi + min(wm, max(i, 0))];
            sir = stack[i + radius];
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
            r[yi] = dv[rsum];
            g[yi] = dv[gsum];
            b[yi] = dv[bsum];
            rsum -= routsum;
            gsum -= goutsum;
            bsum -= boutsum;
            stackstart = stackpointer - radius + div;
            sir = stack[stackstart % div];
            routsum -= sir[0];
            goutsum -= sir[1];
            boutsum -= sir[2];
            if (y == 0) {
                vmin[x] = min(x + radius + 1, wm);
            }
            p = data[yw + vmin[x]];
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
            sir = stack[stackpointer % div];
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
            sir[0] = r[yi];
            sir[1] = g[yi];
            sir[2] = b[yi];
            rbs = r1 - abs(i);
            rsum += r[yi] * rbs;
            gsum += g[yi] * rbs;
            bsum += b[yi] * rbs;
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
            data[yi] = -0x1000000 & data[yi] | (dv[rsum] << 16) | (dv[gsum] << 8) | dv[bsum];
            rsum -= routsum;
            gsum -= goutsum;
            bsum -= boutsum;
            stackstart = stackpointer - radius + div;
            sir = stack[stackstart % div];
            routsum -= sir[0];
            goutsum -= sir[1];
            boutsum -= sir[2];
            if (x == 0) {
                vmin[y] = min(y + r1, hm) * width;
            }
            p = x + vmin[y];
            sir[0] = r[p];
            sir[1] = g[p];
            sir[2] = b[p];
            rinsum += sir[0];
            ginsum += sir[1];
            binsum += sir[2];
            rsum += rinsum;
            gsum += ginsum;
            bsum += binsum;
            stackpointer = (stackpointer + 1) % div;
            sir = stack[stackpointer];
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
