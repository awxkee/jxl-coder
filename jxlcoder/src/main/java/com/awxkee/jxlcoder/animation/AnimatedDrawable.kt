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
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.drawable.Animatable
import android.graphics.drawable.Drawable
import android.os.Handler
import android.os.HandlerThread
import android.os.Process
import android.util.Log
import androidx.annotation.Keep
import java.util.LinkedList
import java.util.UUID
import kotlin.math.min

@Keep
public class AnimatedDrawable(private val frameStore: AnimatedFrameStore) : Drawable(), Animatable,
    Runnable {

    data class SyncedFrame(val frame: Bitmap, val frameDuration: Int, val frameIndex: Int)

    private val lock = Any()

    private val handlerThread =
        HandlerThread(
            "AnimationDecoderThread with id: ${UUID.randomUUID()}",
            Process.THREAD_PRIORITY_DISPLAY
        )
    private val mHandlerThread: Handler
    private val queue = LinkedList<SyncedFrame>()

    private var currentBitmap: Bitmap? = null

    private var mCurrentFrameDuration = 0
    private var lastDecodedFrameIndex: Int = 0

    private val deleteQueue = LinkedList<Bitmap>()

    private var isRunning = false
    private var lastSavedState = false

    private val decodingRunnable = Runnable {
        var nextFrameIndex = lastDecodedFrameIndex + 1
        if (nextFrameIndex >= frameStore.framesCount) {
            nextFrameIndex = 0
        }

        var toDeleteItem: Bitmap? = deleteQueue.pollFirst()
        while (toDeleteItem != null) {
            toDeleteItem.recycle()
            toDeleteItem = deleteQueue.pollFirst()
        }

        var haveFrameSent = false

        synchronized(lock) {
            if (queue.size > 0) {
                val frame = queue.pop()
                mCurrentFrameDuration = frame.frameDuration
                lastDecodedFrameIndex = frame.frameIndex
                currentBitmap = frame.frame
                scheduleSelf(this, 0)
                haveFrameSent = true
            }
        }

        if (!haveFrameSent) {
            val nextFrame = frameStore.getFrame(nextFrameIndex)
            val nextFrameDuration = frameStore.getFrameDuration(nextFrameIndex)
            synchronized(lock) {
                mCurrentFrameDuration = nextFrameDuration
                lastDecodedFrameIndex = nextFrameIndex
                currentBitmap = nextFrame
                scheduleSelf(this, 0)
                haveFrameSent = true
            }

            nextFrameIndex += 1
        }

        val maybeCachedFrames = min(14, frameStore.framesCount)
        val vr = queue.lastOrNull()?.frameIndex
        if (vr != null) {
            nextFrameIndex = vr + 1
        }
        if (nextFrameIndex >= frameStore.framesCount) {
            nextFrameIndex = 0
        }
        while (queue.size < maybeCachedFrames) {
            val nextFrame = frameStore.getFrame(nextFrameIndex)
            val nextFrameDuration = frameStore.getFrameDuration(nextFrameIndex)
            queue.add(SyncedFrame(nextFrame, nextFrameDuration, nextFrameIndex))

            nextFrameIndex += 1
            if (nextFrameIndex >= frameStore.framesCount) {
                nextFrameIndex = 0
            }
        }

    }

    init {
        handlerThread.start()
        mHandlerThread = Handler(handlerThread.looper)
        handlerThread.setUncaughtExceptionHandler { t, e ->
            Log.e("AnimatedDrawable", e.message ?: "Failed to decode next frame")
        }
    }

    private val matrix = Matrix()
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)

    private val rect = Rect()

    override fun draw(canvas: Canvas) {
        val bmp = currentBitmap
        if (bmp != null) {
            matrix.reset()
            rect.set(0, 0, bmp.width, bmp.height)
            canvas.drawBitmap(bmp, rect, bounds, paint)
        }
    }

    override fun setAlpha(alpha: Int) {
        paint.alpha = alpha
    }

    override fun getIntrinsicHeight(): Int {
        return frameStore.height
    }

    override fun getIntrinsicWidth(): Int {
        return frameStore.width
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        paint.colorFilter = colorFilter
    }

    @Deprecated("Deprecated in Java")
    override fun getOpacity(): Int {
        return paint.alpha
    }

    override fun start() {
        if (isRunning) {
            return
        }
        isRunning = true
        lastSavedState = true
        mHandlerThread.removeCallbacks(decodingRunnable)
        mHandlerThread.post(decodingRunnable)
    }

    override fun stop() {
        if (!isRunning) {
            return
        }
        isRunning = false
        lastSavedState = false
        unscheduleSelf(this)
    }

    override fun isRunning(): Boolean {
        return isRunning
    }

    override fun unscheduleSelf(what: Runnable) {
        mHandlerThread.removeCallbacks(decodingRunnable)
        super.unscheduleSelf(what)
    }

    override fun setVisible(visible: Boolean, restart: Boolean): Boolean {
        if (!visible) {
            stop()
        } else {
            if (lastSavedState) {
                start()
            }
        }
        return super.setVisible(visible, restart)
    }

    override fun run() {
        if (!isRunning) {
            return
        }
        invalidateSelf()
        synchronized(lock) {
            val duration = mCurrentFrameDuration
            if (duration == 0) {
                mHandlerThread.post(decodingRunnable)
            } else {
                mHandlerThread.postDelayed(decodingRunnable, duration.toLong())
            }
        }
    }

    protected fun finalize() {
        stop()
    }

}