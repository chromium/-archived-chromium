// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shellapi.h>

#include "chrome/browser/views/tab_icon_view.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/gfx/icon_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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

    // The default window icon is the application icon, not the default
    // favicon.
    std::wstring exe_path;
    PathService::Get(base::DIR_EXE, &exe_path);
    file_util::AppendToPath(&exe_path, L"chrome.exe");
    HICON app_icon = ExtractIcon(NULL, exe_path.c_str(), 0);
    g_default_fav_icon =
        IconUtil::CreateSkBitmapFromHICON(app_icon, gfx::Size(16, 16));
    DestroyIcon(app_icon);

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
      is_light_(false),
      throbber_running_(false),
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
    canvas->DrawBitmapInt(bitmap, (width() - kFavIconSize) / 2,
                          (height() - kFavIconSize) / 2);
  } else {
    canvas->DrawBitmapInt(bitmap, 0, 0, bw, bh, 0, 0, width(), height(),
                          true);
  }
}

void TabIconView::Paint(ChromeCanvas* canvas) {
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

