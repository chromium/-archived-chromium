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

#include "chrome/browser/web_app_icon_manager.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/web_contents.h"
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
