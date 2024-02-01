/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 12/1/2024
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

package com.awxkee.jxlcoder.processing

import android.graphics.Bitmap
import android.os.Build
import androidx.annotation.IntRange
import androidx.annotation.Keep
import java.security.InvalidParameterException

@Keep
object BitmapProcessor {

    fun scale(
        bitmap: Bitmap,
        dstWidth: Int,
        dstHeight: Int,
        scaleMode: BitmapScaleMode
    ): Bitmap = scaleImpl(
        bitmap = bitmap,
        dstWidth = dstWidth,
        dstHeight = dstHeight,
        scaleMode = scaleMode.ordinal + 1
    )

    fun boxBlur(bitmap: Bitmap, radius: Int): Bitmap {
        if (radius < 0) throw InvalidParameterException("radius cannot be less than zero")
        if (radius == 0) {
            return bitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        return boxBlurImpl(bitmap, radius)
    }

    fun gaussBlur(bitmap: Bitmap, radius: Float, sigma: Float): Bitmap {
        if (radius < 0) throw InvalidParameterException("radius cannot be less than zero")
        if (radius == 0.0f) {
            return bitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        return gaussBlurImpl(bitmap, radius, sigma)
    }

    fun bilateralBlur(bitmap: Bitmap, radius: Float, sigma: Float, spatialSigma: Float): Bitmap {
        if (radius < 0) throw InvalidParameterException("radius cannot be less than zero")
        if (radius == 0.0f) {
            return bitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        return bilateralBlurImpl(bitmap, radius, sigma, spatialSigma)
    }

    fun medianBlur(bitmap: Bitmap, radius: Int): Bitmap {
        if (radius < 0) throw InvalidParameterException("radius cannot be less than zero")
        if (radius == 0) {
            return bitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        return medianBlurImpl(bitmap, radius)
    }

    fun stackBlur(
        bitmap: Bitmap,
        radius: Int,
    ): Bitmap {
        if (radius < 2) throw InvalidParameterException("radius cannot be less than 2")
        return cstackBlurImpl(bitmap, radius)
    }

    fun tiltShift(
        bitmap: Bitmap,
        radius: Float,
        sigma: Float,
        anchorX: Float = 0.5f,
        anchorY: Float = 0.5f,
        tiltRadius: Float = 0.2f,
    ): Bitmap {
        if (radius < 0) throw InvalidParameterException("radius cannot be less than zero")
        if (radius == 0.0f) {
            return bitmap.copy(Bitmap.Config.ARGB_8888, true)
        }
        return tiltShiftImpl(bitmap, radius, sigma, anchorX, anchorY, tiltRadius)
    }

    private external fun tiltShiftImpl(
        bitmap: Bitmap,
        radius: Float,
        sigma: Float,
        anchorX: Float = 0.5f,
        anchorY: Float = 0.5f,
        tiltRadius: Float = 0.2f,
    ): Bitmap

    private external fun scaleImpl(
        bitmap: Bitmap,
        dstWidth: Int,
        dstHeight: Int,
        scaleMode: Int
    ): Bitmap

    private external fun gaussBlurImpl(bitmap: Bitmap, radius: Float, sigma: Float): Bitmap

    private external fun bilateralBlurImpl(
        bitmap: Bitmap,
        radius: Float,
        sigma: Float,
        spatialSigma: Float
    ): Bitmap

    private external fun stackBlurImpl(
        bitmap: Bitmap,
        radius: Int,
    ): Bitmap

    private external fun cstackBlurImpl(
        bitmap: Bitmap,
        radius: Int,
    ): Bitmap

    private external fun boxBlurImpl(bitmap: Bitmap, radius: Int): Bitmap

    private external fun medianBlurImpl(bitmap: Bitmap, radius: Int): Bitmap

    init {
        if (Build.VERSION.SDK_INT >= 21) {
            System.loadLibrary("jxlcoder")
        }
    }

}