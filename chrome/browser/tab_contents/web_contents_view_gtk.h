// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_

#include "chrome/browser/tab_contents/web_contents_view.h"

class WebContentsViewGtk : public WebContentsView {
 public:
  // The corresponding WebContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit WebContentsViewGtk(WebContents* web_contents);
  virtual ~WebContentsViewGtk();

  // WebContentsView implementation --------------------------------------------

  virtual WebContents* GetWebContents();
  virtual void CreateView();
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host);

  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView() const;
  virtual gfx::NativeWindow GetTopLevelNativeView() const;
  virtual void GetContainerBounds(gfx::Rect* out) const;
  virtual void OnContentsDestroy();
  virtual void SetPageTitle(const std::wstring& title);
  virtual void Invalidate();
  virtual void SizeContents(const gfx::Size& size);
  virtual void FindInPage(const Browser& browser,
                          bool find_next, bool forward_direction);
  virtual void HideFindBar(bool end_session);
  virtual void ReparentFindWindow(Browser* new_browser) const;
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) const;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual WebContents* CreateNewWindowInternal(
      int route_id, base::WaitableEvent* modal_dialog_event);
  virtual RenderWidgetHostView* CreateNewWidgetInternal(int route_id,
                                                        bool activatable);
  virtual void ShowCreatedWindowInternal(WebContents* new_web_contents,
                                         WindowOpenDisposition disposition,
                                         const gfx::Rect& initial_pos,
                                         bool user_gesture);
  virtual void ShowCreatedWidgetInternal(RenderWidgetHostView* widget_host_view,
                                         const gfx::Rect& initial_pos);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void TakeFocus(bool reverse);
  virtual void HandleKeyboardEvent(const WebKeyboardEvent& event);
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);
 private:
  WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewGtk);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_

