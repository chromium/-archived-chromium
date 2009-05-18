// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// View.h : Paints the current movie frame (with scaling) to the display.
// TODO(fbarchard): Consider rewriting as view.cc and view.h

#ifndef MEDIA_PLAYER_VIEW_H_
#define MEDIA_PLAYER_VIEW_H_

// Enable timing code by turning on TESTING macro.
// #define TESTING 1

#ifdef TESTING
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#endif

// Enable swscaler.
// TODO(fbarchard): Include header and change bilinear to point sampling.
// #define TEST_SWSCALER 1

#include <atlscrl.h>

#include "base/basictypes.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/yuv_convert.h"
#include "media/player/movie.h"
#include "media/player/wtl_renderer.h"

#ifdef TESTING
// Fetch current time as milliseconds.
// Return as double for high duration and precision.
static inline double GetTime() {
  LARGE_INTEGER perf_time, perf_hz;
  QueryPerformanceFrequency(&perf_hz);  // May change with speed step.
  QueryPerformanceCounter(&perf_time);
  return perf_time.QuadPart * 1000.0 / perf_hz.QuadPart;  // Convert to ms.
}
#endif

class WtlVideoWindow : public CScrollWindowImpl<WtlVideoWindow> {
 public:
  DECLARE_WND_CLASS_EX(NULL, 0, -1)

  BEGIN_MSG_MAP(WtlVideoWindow)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
  CHAIN_MSG_MAP(CScrollWindowImpl<WtlVideoWindow>);
  END_MSG_MAP()

  WtlVideoWindow() {
    size_.cx = 0;
    size_.cy = 0;
    view_size_ = 2;  // Normal size.
    view_rotate_ = media::ROTATE_0;
    renderer_ = new WtlVideoRenderer(this);
    last_frame_ = NULL;
    hbmp_ = NULL;
  }

  BOOL PreTranslateMessage(MSG* /*msg*/)  {
    return FALSE;
  }

  void AllocateVideoBitmap(CDCHandle dc) {
    // See note on SetSize for why we check size_.cy.
    if (bmp_.IsNull() && size_.cy > 0) {
      BITMAPINFO bmi;
      bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
      bmi.bmiHeader.biWidth = size_.cx;
      bmi.bmiHeader.biHeight = size_.cy;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      bmi.bmiHeader.biSizeImage = 0;
      bmi.bmiHeader.biXPelsPerMeter = 100;
      bmi.bmiHeader.biYPelsPerMeter = 100;
      bmi.bmiHeader.biClrUsed = 0;
      bmi.bmiHeader.biClrImportant = 0;
      void* pBits;
      bmp_.CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
      SetScrollOffset(0, 0, FALSE);
      SetScrollSize(size_);
    }
  }

  // Called on the video renderer's thread.
  // Note that AllocateVideoBitmap examines the size_.cy value to determine
  // if a bitmap should be allocated, so we set it last to avoid a race
  // condition.
  void SetSize(int cx, int cy) {
    size_.cx = cx;
    size_.cy = cy;
  }

  void Reset() {
    if (!bmp_.IsNull()) {
      bmp_.DeleteObject();
    }
    size_.cx = 0;
    size_.cy = 0;
    // TODO(frank): get rid of renderer at reset too.
  }

  LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/,
                            BOOL& /*bHandled*/) {
    CDCHandle dc = reinterpret_cast<HDC>(wParam);
    AllocateVideoBitmap(dc);
    RECT rect;
    GetClientRect(&rect);
    int x = 0;
    int y = 0;
    if (!bmp_.IsNull()) {
      x = size_.cx + 1;
      y = size_.cy + 1;
    }
    if (rect.right > m_sizeAll.cx) {
      RECT rectRight = rect;
      rectRight.left = x;
      rectRight.bottom = y;
      dc.FillRect(&rectRight, COLOR_WINDOW);
    }
    if (rect.bottom > m_sizeAll.cy) {
      RECT rectBottom = rect;
      rectBottom.top = y;
      dc.FillRect(&rectBottom, COLOR_WINDOW);
    }
    if (!bmp_.IsNull()) {
      dc.MoveTo(size_.cx, 0);
      dc.LineTo(size_.cx, size_.cy);
      dc.LineTo(0, size_.cy);
    }
    return 0;
  }

  // Convert the video frame to RGB and Blit.
  void ConvertFrame(media::VideoFrame * video_frame) {
    media::VideoSurface frame_in;
    bool lock_result = video_frame->Lock(&frame_in);
    DCHECK(lock_result);
    BITMAP bm;
    bmp_.GetBitmap(&bm);
    int dibwidth = bm.bmWidth;
    int dibheight = bm.bmHeight;

    uint8 *movie_dib_bits = reinterpret_cast<uint8 *>(bm.bmBits) +
        bm.bmWidthBytes * (bm.bmHeight - 1);
    int dibrowbytes = -bm.bmWidthBytes;
    int clipped_width = frame_in.width;
    if (dibwidth < clipped_width) {
      clipped_width = dibwidth;
    }
    int clipped_height = frame_in.height;
    if (dibheight < clipped_height) {
      clipped_height = dibheight;
    }

    int scaled_width = clipped_width;
    int scaled_height = clipped_height;
    switch (view_size_) {
      case 0:
        scaled_width = clipped_width / 4;
        scaled_height = clipped_height / 4;
        break;

      case 1:
        scaled_width = clipped_width / 2;
        scaled_height = clipped_height / 2;
        break;

      case 2:
      default:  // Assume 1:1 for stray view sizes.
        scaled_width = clipped_width;
        scaled_height = clipped_height;
        break;

      case 3:  // Double.
        scaled_width = clipped_width;
        scaled_height = clipped_height;
        clipped_width = scaled_width / 2;
        clipped_height = scaled_height / 2;
        break;

      case 4:  // Triple.
        scaled_width = clipped_width;
        scaled_height = clipped_height;
        clipped_width = scaled_width / 3;
        clipped_height = scaled_height / 3;
        break;

      case 5:  // Quadruple.
        scaled_width = clipped_width;
        scaled_height = clipped_height;
        clipped_width = scaled_width / 4;
        clipped_height = scaled_height / 4;
        break;
    }

    // Append each frame to end of file.
    bool enable_dump_yuv_file = media::Movie::get()->GetDumpYuvFileEnable();
    if (enable_dump_yuv_file) {
      DumpYUV(frame_in);
    }

#ifdef TESTING
    double yuv_time_start = GetTime();  // Start timer.
#endif

    bool enable_draw = media::Movie::get()->GetDrawEnable();
    if (enable_draw) {
#ifdef TEST_SWSCALER
      bool enable_swscaler = media::Movie::get()->GetSwscalerEnable();
      if (enable_swscaler) {
        uint8* data_out[3];
        int stride_out[3];
        data_out[0] = movie_dib_bits;
        data_out[1] = NULL;
        data_out[2] = NULL;
        stride_out[0] = dibrowbytes;
        stride_out[1] = 0;
        stride_out[2] = 0;

        if (!sws_context_) {
          DCHECK(frame_in.format == VideoSurface::YV12);
          sws_context_ = sws_getContext(frame_in.width, frame_in.height,
                                        PIX_FMT_YUV420P, width_, height_,
                                        PIX_FMT_RGB32, SWS_FAST_BILINEAR,
                                        NULL, NULL, NULL);
          DCHECK(sws_context_);
        }

        sws_scale(sws_context_, frame_in.data, frame_in.strides, 0,
                  height_, data_out, stride_out);
      } else {
#endif
        DCHECK(bm.bmBitsPixel == 32);
        DrawYUV(frame_in,
                movie_dib_bits,
                dibrowbytes,
                clipped_width,
                clipped_height,
                scaled_width,
                scaled_height);
#ifdef TEST_SWSCALER
      }
#endif
    }
#ifdef TESTING
    double yuv_time_end = GetTime();
    static int yuv_time_count = 0;
    static double yuv_time_sum = 0.;
    if (!yuv_time_count)
      yuv_time_sum = 0.;
    yuv_time_sum += (yuv_time_end - yuv_time_start);
    ++yuv_time_count;

    char outputbuf[512];
    _snprintf_s(outputbuf, sizeof(outputbuf), "test %f", yuv_time_end);
    _snprintf_s(outputbuf, sizeof(outputbuf),
                "yuv %5.2f ms avg %5.2f ms\n",
                yuv_time_end - yuv_time_start,
                yuv_time_sum / yuv_time_count);
    OutputDebugStringA(outputbuf);
#endif
  }

  void DoPaint(CDCHandle dc) {
    AllocateVideoBitmap(dc);
    if (!bmp_.IsNull()) {
      scoped_refptr<media::VideoFrame> frame;
      renderer_->GetCurrentFrame(&frame);
      if (frame.get()) {
        base::TimeDelta frame_timestamp = frame->GetTimestamp();
        if (frame != last_frame_ || frame_timestamp != last_timestamp_) {
          last_frame_ = frame;
          last_timestamp_ = frame_timestamp;
          ConvertFrame(frame);
        }
        frame = NULL;
      }

#ifdef TESTING
      double paint_time_start = GetTime();
      static double paint_time_previous = 0;
      if (!paint_time_previous)
        paint_time_previous = paint_time_start;
#endif
      CDC dcMem;
      dcMem.CreateCompatibleDC(dc);
      HBITMAP hBmpOld = hbmp_ ? hbmp_: dcMem.SelectBitmap(bmp_);
      dc.BitBlt(0, 0, size_.cx, size_.cy, dcMem, 0, 0, SRCCOPY);
      dcMem.SelectBitmap(hBmpOld);
#ifdef TESTING
      double paint_time_end = GetTime();
      static int paint_count = 0;
      static double paint_time_sum = 0;
      paint_time_sum += paint_time_end - paint_time_start;
      ++paint_count;
      char outputbuf[512];
      _snprintf_s(outputbuf, sizeof(outputbuf),
                  "paint time %5.2f ms blit %5.2f ms avg %5.2f ms\n",
                  paint_time_start - paint_time_previous,
                  paint_time_end - paint_time_start,
                  paint_time_sum / paint_count);
      OutputDebugStringA(outputbuf);

      paint_time_previous = paint_time_start;
#endif
    }
  }  // End of DoPaint function.

  void SetViewSize(int view_size) {
    view_size_ = view_size;
  }

  int GetViewSize() {
    return view_size_;
  }

  void SetViewRotate(int view_rotate) {
    switch (view_rotate) {
      default:
      case 0:
        view_rotate_  = media::ROTATE_0;
        break;
      case 1:
        view_rotate_  = media::ROTATE_90;
        break;
      case 2:
        view_rotate_  = media::ROTATE_180;
        break;
      case 3:
        view_rotate_  = media::ROTATE_270;
        break;
      case 4:
        view_rotate_  = media::MIRROR_ROTATE_0;
        break;
      case 5:
        view_rotate_  = media::MIRROR_ROTATE_180;
        break;
    }
  }

  int GetViewRotate() {
    int view_rotate = 0;
    switch (view_rotate_) {
      default:
      case media::ROTATE_0:
        view_rotate  = 0;
        break;
      case media::ROTATE_90:
        view_rotate  = 1;
        break;
      case media::ROTATE_180:
        view_rotate  = 2;
        break;
      case media::ROTATE_270:
        view_rotate  = 3;
        break;
      case media::MIRROR_ROTATE_0:
        view_rotate  = 4;
        break;
      case media::MIRROR_ROTATE_180:
        view_rotate  = 5;
        break;
    }
    return view_rotate;
  }

  void SetBitmap(HBITMAP hbmp) {
    hbmp_ = hbmp;
  }

  CBitmap bmp_;  // Used by mainfrm.h.
  SIZE size_;  // Used by WtlVideoWindow.
  scoped_refptr<WtlVideoRenderer> renderer_;  // Used by WtlVideoWindow.

 private:
  HBITMAP hbmp_;  // For Images
  // View Size: 0=1/4, 1=0.5, 2=normal, 3=2x, 4=3x, 5=4x, 3=fit, 4=full.
  int view_size_;

  // View Rotate 0-5 for ID_VIEW_ROTATE0 to ID_VIEW_MIRROR_VERTICAL
  media::Rotate view_rotate_;

  // Draw a frame of YUV to an RGB buffer with scaling.
  // Handles different YUV formats.
  void DrawYUV(const media::VideoSurface &frame_in,
               uint8 *movie_dib_bits,
               int dibrowbytes,
               int clipped_width,
               int clipped_height,
               int scaled_width,
               int scaled_height) {
    media::YUVType yuv_type = (frame_in.format == media::VideoSurface::YV12) ?
        media::YV12 : media::YV16;

    // Simple convert is not necessary for performance, but allows
    // easier alternative implementations.
    if ((view_rotate_ == media::ROTATE_0) &&   // Not scaled or rotated
        (view_size_ == 2)) {
      media::ConvertYUVToRGB32(frame_in.data[0],
                               frame_in.data[1],
                               frame_in.data[2],
                               movie_dib_bits,
                               scaled_width, scaled_height,
                               frame_in.strides[0],
                               frame_in.strides[1],
                               dibrowbytes,
                               yuv_type);
    } else {
      media::ScaleYUVToRGB32(frame_in.data[0],
                             frame_in.data[1],
                             frame_in.data[2],
                             movie_dib_bits,
                             clipped_width, clipped_height,
                             scaled_width, scaled_height,
                             frame_in.strides[0],
                             frame_in.strides[1],
                             dibrowbytes,
                             yuv_type,
                             view_rotate_);
    }
  }

  // Diagnostic function to write out YUV in format compatible with PYUV tool.
  void DumpYUV(const media::VideoSurface &frame_in) {
    FILE * file_yuv = fopen("raw.yuv", "ab+");  // Open for append binary.
    if (file_yuv != NULL) {
      fseek(file_yuv, 0, SEEK_END);
      const size_t frame_size = frame_in.width * frame_in.height;
      for (size_t y = 0; y < frame_in.height; ++y)
        fwrite(frame_in.data[0]+frame_in.strides[0]*y,
          frame_in.width,   sizeof(uint8), file_yuv);
      for (size_t y = 0; y < frame_in.height/2; ++y)
        fwrite(frame_in.data[1]+frame_in.strides[1]*y,
          frame_in.width/2, sizeof(uint8), file_yuv);
      for (size_t y = 0; y < frame_in.height/2; ++y)
        fwrite(frame_in.data[2]+frame_in.strides[2]*y,
          frame_in.width/2, sizeof(uint8), file_yuv);
      fclose(file_yuv);

#if TESTING
      static int frame_dump_count = 0;
      char outputbuf[512];
      _snprintf_s(outputbuf, sizeof(outputbuf), "yuvdump %4d %dx%d stride %d\n",
                  frame_dump_count, frame_in.width, frame_in.height,
                  frame_in.strides[0]);
      OutputDebugStringA(outputbuf);
      ++frame_dump_count;
#endif
    }
  }

  media::VideoFrame* last_frame_;
  base::TimeDelta last_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(WtlVideoWindow);
};

#endif  // MEDIA_PLAYER_VIEW_H_

