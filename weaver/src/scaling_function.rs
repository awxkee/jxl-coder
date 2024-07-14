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
            _ => { ScalingFunction::Bilinear }
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