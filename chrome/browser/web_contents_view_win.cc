// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_contents_view_win.h"

#include <windows.h>

#include "chrome/browser/find_in_page_controller.h"
#include "chrome/browser/render_view_context_menu.h"
#include "chrome/browser/render_view_context_menu_controller.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/views/info_bar_message_view.h"
#include "chrome/browser/views/info_bar_view.h"
#include "chrome/browser/views/sad_tab_view.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_drag_source.h"
#include "chrome/browser/web_drop_target.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/os_exchange_data.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"

namespace {

// Windows callback for DetachPluginWindows.
BOOL CALLBACK EnumPluginWindowsCallback(HWND window, LPARAM param) {
  if (WebPluginDelegateImpl::IsPluginDelegateWindow(window)) {
    ::ShowWindow(window, SW_HIDE);
    SetParent(window, NULL);
  }
  return TRUE;
}

}  // namespace

WebContentsViewWin::WebContentsViewWin(WebContents* web_contents)
    : web_contents_(web_contents),
      error_info_bar_message_(NULL),
      info_bar_visible_(false) {
}

WebContentsViewWin::~WebContentsViewWin() {
}

void WebContentsViewWin::CreateView(HWND parent_hwnd,
                                    const gfx::Rect& initial_bounds) {
  set_delete_on_destroy(false);
  HWNDViewContainer::Init(parent_hwnd, initial_bounds, false);

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetHWND());
  drop_target_ = new WebDropTarget(GetHWND(), web_contents_);
}

RenderWidgetHostViewWin* WebContentsViewWin::CreatePageView(
    RenderViewHost* render_view_host) {
  // Create the View as well. Its lifetime matches the child process'.
  DCHECK(!render_view_host->view());
  RenderWidgetHostViewWin* view = new RenderWidgetHostViewWin(render_view_host);
  render_view_host->set_view(view);
  view->Create(GetHWND());
  view->ShowWindow(SW_SHOW);
  return view;
}

HWND WebContentsViewWin::GetContainerHWND() const {
  return GetHWND();
}

HWND WebContentsViewWin::GetContentHWND() const {
  if (!web_contents_->render_widget_host_view())
    return NULL;
  return web_contents_->render_widget_host_view()->GetPluginHWND();
}

void WebContentsViewWin::GetContainerBounds(gfx::Rect *out) const {
  CRect r;
  GetBounds(&r, false);
  *out = r;
}

void WebContentsViewWin::StartDragging(const WebDropData& drop_data) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);

  // TODO(tc): Generate an appropriate drag image.

  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over a
  // shortcut.
  if (!drop_data.file_contents.empty()) {
    data->SetFileContents(drop_data.file_description_filename,
                          drop_data.file_contents);
  }
  if (!drop_data.cf_html.empty())
    data->SetCFHtml(drop_data.cf_html);
  if (drop_data.url.is_valid())
    data->SetURL(drop_data.url, drop_data.url_title);
  if (!drop_data.plain_text.empty())
    data->SetString(drop_data.plain_text);

  scoped_refptr<WebDragSource> drag_source(
      new WebDragSource(GetHWND(), web_contents_->render_view_host()));

  DWORD effects;

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  DoDragDrop(data, drag_source, DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  if (web_contents_->render_view_host())
    web_contents_->render_view_host()->DragSourceSystemDragEnded();
}

void WebContentsViewWin::DetachPluginWindows() {
  EnumChildWindows(GetHWND(), EnumPluginWindowsCallback, NULL);
}

void WebContentsViewWin::DisplayErrorInInfoBar(const std::wstring& text) {
  InfoBarView* view = GetInfoBarView();
  if (-1 == view->GetChildIndex(error_info_bar_message_)) {
    error_info_bar_message_ = new InfoBarMessageView(text);
    view->AddChildView(error_info_bar_message_);
  } else {
    error_info_bar_message_->SetMessageText(text);
  }
}

void WebContentsViewWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(GetHWND());
    drop_target_ = NULL;
  }
}

void WebContentsViewWin::SetInfoBarVisible(bool visible) {
  if (info_bar_visible_ != visible) {
    info_bar_visible_ = visible;
    if (info_bar_visible_) {
      // Invoke GetInfoBarView to force the info bar to be created.
      GetInfoBarView();
    }
    web_contents_->ToolbarSizeChanged(false);
  }
}

bool WebContentsViewWin::IsInfoBarVisible() const {
  return info_bar_visible_;
}

InfoBarView* WebContentsViewWin::GetInfoBarView() {
  if (info_bar_view_.get() == NULL) {
    // TODO(brettw) currently the InfoBar thinks its owned by the WebContents,
    // but it should instead think it's owned by us.
    info_bar_view_.reset(new InfoBarView(web_contents_));
    // We own the info-bar.
    info_bar_view_->SetParentOwned(false);
  }
  return info_bar_view_.get();
}

void WebContentsViewWin::UpdateDragCursor(bool is_drop_target) {
  drop_target_->set_is_drop_target(is_drop_target);
}

void WebContentsViewWin::ShowContextMenu(
    const ViewHostMsg_ContextMenu_Params& params) {
  RenderViewContextMenuController menu_controller(web_contents_, params);
  RenderViewContextMenu menu(&menu_controller,
                             GetHWND(),
                             params.type,
                             params.misspelled_word,
                             params.dictionary_suggestions,
                             web_contents_->profile());

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetHWND(), HWND_DESKTOP, &screen_pt, 1);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  menu.RunMenuAt(screen_pt.x, screen_pt.y);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void WebContentsViewWin::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  // The renderer returned a keyboard event it did not process. This may be
  // a keyboard shortcut that we have to process.
  if (event.type == WebInputEvent::KEY_DOWN) {
    ChromeViews::FocusManager* focus_manager =
        ChromeViews::FocusManager::GetFocusManager(GetHWND());
    // We may not have a focus_manager at this point (if the tab has been
    // switched by the time this message returned).
    if (focus_manager) {
      ChromeViews::Accelerator accelerator(event.key_code,
          (event.modifiers & WebInputEvent::SHIFT_KEY) ==
              WebInputEvent::SHIFT_KEY,
          (event.modifiers & WebInputEvent::CTRL_KEY) ==
              WebInputEvent::CTRL_KEY,
          (event.modifiers & WebInputEvent::ALT_KEY) ==
              WebInputEvent::ALT_KEY);
      if (focus_manager->ProcessAccelerator(accelerator, false))
        return;
    }
  }

  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like Alt+F4, etc to work correctly.
  DefWindowProc(event.actual_message.hwnd,
                event.actual_message.message,
                event.actual_message.wParam,
                event.actual_message.lParam);
}

void WebContentsViewWin::OnHScroll(int scroll_type, short position,
                                   HWND scrollbar) {
  ScrollCommon(WM_HSCROLL, scroll_type, position, scrollbar);
}

void WebContentsViewWin::OnMouseLeave() {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (web_contents_->delegate())
    web_contents_->delegate()->ContentsMouseEvent(web_contents_, WM_MOUSELEAVE);
  SetMsgHandled(FALSE);
}

LRESULT WebContentsViewWin::OnMouseRange(UINT msg,
                                         WPARAM w_param, LPARAM l_param) {
  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
      // Make sure this TabContents is activated when it is clicked on.
      if (web_contents_->delegate())
        web_contents_->delegate()->ActivateContents(web_contents_);
      break;
    case WM_MOUSEMOVE:
      // Let our delegate know that the mouse moved (useful for resetting status
      // bubble state).
      if (web_contents_->delegate()) {
        web_contents_->delegate()->ContentsMouseEvent(web_contents_,
                                                      WM_MOUSEMOVE);
      }
      break;
    default:
      break;
  }

  return 0;
}

void WebContentsViewWin::OnPaint(HDC junk_dc) {
  if (web_contents_->render_view_host() &&
      !web_contents_->render_view_host()->IsRenderViewLive()) {
    if (!web_contents_->sad_tab_.get())
      web_contents_->sad_tab_.reset(new SadTabView);
    CRect cr;
    GetClientRect(&cr);
    web_contents_->sad_tab_->SetBounds(cr);
    ChromeCanvasPaint canvas(GetHWND(), true);
    web_contents_->sad_tab_->ProcessPaint(&canvas);
    return;
  }

  // We need to do this to validate the dirty area so we don't end up in a
  // WM_PAINTstorm that causes other mysterious bugs (such as WM_TIMERs not
  // firing etc). It doesn't matter that we don't have any non-clipped area.
  CPaintDC dc(GetHWND());
  SetMsgHandled(FALSE);
}

// A message is reflected here from view().
// Return non-zero to indicate that it is handled here.
// Return 0 to allow view() to further process it.
LRESULT WebContentsViewWin::OnReflectedMessage(UINT msg, WPARAM w_param,
                                        LPARAM l_param) {
  MSG* message = reinterpret_cast<MSG*>(l_param);
  switch (message->message) {
    case WM_MOUSEWHEEL:
      // This message is reflected from the view() to this window.
      if (GET_KEYSTATE_WPARAM(message->wParam) & MK_CONTROL) {
        WheelZoom(GET_WHEEL_DELTA_WPARAM(message->wParam));
        return 1;
      }
      break;
    case WM_HSCROLL:
    case WM_VSCROLL:
      if (ScrollZoom(LOWORD(message->wParam)))
        return 1;
    default:
      break;
  }

  return 0;
}

void WebContentsViewWin::OnSetFocus(HWND window) {
  // TODO(jcampan): figure out why removing this prevents tabs opened in the
  //                background from properly taking focus.
  // We NULL-check the render_view_host_ here because Windows can send us
  // messages during the destruction process after it has been destroyed.
  if (web_contents_->render_widget_host_view()) {
    HWND inner_hwnd = web_contents_->render_widget_host_view()->GetPluginHWND();
    if (::IsWindow(inner_hwnd))
      ::SetFocus(inner_hwnd);
  }
}

void WebContentsViewWin::OnVScroll(int scroll_type, short position,
                                   HWND scrollbar) {
  ScrollCommon(WM_VSCROLL, scroll_type, position, scrollbar);
}

void WebContentsViewWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (window_pos->flags & SWP_HIDEWINDOW) {
    web_contents_->HideContents();
  } else {
    // The WebContents was shown by a means other than the user selecting a
    // Tab, e.g. the window was minimized then restored.
    if (window_pos->flags & SWP_SHOWWINDOW)
      web_contents_->ShowContents();
    // Unless we were specifically told not to size, cause the renderer to be
    // sized to the new bounds, which forces a repaint. Not required for the
    // simple minimize-restore case described above, for example, since the
    // size hasn't changed.
    if (!(window_pos->flags & SWP_NOSIZE)) {
      gfx::Size size(window_pos->cx, window_pos->cy);
      web_contents_->SizeContents(size);  // FIXME(brettw) should this be on this class?
    }

    // If we have a FindInPage dialog, notify it that the window changed.
    if (web_contents_->find_in_page_controller_.get() &&
        web_contents_->find_in_page_controller_->IsVisible()) {
      web_contents_->find_in_page_controller_->MoveWindowIfNecessary(
          gfx::Rect());
    }
  }
}

void WebContentsViewWin::OnSize(UINT param, const CSize& size) {
  HWNDViewContainer::OnSize(param, size);

  // Hack for thinkpad touchpad driver.
  // Set fake scrollbars so that we can get scroll messages,
  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_ALL;

  si.nMin = 1;
  si.nMax = 100;
  si.nPage = 10;
  si.nTrackPos = 50;

  ::SetScrollInfo(GetHWND(), SB_HORZ, &si, FALSE);
  ::SetScrollInfo(GetHWND(), SB_VERT, &si, FALSE);
}

LRESULT WebContentsViewWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  // Hack for thinkpad mouse wheel driver. We have set the fake scroll bars
  // to receive scroll messages from thinkpad touchpad driver. Suppress
  // painting of scrollbars by returning 0 size for them.
  return 0;
}

void WebContentsViewWin::OnNCPaint(HRGN rgn) {
  // Suppress default WM_NCPAINT handling. We don't need to do anything
  // here since the view will draw everything correctly.
}

void WebContentsViewWin::ScrollCommon(UINT message, int scroll_type,
                                      short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // Trackpad scroll wheel emulation.
  if (!ScrollZoom(scroll_type)) {
    // Reflect scroll message to the view() to give it a chance
    // to process scrolling.
    SendMessage(GetContentHWND(), message, MAKELONG(scroll_type, position),
                (LPARAM) scrollbar);
  }
}

bool WebContentsViewWin::ScrollZoom(int scroll_type) {
  // If ctrl is held, zoom the UI.  There are three issues with this:
  // 1) Should the event be eaten or forwarded to content?  We eat the event,
  //    which is like Firefox and unlike IE.
  // 2) Should wheel up zoom in or out?  We zoom in (increase font size), which
  //    is like IE and Google maps, but unlike Firefox.
  // 3) Should the mouse have to be over the content area?  We zoom as long as
  //    content has focus, although FF and IE require that the mouse is over
  //    content.  This is because all events get forwarded when content has
  //    focus.
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
    int distance = 0;
    switch (scroll_type) {
      case SB_LINEUP:
        distance = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:
        distance = -WHEEL_DELTA;
        break;
        // TODO(joshia): Handle SB_PAGEUP, SB_PAGEDOWN, SB_THUMBPOSITION,
        // and SB_THUMBTRACK for completeness
      default:
        break;
    }

    WheelZoom(distance);
    return true;
  }
  return false;
}

void WebContentsViewWin::WheelZoom(int distance) {
  if (web_contents_->delegate()) {
    bool zoom_in = distance > 0;
    web_contents_->delegate()->ContentsZoomChange(zoom_in);
  }
}

