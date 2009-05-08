// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_H_

#include "chrome/browser/extensions/extensions_service.h"
#include "app/gfx/chrome_canvas.h"
#include "chrome/common/notification_observer.h"
#include "views/view.h"

class Browser;

class ExtensionShelf : public views::View,
                       public NotificationObserver {
 public:
  explicit ExtensionShelf(Browser* browser);
  virtual ~ExtensionShelf();

  // View
  virtual void Paint(ChromeCanvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // NotificationService method.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  bool AddExtensionViews(const ExtensionList* extensions);
  bool HasExtensionViews();

 private:
  // Inits the background bitmap.
  void InitBackground(ChromeCanvas* canvas, const SkRect& subset);

  Browser* browser_;

  // Background bitmap to draw under extension views.
  SkBitmap background_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelf);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_H_
