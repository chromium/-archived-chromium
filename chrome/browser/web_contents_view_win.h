// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_CONTENTS_VIEW_WIN_H_
#define CHROME_BROWSER_WEB_CONTENTS_VIEW_WIN_H_

#include "chrome/browser/web_contents_view.h"
#include "chrome/views/hwnd_view_container.h"

class InfoBarView;
class InfoBarMessageView;
struct WebDropData;
class WebDropTarget;

// Windows-specific implementation of the WebContentsView. It is a HWND that
// contains all of the contents of the tab and associated child views.
class WebContentsViewWin : public WebContentsView,
                           public ChromeViews::HWNDViewContainer {
 public:
  // The corresponding WebContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit WebContentsViewWin(WebContents* web_contents);
  virtual ~WebContentsViewWin();

  // WebContentsView implementation --------------------------------------------

  // TODO(brettw) what on earth is the difference between this and
  // CreatePageView. Do we really need both?
  virtual void CreateView(HWND parent_hwnd,
                          const gfx::Rect& initial_bounds);
  virtual RenderWidgetHostHWND* CreatePageView(
      RenderViewHost* render_view_host);
  virtual HWND GetContainerHWND() const;
  virtual HWND GetContentHWND() const;
  virtual void GetContainerBounds(gfx::Rect* out) const;
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void DetachPluginWindows();
  virtual void DisplayErrorInInfoBar(const std::wstring& text);
  virtual void SetInfoBarVisible(bool visible);
  virtual bool IsInfoBarVisible() const;
  virtual InfoBarView* GetInfoBarView();
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void ShowContextMenu(
      const ViewHostMsg_ContextMenu_Params& params);
  virtual void HandleKeyboardEvent(const WebKeyboardEvent& event);

 private:
  // Windows events ------------------------------------------------------------

  // Overrides from HWNDViewContainer.
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

  // TODO(brettw) comment these. They're confusing.
  bool ScrollZoom(int scroll_type);
  void WheelZoom(int distance);

  // ---------------------------------------------------------------------------

  // TODO(brettw) when this class is separated from WebContents, we should own
  WebContents* web_contents_;

  // A drop target object that handles drags over this WebContents.
  scoped_refptr<WebDropTarget> drop_target_;

  // InfoBarView, lazily created.
  scoped_ptr<InfoBarView> info_bar_view_;

  // Info bar for crashed plugin message.
  // IMPORTANT: This instance is owned by the InfoBarView. It is valid
  // only if InfoBarView::GetChildIndex for this view is valid.
  InfoBarMessageView* error_info_bar_message_;

  // Whether the info bar view is visible.
  bool info_bar_visible_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewWin);
};

#endif  // CHROME_BROWSER_WEB_CONTENTS_VIEW_WIN_H_
