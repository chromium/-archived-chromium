// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "webkit/tools/test_shell/image_decoder_unittest.h"

#if !defined(OS_WIN)
#include <unistd.h>
#endif

#include "PNGImageDecoder.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/gfx/gdi_util.h"
#include "base/gfx/png_encoder.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "skia/ext/vector_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"

namespace skia {

namespace {

const wchar_t* const kGenerateSwitch = L"vector-canvas-generate";

// Lightweight HDC management.
class Context {
 public:
  Context() : context_(CreateCompatibleDC(NULL)) {
    EXPECT_TRUE(context_);
  }
  ~Context() {
    DeleteDC(context_);
  }

  HDC context() const { return context_; }

 private:
  HDC context_;

  DISALLOW_EVIL_CONSTRUCTORS(Context);
};

// Lightweight HBITMAP management.
class Bitmap {
 public:
  Bitmap(const Context& context, int x, int y) {
    BITMAPINFOHEADER hdr;
    gfx::CreateBitmapHeader(x, y, &hdr);
    bitmap_ = CreateDIBSection(context.context(),
                               reinterpret_cast<BITMAPINFO*>(&hdr), 0,
                               &data_, NULL, 0);
    EXPECT_TRUE(bitmap_);
    EXPECT_TRUE(SelectObject(context.context(), bitmap_));
  }
  ~Bitmap() {
    EXPECT_TRUE(DeleteObject(bitmap_));
  }

 private:
  HBITMAP bitmap_;

  void* data_;

  DISALLOW_EVIL_CONSTRUCTORS(Bitmap);
};

// Lightweight raw-bitmap management. The image, once initialized, is immuable.
// It is mainly used for comparison.
class Image {
 public:
  // Creates the image from the given filename on disk.
  Image(const std::wstring& filename) : ignore_alpha_(true) {
    Vector<char> compressed;
    ReadFileToVector(filename, &compressed);
    EXPECT_TRUE(compressed.size());
    WebCore::PNGImageDecoder decoder;
    decoder.setData(WebCore::SharedBuffer::adoptVector(compressed).get(), true);
    scoped_ptr<NativeImageSkia> image_data(
        decoder.frameBufferAtIndex(0)->asNewNativeImage());
    SetSkBitmap(*image_data);
  }

  // Loads the image from a canvas.
  Image(const skia::PlatformCanvas& canvas) : ignore_alpha_(true) {
    // Use a different way to access the bitmap. The normal way would be to
    // query the SkBitmap.
    HDC context = canvas.getTopPlatformDevice().getBitmapDC();
    HGDIOBJ bitmap = GetCurrentObject(context, OBJ_BITMAP);
    EXPECT_TRUE(bitmap != NULL);
    // Initialize the clip region to the entire bitmap.
    BITMAP bitmap_data;
    EXPECT_EQ(GetObject(bitmap, sizeof(BITMAP), &bitmap_data), sizeof(BITMAP));
    width_ = bitmap_data.bmWidth;
    height_ = bitmap_data.bmHeight;
    row_length_ = bitmap_data.bmWidthBytes;
    size_t size = row_length_ * height_;
    data_.resize(size);
    memcpy(&*data_.begin(), bitmap_data.bmBits, size);
  }

  // Loads the image from a canvas.
  Image(const SkBitmap& bitmap) : ignore_alpha_(true) {
    SetSkBitmap(bitmap);
  }

  int width() const { return width_; }
  int height() const { return height_; }
  int row_length() const { return row_length_; }

  // Save the image to a png file. Used to create the initial test files.
  void SaveToFile(const std::wstring& filename) {
    std::vector<unsigned char> compressed;
    ASSERT_TRUE(PNGEncoder::Encode(&*data_.begin(),
                                   PNGEncoder::FORMAT_BGRA,
                                   width_,
                                   height_,
                                   row_length_,
                                   true,
                                   &compressed));
    ASSERT_TRUE(compressed.size());
    FILE* f = file_util::OpenFile(filename, "wb");
    ASSERT_TRUE(f);
    ASSERT_EQ(fwrite(&*compressed.begin(), 1, compressed.size(), f),
              compressed.size());
    file_util::CloseFile(f);
  }

  // Returns the percentage of the image that is different from the other,
  // between 0 and 100.
  double PercentageDifferent(const Image& rhs) const {
    if (width_ != rhs.width_ ||
        height_ != rhs.height_ ||
        row_length_ != rhs.row_length_ ||
        width_ == 0 ||
        height_ == 0) {
      return 100.;  // When of different size or empty, they are 100% different.
    }
    // Compute pixels different in the overlap
    int pixels_different = 0;
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        uint32_t lhs_pixel = pixel_at(x, y);
        uint32_t rhs_pixel = rhs.pixel_at(x, y);
        if (lhs_pixel != rhs_pixel)
          ++pixels_different;
      }
    }

    // Like the WebKit ImageDiff tool, we define percentage different in terms
    // of the size of the 'actual' bitmap.
    double total_pixels = static_cast<double>(width_) *
                          static_cast<double>(height_);
    return static_cast<double>(pixels_different) / total_pixels * 100.;
  }

  // Returns the 0x0RGB or 0xARGB value of the pixel at the given location,
  // depending on ignore_alpha_.
  uint32 pixel_at(int x, int y) const {
    EXPECT_TRUE(x >= 0 && x < width_);
    EXPECT_TRUE(y >= 0 && y < height_);
    const uint32* data = reinterpret_cast<const uint32*>(&*data_.begin());
    const uint32* data_row = data + y * row_length_ / sizeof(uint32);
    if (ignore_alpha_)
      return data_row[x] & 0xFFFFFF;  // Strip out A.
    else
      return data_row[x];
  }

 protected:
  void SetSkBitmap(const SkBitmap& bitmap) {
    SkAutoLockPixels lock(bitmap);
    width_ = bitmap.width();
    height_ = bitmap.height();
    row_length_ = static_cast<int>(bitmap.rowBytes());
    size_t size = row_length_ * height_;
    data_.resize(size);
    memcpy(&*data_.begin(), bitmap.getAddr(0, 0), size);
  }

 private:
  // Pixel dimensions of the image.
  int width_;
  int height_;

  // Length of a line in bytes.
  int row_length_;

  // Actual bitmap data in arrays of RGBAs (so when loaded as uint32, it's
  // 0xABGR).
  std::vector<unsigned char> data_;

  // Flag to signal if the comparison functions should ignore the alpha channel.
  const bool ignore_alpha_;

  DISALLOW_EVIL_CONSTRUCTORS(Image);
};

// Base for tests. Capability to process an image.
class ImageTest : public testing::Test {
 public:
  // In what state is the test running.
  enum ProcessAction {
    GENERATE,
    COMPARE,
    NOOP,
  };

  ImageTest(ProcessAction default_action)
      : action_(default_action) {
  }

 protected:
  virtual void SetUp() {
    const testing::TestInfo& test_info =
        *testing::UnitTest::GetInstance()->current_test_info();
    PathService::Get(base::DIR_SOURCE_ROOT, &test_dir_);
    file_util::AppendToPath(&test_dir_, L"skia");
    file_util::AppendToPath(&test_dir_, L"ext");
    file_util::AppendToPath(&test_dir_, L"data");
    file_util::AppendToPath(&test_dir_,
        ASCIIToWide(test_info.test_case_name()));
    file_util::AppendToPath(&test_dir_, ASCIIToWide(test_info.name()));

    // Hack for a quick lowercase. We assume all the tests names are ASCII.
    std::string tmp(WideToASCII(test_dir_));
    for (size_t i = 0; i < tmp.size(); ++i)
      tmp[i] = ToLowerASCII(tmp[i]);
    test_dir_ = ASCIIToWide(tmp);

    if (action_ == GENERATE) {
      // Make sure the directory exist.
      file_util::CreateDirectory(test_dir_);
    }
  }

  // Returns the fully qualified path of a data file.
  std::wstring test_file(const std::wstring& filename) const {
    // Hack for a quick lowercase. We assume all the test data file names are
    // ASCII.
    std::string tmp(WideToASCII(filename));
    for (size_t i = 0; i < tmp.size(); ++i)
      tmp[i] = ToLowerASCII(tmp[i]);

    std::wstring path(test_dir_);
    file_util::AppendToPath(&path, ASCIIToWide(tmp));
    return path;
  }

  // Compares or saves the bitmap currently loaded in the context, depending on
  // kGenerating value. Returns 0 on success or any positive value between ]0,
  // 100] on failure. The return value is the percentage of difference between
  // the image in the file and the image in the canvas.
  double ProcessCanvas(const skia::PlatformCanvas& canvas,
                       std::wstring filename) const {
    filename +=  L".png";
    switch (action_) {
      case GENERATE:
        SaveImage(canvas, filename);
        return 0.;
      case COMPARE:
        return CompareImage(canvas, filename);
      case NOOP:
        return 0;
      default:
        // Invalid state, returns that the image is 100 different.
        return 100.;
    }
  }

  // Compares the bitmap currently loaded in the context with the file. Returns
  // the percentage of pixel difference between both images, between 0 and 100.
  double CompareImage(const skia::PlatformCanvas& canvas,
                      const std::wstring& filename) const {
    Image image1(canvas);
    Image image2(test_file(filename));
    double diff = image1.PercentageDifferent(image2);
    return diff;
  }

  // Saves the bitmap currently loaded in the context into the file.
  void SaveImage(const skia::PlatformCanvas& canvas,
                 const std::wstring& filename) const {
    Image(canvas).SaveToFile(test_file(filename));
  }

  ProcessAction action_;

  // Path to directory used to contain the test data.
  std::wstring test_dir_;

  DISALLOW_EVIL_CONSTRUCTORS(ImageTest);
};

// Premultiply the Alpha channel on the R, B and G channels.
void Premultiply(SkBitmap bitmap) {
  SkAutoLockPixels lock(bitmap);
  for (int x = 0; x < bitmap.width(); ++x) {
    for (int y = 0; y < bitmap.height(); ++y) {
      uint32_t* pixel_addr = bitmap.getAddr32(x, y);
      uint32_t color = *pixel_addr;
      BYTE alpha = SkColorGetA(color);
      if (!alpha) {
        *pixel_addr = 0;
      } else {
        BYTE alpha_offset = alpha / 2;
        *pixel_addr = SkColorSetARGB(
            SkColorGetA(color),
            (SkColorGetR(color) * 255 + alpha_offset) / alpha,
            (SkColorGetG(color) * 255 + alpha_offset) / alpha,
            (SkColorGetB(color) * 255 + alpha_offset) / alpha);
      }
    }
  }
}

void LoadPngFileToSkBitmap(const std::wstring& filename,
                           SkBitmap* bitmap,
                           bool is_opaque) {
  Vector<char> compressed;
  ReadFileToVector(filename, &compressed);
  EXPECT_TRUE(compressed.size());
  WebCore::PNGImageDecoder decoder;
  decoder.setData(WebCore::SharedBuffer::adoptVector(compressed).get(), true);
  scoped_ptr<NativeImageSkia> image_data(
      decoder.frameBufferAtIndex(0)->asNewNativeImage());
  *bitmap = *image_data;
  EXPECT_EQ(is_opaque, bitmap->isOpaque());
  Premultiply(*bitmap);
}

}  // namespace

// Streams an image.
inline std::ostream& operator<<(std::ostream& out, const Image& image) {
  return out << "Image(" << image.width() << ", "
             << image.height() << ", " << image.row_length() << ")";
}

// Runs simultaneously the same drawing commands on VectorCanvas and
// PlatformCanvas and compare the results.
class VectorCanvasTest : public ImageTest {
 public:
  typedef ImageTest parent;

  VectorCanvasTest() : parent(CurrentMode()), compare_canvas_(true) {
  }

 protected:
  virtual void SetUp() {
    parent::SetUp();
    Init(100);
    number_ = 0;
  }

  virtual void TearDown() {
    delete pcanvas_;
    pcanvas_ = NULL;

    delete vcanvas_;
    vcanvas_ = NULL;

    delete bitmap_;
    bitmap_ = NULL;

    delete context_;
    context_ = NULL;

    parent::TearDown();
  }

  void Init(int size) {
    size_ = size;
    context_ = new Context();
    bitmap_ = new Bitmap(*context_, size_, size_);
    vcanvas_ = new VectorCanvas(context_->context(), size_, size_);
    pcanvas_ = new PlatformCanvas(size_, size_, false);

    // Clear white.
    vcanvas_->drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
    pcanvas_->drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
  }

  // Compares both canvas and returns the pixel difference in percentage between
  // both images. 0 on success and ]0, 100] on failure.
  double ProcessImage(const std::wstring& filename) {
    std::wstring number(StringPrintf(L"%02d_", number_++));
    double diff1 = parent::ProcessCanvas(*vcanvas_, number + L"vc_" + filename);
    double diff2 = parent::ProcessCanvas(*pcanvas_, number + L"pc_" + filename);
    if (!compare_canvas_)
      return std::max(diff1, diff2);

    Image image1(*vcanvas_);
    Image image2(*pcanvas_);
    double diff = image1.PercentageDifferent(image2);
    return std::max(std::max(diff1, diff2), diff);
  }

  // Returns COMPARE, which is the default. If kGenerateSwitch command
  // line argument is used to start this process, GENERATE is returned instead.
  static ProcessAction CurrentMode() {
    return CommandLine::ForCurrentProcess()->HasSwitch(kGenerateSwitch) ?
               GENERATE : COMPARE;
  }

  // Length in x and y of the square canvas.
  int size_;

  // Current image number in the current test. Used to number of test files.
  int number_;

  // A temporary HDC to draw into.
  Context* context_;

  // Bitmap created inside context_.
  Bitmap* bitmap_;

  // Vector based canvas.
  VectorCanvas* vcanvas_;

  // Pixel based canvas.
  PlatformCanvas* pcanvas_;

  // When true (default), vcanvas_ and pcanvas_ contents are compared and
  // verified to be identical.
  bool compare_canvas_;
};


////////////////////////////////////////////////////////////////////////////////
// Actual tests

TEST_F(VectorCanvasTest, Uninitialized) {
  // Do a little mubadumba do get uninitialized stuff.
  VectorCanvasTest::TearDown();

  // The goal is not to verify that have the same uninitialized data.
  compare_canvas_ = false;

  context_ = new Context();
  bitmap_ = new Bitmap(*context_, size_, size_);
  vcanvas_ = new VectorCanvas(context_->context(), size_, size_);
  pcanvas_ = new PlatformCanvas(size_, size_, false);

  // VectorCanvas default initialization is black.
  // PlatformCanvas default initialization is almost white 0x01FFFEFD (invalid
  // Skia color) in both Debug and Release. See magicTransparencyColor in
  // platform_device.cc
  EXPECT_EQ(0., ProcessImage(L"empty"));
}

TEST_F(VectorCanvasTest, BasicDrawing) {
  EXPECT_EQ(Image(*vcanvas_).PercentageDifferent(Image(*pcanvas_)), 0.)
      << L"clean";
  EXPECT_EQ(0., ProcessImage(L"clean"));

  // Clear white.
  {
    vcanvas_->drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
    pcanvas_->drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
  }
  EXPECT_EQ(0., ProcessImage(L"drawARGB"));

  // Diagonal line top-left to bottom-right.
  {
    SkPaint paint;
    // Default color is black.
    vcanvas_->drawLine(10, 10, 90, 90, paint);
    pcanvas_->drawLine(10, 10, 90, 90, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawLine_black"));

  // Rect.
  {
    SkPaint paint;
    paint.setColor(SK_ColorGREEN);
    vcanvas_->drawRectCoords(25, 25, 75, 75, paint);
    pcanvas_->drawRectCoords(25, 25, 75, 75, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawRect_green"));

  // A single-point rect doesn't leave any mark.
  {
    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    vcanvas_->drawRectCoords(5, 5, 5, 5, paint);
    pcanvas_->drawRectCoords(5, 5, 5, 5, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawRect_noop"));

  // Rect.
  {
    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    vcanvas_->drawRectCoords(75, 50, 80, 55, paint);
    pcanvas_->drawRectCoords(75, 50, 80, 55, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawRect_noop"));

  // Empty again
  {
    vcanvas_->drawPaint(SkPaint());
    pcanvas_->drawPaint(SkPaint());
  }
  EXPECT_EQ(0., ProcessImage(L"drawPaint_black"));

  // Horizontal line left to right.
  {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    vcanvas_->drawLine(10, 20, 90, 20, paint);
    pcanvas_->drawLine(10, 20, 90, 20, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawLine_left_to_right"));

  // Vertical line downward.
  {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    vcanvas_->drawLine(30, 10, 30, 90, paint);
    pcanvas_->drawLine(30, 10, 30, 90, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawLine_red"));
}

TEST_F(VectorCanvasTest, Circles) {
  // There is NO WAY to make them agree. At least verify that the output doesn't
  // change across versions. This test is disabled. See bug 1060231.
  compare_canvas_ = false;

  // Stroked Circle.
  {
    SkPaint paint;
    SkPath path;
    path.addCircle(50, 75, 10);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorMAGENTA);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"circle_stroke"));

  // Filled Circle.
  {
    SkPaint paint;
    SkPath path;
    path.addCircle(50, 25, 10);
    paint.setStyle(SkPaint::kFill_Style);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"circle_fill"));

  // Stroked Circle over.
  {
    SkPaint paint;
    SkPath path;
    path.addCircle(50, 25, 10);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorBLUE);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"circle_over_strike"));

  // Stroke and Fill Circle.
  {
    SkPaint paint;
    SkPath path;
    path.addCircle(12, 50, 10);
    paint.setStyle(SkPaint::kStrokeAndFill_Style);
    paint.setColor(SK_ColorRED);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"circle_stroke_and_fill"));

  // Line + Quad + Cubic.
  {
    SkPaint paint;
    SkPath path;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorGREEN);
    path.moveTo(1, 1);
    path.lineTo(60, 40);
    path.lineTo(80, 80);
    path.quadTo(20, 50, 10, 90);
    path.quadTo(50, 20, 90, 10);
    path.cubicTo(20, 40, 50, 50, 10, 10);
    path.cubicTo(30, 20, 50, 50, 90, 10);
    path.addRect(90, 90, 95, 96);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"mixed_stroke"));
}

TEST_F(VectorCanvasTest, LineOrientation) {
  // There is NO WAY to make them agree. At least verify that the output doesn't
  // change across versions. This test is disabled. See bug 1060231.
  compare_canvas_ = false;

  // Horizontal lines.
  {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    // Left to right.
    vcanvas_->drawLine(10, 20, 90, 20, paint);
    pcanvas_->drawLine(10, 20, 90, 20, paint);
    // Right to left.
    vcanvas_->drawLine(90, 30, 10, 30, paint);
    pcanvas_->drawLine(90, 30, 10, 30, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"horizontal"));

  // Vertical lines.
  {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    // Top down.
    vcanvas_->drawLine(20, 10, 20, 90, paint);
    pcanvas_->drawLine(20, 10, 20, 90, paint);
    // Bottom up.
    vcanvas_->drawLine(30, 90, 30, 10, paint);
    pcanvas_->drawLine(30, 90, 30, 10, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"vertical"));

  // Try again with a 180 degres rotation.
  vcanvas_->rotate(180);
  pcanvas_->rotate(180);

  // Horizontal lines (rotated).
  {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    vcanvas_->drawLine(-10, -25, -90, -25, paint);
    pcanvas_->drawLine(-10, -25, -90, -25, paint);
    vcanvas_->drawLine(-90, -35, -10, -35, paint);
    pcanvas_->drawLine(-90, -35, -10, -35, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"horizontal_180"));

  // Vertical lines (rotated).
  {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    vcanvas_->drawLine(-25, -10, -25, -90, paint);
    pcanvas_->drawLine(-25, -10, -25, -90, paint);
    vcanvas_->drawLine(-35, -90, -35, -10, paint);
    pcanvas_->drawLine(-35, -90, -35, -10, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"vertical_180"));
}

TEST_F(VectorCanvasTest, PathOrientation) {
  // There is NO WAY to make them agree. At least verify that the output doesn't
  // change across versions. This test is disabled. See bug 1060231.
  compare_canvas_ = false;

  // Horizontal lines.
  {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorRED);
    SkPath path;
    SkPoint start;
    start.set(10, 20);
    SkPoint end;
    end.set(90, 20);
    path.moveTo(start);
    path.lineTo(end);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawPath_ltr"));

  // Horizontal lines.
  {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorRED);
    SkPath path;
    SkPoint start;
    start.set(90, 30);
    SkPoint end;
    end.set(10, 30);
    path.moveTo(start);
    path.lineTo(end);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"drawPath_rtl"));
}

TEST_F(VectorCanvasTest, DiagonalLines) {
  SkPaint paint;
  paint.setColor(SK_ColorRED);

  vcanvas_->drawLine(10, 10, 90, 90, paint);
  pcanvas_->drawLine(10, 10, 90, 90, paint);
  EXPECT_EQ(0., ProcessImage(L"nw-se"));

  // Starting here, there is NO WAY to make them agree. At least verify that the
  // output doesn't change across versions. This test is disabled. See bug
  // 1060231.
  compare_canvas_ = false;

  vcanvas_->drawLine(10, 95, 90, 15, paint);
  pcanvas_->drawLine(10, 95, 90, 15, paint);
  EXPECT_EQ(0., ProcessImage(L"sw-ne"));

  vcanvas_->drawLine(90, 10, 10, 90, paint);
  pcanvas_->drawLine(90, 10, 10, 90, paint);
  EXPECT_EQ(0., ProcessImage(L"ne-sw"));

  vcanvas_->drawLine(95, 90, 15, 10, paint);
  pcanvas_->drawLine(95, 90, 15, 10, paint);
  EXPECT_EQ(0., ProcessImage(L"se-nw"));
}

TEST_F(VectorCanvasTest, PathEffects) {
  {
    SkPaint paint;
    SkScalar intervals[] = { 1, 1 };
    SkPathEffect* effect = new SkDashPathEffect(intervals, arraysize(intervals),
                                                0);
    paint.setPathEffect(effect)->unref();
    paint.setColor(SK_ColorMAGENTA);
    paint.setStyle(SkPaint::kStroke_Style);

    vcanvas_->drawLine(10, 10, 90, 10, paint);
    pcanvas_->drawLine(10, 10, 90, 10, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"dash_line"));


  // Starting here, there is NO WAY to make them agree. At least verify that the
  // output doesn't change across versions. This test is disabled. See bug
  // 1060231.
  compare_canvas_ = false;

  {
    SkPaint paint;
    SkScalar intervals[] = { 3, 5 };
    SkPathEffect* effect = new SkDashPathEffect(intervals, arraysize(intervals),
                                                0);
    paint.setPathEffect(effect)->unref();
    paint.setColor(SK_ColorMAGENTA);
    paint.setStyle(SkPaint::kStroke_Style);

    SkPath path;
    path.moveTo(10, 15);
    path.lineTo(90, 15);
    path.lineTo(90, 90);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"dash_path"));

  {
    SkPaint paint;
    SkScalar intervals[] = { 2, 1 };
    SkPathEffect* effect = new SkDashPathEffect(intervals, arraysize(intervals),
                                                0);
    paint.setPathEffect(effect)->unref();
    paint.setColor(SK_ColorMAGENTA);
    paint.setStyle(SkPaint::kStroke_Style);

    vcanvas_->drawRectCoords(20, 20, 30, 30, paint);
    pcanvas_->drawRectCoords(20, 20, 30, 30, paint);
  }
  EXPECT_EQ(0., ProcessImage(L"dash_rect"));

  // This thing looks like it has been drawn by a 3 years old kid. I haven't
  // filed a bug on this since I guess nobody is expecting this to look nice.
  {
    SkPaint paint;
    SkScalar intervals[] = { 1, 1 };
    SkPathEffect* effect = new SkDashPathEffect(intervals, arraysize(intervals),
                                                0);
    paint.setPathEffect(effect)->unref();
    paint.setColor(SK_ColorMAGENTA);
    paint.setStyle(SkPaint::kStroke_Style);

    SkPath path;
    path.addCircle(50, 75, 10);
    vcanvas_->drawPath(path, paint);
    pcanvas_->drawPath(path, paint);
    EXPECT_EQ(0., ProcessImage(L"circle"));
  }
}

TEST_F(VectorCanvasTest, Bitmaps) {
  {
    SkBitmap bitmap;
    LoadPngFileToSkBitmap(test_file(L"bitmap_opaque.png"), &bitmap, true);
    vcanvas_->drawBitmap(bitmap, 13, 3, NULL);
    pcanvas_->drawBitmap(bitmap, 13, 3, NULL);
    EXPECT_EQ(0., ProcessImage(L"opaque"));
  }

  {
    SkBitmap bitmap;
    LoadPngFileToSkBitmap(test_file(L"bitmap_alpha.png"), &bitmap, false);
    vcanvas_->drawBitmap(bitmap, 5, 15, NULL);
    pcanvas_->drawBitmap(bitmap, 5, 15, NULL);
    EXPECT_EQ(0., ProcessImage(L"alpha"));
  }
}

TEST_F(VectorCanvasTest, ClippingRect) {
  SkBitmap bitmap;
  LoadPngFileToSkBitmap(test_file(L"..\\bitmaps\\bitmap_opaque.png"), &bitmap,
                        true);
  SkRect rect;
  rect.fLeft = 2;
  rect.fTop = 2;
  rect.fRight = 30.5f;
  rect.fBottom = 30.5f;
  vcanvas_->clipRect(rect);
  pcanvas_->clipRect(rect);

  vcanvas_->drawBitmap(bitmap, 13, 3, NULL);
  pcanvas_->drawBitmap(bitmap, 13, 3, NULL);
  EXPECT_EQ(0., ProcessImage(L"rect"));
}

TEST_F(VectorCanvasTest, ClippingPath) {
  SkBitmap bitmap;
  LoadPngFileToSkBitmap(test_file(L"..\\bitmaps\\bitmap_opaque.png"), &bitmap,
                        true);
  SkPath path;
  path.addCircle(20, 20, 10);
  vcanvas_->clipPath(path);
  pcanvas_->clipPath(path);

  vcanvas_->drawBitmap(bitmap, 14, 3, NULL);
  pcanvas_->drawBitmap(bitmap, 14, 3, NULL);
  EXPECT_EQ(0., ProcessImage(L"path"));
}

TEST_F(VectorCanvasTest, ClippingCombined) {
  SkBitmap bitmap;
  LoadPngFileToSkBitmap(test_file(L"..\\bitmaps\\bitmap_opaque.png"), &bitmap,
                        true);

  SkRect rect;
  rect.fLeft = 2;
  rect.fTop = 2;
  rect.fRight = 30.5f;
  rect.fBottom = 30.5f;
  vcanvas_->clipRect(rect);
  pcanvas_->clipRect(rect);
  SkPath path;
  path.addCircle(20, 20, 10);
  vcanvas_->clipPath(path, SkRegion::kUnion_Op);
  pcanvas_->clipPath(path, SkRegion::kUnion_Op);

  vcanvas_->drawBitmap(bitmap, 15, 3, NULL);
  pcanvas_->drawBitmap(bitmap, 15, 3, NULL);
  EXPECT_EQ(0., ProcessImage(L"combined"));
}

TEST_F(VectorCanvasTest, ClippingIntersect) {
  SkBitmap bitmap;
  LoadPngFileToSkBitmap(test_file(L"..\\bitmaps\\bitmap_opaque.png"), &bitmap,
                        true);

  SkRect rect;
  rect.fLeft = 2;
  rect.fTop = 2;
  rect.fRight = 30.5f;
  rect.fBottom = 30.5f;
  vcanvas_->clipRect(rect);
  pcanvas_->clipRect(rect);
  SkPath path;
  path.addCircle(23, 23, 15);
  vcanvas_->clipPath(path);
  pcanvas_->clipPath(path);

  vcanvas_->drawBitmap(bitmap, 15, 3, NULL);
  pcanvas_->drawBitmap(bitmap, 15, 3, NULL);
  EXPECT_EQ(0., ProcessImage(L"intersect"));
}

TEST_F(VectorCanvasTest, ClippingClean) {
  SkBitmap bitmap;
  LoadPngFileToSkBitmap(test_file(L"..\\bitmaps\\bitmap_opaque.png"), &bitmap,
                        true);
  {
    SkRegion old_region(pcanvas_->getTotalClip());
    SkRect rect;
    rect.fLeft = 2;
    rect.fTop = 2;
    rect.fRight = 30.5f;
    rect.fBottom = 30.5f;
    vcanvas_->clipRect(rect);
    pcanvas_->clipRect(rect);

    vcanvas_->drawBitmap(bitmap, 15, 3, NULL);
    pcanvas_->drawBitmap(bitmap, 15, 3, NULL);
    EXPECT_EQ(0., ProcessImage(L"clipped"));
    vcanvas_->clipRegion(old_region, SkRegion::kReplace_Op);
    pcanvas_->clipRegion(old_region, SkRegion::kReplace_Op);
  }
  {
    // Verify that the clipping region has been fixed back.
    vcanvas_->drawBitmap(bitmap, 55, 3, NULL);
    pcanvas_->drawBitmap(bitmap, 55, 3, NULL);
    EXPECT_EQ(0., ProcessImage(L"unclipped"));
  }
}

TEST_F(VectorCanvasTest, DISABLED_Matrix) {
  SkBitmap bitmap;
  LoadPngFileToSkBitmap(test_file(L"..\\bitmaps\\bitmap_opaque.png"), &bitmap,
                        true);
  {
    vcanvas_->translate(15, 3);
    pcanvas_->translate(15, 3);
    vcanvas_->drawBitmap(bitmap, 0, 0, NULL);
    pcanvas_->drawBitmap(bitmap, 0, 0, NULL);
    EXPECT_EQ(0., ProcessImage(L"translate1"));
  }
  {
    vcanvas_->translate(-30, -23);
    pcanvas_->translate(-30, -23);
    vcanvas_->drawBitmap(bitmap, 0, 0, NULL);
    pcanvas_->drawBitmap(bitmap, 0, 0, NULL);
    EXPECT_EQ(0., ProcessImage(L"translate2"));
  }
  vcanvas_->resetMatrix();
  pcanvas_->resetMatrix();

  // For scaling and rotation, they use a different algorithm (nearest
  // neighborhood vs smoothing). At least verify that the output doesn't change
  // across versions.
  compare_canvas_ = false;

  {
    vcanvas_->scale(SkDoubleToScalar(1.9), SkDoubleToScalar(1.5));
    pcanvas_->scale(SkDoubleToScalar(1.9), SkDoubleToScalar(1.5));
    vcanvas_->drawBitmap(bitmap, 1, 1, NULL);
    pcanvas_->drawBitmap(bitmap, 1, 1, NULL);
    EXPECT_EQ(0., ProcessImage(L"scale"));
  }
  vcanvas_->resetMatrix();
  pcanvas_->resetMatrix();

  {
    vcanvas_->rotate(67);
    pcanvas_->rotate(67);
    vcanvas_->drawBitmap(bitmap, 20, -50, NULL);
    pcanvas_->drawBitmap(bitmap, 20, -50, NULL);
    EXPECT_EQ(0., ProcessImage(L"rotate"));
  }
}

}  // namespace skia
