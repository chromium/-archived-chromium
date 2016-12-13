// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <vector>

#include "skia/ext/image_operations.h"

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/stack_container.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "skia/ext/convolver.h"

namespace skia {

// TODO(brettw) remove this and put this file in the skia namespace.
using namespace gfx;

namespace {

// Returns the ceiling/floor as an integer.
inline int CeilInt(float val) {
  return static_cast<int>(ceil(val));
}
inline int FloorInt(float val) {
  return static_cast<int>(floor(val));
}

// Filter function computation -------------------------------------------------

// Evaluates the box filter, which goes from -0.5 to +0.5.
float EvalBox(float x) {
  return (x >= -0.5f && x < 0.5f) ? 1.0f : 0.0f;
}

// Evaluates the Lanczos filter of the given filter size window for the given
// position.
//
// |filter_size| is the width of the filter (the "window"), outside of which
// the value of the function is 0. Inside of the window, the value is the
// normalized sinc function:
//   lanczos(x) = sinc(x) * sinc(x / filter_size);
// where
//   sinc(x) = sin(pi*x) / (pi*x);
float EvalLanczos(int filter_size, float x) {
  if (x <= -filter_size || x >= filter_size)
    return 0.0f;  // Outside of the window.
  if (x > -std::numeric_limits<float>::epsilon() &&
      x < std::numeric_limits<float>::epsilon())
    return 1.0f;  // Special case the discontinuity at the origin.
  float xpi = x * static_cast<float>(M_PI);
  return (sin(xpi) / xpi) *  // sinc(x)
          sin(xpi / filter_size) / (xpi / filter_size);  // sinc(x/filter_size)
}

// ResizeFilter ----------------------------------------------------------------

// Encapsulates computation and storage of the filters required for one complete
// resize operation.
class ResizeFilter {
 public:
  ResizeFilter(ImageOperations::ResizeMethod method,
               int src_full_width, int src_full_height,
               int dest_width, int dest_height,
               const gfx::Rect& dest_subset);

  // Returns the bounds in the input bitmap of data that is used in the output.
  // The filter offsets are within this rectangle.
  const gfx::Rect& src_depend() { return src_depend_; }

  // Returns the filled filter values.
  const ConvolusionFilter1D& x_filter() { return x_filter_; }
  const ConvolusionFilter1D& y_filter() { return y_filter_; }

 private:
  // Returns the number of pixels that the filer spans, in filter space (the
  // destination image).
  float GetFilterSupport(float scale) {
    switch (method_) {
      case ImageOperations::RESIZE_BOX:
        // The box filter just scales with the image scaling.
        return 0.5f;  // Only want one side of the filter = /2.
      case ImageOperations::RESIZE_LANCZOS3:
        // The lanczos filter takes as much space in the source image in
        // each direction as the size of the window = 3 for Lanczos3.
        return 3.0f;
      default:
        NOTREACHED();
        return 1.0f;
    }
  }

  // Computes one set of filters either horizontally or vertically. The caller
  // will specify the "min" and "max" rather than the bottom/top and
  // right/bottom so that the same code can be re-used in each dimension.
  //
  // |src_depend_lo| and |src_depend_size| gives the range for the source
  // depend rectangle (horizontally or vertically at the caller's discretion
  // -- see above for what this means).
  //
  // Likewise, the range of destination values to compute and the scale factor
  // for the transform is also specified.
  void ComputeFilters(int src_size,
                      int dest_subset_lo, int dest_subset_size,
                      float scale, float src_support,
                      ConvolusionFilter1D* output);

  // Computes the filter value given the coordinate in filter space.
  inline float ComputeFilter(float pos) {
    switch (method_) {
      case ImageOperations::RESIZE_BOX:
        return EvalBox(pos);
      case ImageOperations::RESIZE_LANCZOS3:
        return EvalLanczos(3, pos);
      default:
        NOTREACHED();
        return 0;
    }
  }

  ImageOperations::ResizeMethod method_;

  // Subset of source the filters will touch.
  gfx::Rect src_depend_;

  // Size of the filter support on one side only in the destination space.
  // See GetFilterSupport.
  float x_filter_support_;
  float y_filter_support_;

  // Subset of scaled destination bitmap to compute.
  gfx::Rect out_bounds_;

  ConvolusionFilter1D x_filter_;
  ConvolusionFilter1D y_filter_;

  DISALLOW_EVIL_CONSTRUCTORS(ResizeFilter);
};

ResizeFilter::ResizeFilter(ImageOperations::ResizeMethod method,
                           int src_full_width, int src_full_height,
                           int dest_width, int dest_height,
                           const gfx::Rect& dest_subset)
    : method_(method),
      out_bounds_(dest_subset) {
  float scale_x = static_cast<float>(dest_width) /
                  static_cast<float>(src_full_width);
  float scale_y = static_cast<float>(dest_height) /
                  static_cast<float>(src_full_height);

  x_filter_support_ = GetFilterSupport(scale_x);
  y_filter_support_ = GetFilterSupport(scale_y);

  gfx::Rect src_full(0, 0, src_full_width, src_full_height);
  gfx::Rect dest_full(0, 0,
                      static_cast<int>(src_full_width * scale_x + 0.5),
                      static_cast<int>(src_full_height * scale_y + 0.5));

  // Support of the filter in source space.
  float src_x_support = x_filter_support_ / scale_x;
  float src_y_support = y_filter_support_ / scale_y;

  ComputeFilters(src_full_width, dest_subset.x(), dest_subset.width(),
                 scale_x, src_x_support, &x_filter_);
  ComputeFilters(src_full_height, dest_subset.y(), dest_subset.height(),
                 scale_y, src_y_support, &y_filter_);
}

void ResizeFilter::ComputeFilters(int src_size,
                                  int dest_subset_lo, int dest_subset_size,
                                  float scale, float src_support,
                                  ConvolusionFilter1D* output) {
  int dest_subset_hi = dest_subset_lo + dest_subset_size;  // [lo, hi)

  // When we're doing a magnification, the scale will be larger than one. This
  // means the destination pixels are much smaller than the source pixels, and
  // that the range covered by the filter won't necessarily cover any source
  // pixel boundaries. Therefore, we use these clamped values (max of 1) for
  // some computations.
  float clamped_scale = std::min(1.0f, scale);

  // Speed up the divisions below by turning them into multiplies.
  float inv_scale = 1.0f / scale;

  StackVector<float, 64> filter_values;
  StackVector<int16, 64> fixed_filter_values;

  // Loop over all pixels in the output range. We will generate one set of
  // filter values for each one. Those values will tell us how to blend the
  // source pixels to compute the destination pixel.
  for (int dest_subset_i = dest_subset_lo; dest_subset_i < dest_subset_hi;
       dest_subset_i++) {
    // Reset the arrays. We don't declare them inside so they can re-use the
    // same malloc-ed buffer.
    filter_values->clear();
    fixed_filter_values->clear();

    // This is the pixel in the source directly under the pixel in the dest.
    float src_pixel = dest_subset_i * inv_scale;

    // Compute the (inclusive) range of source pixels the filter covers.
    int src_begin = std::max(0, FloorInt(src_pixel - src_support));
    int src_end = std::min(src_size - 1, CeilInt(src_pixel + src_support));

    // Compute the unnormalized filter value at each location of the source
    // it covers.
    float filter_sum = 0.0f;  // Sub of the filter values for normalizing.
    for (int cur_filter_pixel = src_begin; cur_filter_pixel <= src_end;
         cur_filter_pixel++) {
      // Distance from the center of the filter, this is the filter coordinate
      // in source space.
      float src_filter_pos = cur_filter_pixel - src_pixel;

      // Since the filter really exists in dest space, map it there.
      float dest_filter_pos = src_filter_pos * clamped_scale;

      // Compute the filter value at that location.
      float filter_value = ComputeFilter(dest_filter_pos);
      filter_values->push_back(filter_value);

      filter_sum += filter_value;
    }
    DCHECK(!filter_values->empty()) << "We should always get a filter!";

    // The filter must be normalized so that we don't affect the brightness of
    // the image. Convert to normalized fixed point.
    int16 fixed_sum = 0;
    for (size_t i = 0; i < filter_values->size(); i++) {
      int16 cur_fixed = output->FloatToFixed(filter_values[i] / filter_sum);
      fixed_sum += cur_fixed;
      fixed_filter_values->push_back(cur_fixed);
    }

    // The conversion to fixed point will leave some rounding errors, which
    // we add back in to avoid affecting the brightness of the image. We
    // arbitrarily add this to the center of the filter array (this won't always
    // be the center of the filter function since it could get clipped on the
    // edges, but it doesn't matter enough to worry about that case).
    int16 leftovers = output->FloatToFixed(1.0f) - fixed_sum;
    fixed_filter_values[fixed_filter_values->size() / 2] += leftovers;

    // Now it's ready to go.
    output->AddFilter(src_begin, &fixed_filter_values[0],
                      static_cast<int>(fixed_filter_values->size()));
  }
}

}  // namespace

// Resize ----------------------------------------------------------------------

// static
SkBitmap ImageOperations::Resize(const SkBitmap& source,
                                 ResizeMethod method,
                                 int dest_width, int dest_height,
                                 const gfx::Rect& dest_subset) {
  // Time how long this takes to see if it's a problem for users.
  base::TimeTicks resize_start = base::TimeTicks::Now();

  DCHECK(gfx::Rect(dest_width, dest_height).Contains(dest_subset)) <<
      "The supplied subset does not fall within the destination image.";

  // If the size of source or destination is 0, i.e. 0x0, 0xN or Nx0, just
  // return empty.
  if (source.width() < 1 || source.height() < 1 ||
      dest_width < 1 || dest_height < 1)
    return SkBitmap();

  SkAutoLockPixels locker(source);

  ResizeFilter filter(method, source.width(), source.height(),
                      dest_width, dest_height, dest_subset);

  // Get a source bitmap encompassing this touched area. We construct the
  // offsets and row strides such that it looks like a new bitmap, while
  // referring to the old data.
  const uint8* source_subset =
      reinterpret_cast<const uint8*>(source.getPixels());

  // Convolve into the result.
  SkBitmap result;
  result.setConfig(SkBitmap::kARGB_8888_Config,
                   dest_subset.width(), dest_subset.height());
  result.allocPixels();
  BGRAConvolve2D(source_subset, static_cast<int>(source.rowBytes()),
                 !source.isOpaque(), filter.x_filter(), filter.y_filter(),
                 static_cast<unsigned char*>(result.getPixels()));

  // Preserve the "opaque" flag for use as an optimization later.
  result.setIsOpaque(source.isOpaque());

  base::TimeDelta delta = base::TimeTicks::Now() - resize_start;
  UMA_HISTOGRAM_TIMES("Image.ResampleMS", delta);

  return result;
}

// static
SkBitmap ImageOperations::Resize(const SkBitmap& source,
                                 ResizeMethod method,
                                 int dest_width, int dest_height) {
  gfx::Rect dest_subset(0, 0, dest_width, dest_height);
  return Resize(source, method, dest_width, dest_height, dest_subset);
}

// static
SkBitmap ImageOperations::CreateBlendedBitmap(const SkBitmap& first,
                                              const SkBitmap& second,
                                              double alpha) {
  DCHECK(alpha <= 1 && alpha >= 0);
  DCHECK(first.width() == second.width());
  DCHECK(first.height() == second.height());
  DCHECK(first.bytesPerPixel() == second.bytesPerPixel());
  DCHECK(first.config() == SkBitmap::kARGB_8888_Config);

  // Optimize for case where we won't need to blend anything.
  static const double alpha_min = 1.0 / 255;
  static const double alpha_max = 254.0 / 255;
  if (alpha < alpha_min)
    return first;
  else if (alpha > alpha_max)
    return second;

  SkAutoLockPixels lock_first(first);
  SkAutoLockPixels lock_second(second);

  SkBitmap blended;
  blended.setConfig(SkBitmap::kARGB_8888_Config, first.width(),
                    first.height(), 0);
  blended.allocPixels();
  blended.eraseARGB(0, 0, 0, 0);

  double first_alpha = 1 - alpha;

  for (int y = 0; y < first.height(); y++) {
    uint32* first_row = first.getAddr32(0, y);
    uint32* second_row = second.getAddr32(0, y);
    uint32* dst_row = blended.getAddr32(0, y);

    for (int x = 0; x < first.width(); x++) {
      uint32 first_pixel = first_row[x];
      uint32 second_pixel = second_row[x];

      int a = static_cast<int>(
          SkColorGetA(first_pixel) * first_alpha +
          SkColorGetA(second_pixel) * alpha);
      int r = static_cast<int>(
          SkColorGetR(first_pixel) * first_alpha +
          SkColorGetR(second_pixel) * alpha);
      int g = static_cast<int>(
          SkColorGetG(first_pixel) * first_alpha +
          SkColorGetG(second_pixel) * alpha);
      int b = static_cast<int>(
          SkColorGetB(first_pixel) * first_alpha +
          SkColorGetB(second_pixel) * alpha);

      dst_row[x] = SkColorSetARGB(a, r, g, b);
    }
  }

  return blended;
}

// static
SkBitmap ImageOperations::CreateMaskedBitmap(const SkBitmap& rgb,
                                             const SkBitmap& alpha) {
  DCHECK(rgb.width() == alpha.width());
  DCHECK(rgb.height() == alpha.height());
  DCHECK(rgb.bytesPerPixel() == alpha.bytesPerPixel());
  DCHECK(rgb.config() == SkBitmap::kARGB_8888_Config);
  DCHECK(alpha.config() == SkBitmap::kARGB_8888_Config);

  SkBitmap masked;
  masked.setConfig(SkBitmap::kARGB_8888_Config, rgb.width(), rgb.height(), 0);
  masked.allocPixels();
  masked.eraseARGB(0, 0, 0, 0);

  SkAutoLockPixels lock_rgb(rgb);
  SkAutoLockPixels lock_alpha(alpha);
  SkAutoLockPixels lock_masked(masked);

  for (int y = 0; y < rgb.height(); y++) {
    uint32* rgb_row = rgb.getAddr32(0, y);
    uint32* alpha_row = alpha.getAddr32(0, y);
    uint32* dst_row = masked.getAddr32(0, y);

    for (int x = 0; x < rgb.width(); x++) {
      uint32 alpha_pixel = alpha_row[x];
      uint32 rgb_pixel = rgb_row[x];

      int alpha = SkColorGetA(alpha_pixel);
      dst_row[x] = SkColorSetARGB(alpha,
                                  SkAlphaMul(SkColorGetR(rgb_pixel), alpha),
                                  SkAlphaMul(SkColorGetG(rgb_pixel), alpha),
                                  SkAlphaMul(SkColorGetB(rgb_pixel), alpha));
    }
  }

  return masked;
}

// static
SkBitmap ImageOperations::CreateButtonBackground(SkColor color,
                                                 const SkBitmap& image,
                                                 const SkBitmap& mask) {
  DCHECK(image.config() == SkBitmap::kARGB_8888_Config);
  DCHECK(mask.config() == SkBitmap::kARGB_8888_Config);

  SkBitmap background;
  background.setConfig(SkBitmap::kARGB_8888_Config,
                       mask.width(),
                       mask.height(), 0);
  background.allocPixels();

  int bg_a = SkColorGetA(color);
  int bg_r = SkColorGetR(color);
  int bg_g = SkColorGetG(color);
  int bg_b = SkColorGetB(color);

  SkAutoLockPixels lock_mask(mask);
  SkAutoLockPixels lock_image(image);
  SkAutoLockPixels lock_background(background);

  for (int y = 0; y < mask.height(); y++) {
    uint32* dst_row = background.getAddr32(0, y);
    uint32* image_row = image.getAddr32(0, y % image.height());
    uint32* mask_row = mask.getAddr32(0, y);

    for (int x = 0; x < mask.width(); x++) {
      uint32 mask_pixel = mask_row[x];
      uint32 image_pixel = image_row[x % image.width()];

      int img_a = SkColorGetA(image_pixel);
      int img_r = SkColorGetR(image_pixel);
      int img_g = SkColorGetG(image_pixel);
      int img_b = SkColorGetB(image_pixel);

      double img_alpha = static_cast<double>(img_a) / 255.0;
      double img_inv = 1 - img_alpha;

      double mask_a = static_cast<double>(SkColorGetA(mask_pixel)) / 255.0;

      dst_row[x] = SkColorSetARGB(
          static_cast<int>(std::min(255, bg_a + img_a) * mask_a),
          static_cast<int>((bg_r * img_inv + img_r * img_alpha) * mask_a),
          static_cast<int>((bg_g * img_inv + img_g * img_alpha) * mask_a),
          static_cast<int>((bg_b * img_inv + img_b * img_alpha) * mask_a));
    }
  }

  return background;
}


SkBitmap ImageOperations::CreateBlurredBitmap(const SkBitmap& bitmap,
                                              int blur_amount ) {
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);

  // Blur factor (1 divided by how many pixels the blur takes place over).
  double v = 1.0 / pow(static_cast<double>(blur_amount * 2 + 1), 2);

  SkBitmap blurred;
  blurred.setConfig(SkBitmap::kARGB_8888_Config, bitmap.width(),
                    bitmap.height(), 0);
  blurred.allocPixels();
  blurred.eraseARGB(0, 0, 0, 0);

  SkAutoLockPixels lock_bitmap(bitmap);
  SkAutoLockPixels lock_blurred(blurred);

  // Loop through every pixel in the image.
  for (int y = 0; y < bitmap.height(); y++) {  // Skip top and bottom edges.
    uint32* dst_row = blurred.getAddr32(0, y);

    for (int x = 0; x < bitmap.width(); x++) {  // Skip left and right edges.
      // Sums for this pixel.
      double a = 0;
      double r = 0;
      double g = 0;
      double b = 0;

      for (int ky = -blur_amount; ky <= blur_amount; ky++) {
        for (int kx = -blur_amount; kx <= blur_amount; kx++) {
          // Calculate the adjacent pixel for this kernel point. Blurs
          // are wrapped.
          int bx = (x + kx) % bitmap.width();
          while (bx < 0)
            bx += bitmap.width();
          int by = (y + ky) % bitmap.height();
          while (by < 0)
            by += bitmap.height();

          uint32 src_pixel = bitmap.getAddr32(0, by)[bx];

          a += v * static_cast<double>(SkColorGetA(src_pixel));
          r += v * static_cast<double>(SkColorGetR(src_pixel));
          g += v * static_cast<double>(SkColorGetG(src_pixel));
          b += v * static_cast<double>(SkColorGetB(src_pixel));
        }
      }

      dst_row[x] = SkColorSetARGB(
          static_cast<int>(a),
          static_cast<int>(r),
          static_cast<int>(g),
          static_cast<int>(b));
    }
  }

  return blurred;
}

// static
SkBitmap ImageOperations::CreateHSLShiftedBitmap(const SkBitmap& bitmap,
                                                 HSL hsl_shift) {
  DCHECK(bitmap.empty() == false);
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);

  SkBitmap shifted;
  shifted.setConfig(SkBitmap::kARGB_8888_Config, bitmap.width(),
                    bitmap.height(), 0);
  shifted.allocPixels();
  shifted.eraseARGB(0, 0, 0, 0);
  shifted.setIsOpaque(false);

  SkAutoLockPixels lock_bitmap(bitmap);
  SkAutoLockPixels lock_shifted(shifted);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < bitmap.height(); y++) {
    SkPMColor* pixels = bitmap.getAddr32(0, y);
    SkPMColor* tinted_pixels = shifted.getAddr32(0, y);

    for (int x = 0; x < bitmap.width(); x++) {
      // Convert the color of this pixel to HSL.
      SkPMColor color = pixels[x];
      int alpha = SkColorGetA(color);
      if (alpha != 255) {
        // We have to normalize the colors as they're pre-multiplied.
        double r = SkColorGetR(color) / static_cast<double>(alpha);
        double g = SkColorGetG(color) / static_cast<double>(alpha);
        double b = SkColorGetB(color) / static_cast<double>(alpha);
        color = SkColorSetARGB(255,
                               static_cast<int>(r * 255.0),
                               static_cast<int>(g * 255.0),
                               static_cast<int>(b * 255.0));
      }

      HSL pixel_hsl = { 0, 0, 0 };
      SkColorToHSL(color, pixel_hsl);

      // Replace the hue with the tint's hue.
      if (hsl_shift.h >= 0)
        pixel_hsl.h = hsl_shift.h;

      // Change the saturation.
      if (hsl_shift.s >= 0) {
        if (hsl_shift.s <= 0.5) {
          pixel_hsl.s *= hsl_shift.s * 2.0;
        } else {
          pixel_hsl.s = pixel_hsl.s + (1.0 - pixel_hsl.s) *
              ((hsl_shift.s - 0.5) * 2.0);
        }
      }

      // Change the lightness.
      if (hsl_shift.l >= 0) {
        if (hsl_shift.l <= 0.5) {
          pixel_hsl.l *= hsl_shift.l * 2.0;
        } else {
          pixel_hsl.l = pixel_hsl.l + (1.0 - pixel_hsl.l) *
              ((hsl_shift.l - 0.5) * 2.0);
        }
      }

      // Convert back to RGB.
      tinted_pixels[x] = HSLToSKColor(alpha, pixel_hsl);
    }
  }

  return shifted;
}

// static
SkBitmap ImageOperations::CreateTiledBitmap(const SkBitmap& source,
                                            int src_x, int src_y,
                                            int dst_w, int dst_h) {
  DCHECK(source.getConfig() == SkBitmap::kARGB_8888_Config);

  SkBitmap cropped;
  cropped.setConfig(SkBitmap::kARGB_8888_Config, dst_w, dst_h, 0);
  cropped.allocPixels();
  cropped.eraseARGB(0, 0, 0, 0);

  SkAutoLockPixels lock_source(source);
  SkAutoLockPixels lock_cropped(cropped);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < dst_h; y++) {
    int y_pix = (src_y + y) % source.height();
    while (y_pix < 0)
      y_pix += source.height();

    uint32* source_row = source.getAddr32(0, y_pix);
    uint32* dst_row = cropped.getAddr32(0, y);

    for (int x = 0; x < dst_w; x++) {
      int x_pix = (src_x + x) % source.width();
      while (x_pix < 0)
        x_pix += source.width();

      dst_row[x] = source_row[x_pix];
    }
  }

  return cropped;
}

// static
SkBitmap ImageOperations::DownsampleByTwo(const SkBitmap& bitmap) {
  // Handle the nop case.
  if (bitmap.width() <= 1 || bitmap.height() <= 1)
    return bitmap;

  SkBitmap result;
  result.setConfig(SkBitmap::kARGB_8888_Config,
                   (bitmap.width() + 1) / 2,
                   (bitmap.height() + 1) / 2);
  result.allocPixels();

  SkAutoLockPixels lock(bitmap);
  for (int dest_y = 0; dest_y < result.height(); dest_y++) {
    for (int dest_x = 0; dest_x < result.width(); dest_x++ ) {
      // This code is based on downsampleby2_proc32 in SkBitmap.cpp. It is very
      // clever in that it does two channels at once: alpha and green ("ag")
      // and red and blue ("rb"). Each channel gets averaged across 4 pixels
      // to get the result.
      int src_x = dest_x << 1;
      int src_y = dest_y << 1;
      const SkPMColor* cur_src = bitmap.getAddr32(src_x, src_y);
      SkPMColor tmp, ag, rb;

      // Top left pixel of the 2x2 block.
      tmp = *cur_src;
      ag = (tmp >> 8) & 0xFF00FF;
      rb = tmp & 0xFF00FF;
      if (src_x < bitmap.width() - 1)
        cur_src += 1;

      // Top right pixel of the 2x2 block.
      tmp = *cur_src;
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;
      if (src_y < bitmap.height() - 1)
        cur_src = bitmap.getAddr32(src_x, src_y + 1);
      else
        cur_src = bitmap.getAddr32(src_x, src_y);  // Move back to the first.

      // Bottom left pixel of the 2x2 block.
      tmp = *cur_src;
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;
      if (src_x < bitmap.width() - 1)
        cur_src += 1;

      // Bottom right pixel of the 2x2 block.
      tmp = *cur_src;
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;

      // Put the channels back together, dividing each by 4 to get the average.
      // |ag| has the alpha and green channels shifted right by 8 bits from
      // there they should end up, so shifting left by 6 gives them in the
      // correct position divided by 4.
      *result.getAddr32(dest_x, dest_y) =
          ((rb >> 2) & 0xFF00FF) | ((ag << 6) & 0xFF00FF00);
    }
  }

  return result;
}

// static
SkBitmap ImageOperations::DownsampleByTwoUntilSize(const SkBitmap& bitmap,
                                                   int min_w, int min_h) {
  if (bitmap.width() <= min_w || bitmap.height() <= min_h ||
      min_w < 0 || min_h < 0)
    return bitmap;

  // Since bitmaps are refcounted, this copy will be fast.
  SkBitmap current = bitmap;
  while (current.width() >= min_w * 2 && current.height() >= min_h * 2 &&
         current.width() > 1 && current.height() > 1)
    current = DownsampleByTwo(current);
  return current;
}

}  // namespace skia

