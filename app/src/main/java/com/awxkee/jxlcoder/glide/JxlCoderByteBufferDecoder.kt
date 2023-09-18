package com.awxkee.jxlcoder.glide

import android.graphics.Bitmap
import android.os.Build
import com.awxkee.jxlcoder.JxlCoder
import com.awxkee.jxlcoder.PreferredColorConfig
import com.awxkee.jxlcoder.ScaleMode
import com.bumptech.glide.load.DecodeFormat
import com.bumptech.glide.load.Options
import com.bumptech.glide.load.ResourceDecoder
import com.bumptech.glide.load.engine.Resource
import com.bumptech.glide.load.engine.bitmap_recycle.BitmapPool
import com.bumptech.glide.load.resource.bitmap.BitmapResource
import com.bumptech.glide.load.resource.bitmap.Downsampler
import com.bumptech.glide.request.target.Target
import java.nio.ByteBuffer

class JxlCoderByteBufferDecoder(private val bitmapPool: BitmapPool) :
    ResourceDecoder<ByteBuffer, Bitmap> {

    private val coder = JxlCoder()

    override fun handles(source: ByteBuffer, options: Options): Boolean {
        return JxlCoder.isJXL(source.array())
    }

    override fun decode(
        source: ByteBuffer,
        width: Int,
        height: Int,
        options: Options
    ): Resource<Bitmap>? {
        val src = refactorToDirect(source)
        val allowedHardwareConfig = options[Downsampler.ALLOW_HARDWARE_CONFIG] ?: false

        var idealWidth = width
        if (idealWidth == Target.SIZE_ORIGINAL) {
            idealWidth = -1
        }
        var idealHeight = height
        if (idealHeight == Target.SIZE_ORIGINAL) {
            idealHeight = -1
        }

        val preferredColorConfig: PreferredColorConfig = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && allowedHardwareConfig) {
                PreferredColorConfig.HARDWARE
            } else {
                if (options[Downsampler.DECODE_FORMAT] === DecodeFormat.PREFER_RGB_565) {
                    PreferredColorConfig.RGB_565
                } else {
                    PreferredColorConfig.DEFAULT
                }
            }

        val bitmap =
            coder.decodeSampled(src, idealWidth, idealHeight, preferredColorConfig, ScaleMode.FIT)

        return BitmapResource.obtain(bitmap, bitmapPool)
    }

    private fun refactorToDirect(source: ByteBuffer): ByteBuffer {
        if (source.isDirect) {
            return source
        }
        val sourceCopy = ByteBuffer.allocateDirect(source.remaining())
        sourceCopy.put(source)
        sourceCopy.flip()
        return sourceCopy
    }

}