/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 9/1/2024
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
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.Rect
import android.graphics.drawable.Animatable
import android.graphics.drawable.Drawable
import android.os.Handler
import android.os.HandlerThread
import android.os.Process
import android.os.SystemClock

/**
 * @author danny.jiang
 */
class FrameSequenceDrawable(animatedImage: JxlAnimatedImage, bitmapProvider: BitmapProvider) :
    Drawable(), Animatable, Runnable {
    private val sLock = Any()
    private var sDecodingThread: HandlerThread? = null
    private var sDecodingThreadHandler: Handler? = null
    private fun initializeDecodingThread() {
        synchronized(sLock) {
            if (sDecodingThread != null) return
            sDecodingThread = HandlerThread(
                "FrameSequence decoding thread",
                Process.THREAD_PRIORITY_BACKGROUND
            )
            sDecodingThread!!.start()
            sDecodingThreadHandler = Handler(sDecodingThread!!.looper)
        }
    }

    interface OnAnimationListener {
        /**
         * Called when a FrameSequenceDrawable has finished looping.
         *
         *
         * Note that this is will not be called if the drawable is explicitly
         * stopped, or marked invisible.
         */
        fun onFinished(drawable: FrameSequenceDrawable?)
        fun onStart(drawable: FrameSequenceDrawable?)
    }

    interface BitmapProvider {
        /**
         * Called by FrameSequenceDrawable to aquire an 8888 Bitmap with minimum dimensions.
         */
        fun acquireBitmap(minWidth: Int, minHeight: Int): Bitmap

        /**
         * Called by FrameSequenceDrawable to release a Bitmap it no longer needs. The Bitmap
         * will no longer be used at all by the drawable, so it is safe to reuse elsewhere.
         *
         *
         * This method may be called by FrameSequenceDrawable on any thread.
         */
        fun releaseBitmap(bitmap: Bitmap?)
    }

    /**
     * Register a callback to be invoked when a FrameSequenceDrawable finishes looping.
     *
     * @see .setLoopBehavior
     */
    fun setOnAnimationListener(onAnimationListener: OnAnimationListener?) {
        mOnAnimationListener = onAnimationListener
    }

    /**
     * Define looping behavior of frame sequence.
     *
     *
     * Must be one of LOOP_ONCE, LOOP_INF, or LOOP_DEFAULT
     */
    fun setLoopBehavior(loopBehavior: Int) {
        mLoopBehavior = loopBehavior
    }

    private val animatedImage: JxlAnimatedImage
    private val mPaint: Paint
    private val mSrcRect: Rect

    //Protects the fields below
    private val mLock = Any()
    private var mBitmapProvider: BitmapProvider? = null
    private var mDestroyed = false
    private var mFrontBitmap: Bitmap? = null
    private var mBackBitmap: Bitmap? = null
    private var mState = 0
    private var mLoopBehavior = LOOP_DEFAULT
    private var mLastSwap: Long = 0
    private var mNextSwap: Long = 0
    private var mNextFrameToDecode: Int = 0
    private var mOnAnimationListener: OnAnimationListener? = null

    /**
     * Runs on decoding thread, only modifies mBackBitmap's pixels
     */
    private var mDecodeRunnable: Runnable? = Runnable {
        var nextFrame: Int
        var bitmap: Bitmap?
        synchronized(mLock) {
            if (mDestroyed) return@Runnable
            nextFrame = mNextFrameToDecode
            if (nextFrame < 0) {
                return@Runnable
            }
            bitmap = mBackBitmap
            mState = STATE_DECODING
        }
        val lastFrame = nextFrame - 2
        var invalidateTimeMs: Int = animatedImage.getFrameDuration(lastFrame)
        if (invalidateTimeMs < MIN_DELAY_MS) {
            invalidateTimeMs = DEFAULT_DELAY_MS.toInt()
        }
        var schedule = false
        var bitmapToRelease: Bitmap? = null
        synchronized(mLock) {
            if (mDestroyed) {
                bitmapToRelease = mBackBitmap
                mBackBitmap = null
            } else if (mNextFrameToDecode >= 0 && mState == STATE_DECODING) {
                schedule = true
                mNextSwap = invalidateTimeMs + mLastSwap
                mState = STATE_WAITING_TO_SWAP
            }
        }
        if (schedule) {
            scheduleSelf(this@FrameSequenceDrawable, mNextSwap.toLong())
        }
        if (bitmapToRelease != null) {
            // destroy the bitmap here, since there's no safe way to get back to
            // drawable thread - drawable is likely detached, so schedule is noop.
            mBitmapProvider!!.releaseBitmap(bitmapToRelease)
        }
    }
    private val startRunnable = Runnable {
        if (mOnAnimationListener != null) {
            mOnAnimationListener!!.onStart(this@FrameSequenceDrawable)
        }
    }
    private val mCallbackRunnable = Runnable {
        if (mOnAnimationListener != null) {
            mOnAnimationListener!!.onFinished(this@FrameSequenceDrawable)
        }
    }

    constructor(animatedImage: JxlAnimatedImage) : this(animatedImage, sAllocatingBitmapProvider)

    init {
        this.animatedImage = animatedImage
        val width: Int = animatedImage.getWidth()
        val height: Int = animatedImage.getHeight()
        mBitmapProvider = bitmapProvider
        mFrontBitmap = acquireAndValidateBitmap(bitmapProvider, width, height)
        mBackBitmap = acquireAndValidateBitmap(bitmapProvider, width, height)
        mSrcRect = Rect(0, 0, width, height)
        mPaint = Paint()
        mPaint.isFilterBitmap = true
        mLastSwap = 0
        mNextFrameToDecode = -1
        initializeDecodingThread()
    }

    private fun checkDestroyedLocked() {
        check(!mDestroyed) { "Cannot perform operation on recycled drawable" }
    }

    val isDestroyed: Boolean
        get() {
            synchronized(mLock) { return mDestroyed }
        }

    /**
     * Marks the drawable as permanently recycled (and thus unusable), and releases any owned
     * Bitmaps drawable to its BitmapProvider, if attached.
     *
     *
     * If no BitmapProvider is attached to the drawable, recycle() is called on the Bitmaps.
     */
    fun destroy() {
        checkNotNull(mBitmapProvider) { "BitmapProvider must be non-null" }
        var bitmapToReleaseA: Bitmap?
        var bitmapToReleaseB: Bitmap? = null
        synchronized(mLock) {
            checkDestroyedLocked()
            bitmapToReleaseA = mFrontBitmap
            mFrontBitmap = null
            if (mState != STATE_DECODING) {
                bitmapToReleaseB = mBackBitmap
                mBackBitmap = null
            }
            mDestroyed = true
        }

        mBitmapProvider?.releaseBitmap(bitmapToReleaseA)
        if (bitmapToReleaseB != null) {
            mBitmapProvider?.releaseBitmap(bitmapToReleaseB)
        }
        if (sDecodingThread != null) {
            sDecodingThread?.quit()
            sDecodingThread = null
        }
        if (sDecodingThreadHandler != null) {
            sDecodingThreadHandler?.removeCallbacks(mDecodeRunnable!!)
            mDecodeRunnable = null
        }
        unscheduleSelf(this)
    }

    @Throws(Throwable::class)
    protected fun finalize() {
        animatedImage.close()
    }

    override fun draw(canvas: Canvas) {
        synchronized(mLock) {
            checkDestroyedLocked()
            if (mState == STATE_WAITING_TO_SWAP) {
                // may have failed to schedule mark ready runnable,
                // so go ahead and swap if swapping is due
                if (mNextSwap - SystemClock.uptimeMillis() <= 0) {
                    mState = STATE_READY_TO_SWAP
                }
            }
            if (isRunning && mState == STATE_READY_TO_SWAP) {
                // Because draw has occurred, the view system is guaranteed to no longer hold a
                // reference to the old mFrontBitmap, so we now use it to produce the next frame
                val tmp = mBackBitmap
                mBackBitmap = mFrontBitmap
                mFrontBitmap = tmp
                mLastSwap = SystemClock.uptimeMillis()
                var continueLooping = true
                if (mNextFrameToDecode >= animatedImage.numberOfFrames - 1) {
                    continueLooping = false
                }
                if (continueLooping) {
                    scheduleDecodeLocked()
                } else {
                    scheduleSelf(mCallbackRunnable, 0)
                }
            }
        }
        canvas.drawBitmap(mFrontBitmap!!, mSrcRect, bounds, mPaint)
    }

    private fun scheduleDecodeLocked() {
        mState = STATE_SCHEDULED
        mNextFrameToDecode += 2
        if (mNextFrameToDecode > animatedImage.numberOfFrames - 1) {
            mNextFrameToDecode = animatedImage.numberOfFrames - 1
        }
        sDecodingThreadHandler!!.post(mDecodeRunnable!!)
    }

    override fun run() {
        if (mDestroyed) {
            return
        }
        // set ready to swap as necessary
        var invalidate = false
        synchronized(mLock) {
            if (mNextFrameToDecode >= 0 && mState == STATE_WAITING_TO_SWAP) {
                mState = STATE_READY_TO_SWAP
                invalidate = true
            }
        }
        if (invalidate) {
            invalidateSelf()
        }
    }

    override fun start() {
        if (!isRunning) {
            synchronized(mLock) {
                checkDestroyedLocked()
                if (mState == STATE_SCHEDULED) return  // already scheduled
                scheduleSelf(startRunnable, 0)
                scheduleDecodeLocked()
            }
        }
    }

    override fun stop() {
        if (isRunning) {
            unscheduleSelf(this)
        }
    }

    override fun isRunning(): Boolean {
        synchronized(mLock) { return mNextFrameToDecode > -1 && !mDestroyed }
    }

    override fun unscheduleSelf(what: Runnable) {
        synchronized(mLock) {
            mNextFrameToDecode = -1
            mState = 0
        }
        super.unscheduleSelf(what)
    }

    override fun setVisible(visible: Boolean, restart: Boolean): Boolean {
        val changed = super.setVisible(visible, restart)
        if (!visible) {
            stop()
        } else if (restart || changed) {
            stop()
            start()
        }
        return changed
    }

    // drawing properties
    override fun setFilterBitmap(filter: Boolean) {
        mPaint.isFilterBitmap = filter
    }

    override fun setAlpha(alpha: Int) {
        mPaint.alpha = alpha
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        mPaint.setColorFilter(colorFilter)
    }

    override fun getIntrinsicWidth(): Int {
        return animatedImage.getWidth()
    }

    override fun getIntrinsicHeight(): Int {
        return animatedImage.getHeight()
    }

    override fun getOpacity(): Int {
        return PixelFormat.TRANSPARENT
    }

    companion object {
        private const val TAG = "FrameSequenceDrawable"

        /**
         * These constants are chosen to imitate common browser behavior for WebP/GIF.
         * If other decoders are added, this behavior should be moved into the WebP/GIF decoders.
         *
         *
         * Note that 0 delay is undefined behavior in the GIF standard.
         */
        private const val MIN_DELAY_MS: Long = 100
        private const val DEFAULT_DELAY_MS: Long = 100
        var sAllocatingBitmapProvider: BitmapProvider = object : BitmapProvider {
            override fun acquireBitmap(minWidth: Int, minHeight: Int): Bitmap {
                return Bitmap.createBitmap(minWidth, minHeight, Bitmap.Config.ARGB_8888)
            }

            override fun releaseBitmap(bitmap: Bitmap?) {}
        }

        /**
         * Loop only once.
         */
        const val LOOP_ONCE = 1

        /**
         * Loop continuously. The OnFinishedListener will never be called.
         */
        const val LOOP_INF = 2

        /**
         * Use loop count stored in source data, or LOOP_ONCE if not present.
         */
        const val LOOP_DEFAULT = 3
        private const val STATE_SCHEDULED = 1
        private const val STATE_DECODING = 2
        private const val STATE_WAITING_TO_SWAP = 3
        private const val STATE_READY_TO_SWAP = 4

        private fun acquireAndValidateBitmap(
            bitmapProvider: BitmapProvider,
            minWidth: Int, minHeight: Int
        ): Bitmap {
            val bitmap = bitmapProvider.acquireBitmap(minWidth, minHeight)
            require(!(bitmap.width < minWidth || bitmap.height < minHeight || bitmap.config != Bitmap.Config.ARGB_8888)) { "Invalid bitmap provided" }
            return bitmap
        }
    }
}