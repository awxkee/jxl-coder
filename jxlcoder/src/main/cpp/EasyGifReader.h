
#pragma once

#include <cstddef>
#include <cstdint>

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//
//  EASY GIF READER v1.0 by Viktor Chlumsky (c) 2021
//  ------------------------------------------------
//
//  This is a single-class C++ library that aims to simplify reading an animated GIF file.
//  It is built on top of and depends on giflib.
//
//  MIT license
//  https://github.com/Chlumsky/EasyGifReader
//

class EasyGifReader {
    struct Internal;
    struct FrameBounds;

public:
    enum class Error {
        UNKNOWN,
        INVALID_OPERATION,
        OPEN_FAILED,
        READ_FAILED,
        INVALID_FILENAME,
        NOT_A_GIF_FILE,
        INVALID_GIF_FILE,
        OUT_OF_MEMORY,
        CLOSE_FAILED,
        NOT_READABLE,
        IMAGE_DEFECT,
        UNEXPECTED_EOF
    };

    struct FrameDuration {
        int centiseconds;
        int milliseconds() const;
        double seconds() const;
        FrameDuration & operator+=(FrameDuration other);
        FrameDuration & operator-=(FrameDuration other);
        FrameDuration operator+(FrameDuration other) const;
        FrameDuration operator-(FrameDuration other) const;
        bool operator==(FrameDuration other) const;
        bool operator!=(FrameDuration other) const;
        bool operator<(FrameDuration other) const;
        bool operator>(FrameDuration other) const;
        bool operator<=(FrameDuration other) const;
        bool operator>=(FrameDuration other) const;
    };

    class Frame {
    public:
        Frame();
        Frame(const Frame &orig);
        Frame(Frame &&orig);
        ~Frame();
        Frame & operator=(const Frame &orig);
        Frame & operator=(Frame &&orig);
        const std::uint32_t * pixels() const;
        int width() const;
        int height() const;
        FrameDuration duration() const;
        FrameDuration rawDuration() const;
    protected:
        const Internal *parentData;
        int index;
        int w, h;
        void nextFrame();
    private:
        std::uint32_t *pixelBuffer;
        int disposal;
        int delay;
        std::uint32_t * row(int y);
        std::uint32_t * corner(const FrameBounds &bounds);
    };

    class FrameIterator : public Frame {
    public:
        enum Position {
            BEGIN,
            END,
            LOOP_END
        };
        explicit FrameIterator(const EasyGifReader *decoder = nullptr, Position position = BEGIN);
        FrameIterator & operator++();
        void operator++(int);
        bool operator==(const FrameIterator &other) const;
        bool operator!=(const FrameIterator &other) const;
        const Frame & operator*() const;
        const Frame * operator->() const;
        void rewind();
    };

    static EasyGifReader openFile(const char *filename);
    static EasyGifReader openMemory(const void *buffer, std::size_t size);
    static EasyGifReader openCustom(std::size_t (*readFunction)(void *outData, std::size_t size, void *userPtr), void *userPtr);

    EasyGifReader();
    EasyGifReader(const EasyGifReader &) = delete;
    EasyGifReader(EasyGifReader &&orig);
    ~EasyGifReader();
    EasyGifReader & operator=(const EasyGifReader &) = delete;
    EasyGifReader & operator=(EasyGifReader &&orig);

    int width() const;
    int height() const;
    int frameCount() const;
    int repeatCount() const;
    bool repeatInfinitely() const;
    FrameIterator begin() const;
    FrameIterator end() const;
    FrameIterator loopEnd() const;

private:
    Internal *data;

    explicit EasyGifReader(Internal *data);

};
