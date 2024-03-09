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
import com.awxkee.jxlcoder.JxlAnimatedImage

public class JxlAnimatedStore(
    private val jxlAnimatedImage: JxlAnimatedImage,
    val targetWidth: Int = 0,
    val targetHeight: Int = 0,
) : AnimatedFrameStore {
    override val width: Int
        get() = if (targetWidth > 0 && targetHeight > 0) targetWidth else jxlAnimatedImage.getWidth()
    override val height: Int
        get() = if (targetWidth > 0 && targetHeight > 0) targetHeight else jxlAnimatedImage.getHeight()

    override fun getFrame(frame: Int): Bitmap {
        return jxlAnimatedImage.getFrame(frame)
    }

    override fun getFrameDuration(frame: Int): Int {
        return jxlAnimatedImage.getFrameDuration(frame)
    }

    override val framesCount: Int
        get() = jxlAnimatedImage.numberOfFrames
}