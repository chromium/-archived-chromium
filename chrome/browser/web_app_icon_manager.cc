// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_app_icon_manager.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gfx/icon_util.h"
#include "skia/include/SkBitmap.h"

// Returns the default icon for the window.
static HICON GetDefaultIcon() {
  HMODULE chrome_dll = GetModuleHandle(L"chrome.dll");
  return ::LoadIcon(chrome_dll, MAKEINTRESOURCE(IDR_MAINFRAME));
}

// Updates the icon of the window from the specified SkBitmap. If the image
// is empty, the default icon is used. If icon is non-null it is updated to
// reflect the new icon (unless the default is used).
static void UpdateIcon(HWND hwnd,
                       const SkBitmap& image,
                       int icon_type,
                       HICON* icon) {
  if (icon) {
    if (*icon)
      DestroyIcon(*icon);
    *icon = NULL;
  }

  HICON icon_to_set;
  if (image.width() > 0) {
    icon_to_set = IconUtil::CreateHICONFromSkBitmap(image);
    if (icon)
      *icon = icon_to_set;
  } else {
    icon_to_set = GetDefaultIcon();
  }
  ::SendMessage(hwnd, WM_SETICON, icon_type,
                reinterpret_cast<LPARAM>(icon_to_set));
}

WebAppIconManager::WebAppIconManager(HWND parent)
    : hwnd_(parent), small_icon_(NULL), big_icon_(NULL), enabled_(true) {
}

WebAppIconManager::~WebAppIconManager() {
  if (small_icon_)
    DestroyIcon(small_icon_);

  if (big_icon_)
    DestroyIcon(big_icon_);

  if (app_.get())
    app_->RemoveObserver(this);
}

void WebAppIconManager::SetContents(TabContents* contents) {
  WebApp* new_app = NULL;
  if (contents && contents->AsWebContents())
    new_app = contents->AsWebContents()->web_app();

  if (new_app == app_.get())
    return;

  if (app_.get())
    app_->RemoveObserver(this);
  app_ = new_app;
  if (app_.get()) {
    app_->AddObserver(this);
    UpdateIconsFromApp();
  } else if (enabled_) {
    UpdateIcon(hwnd_, SkBitmap(), ICON_SMALL, NULL);
    UpdateIcon(hwnd_, SkBitmap(), ICON_BIG, NULL);
  }
}

void WebAppIconManager::SetUpdatesEnabled(bool enabled) {
  if (enabled == enabled_)
    return;

  enabled_ = enabled;
  if (enabled_)
    UpdateIconsFromApp();
}

void WebAppIconManager::WebAppImagesChanged(WebApp* web_app) {
  UpdateIconsFromApp();
}

void WebAppIconManager::UpdateIconsFromApp() {
  if (!enabled_)
    return;

  SkBitmap small_image;
  SkBitmap big_image;
  if (app_.get() && !app_->GetImages().empty()) {
    const WebApp::Images& images = app_->GetImages();
    WebApp::Images::const_iterator smallest = images.begin();
    WebApp::Images::const_iterator biggest = images.begin();
    for (WebApp::Images::const_iterator i = images.begin() + 1;
         i != images.end(); ++i) {
      if (i->width() > biggest->width())
        biggest = i;
      else if (i->width() > 0 && i->width() < smallest->width())
        smallest = i;
    }
    small_image = *smallest;
    big_image = *biggest;
  }
  UpdateIcon(hwnd_, small_image, ICON_SMALL, &small_icon_);
  UpdateIcon(hwnd_, big_image, ICON_BIG, &big_icon_);
}

