// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/gfx/icon_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/views/tab_icon_view.h"
#include "chrome/app/chrome_dll_resource.h"

static bool g_initialized = false;
static SkBitmap* g_default_fav_icon = NULL;
static SkBitmap* g_throbber_frames = NULL;
static SkBitmap* g_throbber_frames_light = NULL;
static int g_throbber_frame_count;

// static
void TabIconView::InitializeIfNeeded() {
  if (!g_initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    g_default_fav_icon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    g_throbber_frames = rb.GetBitmapNamed(IDR_THROBBER);
    g_throbber_frames_light = rb.GetBitmapNamed(IDR_THROBBER_LIGHT);
    g_throbber_frame_count = g_throbber_frames->width() /
        g_throbber_frames->height();

    // Verify that our light and dark styles have the same number of frames.
    DCHECK(g_throbber_frame_count ==
        g_throbber_frames_light->width() / g_throbber_frames_light->height());
    g_initialized = true;
  }
}

TabIconView::TabIconView(TabContentsProvider* provider)
    : provider_(provider),
      is_light_(false),
      throbber_running_(false),
      throbber_frame_(0) {
  InitializeIfNeeded();
}

TabIconView::~TabIconView() {
}

void TabIconView::Update() {
  TabContents* contents = provider_->GetCurrentTabContents();
  if (throbber_running_) {
    // We think the tab is loading.
    if (!contents || !contents->is_loading()) {
      // Woops, tab is invalid or not loading, reset our status and schedule
      // a paint.
      throbber_running_ = false;
      SchedulePaint();
    } else {
      // The tab is still loading, increment the frame.
      throbber_frame_ = (throbber_frame_ + 1) % g_throbber_frame_count;
      SchedulePaint();
    }
  } else if (contents && contents->is_loading()) {
    // We didn't think we were loading, but the tab is loading. Reset the
    // frame and status and schedule a paint.
    throbber_running_ = true;
    throbber_frame_ = 0;
    SchedulePaint();
  }
}

void TabIconView::PaintThrobber(ChromeCanvas* canvas) {
  int image_size = g_throbber_frames->height();
  int image_offset = throbber_frame_ * image_size;
  canvas->DrawBitmapInt(is_light_ ? *g_throbber_frames_light :
                                    *g_throbber_frames,
                        image_offset, 0, image_size, image_size,
                        0, 0, image_size, image_size, false);
}

void TabIconView::PaintFavIcon(ChromeCanvas* canvas, const SkBitmap& bitmap) {
  int bw = bitmap.width();
  int bh = bitmap.height();
  if (bw <= kFavIconSize && bh <= kFavIconSize) {
    canvas->DrawBitmapInt(bitmap, (GetWidth() - kFavIconSize) / 2,
                          (GetHeight() - kFavIconSize) / 2);
  } else {
    canvas->DrawBitmapInt(bitmap, 0, 0, bw, bh, 0, 0, GetWidth(), GetHeight(),
                          true);
  }
}

void TabIconView::Paint(ChromeCanvas* canvas) {
  TabContents* contents = provider_->GetCurrentTabContents();
  bool rendered = false;

  if (contents) {
    if (throbber_running_) {
      rendered = true;
      PaintThrobber(canvas);
    } else {
      SkBitmap favicon = provider_->GetFavIcon();
      if (!favicon.isNull()) {
        rendered = true;
        PaintFavIcon(canvas, favicon);
      }
    }
  }

  if (!rendered) {
    PaintFavIcon(canvas, *g_default_fav_icon);
  }
}

void TabIconView::GetPreferredSize(CSize* out) {
  out->cx = out->cy = kFavIconSize;
}
