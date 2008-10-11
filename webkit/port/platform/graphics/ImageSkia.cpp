// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#include <windows.h>
#include <vssym32.h>
#include "AffineTransform.h"
#include "BitmapImage.h"
#include "BitmapImageSingleFrameSkia.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Logging.h"
#include "NativeImageSkia.h"
#include "notImplemented.h"
#include "PlatformScrollBar.h"
#include "PlatformString.h"

#include "SkiaUtils.h"
#include "SkShader.h"

#include "base/gfx/gdi_util.h"
#include "base/gfx/image_operations.h"
#include "base/gfx/native_theme.h"
#include "base/gfx/platform_canvas_win.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkit_resources.h"

namespace WebCore {

namespace {

// Transforms the given dimensions with the given matrix. Used to see how big
// images will be once transformed.
void TransformDimensions(const SkMatrix& matrix,
                         float src_width,
                         float src_height,
                         float* dest_width,
                         float* dest_height) {
    // Transform 3 points to see how long each side of the bitmap will be.
    SkPoint src_points[3];  // (0, 0), (width, 0), (0, height).
    src_points[0].set(0, 0);
    src_points[1].set(SkFloatToScalar(src_width), 0);
    src_points[2].set(0, SkFloatToScalar(src_height));

    // Now measure the length of the two transformed vectors relative to the
    // transformed origin to see how big the bitmap will be. Note: for skews,
    // this isn't the best thing, but we don't have skews.
    SkPoint dest_points[3];
    matrix.mapPoints(dest_points, src_points, 3);
    *dest_width = SkScalarToFloat((dest_points[1] - dest_points[0]).length());
    *dest_height = SkScalarToFloat((dest_points[2] - dest_points[0]).length());
}

// Creates an Image for the text area resize corner. We do this by drawing the
// theme native control into a memory buffer then converting the memory buffer
// into a BMP byte stream, then feeding it into the Image object.  We have to
// convert the HBITMAP into a BMP file because the Image object doesn't allow
// us to directly manipulate the image data. We don't bother caching this
// image because the caller holds onto a static copy (see
// WebCore/rendering/RenderLayer.cpp).
static PassRefPtr<Image> GetTextAreaResizeCorner()
{
    // Get the size of the resizer.
    const int width = PlatformScrollbar::verticalScrollbarWidth();
    const int height = PlatformScrollbar::horizontalScrollbarHeight();

    // Setup a memory buffer.
    gfx::PlatformCanvasWin canvas(width, height, false);
    gfx::PlatformDeviceWin& device = canvas.getTopPlatformDevice();
    device.prepareForGDI(0, 0, width, height);
    HDC hdc = device.getBitmapDC();
    RECT widgetRect = { 0, 0, width, height };

    // Do the drawing.
    gfx::NativeTheme::instance()->PaintStatusGripper(hdc, SP_GRIPPER, 0, 0,
                                                     &widgetRect);
    device.postProcessGDI(0, 0, width, height);
    return BitmapImageSingleFrameSkia::create(device.accessBitmap(false));
}

}  // namespace

void FrameData::clear()
{
    // The frame data is released in ImageSource::clear.
    m_frame = 0;
    m_duration = 0.0f;
    m_hasAlpha = true;
}

static inline PassRefPtr<Image> loadImageWithResourceId(int resourceId)
{
    RefPtr<Image> image = BitmapImage::create();
    // Load the desired resource.
    std::string data(webkit_glue::GetDataResource(resourceId));
    RefPtr<SharedBuffer> buffer(SharedBuffer::create(data.data(), data.length()));
    image->setData(buffer, true);
    return image.release();
}

// static
PassRefPtr<Image> Image::loadPlatformResource(const char *name)
{
    if (!strcmp(name, "missingImage"))
        return loadImageWithResourceId(IDR_BROKENIMAGE);
    if (!strcmp(name, "tickmarkDash"))
        return loadImageWithResourceId(IDR_TICKMARK_DASH);
    if (!strcmp(name, "textAreaResizeCorner"))
        return GetTextAreaResizeCorner();
    if (!strcmp(name, "deleteButton") || !strcmp(name, "deleteButtonPressed")) {
        LOG(NotYetImplemented, "Image resource %s does not yet exist\n", name);
        return Image::nullImage();
    }

    LOG(NotYetImplemented, "Unknown image resource %s requested\n", name);
    return Image::nullImage();
}

static bool subsetBitmap(SkBitmap* dst, const SkBitmap& src, const FloatRect& clip)
{
    FloatRect floatBounds(0, 0, src.width(), src.height());
    if (!floatBounds.intersects(clip))
        return false;

    SkAutoLockPixels src_lock(src);
    IntRect bounds(floatBounds);
    void* addr;
    switch (src.getConfig()) {
    case SkBitmap::kIndex8_Config:
    case SkBitmap::kA8_Config:
        addr = (void*)src.getAddr8(bounds.x(), bounds.y());
        break;
    case SkBitmap::kRGB_565_Config:
        addr = (void*)src.getAddr16(bounds.x(), bounds.y());
        break;
    case SkBitmap::kARGB_8888_Config:
        addr = (void*)src.getAddr32(bounds.x(), bounds.y());
        break;
    default:
        SkDEBUGF(("subset_bitmap: can't subset this bitmap config %d\n", src.getConfig()));
        return false;
    }
    dst->setConfig(src.getConfig(), bounds.width(), bounds.height(), src.rowBytes());
    dst->setPixels(addr);
    return false;
}

void Image::drawPattern(GraphicsContext* context,
                        const FloatRect& floatSrcRect,
                        const AffineTransform& patternTransform,
                        const FloatPoint& phase,
                        CompositeOperator compositeOp,
                        const FloatRect& destRect)
{
    if (destRect.isEmpty() || floatSrcRect.isEmpty())
        return;  // nothing to draw

    NativeImageSkia* bitmap = nativeImageForCurrentFrame();
    if (!bitmap)
        return;

    // This is a very inexpensive operation. It will generate a new bitmap but
    // it will internally reference the old bitmap's pixels, adjusting the row
    // stride so the extra pixels appear as padding to the subsetted bitmap.
    SkBitmap src_subset;
    SkIRect srcRect = enclosingIntRect(floatSrcRect);
    bitmap->extractSubset(&src_subset, srcRect);

    SkBitmap resampled;
    SkShader* shader;

    // Figure out what size the bitmap will be in the destination. The
    // destination rect is the bounds of the pattern, we need to use the
    // matrix to see how bit it will be.
    float dest_bitmap_width, dest_bitmap_height;
    TransformDimensions(patternTransform, srcRect.width(), srcRect.height(),
                        &dest_bitmap_width, &dest_bitmap_height);

    // Compute the resampling mode.
    PlatformContextSkia::ResamplingMode resampling;
    if (context->platformContext()->IsPrinting())
      resampling = PlatformContextSkia::RESAMPLE_LINEAR;
    else {
      resampling = PlatformContextSkia::computeResamplingMode(
          *bitmap,
          srcRect.width(), srcRect.height(),
          dest_bitmap_width, dest_bitmap_height);
    }

    // Load the transform WebKit requested.
    SkMatrix matrix(patternTransform);

    if (resampling == PlatformContextSkia::RESAMPLE_AWESOME) {
        // Do nice resampling.
        SkBitmap resampled = gfx::ImageOperations::Resize(src_subset,
            gfx::ImageOperations::RESIZE_LANCZOS3,
            gfx::Size(static_cast<int>(dest_bitmap_width),
                      static_cast<int>(dest_bitmap_height)));
        shader = SkShader::CreateBitmapShader(
            resampled, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);

        // Since we just resized the bitmap, we need to undo the scale set in
        // the image transform.
        matrix.setScaleX(SkIntToScalar(1));
        matrix.setScaleY(SkIntToScalar(1));
    } else {
        // No need to do nice resampling.
        shader = SkShader::CreateBitmapShader(
            src_subset, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);
    }

    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the patter. If WebKit wants
    // a shifted image, it will shift it from there using the patternTransform.
    matrix.postTranslate(SkFloatToScalar(phase.x()),
                         SkFloatToScalar(phase.y()));
    shader->setLocalMatrix(matrix);

    SkPaint paint;
    paint.setShader(shader)->unref();
    paint.setPorterDuffXfermode(WebCoreCompositeToSkiaComposite(compositeOp));
    paint.setFilterBitmap(resampling == PlatformContextSkia::RESAMPLE_LINEAR);

    context->platformContext()->paintSkPaint(destRect, paint);
}

// ================================================
// BitmapImage Class
// ================================================

void BitmapImage::initPlatformData()
{
    // This is not used. On Mac, the "platform" data is a cache of some OS
    // specific versions of the image that are created is some cases. These
    // aren't normally used, it is equivalent to getHBITMAP on Windows, and
    // the platform data is the cache.
}

void BitmapImage::invalidatePlatformData()
{
    // See initPlatformData above.
}

void BitmapImage::checkForSolidColor()
{
}

bool BitmapImage::getHBITMAP(HBITMAP bmp)
{
    NativeImageSkia* bm = nativeImageForCurrentFrame();
    if (!bm)
      return false;

    // |bmp| is already allocated and sized correctly, we just need to draw
    // into it.
    BITMAPINFOHEADER hdr;
    gfx::CreateBitmapHeader(bm->width(), bm->height(), &hdr);
    SkAutoLockPixels bm_lock(*bm); 
    return SetDIBits(0, bmp, 0, bm->height(), bm->getPixels(),
                     reinterpret_cast<BITMAPINFO*>(&hdr), DIB_RGB_COLORS) ==
        bm->height();
}

bool BitmapImage::getHBITMAPOfSize(HBITMAP bmp, LPSIZE size)
{
    notImplemented();
    return false;
}

void BitmapImage::drawFrameMatchingSourceSize(GraphicsContext*,
                                              const FloatRect& dstRect,
                                              const IntSize& srcSize,
                                              CompositeOperator)
{
    notImplemented();
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect,
                       const FloatRect& srcRect, CompositeOperator compositeOp)
{
    if (!m_source.initialized())
        return;
    
    const NativeImageSkia* bm = nativeImageForCurrentFrame();
    if (!bm)
        return;  // It's too early and we don't have an image yet.

    if (srcRect.isEmpty() || dstRect.isEmpty())
        return;  // Nothing to draw.

    ctxt->platformContext()->paintSkBitmap(*bm, enclosingIntRect(srcRect),
        enclosingIntRect(dstRect), WebCoreCompositeToSkiaComposite(compositeOp));

    startAnimation();
}

void BitmapImageSingleFrameSkia::draw(GraphicsContext* ctxt,
                                      const FloatRect& dstRect,
                                      const FloatRect& srcRect,
                                      CompositeOperator compositeOp)
{
    if (srcRect.isEmpty() || dstRect.isEmpty())
        return;  // Nothing to draw.

    ctxt->platformContext()->paintSkBitmap(
        m_nativeImage,
        enclosingIntRect(srcRect),
        enclosingIntRect(dstRect),
        WebCoreCompositeToSkiaComposite(compositeOp));
}

PassRefPtr<BitmapImageSingleFrameSkia> BitmapImageSingleFrameSkia::create(
    const SkBitmap& bitmap)
{
    RefPtr<BitmapImageSingleFrameSkia> image(new BitmapImageSingleFrameSkia());
    if (!bitmap.copyTo(&image->m_nativeImage, bitmap.config()))
        return 0;
    return image.release();
}

} // namespace WebCore
