package com.awxkee.jxlcoder

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.drawable.BitmapDrawable
import android.os.Build
import android.util.Size
import coil.ImageLoader
import coil.decode.DecodeResult
import coil.decode.Decoder
import coil.fetch.SourceResult
import coil.request.Options
import coil.size.Scale
import coil.size.pxOrElse
import kotlinx.coroutines.runInterruptible
import okio.BufferedSource
import okio.ByteString.Companion.toByteString

class JxlDecoder(
    private val source: SourceResult,
    private val options: Options,
    private val imageLoader: ImageLoader
) : Decoder {

    override suspend fun decode(): DecodeResult? = runInterruptible {
        // ColorSpace is preferred to be ignored due to lib is trying to handle all color profile by itself
        val sourceData = source.source.source().readByteArray()

        var mPreferredColorConfig: PreferredColorConfig = when (options.config) {
            Bitmap.Config.ALPHA_8 -> PreferredColorConfig.RGBA_8888
            Bitmap.Config.RGB_565 -> if (options.allowRgb565) PreferredColorConfig.RGB_565 else PreferredColorConfig.DEFAULT
            Bitmap.Config.ARGB_8888 -> PreferredColorConfig.RGBA_8888
            else -> PreferredColorConfig.DEFAULT
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && options.config == Bitmap.Config.RGBA_F16) {
            mPreferredColorConfig = PreferredColorConfig.RGBA_F16
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && options.config == Bitmap.Config.HARDWARE) {
            mPreferredColorConfig = PreferredColorConfig.HARDWARE
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && options.config == Bitmap.Config.RGBA_1010102) {
            mPreferredColorConfig = PreferredColorConfig.RGBA_1010102
        }

        if (options.size == coil.size.Size.ORIGINAL) {
            val originalImage =
                JxlCoder().decode(
                    sourceData,
                    preferredColorConfig = mPreferredColorConfig
                )
            return@runInterruptible DecodeResult(
                BitmapDrawable(
                    options.context.resources,
                    originalImage
                ), false
            )
        }

        val dstWidth = options.size.width.pxOrElse { 0 }
        val dstHeight = options.size.height.pxOrElse { 0 }
        val scaleMode = when (options.scale) {
            Scale.FILL -> ScaleMode.FILL
            Scale.FIT -> ScaleMode.FIT
        }

        val originalImage =
            JxlCoder().decodeSampled(
                sourceData,
                dstWidth,
                dstHeight,
                preferredColorConfig = mPreferredColorConfig,
                scaleMode,
            )
        return@runInterruptible DecodeResult(
            BitmapDrawable(
                options.context.resources,
                originalImage
            ), true
        )
    }
    class Factory : Decoder.Factory {
        override fun create(
            result: SourceResult,
            options: Options,
            imageLoader: ImageLoader
        ) = if (isJXL(result.source.source())) {
            JxlDecoder(result, options, imageLoader)
        } else null

        private val MAGIC_1 = byteArrayOf(0xFF.toByte(), 0x0A).toByteString()
        private val MAGIC_2 = byteArrayOf(
            0x0.toByte(),
            0x0.toByte(),
            0x0.toByte(),
            0x0C.toByte(),
            0x4A,
            0x58,
            0x4C,
            0x20,
            0x0D,
            0x0A,
            0x87.toByte(),
            0x0A
        ).toByteString()

        private fun isJXL(source: BufferedSource): Boolean {
            return source.rangeEquals(0, MAGIC_1) || source.rangeEquals(
                0,
                MAGIC_2
            )
        }
    }

}
