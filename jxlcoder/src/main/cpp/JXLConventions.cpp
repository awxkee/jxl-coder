#include <jni.h>

/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 08/03/2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <jni.h>
#include <string>
#include <vector>
#include "JniExceptions.h"
#include "EasyGifReader.h"
#include "interop/JxlAnimatedEncoder.hpp"
#include "pnglibconf.h"
#include "png.h"
#include <memory>
#include <setjmp.h>

std::string getGifError(EasyGifReader::Error gifError) {
  std::string errorString = "GIF cannot be read with error: ";

  switch (gifError) {

    case EasyGifReader::Error::UNKNOWN: {
      errorString += "Error is unknown";
    }
      break;
    case EasyGifReader::Error::INVALID_OPERATION: {
      errorString += "Invalid operation has occur while reading";
    }
      break;
    case EasyGifReader::Error::OPEN_FAILED: {
      errorString += "Open GIF has failed";
    }
      break;
    case EasyGifReader::Error::READ_FAILED: {
      errorString += "Read GIF has failed";
    }
      break;
    case EasyGifReader::Error::INVALID_FILENAME: {
      errorString += "Invalid GIF filename";
    }
      break;
    case EasyGifReader::Error::NOT_A_GIF_FILE: {
      errorString += "Not a GIF";
    }
      break;
    case EasyGifReader::Error::INVALID_GIF_FILE: {
      errorString += "Invalid GIF";
    }
      break;
    case EasyGifReader::Error::OUT_OF_MEMORY: {
      errorString += "Out of memory while reading GIF";
    }
      break;
    case EasyGifReader::Error::CLOSE_FAILED: {
      errorString += "GIF closing failed";
    }
      break;
    case EasyGifReader::Error::NOT_READABLE: {
      errorString += "GIF not readable";
    }
      break;
    case EasyGifReader::Error::IMAGE_DEFECT: {
      errorString += "GIF image defected";
    }
      break;
    case EasyGifReader::Error::UNEXPECTED_EOF: {
      errorString += "Unexpected EOF";
    }
      break;
  }
  return errorString;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_gif2JXLImpl(JNIEnv *env, jobject thiz, jbyteArray gifData,
                                              jint quality, jint effort, jint decodingSpeed) {
  try {
    if (effort < 0 || effort > 10) {
      throwInvalidCompressionOptionException(env);
      return 0;
    }

    if (quality < 0 || quality > 100) {
      std::string exc = "Quality must be in 0...100";
      throwException(env, exc);
      return 0;
    }
    auto totalLength = env->GetArrayLength(gifData);
    if (totalLength <= 0) {
      std::string errorString = "Invalid input array length";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(gifData, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.data()));
    EasyGifReader gifReader = EasyGifReader::openMemory(srcBuffer.data(), srcBuffer.size());
    const int frameCount = gifReader.frameCount(); // TODO: Do something with frame count
    if (frameCount <= 0) {
      std::string detail = "Image contains negative count of frames";
      std::string errorString = "GIF cannot be read with error: " + detail;
      throwException(env, errorString);
      return nullptr;
    }
    const int width = gifReader.width();
    const int height = gifReader.height();
    const int repeatCount = gifReader.repeatCount();

    JxlAnimatedEncoder encoder(width, height, rgba,
                               UNSIGNED_8,
                               lossy,
                               repeatCount,
                               quality,
                               effort, decodingSpeed);

    const size_t frameSize = width * height * sizeof(uint8_t) * 4;
    std::vector<uint8_t> mPixelStore(frameSize);

    for (const EasyGifReader::Frame &frame : gifReader) {
      const std::uint32_t *framePixels = frame.pixels();
      int frameDuration = frame.duration().milliseconds();
      const uint8_t *mSource = reinterpret_cast<const uint8_t *>(framePixels);
      std::copy(mSource, mSource + frameSize, mPixelStore.begin());
      encoder.addFrame(mPixelStore, frameDuration);
    }

    mPixelStore.resize(0);

    std::vector<uint8_t> buffer;
    encoder.encode(buffer);
    jbyteArray byteArray = env->NewByteArray((jsize) buffer.size());
    char *memBuf = reinterpret_cast<char *>(buffer.data());
    env->SetByteArrayRegion(byteArray, 0, (jint) buffer.size(),
                            reinterpret_cast<const jbyte *>(memBuf));
    buffer.clear();
    return byteArray;
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to encode this image";
    throwException(env, errorString);
    return nullptr;
  } catch (EasyGifReader::Error gifError) {
    std::string errorString = getGifError(gifError);
    throwException(env, errorString);
    return nullptr;
  }
}

void BlendOverAPNG(unsigned char **rows_dst, unsigned char **rows_src, unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
  unsigned int i, j;
  int u, v, al;

  for (j = 0; j < h; j++) {
    unsigned char *sp = rows_src[j];
    unsigned char *dp = rows_dst[j + y] + x * 4;

    for (i = 0; i < w; i++, sp += 4, dp += 4) {
      if (sp[3] == 255)
        memcpy(dp, sp, 4);
      else if (sp[3] != 0) {
        if (dp[3] != 0) {
          u = sp[3] * 255;
          v = (255 - sp[3]) * dp[3];
          al = u + v;
          dp[0] = (sp[0] * u + dp[0] * v) / al;
          dp[1] = (sp[1] * u + dp[1] * v) / al;
          dp[2] = (sp[2] * u + dp[2] * v) / al;
          dp[3] = al / 255;
        } else
          memcpy(dp, sp, 4);
      }
    }
  }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_apng2JXLImpl(JNIEnv *env,
                                               jobject thiz,
                                               jbyteArray apngData,
                                               jint quality,
                                               jint effort,
                                               jint decodingSpeed) {
  try {
    if (effort < 0 || effort > 10) {
      throwInvalidCompressionOptionException(env);
      return 0;
    }

    if (quality < 0 || quality > 100) {
      std::string exc = "Quality must be in 0...100";
      throwException(env, exc);
      return 0;
    }
    auto totalLength = env->GetArrayLength(apngData);
    if (totalLength <= 0) {
      std::string errorString = "Invalid input array length";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(apngData, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.data()));

    png_structp mPngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!mPngPtr) {
      std::string errorString = "Cannot initialize libpng";
      throwException(env, errorString);
      return nullptr;
    }

    png_infop infoPtr = nullptr;

    std::shared_ptr<png_struct> pngPtr(mPngPtr, [&](auto x) {
      png_destroy_read_struct(&x, &infoPtr, nullptr);
    });

    infoPtr = png_create_info_struct(mPngPtr);
    if (!infoPtr) {
      std::string errorString = "Cannot initialize libpng";
      throwException(env, errorString);
      return nullptr;
    }

    if (setjmp(png_jmpbuf(pngPtr.get()))) {
      std::string errorString = "Reading APNG has failed";
      throwException(env, errorString);
      return nullptr;
    }

    png_set_read_fn(pngPtr.get(), static_cast<png_voidp>(&srcBuffer), [](png_structp png, png_bytep data, png_size_t length) {
      auto &buffer = *static_cast<std::vector<uint8_t> *>(png_get_io_ptr(png));
      if (buffer.size() >= length) {
        std::copy(buffer.begin(), buffer.begin() + length, data);
        buffer.erase(buffer.begin(), buffer.begin() + length);
      }
    });

//    png_set_sig_bytes(pngPtr.get(), 8);
    png_read_info(pngPtr.get(), infoPtr);
    png_set_expand(pngPtr.get());
    png_set_strip_16(pngPtr.get());
    png_set_gray_to_rgb(pngPtr.get());
//    png_set_add_alpha(pngPtr.get(), 0xff, PNG_FILLER_AFTER);
//    png_set_bgr(pngPtr.get());
    (void) png_set_interlace_handling(pngPtr.get());
    png_read_update_info(pngPtr.get(), infoPtr);
    const int width = png_get_image_width(pngPtr.get(), infoPtr);
    const int height = png_get_image_height(pngPtr.get(), infoPtr);
    const int channels = png_get_channels(pngPtr.get(), infoPtr);
    const int rowbytes = png_get_rowbytes(pngPtr.get(), infoPtr);

    png_bytep iccpData = nullptr;
    png_uint_32 iccpLength = 0;
    png_charp iccpName = nullptr;
    int compressionType = 0;
    std::vector<uint8_t> iccProfile(0);

    if (png_get_iCCP(pngPtr.get(), infoPtr, &iccpName, &compressionType, &iccpData, &iccpLength)) {
      if (iccpLength > 0) {
        iccProfile.resize(iccpLength);
        std::copy(iccpData, iccpData + iccpLength, iccProfile.begin());
      }
    }

    const size_t frameSize = rowbytes * height;
    std::vector<uint8_t> mFrame(frameSize);
    std::vector<uint8_t> mImage(frameSize);
    std::vector<uint8_t> mTempImage(frameSize);

    png_uint_32 frames = 1;
    png_uint_32 repeatCount = 0;

    if (png_get_valid(pngPtr.get(), infoPtr, PNG_INFO_acTL))
      png_get_acTL(pngPtr.get(), infoPtr, &frames, &repeatCount);

    JxlColorPixelType colorPixelType = channels > 3 ? rgba : rgb;

    JxlAnimatedEncoder encoder(width, height, colorPixelType,
                               UNSIGNED_8,
                               lossy,
                               repeatCount,
                               quality,
                               effort, decodingSpeed);

    if (!iccProfile.empty()) {
      encoder.setICCProfile(iccProfile);
    }

    png_uint_32 w0 = width;
    png_uint_32 h0 = height;
    png_uint_32 x0 = 0;
    png_uint_32 y0 = 0;
    unsigned short delay_num = 1;
    unsigned short delay_den = 10;
    unsigned char dop = 0;
    unsigned char bop = 0;

    std::vector<png_bytep> rowsFrame(height * sizeof(png_bytep));
    std::vector<png_bytep> rowsImage(height * sizeof(png_bytep));

    for (int j = 0; j < height; j++)
      rowsFrame[j] = reinterpret_cast<uint8_t *>(mFrame.data()) + j * rowbytes;

    for (int j = 0; j < height; j++)
      rowsImage[j] = reinterpret_cast<uint8_t *>(mImage.data()) + j * rowbytes;

    for (int i = 0; i < frames; i++) {
      if (png_get_valid(pngPtr.get(), infoPtr, PNG_INFO_acTL)) {
        png_read_frame_head(pngPtr.get(), infoPtr);
        png_get_next_frame_fcTL(pngPtr.get(), infoPtr, &w0,
                                &h0, &x0,
                                &y0, &delay_num, &delay_den, &dop, &bop);
        if (i == 0) {
          bop = PNG_BLEND_OP_SOURCE;
          if (dop == PNG_DISPOSE_OP_PREVIOUS)
            dop = PNG_DISPOSE_OP_BACKGROUND;
        }
      }
      png_read_image(pngPtr.get(), rowsFrame.data());

      if (dop == PNG_DISPOSE_OP_PREVIOUS)
        std::copy(mImage.begin(), mImage.end(), mTempImage.begin());

      if (bop == PNG_BLEND_OP_OVER) {
        BlendOverAPNG(rowsImage.data(), rowsFrame.data(), x0, y0, w0, h0);
      } else {
        for (int j = 0; j < h0; j++)
          memcpy(rowsImage[j + y0] + x0 * 4, rowsFrame[j], w0 * 4);
      }

      const int frameDuration = std::roundf(static_cast<float>(delay_num) / static_cast<float>(delay_den) * 1000.f);
      encoder.addFrame(mImage, frameDuration);

      if (dop == PNG_DISPOSE_OP_PREVIOUS)
        std::copy(mTempImage.begin(), mTempImage.end(), mImage.begin());
      else if (dop == PNG_DISPOSE_OP_BACKGROUND) {
        for (int j = 0; j < h0; j++)
          memset(rowsImage[j + y0] + x0 * 4, 0, w0 * 4);
      }
    }

    png_read_end(pngPtr.get(), infoPtr);

    mTempImage.resize(0);
    mFrame.resize(0);
    mImage.resize(0);

    std::vector<uint8_t> buffer;
    encoder.encode(buffer);
    jbyteArray byteArray = env->NewByteArray((jsize) buffer.size());
    char *memBuf = reinterpret_cast<char *>(buffer.data());
    env->SetByteArrayRegion(byteArray, 0, (jint) buffer.size(), reinterpret_cast<const jbyte *>(memBuf));
    buffer.clear();
    return byteArray;
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to encode this image";
    throwException(env, errorString);
    return nullptr;
  } catch (AnimatedEncoderError &err) {
    std::string errorString = err.what();
    throwException(env, errorString);
    return nullptr;
  }
}