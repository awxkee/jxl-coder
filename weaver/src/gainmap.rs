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

//! Conversion from Android gainmap parameters to ISO 21496-1 metadata.

use anyhow::{ensure, Result};
use jixel::{GainMapFloats, IsoGainMap};

#[derive(Clone, Copy)]
pub(crate) struct GainMapParameters {
    pub(crate) ratio_min: [f32; 3],
    pub(crate) ratio_max: [f32; 3],
    pub(crate) gamma: [f32; 3],
    pub(crate) epsilon_sdr: [f32; 3],
    pub(crate) epsilon_hdr: [f32; 3],
    pub(crate) display_ratio_sdr: f32,
    pub(crate) display_ratio_hdr: f32,
    pub(crate) hdr_base: bool,
    pub(crate) use_base_color_space: bool,
}

impl GainMapParameters {
    pub(crate) fn to_iso(self) -> Result<IsoGainMap> {
        for channel in 0..3 {
            ensure!(
                self.ratio_min[channel].is_finite()
                    && self.ratio_min[channel] > 0.0
                    && self.ratio_max[channel].is_finite()
                    && self.ratio_max[channel] >= self.ratio_min[channel],
                "Gainmap ratios must be finite, positive, and ordered"
            );
            ensure!(
                self.gamma[channel].is_finite() && self.gamma[channel] > 0.0,
                "Gainmap gamma must be finite and positive"
            );
            ensure!(
                self.epsilon_sdr[channel].is_finite() && self.epsilon_hdr[channel].is_finite(),
                "Gainmap offsets must be finite"
            );
        }
        ensure!(
            self.display_ratio_sdr.is_finite()
                && self.display_ratio_sdr >= 1.0
                && self.display_ratio_hdr.is_finite()
                && self.display_ratio_hdr >= self.display_ratio_sdr,
            "Gainmap display ratios must be finite, at least one, and ordered"
        );

        let (base_offset, alternate_offset, base_headroom, alternate_headroom) = if self.hdr_base {
            (
                self.epsilon_hdr,
                self.epsilon_sdr,
                self.display_ratio_hdr,
                self.display_ratio_sdr,
            )
        } else {
            (
                self.epsilon_sdr,
                self.epsilon_hdr,
                self.display_ratio_sdr,
                self.display_ratio_hdr,
            )
        };
        Ok(IsoGainMap::from_floats(&GainMapFloats {
            min: self.ratio_min.map(f32::log2),
            max: self.ratio_max.map(f32::log2),
            // Android stores the decoding exponent; ISO stores its reciprocal.
            gamma: self.gamma.map(f32::recip),
            base_offset,
            alternate_offset,
            base_hdr_headroom: base_headroom.log2(),
            alternate_hdr_headroom: alternate_headroom.log2(),
            // Version 0 leaves this flag clear, including for an HDR base.
            // Headroom ordering determines whether the gain is applied inversely.
            backward_direction: false,
            use_base_color_space: self.use_base_color_space,
        })?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parameters() -> GainMapParameters {
        GainMapParameters {
            ratio_min: [0.5, 1.0, 2.0],
            ratio_max: [2.0, 4.0, 8.0],
            gamma: [0.5, 1.0, 2.0],
            epsilon_sdr: [0.01, 0.02, 0.03],
            epsilon_hdr: [0.04, 0.05, 0.06],
            display_ratio_sdr: 2.0,
            display_ratio_hdr: 8.0,
            hdr_base: false,
            use_base_color_space: true,
        }
    }

    #[test]
    fn preserves_android_gain_math_through_iso_serialization() {
        let android = parameters();
        let metadata = android.to_iso().unwrap();
        let parsed = IsoGainMap::from_metadata(&metadata.to_metadata().unwrap()).unwrap();
        let iso = parsed.to_floats();
        assert_eq!(iso.min, [-1.0, 0.0, 1.0]);
        assert_eq!(iso.max, [1.0, 2.0, 3.0]);
        assert_eq!(iso.gamma, [2.0, 1.0, 0.5]);
        assert_eq!(iso.base_hdr_headroom, 1.0);
        assert_eq!(iso.alternate_hdr_headroom, 3.0);
        assert!(!iso.backward_direction);
        assert!(iso.use_base_color_space);
        for c in 0..3 {
            let sample = 0.37_f32;
            let base = 0.25_f32;
            let android_log_gain = android.ratio_min[c].log2()
                + (android.ratio_max[c].log2() - android.ratio_min[c].log2())
                    * sample.powf(android.gamma[c]);
            let iso_log_gain =
                iso.min[c] + (iso.max[c] - iso.min[c]) * sample.powf(1.0 / iso.gamma[c]);
            let expected =
                (base + android.epsilon_sdr[c]) * android_log_gain.exp2() - android.epsilon_hdr[c];
            let actual =
                (base + iso.base_offset[c]) * iso_log_gain.exp2() - iso.alternate_offset[c];
            assert!((expected - actual).abs() < 1e-6);
        }
    }

    #[test]
    fn hdr_base_swaps_offsets_and_headrooms() {
        let mut android = parameters();
        android.hdr_base = true;
        android.use_base_color_space = false;
        let iso = android.to_iso().unwrap().to_floats();
        assert!(!iso.backward_direction);
        assert!(!iso.use_base_color_space);
        assert_eq!(iso.base_hdr_headroom, 3.0);
        assert_eq!(iso.alternate_hdr_headroom, 1.0);
        assert_eq!(iso.base_offset, android.epsilon_hdr);
        assert_eq!(iso.alternate_offset, android.epsilon_sdr);
    }

    #[test]
    fn rejects_invalid_gainmap_parameters() {
        for invalid in [0.0, -1.0, f32::NAN, f32::INFINITY] {
            let mut p = parameters();
            p.ratio_min[0] = invalid;
            assert!(p.to_iso().is_err());
            let mut p = parameters();
            p.gamma[0] = invalid;
            assert!(p.to_iso().is_err());
            let mut p = parameters();
            p.display_ratio_hdr = invalid;
            assert!(p.to_iso().is_err());
        }
        let mut p = parameters();
        p.ratio_max[0] = 0.25;
        assert!(p.to_iso().is_err());
        let mut p = parameters();
        p.display_ratio_sdr = 0.5;
        assert!(p.to_iso().is_err());
    }

    fn box_payload<'a>(data: &'a [u8], kind: &[u8; 4]) -> Option<&'a [u8]> {
        let mut offset = 0;
        while offset + 8 <= data.len() {
            let size = u32::from_be_bytes(data[offset..offset + 4].try_into().unwrap()) as usize;
            assert!(size >= 8 && offset + size <= data.len());
            if &data[offset + 4..offset + 8] == kind {
                return Some(&data[offset + 8..offset + size]);
            }
            offset += size;
        }
        None
    }

    #[test]
    fn encodes_gainmap_bundle_alongside_exif_and_rgba_base() {
        use crate::jxl_decode::{decode_packed_jxl, read_jxl_info, PackedJxl};
        use jixel::{EncodeConfig, GainMap};

        let metadata = parameters().to_iso().unwrap();
        let map_pixels: Vec<u8> = (0..32).map(|i| (i * 7) as u8).collect();
        let rgba: Vec<u8> = (0..32 * 16)
            .flat_map(|i| [(i % 251) as u8, 83, 211, 170])
            .collect();
        let exif = b"II\x2a\0\x08\0\0\0\0\0\0\0\0\0".to_vec();
        let map = GainMap::gray8(map_pixels.clone(), 8, 4, metadata).with_lossless(true);
        let config = EncodeConfig::default()
            .with_lossless(true)
            .with_num_threads(1)
            .with_exif(exif)
            .with_gain_map(map);
        let encoded = jixel::encode_image_with_alpha(&rgba, 32, 16, &config).unwrap();
        assert!(box_payload(&encoded, b"Exif").is_some());
        let bundle = box_payload(&encoded, b"jhgm").expect("missing gainmap box");
        assert_eq!(bundle[0], 0);
        let metadata_size = u16::from_be_bytes(bundle[1..3].try_into().unwrap()) as usize;
        assert_eq!(
            IsoGainMap::from_metadata(&bundle[3..3 + metadata_size]).unwrap(),
            metadata
        );
        let color_size_offset = 3 + metadata_size;
        let icc_size_offset = color_size_offset + 1 + bundle[color_size_offset] as usize;
        let icc_size = u32::from_be_bytes(
            bundle[icc_size_offset..icc_size_offset + 4]
                .try_into()
                .unwrap(),
        ) as usize;
        assert_eq!(icc_size, 0);
        let gain_stream = &bundle[icc_size_offset + 4..];
        assert_eq!(read_jxl_info(gain_stream).unwrap(), (8, 4, 8));
        let PackedJxl::Regular(gain) = decode_packed_jxl(gain_stream).unwrap() else {
            panic!("expected 8-bit gainmap");
        };
        for (pixel, value) in gain.data.chunks_exact(4).zip(map_pixels) {
            assert_eq!(pixel, [value, value, value, 255]);
        }
        let PackedJxl::Regular(base) = decode_packed_jxl(&encoded).unwrap() else {
            panic!("expected 8-bit base");
        };
        assert_eq!((base.width, base.height), (32, 16));
        assert_eq!(base.data, rgba);
    }

    #[test]
    fn encodes_rgb_gainmaps_with_10_bit_bases_and_alternate_profile() {
        use crate::jxl_decode::{decode_packed_jxl, read_jxl_info, PackedJxl};
        use jixel::{EncodeConfig, GainMap};

        let metadata = parameters().to_iso().unwrap();
        let alternate_icc = moxcms::ColorProfile::new_display_p3().encode().unwrap();
        for (alpha, lossless) in [(false, false), (true, false), (false, true), (true, true)] {
            let map = GainMap::rgb8(vec![42; 8 * 4 * 3], 8, 4, metadata)
                .with_alternate_icc_profile(alternate_icc.clone())
                .with_lossless(lossless);
            let config = EncodeConfig::default()
                .with_lossless(lossless)
                .with_num_threads(1)
                .with_gain_map(map);
            let encoded = if alpha {
                jixel::encode_image_with_alpha_10bit(&vec![512; 32 * 16 * 4], 32, 16, &config)
            } else {
                jixel::encode_image_10bit(&vec![512; 32 * 16 * 3], 32, 16, &config)
            }
            .unwrap();
            assert_eq!(read_jxl_info(&encoded).unwrap(), (32, 16, 10));
            let bundle = box_payload(&encoded, b"jhgm").unwrap();
            let metadata_size = u16::from_be_bytes(bundle[1..3].try_into().unwrap()) as usize;
            let color_offset = 3 + metadata_size;
            let icc_offset = color_offset + 1 + bundle[color_offset] as usize;
            let icc_size =
                u32::from_be_bytes(bundle[icc_offset..icc_offset + 4].try_into().unwrap()) as usize;
            assert!(icc_size > 0, "alternate profile must be retained");
            let gain_stream = &bundle[icc_offset + 4 + icc_size..];
            assert_eq!(read_jxl_info(gain_stream).unwrap(), (8, 4, 8));
            let PackedJxl::HighBitDepth(base) = decode_packed_jxl(&encoded).unwrap() else {
                panic!("expected a high-bit-depth base");
            };
            assert_eq!((base.width, base.height), (32, 16));
            assert_eq!(base.has_real_alpha, alpha);
        }
    }
}
