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
import kotlin.math.roundToInt

public class JxlAnimatedStore(
    private val jxlAnimatedImage: JxlAnimatedImage,
    val targetWidth: Int = 0,
    val targetHeight: Int = 0,
) : AnimatedFrameStore {

    private val cachedOriginalWidth: Int = jxlAnimatedImage.getWidth()
    private val cachedOriginalHeight: Int = jxlAnimatedImage.getHeight()

    private val dstSize: Size = resolveSize()

    private fun resolveSize(): Size {
        // Match the dimension sentinels and rounding in weaver/src/scaling.rs.
        fun even(value: Int): Int = (value.toLong().coerceAtLeast(1) + 1L)
            .and(-2L).coerceAtMost(Int.MAX_VALUE.toLong()).toInt()

        val requested = when {
            targetWidth == -2 && targetHeight == -2 ->
                Size(even(cachedOriginalWidth), even(cachedOriginalHeight))
            targetWidth <= 0 && targetHeight <= 0 ->
                Size(cachedOriginalWidth, cachedOriginalHeight)
            targetWidth > 0 && targetHeight in -2..-1 -> {
                val height = (cachedOriginalHeight.toDouble() * targetWidth / cachedOriginalWidth)
                    .roundToInt().coerceAtLeast(1)
                Size(targetWidth, if (targetHeight == -2) even(height) else height)
            }
            targetHeight > 0 && targetWidth in -2..-1 -> {
                val width = (cachedOriginalWidth.toDouble() * targetHeight / cachedOriginalHeight)
                    .roundToInt().coerceAtLeast(1)
                Size(if (targetWidth == -2) even(width) else width, targetHeight)
            }
            else -> Size(targetWidth.coerceAtLeast(1), targetHeight.coerceAtLeast(1))
        }

        // FILL crops to the requested rectangle, and RESIZE stretches to it.
        if (jxlAnimatedImage.scaleMode != ScaleMode.FIT) return requested

        return if (requested.width.toLong() * cachedOriginalHeight <=
            requested.height.toLong() * cachedOriginalWidth
        ) {
            Size(
                requested.width,
                (cachedOriginalHeight.toDouble() * requested.width / cachedOriginalWidth)
                    .roundToInt().coerceIn(1, requested.height)
            )
        } else {
            Size(
                (cachedOriginalWidth.toDouble() * requested.height / cachedOriginalHeight)
                    .roundToInt().coerceIn(1, requested.width),
                requested.height
            )
        }
    }

    override val width: Int
        get() = dstSize.width
    override val height: Int
        get() = dstSize.height

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
