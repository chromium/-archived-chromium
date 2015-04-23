// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tab_icon_view.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

#include "app/gfx/canvas.h"
#include "app/gfx/favicon_size.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"

#if defined(OS_WIN)
#include "app/gfx/icon_util.h"
#endif

static bool g_initialized = false;
static SkBitmap* g_default_fav_icon = NULL;
static SkBitmap* g_throbber_frames = NULL;
static SkBitmap* g_throbber_frames_light = NULL;
static int g_throbber_frame_count;

// static
void TabIconView::InitializeIfNeeded() {
  if (!g_initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();

#if defined(OS_WIN)
    // The default window icon is the application icon, not the default
    // favicon.
    std::wstring exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    file_util::AppendToPath(&exe_path,
        chrome::kBrowserProcessExecutableName);

    HICON app_icon = ExtractIcon(NULL, exe_path.c_str(), 0);
    g_default_fav_icon =
        IconUtil::CreateSkBitmapFromHICON(app_icon, gfx::Size(16, 16));
    DestroyIcon(app_icon);
#else
    NOTIMPLEMENTED();
#endif

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

TabIconView::TabIconView(TabIconViewModel* model)
    : model_(model),
      throbber_running_(false),
      is_light_(false),
      throbber_frame_(0) {
  InitializeIfNeeded();
}

TabIconView::~TabIconView() {
}

void TabIconView::Update() {
  if (throbber_running_) {
    // We think the tab is loading.
    if (!model_->ShouldTabIconViewAnimate()) {
      // Woops, tab is invalid or not loading, reset our status and schedule
      // a paint.
      throbber_running_ = false;
      SchedulePaint();
    } else {
      // The tab is still loading, increment the frame.
      throbber_frame_ = (throbber_frame_ + 1) % g_throbber_frame_count;
      SchedulePaint();
    }
  } else if (model_->ShouldTabIconViewAnimate()) {
    // We didn't think we were loading, but the tab is loading. Reset the
    // frame and status and schedule a paint.
    throbber_running_ = true;
    throbber_frame_ = 0;
    SchedulePaint();
  }
}

void TabIconView::PaintThrobber(gfx::Canvas* canvas) {
  int image_size = g_throbber_frames->height();
  PaintIcon(canvas, is_light_ ? *g_throbber_frames_light : *g_throbber_frames,
            throbber_frame_ * image_size, 0, image_size, image_size, false);
}

void TabIconView::PaintFavIcon(gfx::Canvas* canvas, const SkBitmap& bitmap) {
  PaintIcon(canvas, bitmap, 0, 0, bitmap.width(), bitmap.height(), true);
}

void TabIconView::PaintIcon(gfx::Canvas* canvas,
                            const SkBitmap& bitmap,
                            int src_x,
                            int src_y,
                            int src_w,
                            int src_h,
                            bool filter) {
  // For source images smaller than the favicon square, scale them as if they
  // were padded to fit the favicon square, so we don't blow up tiny favicons
  // into larger or nonproportional results.
  float float_src_w = static_cast<float>(src_w);
  float float_src_h = static_cast<float>(src_h);
  float scalable_w, scalable_h;
  if (src_w <= kFavIconSize && src_h <= kFavIconSize) {
    scalable_w = scalable_h = kFavIconSize;
  } else {
    scalable_w = float_src_w;
    scalable_h = float_src_h;
  }

  // Scale proportionately.
  float scale = std::min(static_cast<float>(width()) / scalable_w,
                         static_cast<float>(height()) / scalable_h);
  int dest_w = static_cast<int>(float_src_w * scale);
  int dest_h = static_cast<int>(float_src_h * scale);

  // Center the scaled image.
  canvas->DrawBitmapInt(bitmap, src_x, src_y, src_w, src_h,
                        (width() - dest_w) / 2, (height() - dest_h) / 2, dest_w,
                        dest_h, filter);
}

void TabIconView::Paint(gfx::Canvas* canvas) {
  bool rendered = false;

  if (throbber_running_) {
    rendered = true;
    PaintThrobber(canvas);
  } else {
    SkBitmap favicon = model_->GetFavIconForTabIconView();
    if (!favicon.isNull()) {
      rendered = true;
      PaintFavIcon(canvas, favicon);
    }
  }

  if (!rendered)
    PaintFavIcon(canvas, *g_default_fav_icon);
}

gfx::Size TabIconView::GetPreferredSize() {
  return gfx::Size(kFavIconSize, kFavIconSize);
}
