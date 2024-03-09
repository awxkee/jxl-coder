
#if defined(_WIN32) && !defined(EASY_GIF_DECODER_NO_UTF8_FILENAME_SUPPORT)
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cstdlib>
#include <io.h>
#include <fcntl.h>
#include <Windows.h>
#endif

#include "EasyGifReader.h"

#include <cstring>
#include <algorithm>
#include <gif_lib.h>

using std::size_t;
using std::uint32_t;

struct EasyGifReader::Internal {
    GifFileType *gif;
    union {
        struct {
            const void *dataPtr;
            size_t remainingSize;
        };
        struct {
            size_t (*readFunction)(void *outData, size_t size, void *userPtr);
            void *userPtr;
        };
    };
    int loopCount;
    size_t pixelBufferSize;

    static Error translateErrorCode(int error);
    static FrameBounds frameBounds(GifFileType *gif, int imageIndex);
    static void clearRows(uint32_t *dst, int width, int height, size_t dstStride);
    static void copyRows(uint32_t *dst, const uint32_t *src, int width, int height, size_t dstStride, size_t srcStride);
    static int memoryRead(GifFileType *gif, GifByteType *outData, int size);
    static int customRead(GifFileType *gif, GifByteType *outData, int size);
    static bool readLoopExtension(int &loopCount, const ExtensionBlock *extensionBlocks, int extensionBlockCount);
};

struct EasyGifReader::FrameBounds {
    int x0, y0, x1, y1;
    int width() const;
    int height() const;
};

#if defined(_WIN32) && !defined(EASY_GIF_DECODER_NO_UTF8_FILENAME_SUPPORT)
static int openFileHandleUTF8Win(const char *utf8Filename, int oFlag, int pMode) {
    int utf8FilenameLen = (int) strlen(utf8Filename);
    int wideFilenameCapacity = utf8FilenameLen+1;
    LPWSTR wideFilename = nullptr;
    try {
        wideFilename = new WCHAR[wideFilenameCapacity];
    } catch (...) {
        throw EasyGifReader::Error::OUT_OF_MEMORY;
    }
    int wideFilenameLen = MultiByteToWideChar(CP_UTF8, 0, utf8Filename, utf8FilenameLen, wideFilename, wideFilenameCapacity);
    if (wideFilenameLen < 0 || wideFilenameLen >= wideFilenameCapacity) {
        delete[] wideFilename;
        throw EasyGifReader::Error::INVALID_FILENAME;
    }
    wideFilename[wideFilenameLen] = (WCHAR) 0;
    int fd = _wopen(wideFilename, oFlag, pMode);
    delete[] wideFilename;
    if (fd == -1)
        throw EasyGifReader::Error::OPEN_FAILED;
    return fd;
}
#endif

EasyGifReader::Error EasyGifReader::Internal::translateErrorCode(int error) {
    switch (error) {
        case D_GIF_ERR_OPEN_FAILED:
            return Error::OPEN_FAILED;
        case D_GIF_ERR_READ_FAILED:
            return Error::READ_FAILED;
        case D_GIF_ERR_NOT_GIF_FILE:
            return Error::NOT_A_GIF_FILE;
        case D_GIF_ERR_NO_SCRN_DSCR:
        case D_GIF_ERR_NO_IMAG_DSCR:
        case D_GIF_ERR_NO_COLOR_MAP:
        case D_GIF_ERR_WRONG_RECORD:
        case D_GIF_ERR_DATA_TOO_BIG:
            return Error::INVALID_GIF_FILE;
        case D_GIF_ERR_NOT_ENOUGH_MEM:
            return Error::OUT_OF_MEMORY;
        case D_GIF_ERR_CLOSE_FAILED:
            return Error::CLOSE_FAILED;
        case D_GIF_ERR_NOT_READABLE:
            return Error::NOT_READABLE;
        case D_GIF_ERR_IMAGE_DEFECT:
            return Error::IMAGE_DEFECT;
        case D_GIF_ERR_EOF_TOO_SOON:
            return Error::UNEXPECTED_EOF;
        default:
            return Error::UNKNOWN;
    }
}

EasyGifReader::FrameBounds EasyGifReader::Internal::frameBounds(GifFileType *gif, int imageIndex) {
    const GifImageDesc &imageDesc = gif->SavedImages[imageIndex].ImageDesc;
    return FrameBounds {
        std::max(imageDesc.Left, 0),
        std::max(imageDesc.Top, 0),
        std::min(imageDesc.Left+imageDesc.Width, gif->SWidth),
        std::min(imageDesc.Top+imageDesc.Height, gif->SHeight)
    };
}

void EasyGifReader::Internal::clearRows(uint32_t *dst, int width, int height, size_t dstStride) {
    for (int y = 0; y < height; ++y) {
        memset(dst, 0, sizeof(uint32_t)*width);
        dst += dstStride;
    }
}

void EasyGifReader::Internal::copyRows(uint32_t *dst, const uint32_t *src, int width, int height, size_t dstStride, size_t srcStride) {
    for (int y = 0; y < height; ++y) {
        memcpy(dst, src, sizeof(uint32_t)*width);
        dst += dstStride;
        src += srcStride;
    }
}

int EasyGifReader::FrameDuration::milliseconds() const {
    return 10*centiseconds;
}

double EasyGifReader::FrameDuration::seconds() const {
    return .01*centiseconds;
}

EasyGifReader::FrameDuration & EasyGifReader::FrameDuration::operator+=(FrameDuration other) {
    centiseconds += other.centiseconds;
    return *this;
}

EasyGifReader::FrameDuration & EasyGifReader::FrameDuration::operator-=(FrameDuration other) {
    centiseconds -= other.centiseconds;
    return *this;
}

EasyGifReader::FrameDuration EasyGifReader::FrameDuration::operator+(FrameDuration other) const {
    return FrameDuration { centiseconds+other.centiseconds };
}

EasyGifReader::FrameDuration EasyGifReader::FrameDuration::operator-(FrameDuration other) const {
    return FrameDuration { centiseconds-other.centiseconds };
}

bool EasyGifReader::FrameDuration::operator==(FrameDuration other) const {
    return centiseconds == other.centiseconds;
}

bool EasyGifReader::FrameDuration::operator!=(FrameDuration other) const {
    return centiseconds != other.centiseconds;
}

bool EasyGifReader::FrameDuration::operator<(FrameDuration other) const {
    return centiseconds < other.centiseconds;
}

bool EasyGifReader::FrameDuration::operator>(FrameDuration other) const {
    return centiseconds > other.centiseconds;
}

bool EasyGifReader::FrameDuration::operator<=(FrameDuration other) const {
    return centiseconds <= other.centiseconds;
}

bool EasyGifReader::FrameDuration::operator>=(FrameDuration other) const {
    return centiseconds >= other.centiseconds;
}

EasyGifReader::Frame::Frame() : parentData(nullptr), index(-1), w(0), h(0), pixelBuffer(nullptr), disposal(DISPOSAL_UNSPECIFIED), delay(0) { }

EasyGifReader::Frame::Frame(const Frame &orig) : parentData(orig.parentData), index(orig.index), w(orig.w), h(orig.h), pixelBuffer(nullptr), disposal(orig.disposal), delay(orig.delay) {
    if (orig.pixelBuffer) {
        pixelBuffer = new uint32_t[parentData->pixelBufferSize];
        memcpy(pixelBuffer, orig.pixelBuffer, sizeof(uint32_t)*parentData->pixelBufferSize);
    }
}

EasyGifReader::Frame::Frame(Frame &&orig) : parentData(orig.parentData), index(orig.index), w(orig.w), h(orig.h), pixelBuffer(orig.pixelBuffer), disposal(orig.disposal), delay(orig.delay) {
    orig.pixelBuffer = nullptr;
}

EasyGifReader::Frame::~Frame() {
    delete[] pixelBuffer;
}

EasyGifReader::Frame & EasyGifReader::Frame::operator=(const Frame &orig) {
    if (this != &orig) {
        bool keepPixelBuffer = pixelBuffer && orig.pixelBuffer && parentData->pixelBufferSize == orig.parentData->pixelBufferSize;
        if (!keepPixelBuffer) {
            delete[] pixelBuffer;
            pixelBuffer = nullptr;
        }
        parentData = orig.parentData;
        index = orig.index;
        w = orig.w, h = orig.h;
        disposal = orig.disposal;
        delay = orig.delay;
        if (orig.pixelBuffer) {
            if (!keepPixelBuffer)
                pixelBuffer = new uint32_t[parentData->pixelBufferSize];
            memcpy(pixelBuffer, orig.pixelBuffer, sizeof(uint32_t)*parentData->pixelBufferSize);
        }
    }
    return *this;
}

EasyGifReader::Frame & EasyGifReader::Frame::operator=(Frame &&orig) {
    if (this != &orig) {
        delete[] pixelBuffer;
        parentData = orig.parentData;
        index = orig.index;
        w = orig.w, h = orig.h;
        disposal = orig.disposal;
        delay = orig.delay;
        pixelBuffer = orig.pixelBuffer;
        orig.pixelBuffer = nullptr;
    }
    return *this;
}

const uint32_t * EasyGifReader::Frame::pixels() const {
    return pixelBuffer;
}

int EasyGifReader::Frame::width() const {
    return w;
}

int EasyGifReader::Frame::height() const {
    return h;
}

EasyGifReader::FrameDuration EasyGifReader::Frame::duration() const {
    return FrameDuration { delay > 1 ? delay : 10 };
}

EasyGifReader::FrameDuration EasyGifReader::Frame::rawDuration() const {
    return FrameDuration { delay };
}

void EasyGifReader::Frame::nextFrame() {
    if (!(parentData && parentData->gif))
        throw Error::INVALID_OPERATION;
    ++index;
    if (!parentData->gif->ImageCount || (parentData->loopCount && index >= parentData->loopCount*parentData->gif->ImageCount))
        return;
    int imageIndex = index%parentData->gif->ImageCount;
    if (!pixelBuffer) {
        if (!imageIndex)
            pixelBuffer = new uint32_t[parentData->pixelBufferSize];
        else
            throw Error::INVALID_OPERATION;
    }
    GraphicsControlBlock gcb;
    gcb.DisposalMode = DISPOSAL_UNSPECIFIED;
    gcb.UserInputFlag = 0;
    gcb.DelayTime = 0;
    gcb.TransparentColor = NO_TRANSPARENT_COLOR;
    DGifSavedExtensionToGCB(parentData->gif, imageIndex, &gcb);
    delay = gcb.DelayTime;
    const ColorMapObject *colorMap = parentData->gif->SavedImages[imageIndex].ImageDesc.ColorMap;
    if (!colorMap)
        colorMap = parentData->gif->SColorMap;
    const GifImageDesc &imageDesc = parentData->gif->SavedImages[imageIndex].ImageDesc;
    const GifByteType *raster = parentData->gif->SavedImages[imageIndex].RasterBits;
    if (!colorMap || (!raster && imageDesc.Width > 0 && imageDesc.Height > 0))
        throw Error::INVALID_GIF_FILE;
    FrameBounds frameBounds = Internal::frameBounds(parentData->gif, imageIndex);
    if (gcb.TransparentColor >= 0 || frameBounds.x0 > 0 || frameBounds.x1 < w || frameBounds.y0 > 0 || frameBounds.y1 < h || gcb.DisposalMode == DISPOSE_PREVIOUS) {
        if (!imageIndex)
            memset(pixelBuffer, 0, sizeof(uint32_t)*w*h);
        else if (disposal == DISPOSE_BACKGROUND || disposal == DISPOSE_PREVIOUS) {
            FrameBounds prevBounds = Internal::frameBounds(parentData->gif, imageIndex-1);
            if (disposal == DISPOSE_PREVIOUS)
                Internal::copyRows(corner(prevBounds), pixelBuffer+w*h, prevBounds.width(), prevBounds.height(), w, prevBounds.width());
            else
                Internal::clearRows(corner(prevBounds), prevBounds.width(), prevBounds.height(), w);
        }
    }
    disposal = gcb.DisposalMode;
    if (disposal == DISPOSE_PREVIOUS) {
        if (imageIndex && imageIndex < parentData->gif->ImageCount-1)
            Internal::copyRows(pixelBuffer+w*h, corner(frameBounds), frameBounds.width(), frameBounds.height(), frameBounds.width(), w);
        else
            disposal = DISPOSE_BACKGROUND;
    }
    for (int y = frameBounds.y0; y < frameBounds.y1; ++y) {
        GifByteType *dst = reinterpret_cast<GifByteType *>(row(y)+frameBounds.x0);
        const GifByteType *src = raster+(imageDesc.Width*(y-imageDesc.Top)+(frameBounds.x0-imageDesc.Left));
        for (int x = frameBounds.x0; x < frameBounds.x1; ++x) {
            int i = int(*src++);
            i *= (unsigned) i < (unsigned) colorMap->ColorCount;
            GifColorType c = colorMap->Colors[i];
            if (i != gcb.TransparentColor) {
                dst[0] = c.Red;
                dst[1] = c.Green;
                dst[2] = c.Blue;
                dst[3] = GifByteType(0xff);
            }
            dst += 4;
        }
    }
}

uint32_t * EasyGifReader::Frame::row(int y) {
#ifdef EASY_GIF_DECODER_BOTTOM_TO_TOP_ROWS
    return pixelBuffer+w*(h-y-1);
#else
    return pixelBuffer+w*y;
#endif
}

uint32_t * EasyGifReader::Frame::corner(const FrameBounds &bounds) {
#ifdef EASY_GIF_DECODER_BOTTOM_TO_TOP_ROWS
    return pixelBuffer+(w*(h-bounds.y1)+bounds.x0);
#else
    return pixelBuffer+(w*bounds.y0+bounds.x0);
#endif
}

EasyGifReader::FrameIterator::FrameIterator(const EasyGifReader *decoder, Position position) {
    if (decoder) {
        if (!((parentData = decoder->data) && parentData->gif))
            throw Error::INVALID_OPERATION;
        w = parentData->gif->SWidth;
        h = parentData->gif->SHeight;
        switch (position) {
            case BEGIN:
                rewind();
                break;
            case END:
                index = parentData->gif->ImageCount;
                break;
            case LOOP_END:
                if (parentData->loopCount)
                    index = parentData->loopCount*parentData->gif->ImageCount;
                else
                    index = -1;
                break;
        }
    }
}

EasyGifReader::FrameIterator & EasyGifReader::FrameIterator::operator++() {
    nextFrame();
    return *this;
}

void EasyGifReader::FrameIterator::operator++(int) {
    nextFrame();
}

bool EasyGifReader::FrameIterator::operator==(const FrameIterator &other) const {
    return parentData == other.parentData && index == other.index;
}

bool EasyGifReader::FrameIterator::operator!=(const FrameIterator &other) const {
    return parentData != other.parentData || index != other.index;
}

const EasyGifReader::Frame & EasyGifReader::FrameIterator::operator*() const {
    return *this;
}

const EasyGifReader::Frame * EasyGifReader::FrameIterator::operator->() const {
    return this;
}

void EasyGifReader::FrameIterator::rewind() {
    index = -1;
    nextFrame();
}

EasyGifReader EasyGifReader::openFile(const char *filename) {
    int error = D_GIF_SUCCEEDED;
#if defined(_WIN32) && !defined(EASY_GIF_DECODER_NO_UTF8_FILENAME_SUPPORT)
    if (GifFileType *gif = DGifOpenFileHandle(openFileHandleUTF8Win(filename, O_RDONLY, 0), &error)) {
#else
    if (GifFileType *gif = DGifOpenFileName(filename, &error)) {
#endif
        Internal *data = nullptr;
        try {
            data = new Internal;
        } catch (...) {
            DGifCloseFile(gif, nullptr);
            throw Error::OUT_OF_MEMORY;
        }
        data->gif = gif;
        return EasyGifReader(data);
    } else
        throw Internal::translateErrorCode(error);
}

int EasyGifReader::Internal::memoryRead(GifFileType *gif, GifByteType *outData, int size) {
    Internal *data = reinterpret_cast<Internal *>(gif->UserData);
    size_t realSize = data->remainingSize < (size_t) size ? data->remainingSize : (size_t) size;
    memcpy(outData, data->dataPtr, realSize);
    data->remainingSize -= realSize;
    data->dataPtr = reinterpret_cast<const unsigned char *>(data->dataPtr)+realSize;
    return (int) realSize;
}

EasyGifReader EasyGifReader::openMemory(const void *buffer, size_t size) {
    Internal *data = nullptr;
    try {
        data = new Internal;
    } catch (...) {
        throw Error::OUT_OF_MEMORY;
    }
    int error = D_GIF_SUCCEEDED;
    data->dataPtr = buffer;
    data->remainingSize = size;
    if ((data->gif = DGifOpen(data, &Internal::memoryRead, &error)))
        return EasyGifReader(data);
    else {
        delete data;
        throw Internal::translateErrorCode(error);
    }
}

int EasyGifReader::Internal::customRead(GifFileType *gif, GifByteType *outData, int size) {
    Internal *data = reinterpret_cast<Internal *>(gif->UserData);
    return (int) (*data->readFunction)(outData, (size_t) size, data->userPtr);
}

EasyGifReader EasyGifReader::openCustom(size_t (*readFunction)(void *outData, size_t size, void *userPtr), void *userPtr) {
    Internal *data = nullptr;
    try {
        data = new Internal;
    } catch (...) {
        throw Error::OUT_OF_MEMORY;
    }
    int error = D_GIF_SUCCEEDED;
    data->readFunction = readFunction;
    data->userPtr = userPtr;
    if ((data->gif = DGifOpen(data, &Internal::customRead, &error)))
        return EasyGifReader(data);
    else {
        delete data;
        throw Internal::translateErrorCode(error);
    }
}

int EasyGifReader::FrameBounds::width() const {
    return x1-x0;
}

int EasyGifReader::FrameBounds::height() const {
    return y1-y0;
}

bool EasyGifReader::Internal::readLoopExtension(int &loopCount, const ExtensionBlock *extensionBlocks, int extensionBlockCount) {
    for (; --extensionBlockCount > 0; ++extensionBlocks) {
        if (
            extensionBlocks[0].Function == 0xff &&
            extensionBlocks[0].ByteCount == 11 &&
            (!memcmp(extensionBlocks[0].Bytes, "NETSCAPE2.0", 11) || !memcmp(extensionBlocks[0].Bytes, "ANIMEXTS1.0", 11)) &&
            extensionBlocks[1].Function == 0x00 &&
            extensionBlocks[1].ByteCount == 3 &&
            extensionBlocks[1].Bytes[0] == 0x01
        ) {
            loopCount = (unsigned) extensionBlocks[1].Bytes[1]|(unsigned) extensionBlocks[1].Bytes[2]<<8;
            return true;
        }
    }
    return false;
}

EasyGifReader::EasyGifReader() : data(nullptr) { }

EasyGifReader::EasyGifReader(Internal *data) : data(data) {
    if (DGifSlurp(data->gif) != GIF_OK) {
        Error error = Internal::translateErrorCode(data->gif->Error);
        DGifCloseFile(data->gif, nullptr);
        delete data;
        throw error;
    }
    data->loopCount = 1;
    if (!Internal::readLoopExtension(data->loopCount, data->gif->ExtensionBlocks, data->gif->ExtensionBlockCount) && data->gif->ImageCount > 0)
        Internal::readLoopExtension(data->loopCount, data->gif->SavedImages[0].ExtensionBlocks, data->gif->SavedImages[0].ExtensionBlockCount);
    size_t frameArea = (size_t) data->gif->SWidth*(size_t) data->gif->SHeight;
    size_t prevFrameBufferSize = 0;
    for (int i = 0; i < data->gif->ImageCount-1; ++i) {
        GraphicsControlBlock gcb;
        gcb.DisposalMode = DISPOSAL_UNSPECIFIED;
        DGifSavedExtensionToGCB(data->gif, i, &gcb);
        if (gcb.DisposalMode == DISPOSE_PREVIOUS) {
            size_t area = (size_t) data->gif->SavedImages[i].ImageDesc.Width*(size_t) data->gif->SavedImages[i].ImageDesc.Height;
            if (area > prevFrameBufferSize) {
                prevFrameBufferSize = area;
                if (area == frameArea)
                    break;
            }
        }
    }
    data->pixelBufferSize = frameArea+prevFrameBufferSize;
}

EasyGifReader::EasyGifReader(EasyGifReader &&orig) : data(orig.data) {
    orig.data = nullptr;
}

EasyGifReader::~EasyGifReader() {
    if (data) {
        DGifCloseFile(data->gif, nullptr);
        delete data;
    }
}

EasyGifReader & EasyGifReader::operator=(EasyGifReader &&orig) {
    if (this != &orig) {
        if (data) {
            DGifCloseFile(data->gif, nullptr);
            delete data;
        }
        data = orig.data;
        orig.data = nullptr;
    }
    return *this;
}

int EasyGifReader::width() const {
    return data->gif->SWidth;
}

int EasyGifReader::height() const {
    return data->gif->SHeight;
}

int EasyGifReader::frameCount() const {
    return data->gif->ImageCount;
}

int EasyGifReader::repeatCount() const {
    return data->loopCount;
}

bool EasyGifReader::repeatInfinitely() const {
    return !data->loopCount;
}

EasyGifReader::FrameIterator EasyGifReader::begin() const {
    return FrameIterator(this, FrameIterator::BEGIN);
}

EasyGifReader::FrameIterator EasyGifReader::end() const {
    return FrameIterator(this, FrameIterator::END);
}

EasyGifReader::FrameIterator EasyGifReader::loopEnd() const {
    return FrameIterator(this, FrameIterator::LOOP_END);
}
