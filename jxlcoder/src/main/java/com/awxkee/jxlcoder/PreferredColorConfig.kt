/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 23/9/2023
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

import android.os.Build
import androidx.annotation.RequiresApi

enum class PreferredColorConfig(internal val value: Int) {
    /**
     * Default then library will determine the best color space by itself
     * @property RGBA_8888 will clip out HDR content if some
     * @property RGBA_F16 available only from 26+ OS version
     * @property RGB_565 will clip out HDR and 8bit content
     * @property RGBA_1010102 supported only on os 33+
     * @property HARDWARE supported only on OS 29+ and have default android HARDWARE bitmap limitations
     */
    DEFAULT(1),
    RGBA_8888(2),

    @RequiresApi(Build.VERSION_CODES.O)
    RGBA_F16(3),
    RGB_565(4),

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    RGBA_1010102(5),

    @RequiresApi(Build.VERSION_CODES.Q)
    HARDWARE(6),
}