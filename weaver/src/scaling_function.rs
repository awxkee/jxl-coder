/*
 * Copyright (c) Radzivon Bartoshyk. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

use pic_scale::ResamplingFunction;

#[repr(C)]
#[derive(Copy, Clone)]
pub enum ScalingFunction {
    Bilinear = 1,
    Nearest = 2,
    Cubic = 3,
    Mitchell = 4,
    Lanczos = 5,
    CatmullRom = 6,
    Hermite = 7,
    BSpline = 8,
    Bicubic = 9,
    Box = 10,
}

impl From<usize> for ScalingFunction {
    fn from(value: usize) -> Self {
        match value {
            1 => ScalingFunction::Bilinear,
            2 => ScalingFunction::Nearest,
            3 => ScalingFunction::Cubic,
            4 => ScalingFunction::Mitchell,
            5 => ScalingFunction::Lanczos,
            6 => ScalingFunction::CatmullRom,
            7 => ScalingFunction::Hermite,
            8 => ScalingFunction::BSpline,
            9 => ScalingFunction::Bicubic,
            10 => ScalingFunction::Box,
            _ => ScalingFunction::Bilinear,
        }
    }
}
impl ScalingFunction {
    pub fn to_resampling_function(&self) -> ResamplingFunction {
        match self {
            ScalingFunction::Bilinear => ResamplingFunction::Bilinear,
            ScalingFunction::Nearest => ResamplingFunction::Nearest,
            ScalingFunction::Cubic => ResamplingFunction::Cubic,
            ScalingFunction::Mitchell => ResamplingFunction::MitchellNetravalli,
            ScalingFunction::Lanczos => ResamplingFunction::Lanczos3,
            ScalingFunction::CatmullRom => ResamplingFunction::CatmullRom,
            ScalingFunction::Hermite => ResamplingFunction::Hermite,
            ScalingFunction::BSpline => ResamplingFunction::BSpline,
            ScalingFunction::Bicubic => ResamplingFunction::Bicubic,
            ScalingFunction::Box => ResamplingFunction::Box,
        }
    }
}
