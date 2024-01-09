/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 8/1/2024
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

package com.awxkee.jxlcoder

import android.graphics.Bitmap
import android.graphics.drawable.AnimatedImageDrawable
import android.graphics.drawable.AnimationDrawable
import android.graphics.drawable.BitmapDrawable
import android.os.Build
import androidx.annotation.Keep
import androidx.annotation.RequiresApi
import java.io.Closeable
import java.nio.ByteBuffer

@Keep
class JxlAnimatedImage : Closeable {

    private var coordinator: Long = -1L
    private val lock = Any()

    private external fun createCoordinator(
        byteArray: ByteBuffer,
        preferredColorConfig: Int,
        scaleMode: Int,
        jxlResizeSampler: Int,
    ): Long

    private external fun createCoordinatorByteArray(
        byteArray: ByteArray,
        preferredColorConfig: Int,
        scaleMode: Int,
        jxlResizeSampler: Int,
    ): Long

    @Keep
    public constructor(
        byteBuffer: ByteBuffer,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        jxlResizeFilter: JxlResizeFilter = JxlResizeFilter.CATMULL_ROM,
    ) {
        coordinator = createCoordinator(
            byteBuffer,
            preferredColorConfig.value,
            scaleMode.value,
            jxlResizeFilter.value,
        )
    }

    @Keep
    public constructor(
        byteArray: ByteArray,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        jxlResizeFilter: JxlResizeFilter = JxlResizeFilter.CATMULL_ROM,
    ) {
        coordinator = createCoordinatorByteArray(
            byteArray,
            preferredColorConfig.value,
            scaleMode.value,
            jxlResizeFilter.value,
        )
    }

    val animatedDrawable: AnimationDrawable
        @Keep
        get() {
            val frames = numberOfFrames
            if (frames == 1) {
                val img = AnimationDrawable()
                @Suppress("DEPRECATION")
                img.addFrame(BitmapDrawable(getFrame(0)), Int.MAX_VALUE)
                return img
            }
            val img = AnimationDrawable()
            for (frame in 0 until frames) {
                @Suppress("DEPRECATION")
                img.addFrame(BitmapDrawable(getFrame(frame)), getFrameDuration(frame))
            }
            return img
        }

    public val numberOfFrames: Int
        @Keep
        get() {
            assertOpen()
            return getNumberOfFrames(coordinator)
        }

    public val loopsCount: Int
        @Keep
        get() {
            assertOpen()
            return getLoopsCount(coordinator)
        }

    @Keep
    public fun getFrameDuration(frame: Int): Int {
        assertOpen()
        return getFrameDurationImpl(coordinator, frame)
    }

    @Keep
    public fun getFrame(frame: Int, scaleWidth: Int = 0, scaleHeight: Int = 0): Bitmap {
        assertOpen()
        return getFrameImpl(coordinator, frame, scaleWidth, scaleHeight)
    }

    @Keep
    fun getWidth(): Int {
        assertOpen()
        return getWidthImpl(coordinator)
    }

    @Keep
    fun getHeight(): Int {
        assertOpen()
        return getHeightImpl(coordinator)
    }

    private fun assertOpen() {
        synchronized(lock) {
            if (coordinator == -1L) {
                throw IllegalStateException("Animated image is already closed, call to it functions is impossible")
            }
        }
    }

    private external fun getHeightImpl(coordinatorPtr: Long): Int
    private external fun getWidthImpl(coordinatorPtr: Long): Int
    private external fun getFrameImpl(
        coordinatorPtr: Long,
        frame: Int,
        width: Int,
        height: Int
    ): Bitmap

    private external fun getLoopsCount(coordinatorPtr: Long): Int
    private external fun getFrameDurationImpl(coordinatorPtr: Long, frame: Int): Int
    private external fun getNumberOfFrames(coordinatorPtr: Long): Int
    private external fun closeAndReleaseAnimatedImage(coordinatorPtr: Long)

    override fun close() {
        synchronized(lock) {
            if (coordinator != -1L) {
                closeAndReleaseAnimatedImage(coordinator)
                coordinator = -1L
            }
        }
    }

    protected fun finalize() {
        close()
    }
}