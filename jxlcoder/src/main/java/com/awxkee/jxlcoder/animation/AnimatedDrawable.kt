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
import kotlin.system.measureTimeMillis

@Keep
/**
 * @param preheatFrames - Count of frames that will be eagerly preloaded before the render to have already some frames cached
 */
public class AnimatedDrawable(
    private val frameStore: AnimatedFrameStore,
    private val preheatFrames: Int = 6,
) : Drawable(), Animatable,
    Runnable {

    data class SyncedFrame(val frame: Bitmap, val frameDuration: Int, val frameIndex: Int)

    private val lock = Any()

    private val handlerThread =
        HandlerThread(
            "AnimationDecoderThread with id: ${UUID.randomUUID()}",
            Process.THREAD_PRIORITY_DISPLAY
        )
    private val mHandlerThread: Handler
    private val syncedFrames = LinkedList<SyncedFrame>()

    private var currentBitmap: Bitmap? = null

    private var mCurrentFrameDuration = 0
    private var lastDecodedFrameIndex: Int = -1

    private var isRunning = false
    private var lastSavedState = false

    private val measuredTimesStore = mutableListOf<Int>()

    private fun decodeNextFrame(nextFrameIndex: Int) {
        val measureTime = measureTimeMillis {
            val syncedNextFrame = syncedFrames.firstOrNull { it.frameIndex == nextFrameIndex }
            if (syncedNextFrame == null) {
                val nextFrame = frameStore.getFrame(nextFrameIndex)
                val nextFrameDuration = frameStore.getFrameDuration(nextFrameIndex)
                synchronized(lock) {
                    syncedFrames.add(SyncedFrame(nextFrame, nextFrameDuration, nextFrameIndex))
                }
            }
        }
        if (measuredTimesStore.size < frameStore.framesCount) {
            measuredTimesStore.add(measureTime.toInt())
        }
    }

    private val preheatRunnable = Runnable {
        var nextFrameIndex = lastDecodedFrameIndex + 1
        if (nextFrameIndex >= frameStore.framesCount) {
            nextFrameIndex = 0
        }

        repeat(preheatFrames) {
            decodeNextFrame(nextFrameIndex)
            nextFrameIndex += 1
            if (nextFrameIndex >= frameStore.framesCount) {
                nextFrameIndex = 0
            }
        }
    }

    private val decodingRunnable = Runnable {
        var nextFrameIndex = lastDecodedFrameIndex + 1
        if (nextFrameIndex >= frameStore.framesCount) {
            nextFrameIndex = 0
        }

        var haveFrameSent = false

        val syncedFrame = syncedFrames.firstOrNull { it.frameIndex == nextFrameIndex }

        if (syncedFrame != null) {
            mCurrentFrameDuration = syncedFrame.frameDuration
            lastDecodedFrameIndex = syncedFrame.frameIndex
            currentBitmap = syncedFrame.frame
            scheduleSelf(this, 0)
            haveFrameSent = true
        }

        if (!haveFrameSent) {
            val nextFrame = frameStore.getFrame(nextFrameIndex)
            val nextFrameDuration = frameStore.getFrameDuration(nextFrameIndex)
            synchronized(lock) {
                syncedFrames.add(SyncedFrame(nextFrame, nextFrameDuration, nextFrameIndex))
                mCurrentFrameDuration = nextFrameDuration
                lastDecodedFrameIndex = nextFrameIndex
                currentBitmap = nextFrame
                scheduleSelf(this, 0)
                haveFrameSent = true
            }

            nextFrameIndex += 1
        }

        if (nextFrameIndex >= frameStore.framesCount) {
            nextFrameIndex = 0
        }

        // Precache next frame

        decodeNextFrame(nextFrameIndex)
        // Since we actually moving with stride *preheatFrames* we have to check if the N + stride frame also exists
        var preheatFrame = nextFrameIndex + preheatFrames
        if (preheatFrame >= frameStore.framesCount) {
            preheatFrame = 0
        }
        decodeNextFrame(preheatFrame)

        val minFrameToIncrease = 1000 / 24

        if (measuredTimesStore.size > 2) {
            val averageDecodingTime = measuredTimesStore.average()
            // If we decode faster than 1000/24ms then do to frames at the time
            if (averageDecodingTime < minFrameToIncrease.toDouble()) {
                nextFrameIndex += preheatFrames + 1
                if (nextFrameIndex >= frameStore.framesCount) {
                    nextFrameIndex = 0
                }
                decodeNextFrame(nextFrameIndex)
            }
        }
    }

    init {
        handlerThread.start()
        mHandlerThread = Handler(handlerThread.looper)
        handlerThread.setUncaughtExceptionHandler { _, e ->
            Log.e("AnimatedDrawable", e.message ?: "Failed to decode next frame")
        }
        mHandlerThread.post(preheatRunnable)
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