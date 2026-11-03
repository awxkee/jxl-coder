//! JPEG XL gainmap parsing and the SDR fallback shared by all Android ABIs.

use crate::jxl_decode::{
    decode_packed_jxl, limited_decoder_options, BitDepth, DecodedJxlPacket, GainmapBase,
    GainmapSampleFormat, PackedJxl,
};
use crate::scaling::{internal_scale_f16, internal_scale_u16, scale_to_fill_crop};
use crate::WeaveScaleMode;
use anyhow::{bail, ensure, Context, Result};
use core::f16;
use gainforge::IsoGainMap;
use jxl::api::{Event, JxlAuxBoxType, JxlColorProfile, JxlDecoder, JxlSignature};
use moxcms::{ColorProfile, DataColorSpace, Layout, ToneReprCurve, TransformOptions};
use pic_scale::{ImageStore, ImageStoreMut, ResamplingFunction, Scaler, WorkloadStrategy};

/// The version-0 `jhgm` envelope. Box extraction, decompression and color
/// profile decoding remain in `jxl`; only the length-prefixed envelope is local.
struct GainMapBundle<'a> {
    metadata: &'a [u8],
    color_profile: Option<JxlColorProfile>,
    gain_map: &'a [u8],
}

impl<'a> GainMapBundle<'a> {
    fn parse(mut payload: &'a [u8]) -> Result<Self> {
        fn take<'a>(payload: &mut &'a [u8], length: usize) -> Result<&'a [u8]> {
            let (field, rest) = payload
                .split_at_checked(length)
                .context("Truncated JPEG XL gainmap bundle")?;
            *payload = rest;
            Ok(field)
        }

        let version = take(&mut payload, 1)?[0];
        ensure!(
            version == 0,
            "Unsupported JPEG XL gainmap version {version}"
        );
        let metadata_length = u16::from_be_bytes(take(&mut payload, 2)?.try_into()?) as usize;
        let metadata = take(&mut payload, metadata_length)?;
        let color_length = take(&mut payload, 1)?[0] as usize;
        let color_encoding = take(&mut payload, color_length)?;
        let icc_length = u32::from_be_bytes(take(&mut payload, 4)?.try_into()?) as usize;
        let compressed_icc = take(&mut payload, icc_length)?;
        let color_profile = if color_encoding.is_empty() && compressed_icc.is_empty() {
            None
        } else {
            Some(JxlColorProfile::from_raw_bytes(
                color_encoding,
                compressed_icc,
            )?)
        };
        Ok(Self {
            metadata,
            color_profile,
            gain_map: payload,
        })
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct GainMetadata {
    pub min: [f32; 3],
    pub max: [f32; 3],
    /// ISO encoding gamma, reciprocal of Android's decoding exponent.
    pub gamma: [f32; 3],
    pub base_offset: [f32; 3],
    pub alternate_offset: [f32; 3],
    pub base_headroom: f32,
    pub alternate_headroom: f32,
    pub use_base_color_space: bool,
}

impl GainMetadata {
    fn parse(bytes: &[u8]) -> Result<Self> {
        ensure!(bytes.len() >= 5, "Truncated ISO gainmap metadata");
        let iso = IsoGainMap::from_metadata(bytes).map_err(|e| anyhow::anyhow!("{e:?}"))?;
        // ISO version 0 reserves the backward-direction flag.
        ensure!(
            !iso.backward_direction,
            "Unsupported gainmap backward direction"
        );
        let m = Self {
            min: iso.map_min().map(|x| x as f32),
            max: iso.map_max().map(|x| x as f32),
            gamma: iso.gain_map_gamma().map(|x| x as f32),
            base_offset: iso.map_base_offset().map(|x| x as f32),
            alternate_offset: iso.map_alternate_offset().map(|x| x as f32),
            base_headroom: iso.base_hdr_headroom() as f32,
            alternate_headroom: iso.alternate_hdr_headroom() as f32,
            use_base_color_space: iso.use_base_color_space,
        };
        for c in 0..3 {
            ensure!(
                m.min[c].is_finite()
                    && m.max[c].is_finite()
                    && m.max[c] >= m.min[c]
                    && m.min[c].exp2() > 0.0
                    && m.max[c].exp2().is_finite()
                    && m.gamma[c].is_finite()
                    && m.gamma[c] > 0.0
                    && m.gamma[c].recip().is_finite()
                    && m.base_offset[c].is_finite()
                    && m.alternate_offset[c].is_finite(),
                "Invalid gainmap channel metadata"
            );
        }
        ensure!(
            m.base_headroom >= 0.0
                && m.base_headroom.exp2().is_finite()
                && m.alternate_headroom >= 0.0
                && m.alternate_headroom.exp2().is_finite(),
            "Invalid gainmap HDR headroom"
        );
        Ok(m)
    }

    pub(crate) fn hdr_base(self) -> bool {
        self.base_headroom > self.alternate_headroom
    }

    fn apply(self, pixel: &mut [f32], gain: &[u16], weight: f32) {
        for c in 0..3 {
            let sample = (gain[c] as f32 / 65535.0).powf(self.gamma[c].recip());
            let log_gain = self.min[c] + (self.max[c] - self.min[c]) * sample;
            pixel[c] = (pixel[c] + self.base_offset[c]) * (log_gain * weight).exp2()
                - self.alternate_offset[c];
        }
    }
}

pub(crate) struct DecodedGainMap {
    pub metadata: GainMetadata,
    pub image: DecodedJxlPacket<u16>,
    alternate_profile: Option<ColorProfile>,
    base_profile: ColorProfile,
}

/// Scan headers rather than decoding the base twice. This also finds boxes
/// after the codestream, size-zero trailing boxes, and Brotli-compressed boxes.
pub(crate) fn read_gainmap(data: &[u8]) -> Result<Option<DecodedGainMap>> {
    if jxl::api::check_signature(data) != JxlSignature::Container {
        return Ok(None);
    }
    let mut options = limited_decoder_options();
    options.scan_frames_only = true;
    options.request_aux_boxes = vec![JxlAuxBoxType::HDR_GAIN_MAP];
    let mut decoder = JxlDecoder::new(options);
    decoder.close_input();
    let mut input = data;
    loop {
        match decoder.process(&mut input, None, None)? {
            Event::Complete => break,
            Event::NeedMoreInput { .. } => bail!("Truncated JPEG XL gainmap container"),
            _ => {}
        }
    }
    let boxes = decoder.aux_boxes(JxlAuxBoxType::HDR_GAIN_MAP);
    let trailing = decoder
        .trailing_box()
        .filter(|b| b.box_type() == JxlAuxBoxType::HDR_GAIN_MAP);
    ensure!(
        boxes.len() + usize::from(trailing.is_some()) <= 1,
        "Multiple JPEG XL gainmaps"
    );
    let Some(aux) = boxes.first().or(trailing) else {
        return Ok(None);
    };
    let payload = aux.data(if trailing.is_some() { input } else { &[] })?;
    let bundle = GainMapBundle::parse(&payload).context("Invalid JPEG XL gainmap bundle")?;
    let metadata = GainMetadata::parse(bundle.metadata)?;
    let alternate_profile = bundle
        .color_profile
        .as_ref()
        .map(|p| {
            let icc = p
                .try_as_icc()
                .context("Gainmap alternate profile has no ICC representation")?;
            Ok::<_, anyhow::Error>(rgba_profile(ColorProfile::new_from_slice(&icc)?))
        })
        .transpose()?;
    let image = into_u16(decode_packed_jxl(bundle.gain_map)?);
    let base_profile = decoder
        .embedded_color_profile()
        .and_then(|p| p.try_as_icc())
        .map(|p| ColorProfile::new_from_slice(&p))
        .transpose()?
        .map(rgba_profile)
        .unwrap_or_else(ColorProfile::new_srgb);
    Ok(Some(DecodedGainMap {
        metadata,
        image,
        alternate_profile,
        base_profile,
    }))
}

fn into_u16(image: PackedJxl) -> DecodedJxlPacket<u16> {
    match image {
        PackedJxl::HighBitDepth(image) => image,
        PackedJxl::Regular(image) => DecodedJxlPacket {
            data: image.data.into_iter().map(|v| u16::from(v) * 257).collect(),
            width: image.width,
            height: image.height,
            icc: image.icc,
            bit_depth: BitDepth::new(16).unwrap(),
            has_real_alpha: image.has_real_alpha,
        },
    }
}

/// Scale numerical gain samples with pic-scale, without color conversion or
/// alpha premultiplication. Fill crops use the base image's coordinate grid.
fn scale_gainmap(
    image: &DecodedJxlPacket<u16>,
    base: (usize, usize),
    output: (usize, usize),
    mode: WeaveScaleMode,
) -> Result<Vec<u16>> {
    let (bw, bh) = base;
    let (ow, oh) = output;
    let (cx, cy, cw, ch) = if mode == WeaveScaleMode::ScaleToFill {
        scale_to_fill_crop(bw, bh, ow, oh)
    } else {
        (0, 0, bw, bh)
    };
    let scaler = Scaler::new(ResamplingFunction::MitchellNetravalli)
        .set_workload_strategy(WorkloadStrategy::PreferQuality);
    let resize = |source: &ImageStore<'_, u16, 4>,
                  width: usize,
                  height: usize|
     -> Result<ImageStore<'static, u16, 4>> {
        let length = width
            .checked_mul(height)
            .and_then(|n| n.checked_mul(4))
            .context("Gainmap output dimensions overflow")?;
        let mut pixels = Vec::new();
        pixels.try_reserve_exact(length)?;
        pixels.resize(length, 0);
        let mut target = ImageStoreMut::borrow(&mut pixels, width, height)?;
        target.bit_depth = 16;
        let plan = scaler.plan_rgba_resampling16(source.size(), target.size(), false, 16)?;
        plan.resample(source, &mut target)?;
        let mut result = ImageStore::new(pixels, width, height)?;
        result.bit_depth = 16;
        Ok(result)
    };
    let mut source = ImageStore::borrow(&image.data, image.width, image.height)?;
    source.bit_depth = 16;
    let aligned;
    let source = if (cx, cy, cw, ch) != (0, 0, bw, bh) {
        // Only align the cropped axis. This preserves fractional map positions
        // without expanding both axes to the full base-image resolution.
        let crop_x = cw != bw;
        let crop_y = ch != bh;
        let width = if crop_x { bw } else { image.width };
        let height = if crop_y { bh } else { image.height };
        aligned = resize(&source, width, height)?;
        aligned.crop(
            if crop_x { cx } else { 0 },
            if crop_y { cy } else { 0 },
            if crop_x { cw } else { width },
            if crop_y { ch } else { height },
        )?
    } else {
        source
    };
    let mut pixels = resize(&source, ow, oh)?.buffer.into_owned();
    for pixel in pixels.as_chunks_mut::<4>().0 {
        pixel[3] = u16::MAX;
    }
    Ok(pixels)
}

fn with_gamma(profile: &ColorProfile, gamma: f32) -> ColorProfile {
    let mut result = profile.clone();
    result.cicp = None;
    result.red_trc = Some(ToneReprCurve::Parametric(vec![gamma]));
    result.green_trc = result.red_trc.clone();
    result.blue_trc = result.red_trc.clone();
    // The gain operation takes place in RGB primaries, not an ICC CLUT.
    result.lut_a_to_b_perceptual = None;
    result.lut_a_to_b_colorimetric = None;
    result.lut_a_to_b_saturation = None;
    result.lut_b_to_a_perceptual = None;
    result.lut_b_to_a_colorimetric = None;
    result.lut_b_to_a_saturation = None;
    result
}

// jxl-rs expands gray pixels to the requested RGBA layout, while the embedded
// and output ICC profiles still describe one channel. Replicate that transfer
// curve in neutral RGB before creating an RGBA CMS transform.
fn rgba_profile(profile: ColorProfile) -> ColorProfile {
    if profile.color_space != DataColorSpace::Gray {
        return profile;
    }
    let mut rgb = ColorProfile::new_srgb();
    let trc = profile
        .gray_trc
        .unwrap_or(ToneReprCurve::Parametric(vec![1.0]));
    rgb.red_trc = Some(trc.clone());
    rgb.green_trc = Some(trc.clone());
    rgb.blue_trc = Some(trc);
    rgb
}

pub(crate) struct PreparedGainMap {
    pub base: PackedJxl,
    pub map: DecodedJxlPacket<u16>,
    pub metadata: GainMetadata,
    /// The system bitmap uses these primaries with a common gamma of 2.2.
    #[cfg(target_os = "android")]
    pub profile: ColorProfile,
}

pub(crate) fn prepare_gainmap(
    base: GainmapBase,
    mut map: DecodedGainMap,
    sdk: i32,
    target_width: i32,
    target_height: i32,
    mode: WeaveScaleMode,
) -> Result<PreparedGainMap> {
    let GainmapBase {
        image: base,
        format,
    } = base;
    let (base_width, base_height, has_alpha) = (base.width, base.height, base.has_real_alpha);
    let icc = base.icc;
    let mut scaled = match format {
        GainmapSampleFormat::U16 => internal_scale_u16(
            std::borrow::Cow::Owned(base.data),
            base_width * 4,
            base_width as u32,
            base_height as u32,
            target_width,
            target_height,
            16,
            has_alpha,
            mode,
        )?,
        GainmapSampleFormat::F16 => internal_scale_f16(
            base.data,
            base_width,
            base_height,
            target_width,
            target_height,
            has_alpha,
            mode,
        )?,
    };
    let (width, height) = (scaled.width, scaled.height);
    let source = icc
        .as_ref()
        .map(|icc| ColorProfile::new_from_slice(icc))
        .transpose()?
        .map(rgba_profile)
        .unwrap_or_else(ColorProfile::new_srgb);
    let working = if map.metadata.use_base_color_space {
        &map.base_profile
    } else {
        map.alternate_profile.as_ref().unwrap_or(&map.base_profile)
    };
    let linear = with_gamma(working, 1.0);
    let system = sdk >= 34;
    let rebase = map.metadata.hdr_base() && sdk < 36;
    let output_profile = if system {
        with_gamma(working, 2.2)
    } else {
        ColorProfile::new_srgb()
    };
    let resize_system_map = system && (width, height) != (base_width, base_height);
    let gains = if rebase || resize_system_map {
        scale_gainmap(&map.image, (base_width, base_height), (width, height), mode)?
    } else {
        Vec::new()
    };

    // Preserve extended HDR/XYB values through color conversion until the
    // inverse gain has been applied; LUT-only transforms clip their inputs.
    let options = TransformOptions {
        allow_extended_range_rgb_xyz: true,
        ..TransformOptions::default()
    };
    let to_linear = source.create_transform_f32(Layout::Rgba, &linear, Layout::Rgba, options)?;
    let to_output =
        linear.create_transform_f32(Layout::Rgba, &output_profile, Layout::Rgba, options)?;
    // Color management and gain math need extended f32 values, but only for
    // one row. Reuse both row buffers and overwrite the u16 texture in place.
    let row_samples = width * 4;
    let mut encoded_row = vec![0.0f32; row_samples];
    let mut linear_row = vec![0.0f32; row_samples];
    let quantize = |v: f32| (v.clamp(0.0, 1.0) * 65535.0).round() as u16;
    for (y, row) in scaled
        .buffer
        .borrow_mut()
        .chunks_exact_mut(row_samples)
        .enumerate()
    {
        for (value, sample) in encoded_row.iter_mut().zip(row.iter()) {
            *value = match format {
                GainmapSampleFormat::U16 => *sample as f32 / 65535.0,
                GainmapSampleFormat::F16 => f16::from_bits(*sample) as f32,
            };
        }
        to_linear.transform(&encoded_row, &mut linear_row)?;
        if rebase {
            let gain_row = &gains[y * row_samples..(y + 1) * row_samples];
            for (pixel, gain) in linear_row
                .as_chunks_mut::<4>()
                .0
                .iter_mut()
                .zip(gain_row.as_chunks::<4>().0)
            {
                map.metadata.apply(pixel, gain, -1.0);
            }
        }
        // SDR bases are already the SDR rendition; only HDR bases need the
        // inverse gain above. Neither operation changes the alpha channel.
        to_output.transform(&linear_row, &mut encoded_row)?;
        for (pixel, converted) in row
            .as_chunks_mut::<4>()
            .0
            .iter_mut()
            .zip(encoded_row.as_chunks::<4>().0)
        {
            let alpha = match format {
                GainmapSampleFormat::U16 => pixel[3],
                GainmapSampleFormat::F16 => quantize(f16::from_bits(pixel[3]) as f32),
            };
            for (sample, value) in pixel[..3].iter_mut().zip(&converted[..3]) {
                *sample = quantize(*value);
            }
            pixel[3] = alpha;
        }
    }
    if rebase {
        std::mem::swap(
            &mut map.metadata.base_offset,
            &mut map.metadata.alternate_offset,
        );
        std::mem::swap(
            &mut map.metadata.base_headroom,
            &mut map.metadata.alternate_headroom,
        );
    }
    if resize_system_map {
        map.image.data = gains;
        map.image.width = width;
        map.image.height = height;
    }
    let data = match scaled.buffer {
        pic_scale::BufferStore::Owned(data) => data,
        pic_scale::BufferStore::Borrowed(data) => data.to_vec(),
    };
    map.metadata.use_base_color_space = true;
    Ok(PreparedGainMap {
        base: PackedJxl::HighBitDepth(DecodedJxlPacket {
            data,
            width,
            height,
            icc: None,
            bit_depth: BitDepth::new(16)?,
            has_real_alpha: has_alpha,
        }),
        map: map.image,
        metadata: map.metadata,
        #[cfg(target_os = "android")]
        profile: output_profile,
    })
}

#[cfg(all(test, any(target_arch = "aarch64", target_arch = "arm")))]
mod tests {
    use super::*;
    use crate::jxl_decode::decode_gainmap_base;
    use jixel::{EncodeConfig, GainMap, GainMapFloats, IsoGainMap};

    fn fixture(hdr: bool, alternate: bool, lossless: bool) -> Vec<u8> {
        let metadata = IsoGainMap::from_floats(&GainMapFloats {
            min: [1.0; 3],
            max: [1.0; 3],
            gamma: [0.5, 1.0, 2.0],
            base_offset: [if hdr { 0.0625 } else { 0.125 }; 3],
            alternate_offset: [if hdr { 0.125 } else { 0.0625 }; 3],
            base_hdr_headroom: if hdr { 2.0 } else { 0.0 },
            alternate_hdr_headroom: if hdr { 0.0 } else { 2.0 },
            backward_direction: false,
            use_base_color_space: !alternate,
        })
        .unwrap();
        let mut map = GainMap::rgb8(vec![127; 3 * 2 * 3], 3, 2, metadata).with_lossless(true);
        if alternate {
            map = map.with_alternate_icc_profile(ColorProfile::new_display_p3().encode().unwrap());
        }
        let config = EncodeConfig::default()
            .with_num_threads(1)
            .with_lossless(lossless)
            .with_icc_profile(with_gamma(&ColorProfile::new_srgb(), 1.0).encode().unwrap())
            .with_gain_map(map);
        jixel::encode_image_with_alpha(&[192, 128, 64, 37].repeat(8 * 4), 8, 4, &config).unwrap()
    }

    fn prepare(data: &[u8], sdk: i32) -> PreparedGainMap {
        prepare_gainmap(
            decode_gainmap_base(data).unwrap(),
            read_gainmap(data).unwrap().unwrap(),
            sdk,
            -1,
            -1,
            WeaveScaleMode::JustResize,
        )
        .unwrap()
    }

    #[test]
    fn integer_gainmap_base_retains_all_16_bits() {
        let pixels = [12345u16, 34567, 54321, 32109].repeat(8 * 4);
        let config = EncodeConfig::default()
            .with_lossless(true)
            .with_num_threads(1);
        let encoded = jixel::encode_image_with_alpha_16bit(&pixels, 8, 4, &config).unwrap();
        let decoded = decode_gainmap_base(&encoded).unwrap();
        assert_eq!(decoded.format, GainmapSampleFormat::U16);
        assert_eq!(decoded.image.data, pixels);
    }

    #[test]
    fn xy_bases_with_icc_use_half_float_storage() {
        let decoded = decode_gainmap_base(&fixture(false, true, false)).unwrap();
        assert_eq!(decoded.format, GainmapSampleFormat::F16);
        assert_eq!(decoded.image.data.len() * size_of::<u16>(), 8 * 4 * 8);
    }

    #[test]
    fn half_float_hdr_values_are_not_clipped_before_inverse_gain() {
        let profile = with_gamma(&ColorProfile::new_srgb(), 1.0);
        let base = GainmapBase {
            format: GainmapSampleFormat::F16,
            image: DecodedJxlPacket {
                data: [-0.125f32, 1.5, 4.0, 0.5]
                    .map(|v| (v as f16).to_bits())
                    .repeat(8 * 4),
                width: 8,
                height: 4,
                icc: Some(profile.encode().unwrap()),
                bit_depth: BitDepth::new(16).unwrap(),
                has_real_alpha: true,
            },
        };
        let map = DecodedGainMap {
            metadata: GainMetadata {
                min: [1.0; 3],
                max: [1.0; 3],
                gamma: [1.0; 3],
                base_offset: [0.0; 3],
                alternate_offset: [0.0; 3],
                base_headroom: 2.0,
                alternate_headroom: 0.0,
                use_base_color_space: true,
            },
            image: DecodedJxlPacket {
                data: vec![65535; 2 * 4],
                width: 2,
                height: 1,
                icc: None,
                bit_depth: BitDepth::new(16).unwrap(),
                has_real_alpha: false,
            },
            alternate_profile: None,
            base_profile: profile,
        };
        let result = prepare_gainmap(base, map, 33, -1, -1, WeaveScaleMode::JustResize).unwrap();
        let PackedJxl::HighBitDepth(base) = result.base else {
            panic!()
        };
        let green = 1.055 * 0.75f32.powf(1.0 / 2.4) - 0.055;
        // ICC matrix quantization can introduce a small cross-channel error.
        assert!(base.data[0] < 128, "{:?}", &base.data[..4]);
        assert!(
            (base.data[1] as f32 / 65535.0 - green).abs() < 0.002,
            "{:?}",
            &base.data[..4]
        );
        assert_eq!(base.data[2], 65535);
        assert_eq!(base.data[3], 32768);
    }

    #[test]
    fn parses_jixel_metadata_samples_and_alternate_color_space() {
        for lossless in [false, true] {
            for alternate in [false, true] {
                let data = fixture(false, alternate, lossless);
                let map = read_gainmap(&data).unwrap().unwrap();
                assert_eq!((map.image.width, map.image.height), (3, 2));
                assert_eq!(map.metadata.gamma, [0.5, 1.0, 2.0]);
                assert_eq!(map.alternate_profile.is_some(), alternate);
                assert_eq!(&map.image.data[..3], &[127 * 257; 3]);
                let result = prepare(&data, 34);
                assert!(result.metadata.use_base_color_space);
                assert_eq!((result.map.width, result.map.height), (3, 2));
            }
        }
    }

    #[test]
    fn legacy_hdr_to_sdr_uses_inverse_gain_and_preserves_alpha() {
        let result = prepare(&fixture(true, false, true), 33);
        let PackedJxl::HighBitDepth(base) = result.base else {
            panic!()
        };
        for c in 0..3 {
            let input = [192.0f32, 128.0, 64.0][c] / 255.0;
            let linear = (input + 0.0625) / 2.0 - 0.125;
            let srgb = if linear <= 0.0031308 {
                linear * 12.92
            } else {
                1.055 * linear.powf(1.0 / 2.4) - 0.055
            };
            assert!((base.data[c] as f32 / 65535.0 - srgb).abs() < 0.002);
        }
        assert_eq!(base.data[3], 37 * 257);
        assert!(!result.metadata.hdr_base());
    }

    #[test]
    fn inverse_gain_uses_each_channels_gamma_range_and_offsets() {
        let f = GainMapFloats {
            min: [0.0, 1.0, -1.0],
            max: [2.0, 3.0, 1.0],
            gamma: [0.5, 1.0, 2.0],
            base_offset: [0.01, 0.02, 0.03],
            alternate_offset: [0.04, 0.01, 0.02],
            base_hdr_headroom: 3.0,
            alternate_hdr_headroom: 0.0,
            backward_direction: false,
            use_base_color_space: true,
        };
        let metadata = IsoGainMap::from_floats(&f).unwrap();
        let config = EncodeConfig::default()
            .with_num_threads(1)
            .with_lossless(true)
            .with_icc_profile(with_gamma(&ColorProfile::new_srgb(), 1.0).encode().unwrap())
            .with_gain_map(
                GainMap::rgb8([64, 127, 192].repeat(2), 2, 1, metadata).with_lossless(true),
            );
        let encoded = jixel::encode_image(&[192, 128, 64].repeat(4 * 2), 4, 2, &config).unwrap();
        let prepared = prepare(&encoded, 33);
        let PackedJxl::HighBitDepth(base) = prepared.base else {
            panic!()
        };
        for c in 0..3 {
            let g = ([64.0f32, 127.0, 192.0][c] / 255.0).powf(1.0 / f.gamma[c]);
            let boost = (f.min[c] + (f.max[c] - f.min[c]) * g).exp2();
            let linear = (([192.0f32, 128.0, 64.0][c] / 255.0 + f.base_offset[c]) / boost
                - f.alternate_offset[c])
                .max(0.0);
            let srgb = if linear <= 0.0031308 {
                linear * 12.92
            } else {
                1.055 * linear.powf(1.0 / 2.4) - 0.055
            };
            assert!((base.data[c] as f32 / 65535.0 - srgb).abs() < 0.002);
        }
    }

    #[test]
    fn android_14_rebases_hdr_but_android_16_keeps_direction() {
        let data = fixture(true, false, true);
        for sdk in [34, 35, 36] {
            let result = prepare(&data, sdk);
            assert_eq!(result.metadata.hdr_base(), sdk >= 36);
            let PackedJxl::HighBitDepth(base) = result.base else {
                panic!()
            };
            let input = 192.0f32 / 255.0;
            let expected = if sdk >= 36 {
                input
            } else {
                (input + 0.0625) / 2.0 - 0.125
            };
            assert!((base.data[0] as f32 / 65535.0 - expected.powf(1.0 / 2.2)).abs() < 0.002);
        }
    }

    #[test]
    fn legacy_sdr_base_keeps_the_sdr_rendition() {
        let result = prepare(&fixture(false, false, true), 21);
        let PackedJxl::HighBitDepth(base) = result.base else {
            panic!()
        };
        let linear = 192.0f32 / 255.0;
        let expected = 1.055 * linear.powf(1.0 / 2.4) - 0.055;
        assert!((base.data[0] as f32 / 65535.0 - expected).abs() < 0.002);
    }

    #[test]
    fn grayscale_bases_with_alpha_and_gainmaps_decode_on_old_and_new_oses() {
        let metadata = IsoGainMap::from_floats(&GainMapFloats {
            max: [2.0; 3],
            alternate_hdr_headroom: 2.0,
            ..GainMapFloats::default()
        })
        .unwrap();
        let config = EncodeConfig::default()
            .with_num_threads(1)
            .with_lossless(true)
            .with_gain_map(GainMap::gray8(vec![127; 4 * 2], 4, 2, metadata).with_lossless(true));
        let data = jixel::encode_image_gray_alpha(&[128, 63].repeat(8 * 4), 8, 4, &config).unwrap();
        for sdk in [21, 33, 34, 36] {
            let result = prepare(&data, sdk);
            let PackedJxl::HighBitDepth(base) = result.base else {
                panic!()
            };
            assert_eq!(base.data[3], 63 * 257);
            assert!(base.data[0].abs_diff(base.data[1]) <= 2);
            assert!(base.data[0].abs_diff(base.data[2]) <= 2);
        }
    }

    #[test]
    fn reads_brotli_compressed_gainmap() {
        let data = include_bytes!("../../jxlcoder/src/androidTest/assets/gainmap-brotli.jxl");
        let map = read_gainmap(data).unwrap().unwrap();
        assert!(map.metadata.hdr_base());
        assert_eq!((map.image.width, map.image.height), (3, 2));
        let result = prepare(data, 33);
        assert!(!result.metadata.hdr_base());
    }

    fn boxes(data: &[u8]) -> Vec<&[u8]> {
        let mut rest = data;
        let mut boxes = Vec::new();
        while !rest.is_empty() {
            let size = u32::from_be_bytes(rest[..4].try_into().unwrap()) as usize;
            boxes.push(&rest[..size]);
            rest = &rest[size..];
        }
        boxes
    }

    #[test]
    fn rejects_truncated_gainmap_bundle_fields_and_oversized_lengths() {
        for alternate in [false, true] {
            let encoded = fixture(false, alternate, true);
            let chunks = boxes(&encoded);
            let payload = &chunks.iter().find(|b| &b[4..8] == b"jhgm").unwrap()[8..];
            let bundle = GainMapBundle::parse(payload).unwrap();
            assert_eq!(bundle.color_profile.is_some(), alternate);
            let header_size = payload.len() - bundle.gain_map.len();
            for size in 0..header_size {
                assert!(
                    GainMapBundle::parse(&payload[..size]).is_err(),
                    "prefix {size}"
                );
            }
            let mut bad_metadata_length = payload.to_vec();
            bad_metadata_length[1..3].fill(255);
            assert!(GainMapBundle::parse(&bad_metadata_length).is_err());
            let color_offset = 3 + bundle.metadata.len();
            let icc_offset = color_offset + 1 + payload[color_offset] as usize;
            let mut bad_icc_length = payload.to_vec();
            bad_icc_length[icc_offset..icc_offset + 4].fill(255);
            assert!(GainMapBundle::parse(&bad_icc_length).is_err());
        }
    }

    #[test]
    fn reads_gainmaps_after_codestream_with_regular_extended_and_zero_box_sizes() {
        let data = fixture(false, false, true);
        let chunks = boxes(&data);
        let gain = chunks.iter().find(|b| &b[4..8] == b"jhgm").unwrap();
        let without: Vec<u8> = chunks
            .iter()
            .filter(|b| &b[4..8] != b"jhgm")
            .flat_map(|b| b.iter().copied())
            .collect();
        for kind in 0..3 {
            let mut data = without.clone();
            match kind {
                0 => data.extend_from_slice(gain),
                1 => {
                    data.extend_from_slice(&1u32.to_be_bytes());
                    data.extend_from_slice(b"jhgm");
                    data.extend_from_slice(&(gain.len() as u64 + 8).to_be_bytes());
                    data.extend_from_slice(&gain[8..]);
                }
                _ => {
                    data.extend_from_slice(&0u32.to_be_bytes());
                    data.extend_from_slice(&gain[4..]);
                }
            }
            assert!(
                read_gainmap(&data).unwrap().is_some(),
                "box size kind {kind}"
            );
        }
    }

    #[test]
    fn rejects_malformed_gainmap_without_silently_returning_the_base() {
        let metadata = IsoGainMap::default().to_metadata().unwrap();
        for size in 0..metadata.len() {
            assert!(GainMetadata::parse(&metadata[..size]).is_err());
        }
        let data = fixture(false, false, true);
        let chunks = boxes(&data);
        let mut corrupt = Vec::new();
        for b in chunks {
            if &b[4..8] == b"jhgm" {
                let mut b = b.to_vec();
                b[8] = 255; // Unsupported bundle version.
                corrupt.extend_from_slice(&b);
            } else {
                corrupt.extend_from_slice(b);
            }
        }
        assert!(read_gainmap(&corrupt).is_err());
        assert!(read_gainmap(&data[..data.len() - 3]).is_err());
        let plain = jixel::encode_image(
            &[90; 8 * 4 * 3],
            8,
            4,
            &EncodeConfig::default().with_lossless(true),
        )
        .unwrap();
        assert!(read_gainmap(&plain).unwrap().is_none());
    }

    #[test]
    fn sampled_gainmap_uses_base_crop_even_with_rounded_map_dimensions() {
        let image = DecodedJxlPacket {
            data: (0..3 * 5)
                .flat_map(|i| [i * 1000, i * 1000, i * 1000, 65535])
                .collect(),
            width: 3,
            height: 5,
            icc: None,
            bit_depth: BitDepth::new(16).unwrap(),
            has_real_alpha: false,
        };
        let cropped = scale_gainmap(&image, (8, 16), (8, 4), WeaveScaleMode::ScaleToFill).unwrap();
        assert_eq!(cropped.len(), 8 * 4 * 4);
        // The 8x4 base crop selects rows 6..10 of the original 8x16 base.
        assert!(
            cropped[0] > 4000 && cropped[0] < 6000,
            "first sample: {}",
            cropped[0]
        );
        assert!(
            cropped[(3 * 8) * 4] > 7000 && cropped[(3 * 8) * 4] < 9000,
            "last sample: {}",
            cropped[(3 * 8) * 4]
        );
        for pixel in cropped.as_chunks::<4>().0 {
            assert_eq!(pixel[3], 65535);
        }
        let horizontal = DecodedJxlPacket {
            data: (0..3)
                .flat_map(|y| {
                    (0..5).flat_map(move |x| {
                        let value = (x * 3 + y) * 1000;
                        [value, value, value, 65535]
                    })
                })
                .collect(),
            width: 5,
            height: 3,
            icc: None,
            bit_depth: BitDepth::new(16).unwrap(),
            has_real_alpha: false,
        };
        let horizontal =
            scale_gainmap(&horizontal, (16, 8), (4, 8), WeaveScaleMode::ScaleToFill).unwrap();
        // The transposed case also exercises pic-scale's strided crop view.
        assert!(horizontal[0] > 4000 && horizontal[0] < 6000);
        assert!(horizontal[3 * 4] > 7000 && horizontal[3 * 4] < 9000);
        for mode in [
            WeaveScaleMode::JustResize,
            WeaveScaleMode::ScaleToFill,
            WeaveScaleMode::ScaleToFit,
        ] {
            let data = fixture(false, false, true);
            let result = prepare_gainmap(
                decode_gainmap_base(&data).unwrap(),
                read_gainmap(&data).unwrap().unwrap(),
                34,
                4,
                4,
                mode,
            )
            .unwrap();
            let PackedJxl::HighBitDepth(base) = result.base else {
                panic!()
            };
            assert_eq!(
                (result.map.width, result.map.height),
                (base.width, base.height)
            );
        }
    }

    #[test]
    fn gainmap_scaling_preserves_16_bit_samples_and_ignores_alpha() {
        let image = DecodedJxlPacket {
            data: (0..6)
                .flat_map(|i| [1001, 32769, 65003, if i % 2 == 0 { 0 } else { 65535 }])
                .collect(),
            width: 3,
            height: 2,
            icc: None,
            bit_depth: BitDepth::new(16).unwrap(),
            has_real_alpha: true,
        };
        for (width, height) in [(3, 2), (7, 5), (2, 1)] {
            let scaled =
                scale_gainmap(&image, (12, 8), (width, height), WeaveScaleMode::JustResize)
                    .unwrap();
            assert_eq!(scaled.len(), width * height * 4);
            for pixel in scaled.as_chunks::<4>().0 {
                for (actual, expected) in pixel[..3].iter().zip([1001, 32769, 65003]) {
                    assert!(actual.abs_diff(expected) <= 1, "{actual} != {expected}");
                }
                assert_eq!(pixel[3], 65535);
            }
        }
    }

    #[test]
    #[ignore = "regenerates the synthetic Android gainmap fixture"]
    fn write_android_gainmap_fixture() {
        std::fs::write(
            "../jxlcoder/src/androidTest/assets/gainmap-hdr-base.jxl",
            fixture(true, false, true),
        )
        .unwrap();
    }
}
