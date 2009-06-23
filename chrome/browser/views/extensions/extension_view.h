// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_VIEW_H_

#include "build/build_config.h"

#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/native/native_view_host.h"

class Browser;
class Extension;
class ExtensionHost;
class ExtensionView;
class RenderViewHost;

// A class that represents the container that this view is in.
// (bottom shelf, side bar, etc.)
class ExtensionContainer {
 public:
  // Mouse event notifications from the view. (useful for hover UI).
  virtual void OnExtensionMouseEvent(ExtensionView* view) = 0;
  virtual void OnExtensionMouseLeave(ExtensionView* view) = 0;
};

// This handles the display portion of an ExtensionHost.
class ExtensionView : public views::NativeViewHost {
 public:
  ExtensionView(ExtensionHost* host, Browser* browser);
  ~ExtensionView();

  ExtensionHost* host() const { return host_; }
  Browser* browser() const { return browser_; }
  Extension* extension() const;
  RenderViewHost* render_view_host() const;
  void SetDidInsertCSS(bool did_insert);
  void set_is_clipped(bool is_clipped) { is_clipped_ = is_clipped; }

  // Notification from ExtensionHost.
  void DidContentsPreferredWidthChange(const int pref_width);
  void HandleMouseEvent();
  void HandleMouseLeave();

  // Set a custom background for the view. The background will be tiled.
  void SetBackground(const SkBitmap& background);

  // Sets the container for this view.
  void SetContainer(ExtensionContainer* container) { container_ = container; }

  // Overridden from views::NativeViewHost:
  virtual void SetVisible(bool is_visible);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View *parent, views::View *child);

  // Call after extension process crash to re-initialize view, so that
  // extension content can be rendered again.
  void RecoverCrashedExtension();

 private:
  friend class ExtensionHost;

  // Initializes the RenderWidgetHostView for this object.
  void CreateWidgetHostView();

  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  // Restore object to initial state. Called on shutdown or after a renderer
  // crash.
  void CleanUp();

  // The running extension instance that we're displaying.
  // Note that host_ owns view
  ExtensionHost* host_;

  // The browser window that this view is in.
  Browser* browser_;

  // True if we've been initialized.
  bool initialized_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  int pending_preferred_width_;

  // The container this view is in (not necessarily its direct superview).
  // Note: the view does not own its container.
  ExtensionContainer* container_;

  // Whether the RenderView has inserted extension css into toolstrip page.
  bool did_insert_css_;

  // Whether this extension view is clipped.
  bool is_clipped_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionView);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_VIEW_H_
