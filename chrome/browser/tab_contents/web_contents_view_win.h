// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_

#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/web_contents_view.h"
#include "chrome/views/widget_win.h"

class FindBarWin;
class SadTabView;
struct WebDropData;
class WebDropTarget;

// Windows-specific implementation of the WebContentsView. It is a HWND that
// contains all of the contents of the tab and associated child views.
class WebContentsViewWin : public WebContentsView,
                           public views::WidgetWin {
 public:
  // The corresponding WebContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit WebContentsViewWin(WebContents* web_contents);
  virtual ~WebContentsViewWin();

  // WebContentsView implementation --------------------------------------------

  virtual WebContents* GetWebContents();
  virtual void CreateView();
  virtual RenderWidgetHostViewWin* CreateViewForWidget(
      RenderWidgetHost* render_widget_host);
  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView() const;
  virtual gfx::NativeView GetTopLevelNativeView() const;
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
  virtual void ShowContextMenu(
      const ViewHostMsg_ContextMenu_Params& params);
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
  // Windows events ------------------------------------------------------------

  // Overrides from WidgetWin.
  virtual void OnDestroy();
  virtual void OnHScroll(int scroll_type, short position, HWND scrollbar);
  virtual void OnMouseLeave();
  virtual LRESULT OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual void OnPaint(HDC junk_dc);
  virtual LRESULT OnReflectedMessage(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual void OnSetFocus(HWND window);
  virtual void OnVScroll(int scroll_type, short position, HWND scrollbar);
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos);
  virtual void OnSize(UINT param, const CSize& size);
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param);
  virtual void OnNCPaint(HRGN rgn);

  // Backend for all scroll messages, the |message| parameter indicates which
  // one it is.
  void ScrollCommon(UINT message, int scroll_type, short position,
                    HWND scrollbar);

  // Handles notifying the WebContents and other operations when the window was
  // shown or hidden.
  void WasHidden();
  void WasShown();

  // Handles resizing of the contents. This will notify the RenderWidgetHostView
  // of the change, reposition popups, and the find in page bar.
  void WasSized(const gfx::Size& size);

  // TODO(brettw) comment these. They're confusing.
  bool ScrollZoom(int scroll_type);
  void WheelZoom(int distance);

  // ---------------------------------------------------------------------------

  WebContents* web_contents_;

  // For find in page. This may be NULL if there is no find bar, and if it is
  // non-NULL, it may or may not be visible.
  scoped_ptr<FindBarWin> find_bar_;

  // A drop target object that handles drags over this WebContents.
  scoped_refptr<WebDropTarget> drop_target_;

  // Used to render the sad tab. This will be non-NULL only when the sad tab is
  // visible.
  scoped_ptr<SadTabView> sad_tab_;

  // Whether to ignore the next CHAR keyboard event.
  bool ignore_next_char_event_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewWin);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
