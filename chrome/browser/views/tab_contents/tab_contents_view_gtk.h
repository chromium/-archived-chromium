// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
#define CHROME_BROWSER_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_

#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "views/widget/widget_gtk.h"

class RenderViewContextMenuWin;
class SadTabView;
namespace views {
class NativeViewHost;
}

// Gtk-specific implementation of the TabContentsView for the views-based front
// end. It is a WidgetGtk that contains all of the contents of the tab and
// associated child views.
class TabContentsViewGtk : public TabContentsView,
                           public views::WidgetGtk {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewGtk(TabContents* tab_contents);
  virtual ~TabContentsViewGtk();

  // TabContentsView implementation --------------------------------------------

  virtual void CreateView();
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host);
  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView() const;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const;
  virtual void GetContainerBounds(gfx::Rect* out) const;
  virtual void OnContentsDestroy();
  virtual void SetPageTitle(const std::wstring& title);
  virtual void OnTabCrashed();
  virtual void SizeContents(const gfx::Size& size);
  virtual void Focus();
  virtual void SetInitialFocus();
  virtual void StoreFocus();
  virtual void RestoreFocus();

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

 private:
  // Signal handlers -----------------------------------------------------------

  // Overridden from views::WidgetGtk:
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);

  // Handles notifying the TabContents and other operations when the window was
  // shown or hidden.
  void WasHidden();
  void WasShown();

  // Handles resizing of the contents. This will notify the RenderWidgetHostView
  // of the change, reposition popups, and the find in page bar.
  void WasSized(const gfx::Size& size);

  // ---------------------------------------------------------------------------

  // Used to render the sad tab. This will be non-NULL only when the sad tab is
  // visible.
  scoped_ptr<SadTabView> sad_tab_;

  // Whether to ignore the next CHAR keyboard event.
  bool ignore_next_char_event_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuWin> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewGtk);
};

#endif  // CHROME_BROWSER_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
