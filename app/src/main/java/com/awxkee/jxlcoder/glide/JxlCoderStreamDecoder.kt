package com.awxkee.jxlcoder.glide

import android.graphics.Bitmap
import com.bumptech.glide.load.Options
import com.bumptech.glide.load.ResourceDecoder
import com.bumptech.glide.load.engine.Resource
import com.bumptech.glide.load.engine.bitmap_recycle.BitmapPool
import com.bumptech.glide.util.ByteBufferUtil
import java.io.InputStream

class JxlCoderStreamDecoder(private val bitmapPool: BitmapPool) :
    ResourceDecoder<InputStream, Bitmap> {

    private val byteBufferDecoder = JxlCoderByteBufferDecoder(bitmapPool)

    override fun handles(source: InputStream, options: Options): Boolean {
        val bb = ByteBufferUtil.fromStream(source)
        source.reset()
        return byteBufferDecoder.handles(bb, options)
    }

    override fun decode(
        source: InputStream,
        width: Int,
        height: Int,
        options: Options
    ): Resource<Bitmap>? {
        val bb = ByteBufferUtil.fromStream(source)
        source.reset()
        return byteBufferDecoder.decode(bb, width, height, options)
    }
}