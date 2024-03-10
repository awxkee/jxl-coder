/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 9/3/2024
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

package com.awxkee.jxlcoder.animation

import android.graphics.Bitmap
import android.util.Size
import com.awxkee.jxlcoder.JxlAnimatedImage
import com.awxkee.jxlcoder.ScaleMode
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

public class JxlAnimatedStore(
    private val jxlAnimatedImage: JxlAnimatedImage,
    val targetWidth: Int = 0,
    val targetHeight: Int = 0,
) : AnimatedFrameStore {

    private val cachedOriginalWidth: Int = jxlAnimatedImage.getWidth()
    private val cachedOriginalHeight: Int = jxlAnimatedImage.getHeight()

    private val dstSize: Size = if (targetWidth > 0 && targetHeight > 0) {
        val xf = targetWidth.toFloat() / cachedOriginalWidth.toFloat()
        val yf = targetHeight.toFloat() / cachedOriginalHeight.toFloat()
        val factor: Float = if (jxlAnimatedImage.scaleMode == ScaleMode.FILL) {
            max(xf, yf)
        } else {
            min(xf, yf)
        }
        val newSize = Size((cachedOriginalWidth * factor).roundToInt(), (cachedOriginalHeight * factor).roundToInt())
        newSize
    } else {
        Size(0,0)
    }

    override val width: Int
        get() = if (targetWidth > 0 && targetHeight > 0) dstSize.width else cachedOriginalWidth
    override val height: Int
        get() = if (targetWidth > 0 && targetHeight > 0) dstSize.height else cachedOriginalHeight

    override fun getFrame(frame: Int): Bitmap {
        return jxlAnimatedImage.getFrame(frame, scaleWidth = targetWidth, scaleHeight = targetHeight)
    }

    override fun getFrameDuration(frame: Int): Int {
        return jxlAnimatedImage.getFrameDuration(frame)
    }

    private var storedFramesCount: Int = -1

    override val framesCount: Int
        get() = if (storedFramesCount == -1) {
            storedFramesCount = jxlAnimatedImage.numberOfFrames
            storedFramesCount
        } else {
            storedFramesCount
        }
}