// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::sync::Arc;

use crate::{simd::f32x8, Point};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Channel {
    Red,
    Green,
    Blue,
    Alpha,
}

impl Channel {
    pub(crate) fn select<T>(self, r: T, g: T, b: T, a: T) -> T {
        match self {
            Channel::Red => r,
            Channel::Green => g,
            Channel::Blue => b,
            Channel::Alpha => a,
        }
    }

    pub(crate) fn select_from_color(self, color: Color) -> f32 {
        match self {
            Channel::Red => color.r,
            Channel::Green => color.g,
            Channel::Blue => color.b,
            Channel::Alpha => color.a,
        }
    }
}

pub const RGBA: [Channel; 4] = [Channel::Red, Channel::Green, Channel::Blue, Channel::Alpha];
pub const BGRA: [Channel; 4] = [Channel::Blue, Channel::Green, Channel::Red, Channel::Alpha];

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

impl Default for Color {
    fn default() -> Self {
        Self { r: 0.0, g: 0.0, b: 0.0, a: 1.0 }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum FillRule {
    NonZero,
    EvenOdd,
}

impl Default for FillRule {
    fn default() -> Self {
        Self::NonZero
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum GradientType {
    Linear,
    Radial,
}

const NO_STOP: f32 = -0.0;

#[derive(Clone, Debug)]
pub struct GradientBuilder {
    r#type: GradientType,
    start: Point,
    end: Point,
    stops: Vec<(Color, f32)>,
}

impl GradientBuilder {
    pub fn new(start: Point, end: Point) -> Self {
        Self { r#type: GradientType::Linear, start, end, stops: Vec::new() }
    }

    pub fn r#type(&mut self, r#type: GradientType) -> &mut Self {
        self.r#type = r#type;
        self
    }

    pub fn color(&mut self, color: Color) -> &mut Self {
        self.stops.push((color, NO_STOP));
        self
    }

    pub fn color_with_stop(&mut self, color: Color, stop: f32) -> &mut Self {
        if !(0.0..=1.0).contains(&stop) {
            panic!("gradient stops must be between 0.0 and 1.0");
        }

        self.stops.push((color, stop));
        self
    }

    pub fn build(mut self) -> Option<Gradient> {
        if self.stops.len() < 2 {
            return None;
        }

        let stop_increment = 1.0 / (self.stops.len() - 1) as f32;
        for (i, (_, stop)) in self.stops.iter_mut().enumerate() {
            if *stop == NO_STOP {
                *stop = i as f32 * stop_increment;
            }
        }

        Some(Gradient {
            r#type: self.r#type,
            start: self.start,
            end: self.end,
            stops: self.stops.into(),
        })
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Gradient {
    r#type: GradientType,
    start: Point,
    end: Point,
    stops: Arc<[(Color, f32)]>,
}

impl Gradient {
    fn get_t(&self, x: f32, y: f32) -> f32x8 {
        let dx = self.end.x - self.start.x;
        let dy = self.end.y - self.start.y;

        let dot = dx * dx + dy * dy;
        let dot_recip = dot.recip();

        match self.r#type {
            GradientType::Linear => {
                let tx = (x - self.start.x) * dx * dot_recip;
                let ty = y - self.start.y;

                ((f32x8::indexed() + f32x8::splat(ty)) * f32x8::splat(dy))
                    .mul_add(f32x8::splat(dot_recip), f32x8::splat(tx))
            }
            GradientType::Radial => {
                let px = x - self.start.x;
                let px2 = f32x8::splat(px * px);
                let py = f32x8::indexed() + f32x8::splat(y - self.start.y);

                (py.mul_add(py, px2) * f32x8::splat(dot_recip)).sqrt()
            }
        }
    }

    pub(crate) fn color_at(&self, x: f32, y: f32) -> [f32x8; 4] {
        let mut channels = [f32x8::splat(0.0); 4];

        let t = self.get_t(x, y);

        let mask = t.le(f32x8::splat(self.stops[0].1));
        if mask.any() {
            let stop = self.stops[0].0;
            for (channel, &stop_channel) in
                channels.iter_mut().zip([stop.r, stop.g, stop.b, stop.a].iter())
            {
                *channel |= f32x8::splat(stop_channel).select(f32x8::splat(0.0), mask);
            }
        }

        let mut start_stop = 0.0;
        let mut start_color = self.stops[0].0;
        let mut acc_mask = mask;

        for &(color, end_stop) in self.stops.iter().skip(1) {
            let mask = acc_mask ^ t.lt(f32x8::splat(end_stop));
            if mask.any() {
                let d = end_stop - start_stop;
                let local_t = (t - f32x8::splat(start_stop)) * f32x8::splat(d.recip());

                for (channel, (&start_channel, &end_channel)) in channels.iter_mut().zip(
                    [start_color.r, start_color.g, start_color.b, start_color.a]
                        .iter()
                        .zip([color.r, color.g, color.b, color.a].iter()),
                ) {
                    *channel |= local_t
                        .mul_add(
                            f32x8::splat(end_channel),
                            (-local_t)
                                .mul_add(f32x8::splat(start_channel), f32x8::splat(start_channel)),
                        )
                        .select(f32x8::splat(0.0), mask);
                }

                acc_mask |= mask;
            }

            start_stop = end_stop;
            start_color = color;
        }

        let mask = !acc_mask;
        if mask.any() {
            let stop = self.stops[self.stops.len() - 1].0;
            for (channel, &stop_channel) in
                channels.iter_mut().zip([stop.r, stop.g, stop.b, stop.a].iter())
            {
                *channel |= f32x8::splat(stop_channel).select(f32x8::splat(0.0), mask);
            }
        }

        channels
    }
}

#[derive(Clone, Debug, PartialEq)]
pub enum Fill {
    Solid(Color),
    Gradient(Gradient),
}

impl Default for Fill {
    fn default() -> Self {
        Self::Solid(Color::default())
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum BlendMode {
    Over,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
}

impl BlendMode {
    pub(crate) fn blend_fn(self) -> fn(f32, f32) -> f32 {
        fn multiply(dst: f32, src: f32) -> f32 {
            dst * src
        }

        fn screen(dst: f32, src: f32) -> f32 {
            dst + src - (dst * src)
        }

        fn hard_light(dst: f32, src: f32) -> f32 {
            if dst <= 0.5 {
                multiply(2.0 * dst, src)
            } else {
                screen(2.0 * dst - 1.0, src)
            }
        }

        match self {
            Self::Over => |_dst, src| src,
            Self::Multiply => multiply,
            Self::Screen => screen,
            Self::Overlay => |dst, src| hard_light(src, dst),
            Self::Darken => |dst, src| dst.min(src),
            Self::Lighten => |dst, src| dst.max(src),
            Self::ColorDodge => |dst, src| {
                if src == 0.0 {
                    0.0
                } else if dst == 1.0 {
                    1.0
                } else {
                    1.0f32.min(src / (1.0 - dst))
                }
            },
            Self::ColorBurn => |dst, src| {
                if src == 1.0 {
                    1.0
                } else if dst == 0.0 {
                    0.0
                } else {
                    1.0 - 1.0f32.min((1.0 - src) / dst)
                }
            },
            Self::HardLight => hard_light,
            Self::SoftLight => |dst, src| {
                fn d(src: f32) -> f32 {
                    if src <= 0.25 {
                        ((16.0 * src - 12.0) * src + 4.0) * src
                    } else {
                        src.sqrt()
                    }
                }

                if dst <= 0.5 {
                    src - (1.0 - 2.0 * dst) * src * (1.0 - src)
                } else {
                    src + (2.0 * dst - 1.0) * (d(src) - src)
                }
            },
            Self::Difference => |dst, src| (dst - src).abs(),
            Self::Exclusion => |dst, src| dst + src - 2.0 * dst * src,
        }
    }

    pub(crate) fn blend(self, dst: Color, src: Color) -> Color {
        let f = self.blend_fn();

        let alpha = src.a;
        let inv_alpha = 1.0 - alpha;

        let current_red = f(dst.r, src.r) * alpha;
        let current_green = f(dst.g, src.g) * alpha;
        let current_blue = f(dst.b, src.b) * alpha;

        Color {
            r: dst.r.mul_add(inv_alpha, current_red),
            g: dst.g.mul_add(inv_alpha, current_green),
            b: dst.b.mul_add(inv_alpha, current_blue),
            a: dst.a.mul_add(inv_alpha, alpha),
        }
    }
}

macro_rules! blend_function {
    (
        $mode:expr,
        $dst0:expr,
        $dst1:expr,
        $dst2:expr,
        $src0:expr,
        $src1:expr,
        $src2:expr
        $( , )?
    ) => {
        match $mode {
            BlendMode::Over => [$src0, $src1, $src2],
            BlendMode::Multiply => [$dst0 * $src0, $dst1 * $src1, $dst2 * $src2],
            BlendMode::Screen => [
                $dst0.mul_add(-$src0, $dst0) + $src0,
                $dst1.mul_add(-$src1, $dst1) + $src1,
                $dst2.mul_add(-$src2, $dst2) + $src2,
            ],
            BlendMode::Overlay => [
                ($dst0 * $src0 * f32x8::splat(2.0)).select(
                    f32x8::splat(2.0) * ($src0 + $dst0 - $src0.mul_add($dst0, f32x8::splat(0.5))),
                    $src0.le(f32x8::splat(0.5)),
                ),
                ($dst1 * $src1 * f32x8::splat(2.0)).select(
                    f32x8::splat(2.0) * ($src1 + $dst1 - $src1.mul_add($dst1, f32x8::splat(0.5))),
                    $src1.le(f32x8::splat(0.5)),
                ),
                ($dst2 * $src2 * f32x8::splat(2.0)).select(
                    f32x8::splat(2.0) * ($src2 + $dst2 - $src2.mul_add($dst2, f32x8::splat(0.5))),
                    $src2.le(f32x8::splat(0.5)),
                ),
            ],
            BlendMode::Darken => [$dst0.min($src0), $dst1.min($src1), $dst2.min($src2)],
            BlendMode::Lighten => [$dst0.max($src0), $dst1.max($src1), $dst2.max($src2)],
            BlendMode::ColorDodge => [
                f32x8::splat(0.0).select(
                    f32x8::splat(1.0).min($src0 / (f32x8::splat(1.0) - $dst0)),
                    $src0.eq(f32x8::splat(0.0)),
                ),
                f32x8::splat(0.0).select(
                    f32x8::splat(1.0).min($src1 / (f32x8::splat(1.0) - $dst1)),
                    $src1.eq(f32x8::splat(0.0)),
                ),
                f32x8::splat(0.0).select(
                    f32x8::splat(1.0).min($src2 / (f32x8::splat(1.0) - $dst2)),
                    $src2.eq(f32x8::splat(0.0)),
                ),
            ],
            BlendMode::ColorBurn => [
                f32x8::splat(1.0).select(
                    f32x8::splat(1.0) - f32x8::splat(1.0).min((f32x8::splat(1.0) - $src0) / $dst0),
                    $src0.eq(f32x8::splat(1.0)),
                ),
                f32x8::splat(1.0).select(
                    f32x8::splat(1.0) - f32x8::splat(1.0).min((f32x8::splat(1.0) - $src1) / $dst1),
                    $src1.eq(f32x8::splat(1.0)),
                ),
                f32x8::splat(1.0).select(
                    f32x8::splat(1.0) - f32x8::splat(1.0).min((f32x8::splat(1.0) - $src2) / $dst2),
                    $src2.eq(f32x8::splat(1.0)),
                ),
            ],
            BlendMode::HardLight => [
                ($dst0 * $src0 * f32x8::splat(2.0)).select(
                    f32x8::splat(2.0) * ($src0 + $dst0 - $src0.mul_add($dst0, f32x8::splat(0.5))),
                    $dst0.le(f32x8::splat(0.5)),
                ),
                ($dst1 * $src1 * f32x8::splat(2.0)).select(
                    f32x8::splat(2.0) * ($src1 + $dst1 - $src1.mul_add($dst1, f32x8::splat(0.5))),
                    $dst1.le(f32x8::splat(0.5)),
                ),
                ($dst2 * $src2 * f32x8::splat(2.0)).select(
                    f32x8::splat(2.0) * ($src2 + $dst2 - $src2.mul_add($dst2, f32x8::splat(0.5))),
                    $dst2.le(f32x8::splat(0.5)),
                ),
            ],
            BlendMode::SoftLight => {
                let d0 = (f32x8::splat(16.0)
                    .mul_add($src0, f32x8::splat(-12.0))
                    .mul_add($src0, f32x8::splat(4.0))
                    * $src0)
                    .select($src0.sqrt(), $src0.le(f32x8::splat(0.25)));
                let d1 = (f32x8::splat(16.0)
                    .mul_add($src1, f32x8::splat(-12.0))
                    .mul_add($src1, f32x8::splat(4.0))
                    * $src1)
                    .select($src1.sqrt(), $src1.le(f32x8::splat(0.25)));
                let d2 = (f32x8::splat(16.0)
                    .mul_add($src2, f32x8::splat(-12.0))
                    .mul_add($src2, f32x8::splat(4.0))
                    * $src2)
                    .select($src2.sqrt(), $src2.le(f32x8::splat(0.25)));

                [
                    (($src0 * (f32x8::splat(1.0) - $src0))
                        .mul_add(f32x8::splat(2.0).mul_add($dst0, f32x8::splat(-1.0)), $src0))
                    .select(
                        (d0 - $src0)
                            .mul_add(f32x8::splat(2.0).mul_add($dst0, f32x8::splat(-1.0)), $src0),
                        $dst0.le(f32x8::splat(0.5)),
                    ),
                    (($src1 * (f32x8::splat(1.0) - $src1))
                        .mul_add(f32x8::splat(2.0).mul_add($dst1, f32x8::splat(-1.0)), $src1))
                    .select(
                        (d1 - $src1)
                            .mul_add(f32x8::splat(2.0).mul_add($dst1, f32x8::splat(-1.0)), $src1),
                        $dst1.le(f32x8::splat(0.5)),
                    ),
                    (($src2 * (f32x8::splat(1.0) - $src2))
                        .mul_add(f32x8::splat(2.0).mul_add($dst2, f32x8::splat(-1.0)), $src2))
                    .select(
                        (d2 - $src2)
                            .mul_add(f32x8::splat(2.0).mul_add($dst2, f32x8::splat(-1.0)), $src2),
                        $dst2.le(f32x8::splat(0.5)),
                    ),
                ]
            }
            BlendMode::Difference => {
                [($dst0 - $src0).abs(), ($dst1 - $src1).abs(), ($dst2 - $src2).abs()]
            }
            BlendMode::Exclusion => [
                (f32x8::splat(-2.0) * $dst0).mul_add($src0, $dst0) + $src0,
                (f32x8::splat(-2.0) * $dst1).mul_add($src1, $dst1) + $src1,
                (f32x8::splat(-2.0) * $dst2).mul_add($src2, $dst2) + $src2,
            ],
        }
    };
}

impl Default for BlendMode {
    fn default() -> Self {
        Self::Over
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Style {
    pub is_clipped: bool,
    pub fill: Fill,
    pub blend_mode: BlendMode,
}

#[cfg(test)]
mod tests {
    use super::*;

    macro_rules! color {
        ( $val:expr ) => {
            Color { r: $val, g: $val, b: $val, a: $val }
        };
    }

    macro_rules! color_array {
        ( $val:expr ) => {
            [$val, $val, $val, $val]
        };
    }

    macro_rules! color_eq {
        ( $val:expr ) => {{
            assert_eq!($val[0], $val[1]);
            assert_eq!($val[1], $val[2]);
            assert_eq!($val[2], $val[3]);

            $val[0]
        }};
    }

    fn colors(separate: &[f32x8; 4]) -> [[f32; 4]; 8] {
        let mut colors = [[0.0, 0.0, 0.0, 0.0]; 8];

        for (i, color) in colors.iter_mut().enumerate() {
            *color = [
                separate[0].as_array()[i],
                separate[1].as_array()[i],
                separate[2].as_array()[i],
                separate[3].as_array()[i],
            ];
        }

        colors
    }

    fn test_blend_mode(blend_mode: BlendMode) {
        let color_values = [0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0];
        let f = blend_mode.blend_fn();

        for &dst in &color_values {
            for &src in &color_values {
                let [c0, c1, c2] = blend_function!(
                    blend_mode,
                    f32x8::splat(dst),
                    f32x8::splat(dst),
                    f32x8::splat(dst),
                    f32x8::splat(src),
                    f32x8::splat(src),
                    f32x8::splat(src),
                );

                assert_eq!(c0.as_array(), f32x8::splat(f(dst, src)).as_array());
                assert_eq!(c1.as_array(), f32x8::splat(f(dst, src)).as_array());
                assert_eq!(c2.as_array(), f32x8::splat(f(dst, src)).as_array());
            }
        }
    }

    #[test]
    fn linear_gradient() {
        let mut builder = GradientBuilder::new(Point::new(0.0, 7.0), Point::new(7.0, 0.0));

        builder
            .color(color!(0.25))
            .color(color!(0.75))
            .color(color!(0.25))
            .color(color!(0.75))
            .color(color!(0.25));

        let gradient = builder.build().unwrap();

        let col = colors(&gradient.color_at(0.0, 0.0));
        assert_eq!(col[0], color_array!(0.25));
        assert!(color_eq!(col[1]) < color_eq!(col[2]));
        assert!(color_eq!(col[2]) < color_eq!(col[3]));
        assert!(color_eq!(col[4]) > color_eq!(col[5]));
        assert!(color_eq!(col[5]) > color_eq!(col[6]));
        assert_eq!(col[7], color_array!(0.25));

        let col = colors(&gradient.color_at(3.0, 0.0));
        assert!(color_eq!(col[0]) < 0.75);
        assert!(color_eq!(col[1]) > color_eq!(col[2]));
        assert!(color_eq!(col[2]) > color_eq!(col[3]));
        assert_eq!(col[3], color_array!(0.25));
        assert!(color_eq!(col[3]) < color_eq!(col[4]));
        assert!(color_eq!(col[4]) < color_eq!(col[5]));
        assert!(color_eq!(col[5]) < color_eq!(col[6]));
        assert!(color_eq!(col[7]) < 0.75);

        let col = colors(&gradient.color_at(7.0, 0.0));
        assert_eq!(col[0], color_array!(0.25));
        assert!(color_eq!(col[1]) < color_eq!(col[2]));
        assert!(color_eq!(col[2]) < color_eq!(col[3]));
        assert!(color_eq!(col[4]) > color_eq!(col[5]));
        assert!(color_eq!(col[5]) > color_eq!(col[6]));
        assert_eq!(col[7], color_array!(0.25));
    }

    #[test]
    fn radial_gradient() {
        let mut builder = GradientBuilder::new(
            Point::new(0.0, 0.0),
            Point::new(7.0 * (1.0 / 2.0f32.sqrt()), 7.0 * (1.0 / 2.0f32.sqrt())),
        );

        builder.r#type(GradientType::Radial).color(color!(0.25)).color(color!(0.75));

        let gradient = builder.build().unwrap();

        let col = colors(&gradient.color_at(0.0, 0.0));
        assert_eq!(col[0], color_array!(0.25));
        assert!(color_eq!(col[1]) < color_eq!(col[2]));
        assert!(color_eq!(col[2]) < color_eq!(col[3]));
        assert!(color_eq!(col[3]) < color_eq!(col[4]));
        assert!(color_eq!(col[4]) < color_eq!(col[5]));
        assert!(color_eq!(col[5]) < color_eq!(col[6]));
        assert_eq!(col[7], color_array!(0.75));

        let col = colors(&gradient.color_at(3.0, 0.0));
        assert!(color_eq!(col[0]) < color_eq!(col[1]));
        assert!(color_eq!(col[1]) < color_eq!(col[2]));
        assert!(color_eq!(col[2]) < color_eq!(col[3]));
        assert!(color_eq!(col[3]) < color_eq!(col[4]));
        assert!(color_eq!(col[4]) < color_eq!(col[5]));
        assert!(color_eq!(col[5]) < color_eq!(col[6]));
        assert_eq!(col[7], color_array!(0.75));

        let col = colors(&gradient.color_at(4.0, 0.0));
        assert!(color_eq!(col[0]) < color_eq!(col[1]));
        assert!(color_eq!(col[1]) < color_eq!(col[2]));
        assert!(color_eq!(col[2]) < color_eq!(col[3]));
        assert!(color_eq!(col[3]) < color_eq!(col[4]));
        assert!(color_eq!(col[4]) < color_eq!(col[5]));
        assert_eq!(col[6], color_array!(0.75));
        assert_eq!(col[7], color_array!(0.75));

        let col = colors(&gradient.color_at(7.0, 0.0));
        assert_eq!(col[0], color_array!(0.75));
        assert_eq!(col[1], color_array!(0.75));
        assert_eq!(col[2], color_array!(0.75));
        assert_eq!(col[3], color_array!(0.75));
        assert_eq!(col[4], color_array!(0.75));
        assert_eq!(col[5], color_array!(0.75));
        assert_eq!(col[6], color_array!(0.75));
        assert_eq!(col[7], color_array!(0.75));
    }

    #[test]
    fn test_blend_mode_over() {
        test_blend_mode(BlendMode::Over);
    }

    #[test]
    fn test_blend_mode_multiply() {
        test_blend_mode(BlendMode::Multiply);
    }

    #[test]
    fn test_blend_mode_screen() {
        test_blend_mode(BlendMode::Screen);
    }

    #[test]
    fn test_blend_mode_overlay() {
        test_blend_mode(BlendMode::Overlay);
    }

    #[test]
    fn test_blend_mode_darken() {
        test_blend_mode(BlendMode::Darken);
    }

    #[test]
    fn test_blend_mode_lighten() {
        test_blend_mode(BlendMode::Lighten);
    }

    #[test]
    fn test_blend_mode_color_dodge() {
        test_blend_mode(BlendMode::ColorDodge);
    }

    #[test]
    fn test_blend_mode_color_burn() {
        test_blend_mode(BlendMode::ColorBurn);
    }

    #[test]
    fn test_blend_mode_hard_light() {
        test_blend_mode(BlendMode::HardLight);
    }

    #[test]
    fn test_blend_mode_soft_light() {
        test_blend_mode(BlendMode::SoftLight);
    }

    #[test]
    fn test_blend_mode_difference() {
        test_blend_mode(BlendMode::Difference);
    }

    #[test]
    fn test_blend_mode_exclusion() {
        test_blend_mode(BlendMode::Exclusion);
    }

    #[test]
    fn channel_select() {
        let channels: [Channel; 4] = [Channel::Blue, Channel::Green, Channel::Red, Channel::Alpha];
        let red = [3f32; 8];
        let green = [2f32; 8];
        let blue = [1f32; 8];
        let alpha = [1f32; 8];
        let color = channels.map(|c| c.select(red, green, blue, alpha));
        assert_eq!(color, [blue, green, red, alpha]);
    }

    #[test]
    fn channel_select_from_color() {
        let channels: [Channel; 4] = [Channel::Blue, Channel::Green, Channel::Red, Channel::Alpha];
        let color = Color { r: 3.0, g: 2.0, b: 1.0, a: 1.0 };
        let color = channels.map(|c| c.select_from_color(color));
        assert_eq!(color, [1.0, 2.0, 3.0, 1.0]);
    }
}
