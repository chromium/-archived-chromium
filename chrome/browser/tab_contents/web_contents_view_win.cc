// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view_win.h"

#include <windows.h>

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/browser.h" // TODO(beng): this dependency is awful.
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/tools_window.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/render_view_context_menu_controller.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_drag_source.h"
#include "chrome/browser/tab_contents/web_drop_target.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/browser/views/sad_tab_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/os_exchange_data.h"
#include "net/base/net_util.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webinputevent.h"

namespace {

// Windows callback for OnDestroy to detach the plugin windows.
BOOL CALLBACK DetachPluginWindowsCallback(HWND window, LPARAM param) {
  if (WebPluginDelegateImpl::IsPluginDelegateWindow(window) &&
      !IsHungAppWindow(window)) {
    ::ShowWindow(window, SW_HIDE);
    SetParent(window, NULL);
  }
  return TRUE;
}

}  // namespace

// static
WebContentsView* WebContentsView::Create(WebContents* web_contents) {
  return new WebContentsViewWin(web_contents);
}

WebContentsViewWin::WebContentsViewWin(WebContents* web_contents)
    : web_contents_(web_contents),
      ignore_next_char_event_(false) {
}

WebContentsViewWin::~WebContentsViewWin() {
}

WebContents* WebContentsViewWin::GetWebContents() {
  return web_contents_;
}

void WebContentsViewWin::CreateView() {
  set_delete_on_destroy(false);
  // Since we create these windows parented to the desktop window initially, we
  // don't want to create them initially visible.
  set_window_style(WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
  WidgetWin::Init(GetDesktopWindow(), gfx::Rect(), false);

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetHWND());
  drop_target_ = new WebDropTarget(GetHWND(), web_contents_);
}

RenderWidgetHostView* WebContentsViewWin::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
  RenderWidgetHostViewWin* view =
      new RenderWidgetHostViewWin(render_widget_host);
  view->Create(GetHWND());
  view->ShowWindow(SW_SHOW);
  return view;
}

gfx::NativeView WebContentsViewWin::GetNativeView() const {
  return GetHWND();
}

gfx::NativeView WebContentsViewWin::GetContentNativeView() const {
  if (!web_contents_->render_widget_host_view())
    return NULL;
  return web_contents_->render_widget_host_view()->GetPluginNativeView();
}

gfx::NativeWindow WebContentsViewWin::GetTopLevelNativeView() const {
  return ::GetAncestor(GetNativeView(), GA_ROOT);
}

void WebContentsViewWin::GetContainerBounds(gfx::Rect* out) const {
  GetBounds(out, false);
}

void WebContentsViewWin::StartDragging(const WebDropData& drop_data) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);

  // TODO(tc): Generate an appropriate drag image.

  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over a
  // shortcut so we add it first.
  if (!drop_data.file_contents.empty()) {
    // Images without ALT text will only have a file extension so we need to
    // synthesize one from the provided extension and URL.
    FilePath file_name(drop_data.file_description_filename);
    file_name = file_name.BaseName().RemoveExtension();
    if (file_name.value().empty()) {
      // Retrieve the name from the URL.
      file_name = FilePath::FromWStringHack(
          net::GetSuggestedFilename(drop_data.url, L"", L""));
    }
    file_name = file_name.ReplaceExtension(drop_data.file_extension);
    data->SetFileContents(file_name.value(), drop_data.file_contents);
  }
  if (!drop_data.text_html.empty())
    data->SetHtml(drop_data.text_html, drop_data.html_base_url);
  if (drop_data.url.is_valid()) {
    if (drop_data.url.SchemeIs("javascript")) {
      // We don't want to allow javascript URLs to be dragged to the desktop,
      // but we do want to allow them to be added to the bookmarks bar
      // (bookmarklets).
      BookmarkDragData::Element bm_elt;
      bm_elt.is_url = true;
      bm_elt.url = drop_data.url;
      bm_elt.title = drop_data.url_title;

      BookmarkDragData bm_drag_data;
      bm_drag_data.elements.push_back(bm_elt);

      bm_drag_data.Write(web_contents_->profile(), data);
    } else {
      data->SetURL(drop_data.url, drop_data.url_title);
    }
  }
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

void WebContentsViewWin::OnContentsDestroy() {
  // TODO(brettw) this seems like maybe it can be moved into OnDestroy and this
  // function can be deleted? If you're adding more here, consider whether it
  // can be moved into OnDestroy which is a Windows message handler as the
  // window is being torn down.

  // When a tab is closed all its child plugin windows are destroyed
  // automatically. This happens before plugins get any notification that its
  // instances are tearing down.
  //
  // Plugins like Quicktime assume that their windows will remain valid as long
  // as they have plugin instances active. Quicktime crashes in this case
  // because its windowing code cleans up an internal data structure that the
  // handler for NPP_DestroyStream relies on.
  //
  // The fix is to detach plugin windows from web contents when it is going
  // away. This will prevent the plugin windows from getting destroyed
  // automatically. The detached plugin windows will get cleaned up in proper
  // sequence as part of the usual cleanup when the plugin instance goes away.
  EnumChildWindows(GetHWND(), DetachPluginWindowsCallback, NULL);

  // Close the find bar if any.
  if (find_bar_.get())
    find_bar_->Close();
}

void WebContentsViewWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(GetHWND());
    drop_target_ = NULL;
  }
}

void WebContentsViewWin::SetPageTitle(const std::wstring& title) {
  if (GetNativeView()) {
    // It's possible to get this after the hwnd has been destroyed.
    ::SetWindowText(GetNativeView(), title.c_str());
    // TODO(brettw) this call seems messy the way it reaches into the widget
    // view, and I'm not sure it's necessary. Maybe we should just remove it.
    ::SetWindowText(
        web_contents_->render_widget_host_view()->GetPluginNativeView(),
        title.c_str());
  }
}

void WebContentsViewWin::Invalidate() {
  // Note that it's possible to get this message after the window was destroyed.
  if (::IsWindow(GetNativeView()))
    InvalidateRect(GetNativeView(), NULL, FALSE);
}

void WebContentsViewWin::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See web_contents_view.h.
  WasSized(size);
}

void WebContentsViewWin::OpenDeveloperTools() {
  if (!tools_window_.get())
    tools_window_.reset(new ToolsWindow);

  RenderViewHost* host = web_contents_->render_view_host();
  if (!host)
    return;

  tools_window_->Show(host->process()->host_id(), host->routing_id());
}

void WebContentsViewWin::ForwardMessageToToolsClient(int tools_message_type,
                                                     const std::wstring& body) {
  if (!tools_window_.get()) {
    NOTREACHED() << "Developer tools window is not open.";
    return;
  }
  tools_window_->SendToolsClientMessage(tools_message_type, body);
}

void WebContentsViewWin::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
  if (!find_bar_.get()) {
    // We want the Chrome top-level (Frame) window.
    HWND hwnd = reinterpret_cast<HWND>(browser.window()->GetNativeHandle());
    find_bar_.reset(new FindBarWin(this, hwnd));
  } else {
    find_bar_->Show();
  }

  if (find_next && !find_bar_->find_string().empty())
    find_bar_->StartFinding(forward_direction);
}

void WebContentsViewWin::HideFindBar(bool end_session) {
  if (find_bar_.get()) {
    if (end_session)
      find_bar_->EndFindSession();
    else
      find_bar_->DidBecomeUnselected();
  }
}

void WebContentsViewWin::ReparentFindWindow(Browser* new_browser) const {
  if (find_bar_.get()) {
    find_bar_->SetParent(
        reinterpret_cast<HWND>(new_browser->window()->GetNativeHandle()));
  }
}

bool WebContentsViewWin::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  CRect window_rect;
  if (!find_bar_.get() ||
      !::IsWindow(find_bar_->GetHWND()) ||
      !::GetWindowRect(find_bar_->GetHWND(), &window_rect)) {
    *position = gfx::Point(0, 0);
    *fully_visible = false;
    return false;
  }

  *position = gfx::Point(window_rect.TopLeft().x, window_rect.TopLeft().y);
  *fully_visible = find_bar_->IsVisible() && !find_bar_->IsAnimating();
  return true;
}

void WebContentsViewWin::UpdateDragCursor(bool is_drop_target) {
  drop_target_->set_is_drop_target(is_drop_target);
}

void WebContentsViewWin::TakeFocus(bool reverse) {
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetNativeView());

  // We may not have a focus manager if the tab has been switched before this
  // message arrived.
  if (focus_manager)
    focus_manager->AdvanceFocus(reverse);
}

void WebContentsViewWin::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  // Previous calls to TranslateMessage can generate CHAR events as well as
  // KEY_DOWN events, even if the latter triggered an accelerator.  In these
  // cases, we discard the CHAR events.
  if (event.type == WebInputEvent::CHAR && ignore_next_char_event_) {
    ignore_next_char_event_ = false;
    return;
  }
  ignore_next_char_event_ = false;

  // The renderer returned a keyboard event it did not process. This may be
  // a keyboard shortcut that we have to process.
  if (event.type == WebInputEvent::KEY_DOWN) {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManager(GetHWND());
    // We may not have a focus_manager at this point (if the tab has been
    // switched by the time this message returned).
    if (focus_manager) {
      views::Accelerator accelerator(event.key_code,
          (event.modifiers & WebInputEvent::SHIFT_KEY) ==
              WebInputEvent::SHIFT_KEY,
          (event.modifiers & WebInputEvent::CTRL_KEY) ==
              WebInputEvent::CTRL_KEY,
          (event.modifiers & WebInputEvent::ALT_KEY) ==
              WebInputEvent::ALT_KEY);

      // This is tricky: we want to set ignore_next_char_event_ if
      // ProcessAccelerator returns true. But ProcessAccelerator might delete
      // |this| if the accelerator is a "close tab" one. So we speculatively
      // set the flag and fix it if no event was handled.
      ignore_next_char_event_ = true;
      if (focus_manager->ProcessAccelerator(accelerator, false)) {
        // DANGER: |this| could be deleted now!
        return;
      } else {
        // ProcessAccelerator didn't handle the accelerator, so we know both
        // that |this| is still valid, and that we didn't want to set the flag.
        ignore_next_char_event_ = false;
      }
    }
  }

  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like Alt+F4, etc to work correctly.
  DefWindowProc(event.actual_message.hwnd,
                event.actual_message.message,
                event.actual_message.wParam,
                event.actual_message.lParam);
}

void WebContentsViewWin::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  if (find_bar_.get()) {
    find_bar_->OnFindReply(request_id, number_of_matches, selection_rect,
                           active_match_ordinal, final_update);
  }
}

void WebContentsViewWin::ShowContextMenu(const ContextMenuParams& params) {
  RenderViewContextMenuController menu_controller(web_contents_, params);
  RenderViewContextMenu menu(&menu_controller,
                             GetHWND(),
                             params.node,
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

WebContents* WebContentsViewWin::CreateNewWindowInternal(
    int route_id,
    base::WaitableEvent* modal_dialog_event) {
  // Create the new web contents. This will automatically create the new
  // WebContentsView. In the future, we may want to create the view separately.
  WebContents* new_contents =
      new WebContents(web_contents_->profile(),
                      web_contents_->GetSiteInstance(),
                      web_contents_->render_view_factory_,
                      route_id,
                      modal_dialog_event);
  new_contents->SetupController(web_contents_->profile());
  WebContentsView* new_view = new_contents->view();

  new_view->CreateView();

  // TODO(brettw) it seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreateViewForWidget(new_contents->render_view_host());
  return new_contents;
}

RenderWidgetHostView* WebContentsViewWin::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  // Create the widget and its associated view.
  // TODO(brettw) can widget creation be cross-platform?
  RenderWidgetHost* widget_host =
      new RenderWidgetHost(web_contents_->process(), route_id);
  RenderWidgetHostViewWin* widget_view =
      new RenderWidgetHostViewWin(widget_host);

  // We set the parent HWDN explicitly as pop-up HWNDs are parented and owned by
  // the first non-child HWND of the HWND that was specified to the CreateWindow
  // call.
  // TODO(brettw) this should not need to get the current RVHView from the
  // WebContents. We should have it somewhere ourselves.
  widget_view->set_parent_hwnd(
      web_contents_->render_widget_host_view()->GetPluginNativeView());
  widget_view->set_close_on_deactivate(true);
  widget_view->set_activatable(activatable);

  return widget_view;
}

void WebContentsViewWin::ShowCreatedWindowInternal(
    WebContents* new_web_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  if (!new_web_contents->render_widget_host_view() ||
      !new_web_contents->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }

  // TODO(brettw) this seems bogus to reach into here and initialize the host.
  new_web_contents->render_view_host()->Init();
  web_contents_->AddNewContents(new_web_contents, disposition, initial_pos,
                                user_gesture);
}

void WebContentsViewWin::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  // TODO(beng): (Cleanup) move all this windows-specific creation and showing
  //             code into RenderWidgetHostView behind some API that a
  //             ChromeView can also reasonably implement.
  RenderWidgetHostViewWin* widget_host_view_win =
      static_cast<RenderWidgetHostViewWin*>(widget_host_view);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }

  // This logic should be implemented by RenderWidgetHostHWND (as mentioned
  // above) in the ::Init function, which should take a parent and some initial
  // bounds.
  widget_host_view_win->Create(GetNativeView(), NULL, NULL,
                               WS_POPUP, WS_EX_TOOLWINDOW);
  widget_host_view_win->MoveWindow(initial_pos.x(), initial_pos.y(),
                                   initial_pos.width(), initial_pos.height(),
                                   TRUE);
  widget_host_view_win->ShowWindow(widget_host_view_win->activatable() ?
                                   SW_SHOW : SW_SHOWNA);
  widget_host->Init();
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
    case WM_RBUTTONDOWN: {
      // Make sure this TabContents is activated when it is clicked on.
      if (web_contents_->delegate())
        web_contents_->delegate()->ActivateContents(web_contents_);
      DownloadRequestManager* drm =
          g_browser_process->download_request_manager();
      if (drm)
        drm->OnUserGesture(web_contents_);
      break;
    }
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
    if (!sad_tab_.get())
      sad_tab_.reset(new SadTabView);
    CRect cr;
    GetClientRect(&cr);
    sad_tab_->SetBounds(gfx::Rect(cr));
    ChromeCanvasPaint canvas(GetHWND(), true);
    sad_tab_->ProcessPaint(&canvas);
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
    HWND inner_hwnd =
        web_contents_->render_widget_host_view()->GetPluginNativeView();
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
    WasHidden();
  } else {
    // The WebContents was shown by a means other than the user selecting a
    // Tab, e.g. the window was minimized then restored.
    if (window_pos->flags & SWP_SHOWWINDOW)
      WasShown();

    // Unless we were specifically told not to size, cause the renderer to be
    // sized to the new bounds, which forces a repaint. Not required for the
    // simple minimize-restore case described above, for example, since the
    // size hasn't changed.
    if (!(window_pos->flags & SWP_NOSIZE))
      WasSized(gfx::Size(window_pos->cx, window_pos->cy));

    // If we have a FindInPage dialog, notify it that the window changed.
    if (find_bar_.get() && find_bar_->IsVisible())
      find_bar_->MoveWindowIfNecessary(gfx::Rect());
  }
}

void WebContentsViewWin::OnSize(UINT param, const CSize& size) {
  WidgetWin::OnSize(param, size);

  // Hack for thinkpad touchpad driver.
  // Set fake scrollbars so that we can get scroll messages,
  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_ALL;

  si.nMin = 1;
  si.nMax = 100;
  si.nPage = 10;
  si.nPos = 50;

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
    SendMessage(GetContentNativeView(), message,
                MAKELONG(scroll_type, position),
                reinterpret_cast<LPARAM>(scrollbar));
  }
}

void WebContentsViewWin::WasHidden() {
  web_contents_->HideContents();
  if (find_bar_.get())
    find_bar_->DidBecomeUnselected();
}

void WebContentsViewWin::WasShown() {
  web_contents_->ShowContents();
  if (find_bar_.get())
    find_bar_->DidBecomeSelected();
}

void WebContentsViewWin::WasSized(const gfx::Size& size) {
  if (web_contents_->interstitial_page())
    web_contents_->interstitial_page()->SetSize(size);
  if (web_contents_->render_widget_host_view())
    web_contents_->render_widget_host_view()->SetSize(size);
  if (find_bar_.get())
    find_bar_->RespondToResize(size);

  // TODO(brettw) this function can probably be moved to this class.
  web_contents_->RepositionSupressedPopupsToFit(size);
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
