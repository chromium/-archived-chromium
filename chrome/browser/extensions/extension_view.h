// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "build/build_config.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_host.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"

// TODO(port): Port these files.
#if defined(OS_WIN)
#include "chrome/views/controls/hwnd_view.h"
#else
#include "chrome/views/view.h"
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class Browser;
class Extension;

// This handles the display portion of an ExtensionHost.
class ExtensionView : public views::HWNDView {
 public:
  ExtensionView(ExtensionHost* host, Browser* browser, const GURL& content_url);
  ~ExtensionView();

  ExtensionHost* host() const { return host_.get(); }
  Browser* browser() const { return browser_; }
  Extension* extension() { return host_->extension(); }
  RenderViewHost* render_view_host() { return host_->render_view_host(); }

  // Notification from ExtensionHost.
  void DidContentsPreferredWidthChange(const int pref_width);

  // Set a custom background for the view. The background will be tiled.
  void SetBackground(const SkBitmap& background);

  // views::HWNDView
  virtual void SetVisible(bool is_visible);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View *parent, views::View *child);
 private:
  friend class ExtensionHost;

  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  // The running extension instance that we're displaying.
  scoped_ptr<ExtensionHost> host_;

  // The browser window that this view is in.
  Browser* browser_;

  // The URL to navigate the host to upon initialization.
  GURL content_url_;

  // True if we've been initialized.
  bool initialized_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  int pending_preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionView);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
