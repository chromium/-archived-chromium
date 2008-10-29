/*
* Copyright 2007 Google Inc. All Rights Reserved.
*
* Portions Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
*
* ***** BEGIN LICENSE BLOCK *****
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* ***** END LICENSE BLOCK *****
*
*/

#include "config.h"
#include "build/build_config.h"

#include "base/compiler_specific.h"
MSVC_PUSH_WARNING_LEVEL(0);
#if defined(OS_WIN)
#include "Cursor.h"
#endif
#include "Document.h"
#include "DocumentLoader.h"
#include "DragController.h"
#include "DragData.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FontDescription.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "Image.h"
#include "InspectorController.h"
#include "IntRect.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginInfoStore.h"
#if defined(OS_WIN)
#include "RenderThemeWin.h"
#endif
#include "ResourceHandle.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
#include "event_conversion.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/context_menu_client_impl.h"
#include "webkit/glue/dragclient_impl.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/inspector_client_impl.h"
#include "webkit/glue/searchable_form_data.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webhistoryitem_impl.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webwidget_impl.h"
#include "webkit/port/platform/graphics/PlatformContextSkia.h"

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
static const double kTextSizeMultiplierRatio = 1.2;
static const double kMinTextSizeMultiplier = 0.5;
static const double kMaxTextSizeMultiplier = 3.0;

// The webcore drag operation type when something is trying to be dropped on
// the webview.  These values are taken from Apple's windows port.
static const WebCore::DragOperation kDropTargetOperation =
    static_cast<WebCore::DragOperation>(DragOperationCopy | DragOperationLink);

// WebView ----------------------------------------------------------------

/*static*/
WebView* WebView::Create(WebViewDelegate* delegate,
                         const WebPreferences& prefs) {
  WebViewImpl* instance = new WebViewImpl();
  instance->AddRef();
  instance->SetPreferences(prefs);
  instance->main_frame_->InitMainFrame(instance);
  // Set the delegate after initializing the main frame, to avoid trying to
  // respond to notifications before we're fully initialized.
  instance->delegate_ = delegate;
  // Restrict the access to the local file system
  // (see WebView.mm WebView::_commonInitializationWithFrameName).
  FrameLoader::setLocalLoadPolicy(
      FrameLoader::AllowLocalLoadsForLocalOnly);
  return instance;
}

WebViewImpl::WebViewImpl()
    : delegate_(NULL),
      pending_history_item_(NULL),
      observed_new_navigation_(false),
#ifndef NDEBUG
      new_navigation_loader_(NULL),
#endif
      zoom_level_(0),
      context_menu_allowed_(false),
      doing_drag_and_drop_(false),
      suppress_next_keypress_event_(false),
      window_open_disposition_(IGNORE_ACTION),
      ime_accept_events_(true) {
  // WebKit/win/WebView.cpp does the same thing, except they call the
  // KJS specific wrapper around this method. We need to have threading
  // initialized because CollatorICU requires it.
  WTF::initializeThreading();

  // set to impossible point so we always get the first mouse pos
  last_mouse_position_.SetPoint(-1, -1);

  // the page will take ownership of the various clients
  page_.reset(new Page(new ChromeClientImpl(this),
                       new ContextMenuClientImpl(this),
                       new EditorClientImpl(this),
                       new DragClientImpl(this),
                       new WebInspectorClient(this)));

  page_->backForwardList()->setClient(this);

  // The group name identifies a namespace of pages.  I'm not sure how it's
  // intended to be used, but keeping all pages in the same group works for us.
  page_->setGroupName("default");

  // This is created with a refcount of 1, and we assign it to a RefPtr,
  // giving a refcount of 2. The ref is done on behalf of
  // FrameWin/FrameLoaderWin which references the WebFrame via the
  // FrameWinClient/FrameLoaderClient interfaces. See the comment at the
  // top of webframe_impl.cc
  main_frame_ = new WebFrameImpl();
}

WebViewImpl::~WebViewImpl() {
  DCHECK(main_frame_ == NULL);
  DCHECK(page_ == NULL);
  ReleaseFocusReferences();
  for (std::set<ImageResourceFetcher*>::iterator i = image_fetchers_.begin();
       i != image_fetchers_.end(); ++i) {
    delete *i;
  }
}

void WebViewImpl::SetUseEditorDelegate(bool value) {
  ASSERT(page_ != 0);  // The macro doesn't like (!page_) with a scoped_ptr.
  ASSERT(page_->editorClient());
  EditorClientImpl* editor_client =
      static_cast<EditorClientImpl*>(page_->editorClient());
  editor_client->SetUseEditorDelegate(value);
}

void WebViewImpl::SetTabKeyCyclesThroughElements(bool value) {
  if (page_ != NULL) {
    page_->setTabKeyCyclesThroughElements(value);
  }
}

void WebViewImpl::MouseMove(const WebMouseEvent& event) {
  if (!main_frame_->frameview())
    return;

  last_mouse_position_.SetPoint(event.x, event.y);

  // We call mouseMoved here instead of handleMouseMovedEvent because we need
  // our ChromeClientImpl to receive changes to the mouse position and
  // tooltip text, and mouseMoved handles all of that.
  main_frame_->frameview()->frame()->eventHandler()->mouseMoved(
      MakePlatformMouseEvent(main_frame_->frameview(), event));
}

void WebViewImpl::MouseLeave(const WebMouseEvent& event) {
  // This event gets sent as the main frame is closing.  In that case, just
  // ignore it.
  if (!main_frame_ || !main_frame_->frameview())
    return;

  delegate_->UpdateTargetURL(this, GURL());

  main_frame_->frameview()->frame()->eventHandler()->handleMouseMoveEvent(
      MakePlatformMouseEvent(main_frame_->frameview(), event));
}

void WebViewImpl::MouseDown(const WebMouseEvent& event) {
  if (!main_frame_->frameview())
    return;

  last_mouse_down_point_ = gfx::Point(event.x, event.y);
  main_frame_->frame()->eventHandler()->handleMousePressEvent(
      MakePlatformMouseEvent(main_frame_->frameview(), event));
}

void WebViewImpl::MouseContextMenu(const WebMouseEvent& event) {
  page_->contextMenuController()->clearContextMenu();

  MakePlatformMouseEvent pme(main_frame_->frameview(), event);

  // Find the right target frame. See issue 1186900.
  IntPoint doc_point(
      main_frame_->frame()->view()->windowToContents(pme.pos()));
  HitTestResult result =
      main_frame_->frame()->eventHandler()->hitTestResultAtPoint(doc_point,
                                                                 false);
  Frame* target_frame;
  if (result.innerNonSharedNode())
      target_frame = result.innerNonSharedNode()->document()->frame();
  else
      target_frame = page_->focusController()->focusedOrMainFrame();

#if defined(OS_WIN)
  target_frame->view()->setCursor(pointerCursor());
#endif

  context_menu_allowed_ = true;
  target_frame->eventHandler()->sendContextMenuEvent(pme);
  context_menu_allowed_ = false;
  // Actually showing the context menu is handled by the ContextMenuClient
  // implementation...
}

void WebViewImpl::MouseUp(const WebMouseEvent& event) {
  if (!main_frame_->frameview())
    return;

  MouseCaptureLost();
  main_frame_->frameview()->frame()->eventHandler()->handleMouseReleaseEvent(
      MakePlatformMouseEvent(main_frame_->frameview(), event));

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (event.button == WebMouseEvent::BUTTON_RIGHT)
    MouseContextMenu(event);
}

void WebViewImpl::MouseWheel(const WebMouseWheelEvent& event) {
  MakePlatformWheelEvent platform_event(main_frame_->frameview(), event);
  main_frame_->frame()->eventHandler()->handleWheelEvent(platform_event);
}

bool WebViewImpl::KeyEvent(const WebKeyboardEvent& event) {
  DCHECK((event.type == WebInputEvent::KEY_DOWN) ||
         (event.type == WebInputEvent::KEY_UP));

  // Please refer to the comments explaining the suppress_next_keypress_event_
  // member.
  // The suppress_next_keypress_event_ is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  EventHandler* handler = frame->eventHandler();
  if (!handler)
    return KeyEventDefault(event);

#if defined(OS_WIN)
  // TODO(pinkerton): figure out these keycodes on non-windows
  if (((event.modifiers == 0) && (event.key_code == VK_APPS)) ||
      ((event.modifiers == WebInputEvent::SHIFT_KEY) &&
       (event.key_code == VK_F10))) {
    SendContextMenuEvent(event);
    return true;
  }
#endif

  MakePlatformKeyboardEvent evt(event);

#if defined(OS_WIN)
  if (WebInputEvent::KEY_DOWN == event.type) {
    MakePlatformKeyboardEvent evt_rawkeydown = evt;
    evt_rawkeydown.SetKeyType(WebCore::PlatformKeyboardEvent::RawKeyDown);
    if (handler->keyEvent(evt_rawkeydown) && !evt_rawkeydown.isSystemKey()) {
      suppress_next_keypress_event_ = true;
      return true;
    }
  } else {
    if (handler->keyEvent(evt)) {
      return true;
    }
  }
#elif defined(OS_MACOSX)
  // Windows and Cocoa handle events in rather different ways. On Windows,
  // you get two events: WM_KEYDOWN/WM_KEYUP and WM_CHAR. In
  // PlatformKeyboardEvent, RawKeyDown represents the raw messages. When
  // processing them, we don't process text editing events, since we'll be
  // getting the data soon enough. In Cocoa, we get one event with both the
  // raw and processed data. Therefore we need to keep the type as KeyDown, so
  // that we'll know that this is the only time we'll have the event and that
  // we need to do our thing.
  if (handler->keyEvent(evt)) {
    return true;
  }
#endif

  return KeyEventDefault(event);
}

bool WebViewImpl::CharEvent(const WebKeyboardEvent& event) {
  DCHECK(event.type == WebInputEvent::CHAR);

  // Please refer to the comments explaining the suppress_next_keypress_event_
  // member.
  // The suppress_next_keypress_event_ is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as it only applies
  // to the current keyPress event.
  if (suppress_next_keypress_event_) {
    suppress_next_keypress_event_ = false;
    return true;
  }

  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  EventHandler* handler = frame->eventHandler();
  if (!handler)
    return KeyEventDefault(event);

  MakePlatformKeyboardEvent evt(event);
  if (!evt.IsCharacterKey())
    return true;

#if defined(OS_WIN)
  // Safari 3.1 does not pass off WM_SYSCHAR messages to the
  // eventHandler::keyEvent. We mimic this behavior.
  if (evt.isSystemKey())
    return handler->handleAccessKey(evt);
#endif

  if (!handler->keyEvent(evt))
    return KeyEventDefault(event);

  return true;
}

/*
* The WebViewImpl::SendContextMenuEvent function is based on the Webkit
* function
* bool WebView::handleContextMenuEvent(WPARAM wParam, LPARAM lParam) in
* webkit\webkit\win\WebView.cpp. The only significant change in this
* function is the code to convert from a Keyboard event to the Right
* Mouse button up event.
* 
* This function is an ugly copy/paste and should be cleaned up when the
* WebKitWin version is cleaned: https://bugs.webkit.org/show_bug.cgi?id=20438
*/
#if defined(OS_WIN)
// TODO(pinkerton): implement on non-windows
bool WebViewImpl::SendContextMenuEvent(const WebKeyboardEvent& event) {
  static const int kContextMenuMargin = 1;
  Frame* main_frame = page()->mainFrame();
  FrameView* view = main_frame->view();
  if (!view)
    return false;

  IntPoint coords(-1, -1);
  int right_aligned = ::GetSystemMetrics(SM_MENUDROPALIGNMENT);
  IntPoint location;

  // The context menu event was generated from the keyboard, so show the
  // context menu by the current selection.
  Position start = main_frame->selection()->selection().start();
  Position end = main_frame->selection()->selection().end();

  if (!start.node() || !end.node()) {
    location =
        IntPoint(right_aligned ? view->contentsWidth() - kContextMenuMargin
                     : kContextMenuMargin, kContextMenuMargin);
  } else {
    RenderObject* renderer = start.node()->renderer();
    if (!renderer)
      return false;

    RefPtr<Range> selection = main_frame->selection()->toRange();
    IntRect first_rect = main_frame->firstRectForRange(selection.get());

    int x = right_aligned ? first_rect.right() : first_rect.x();
    location = IntPoint(x, first_rect.bottom());
  }

  location = view->contentsToWindow(location);
  // FIXME: The IntSize(0, -1) is a hack to get the hit-testing to result in
  // the selected element. Ideally we'd have the position of a context menu
  // event be separate from its target node.
  coords = location + IntSize(0, -1);

  // The contextMenuController() holds onto the last context menu that was
  // popped up on the page until a new one is created. We need to clear
  // this menu before propagating the event through the DOM so that we can
  // detect if we create a new menu for this event, since we won't create
  // a new menu if the DOM swallows the event and the defaultEventHandler does
  // not run.
  page()->contextMenuController()->clearContextMenu();

  Frame* focused_frame = page()->focusController()->focusedOrMainFrame();
  focused_frame->view()->setCursor(pointerCursor());
  WebMouseEvent mouse_event;
  mouse_event.button = WebMouseEvent::BUTTON_RIGHT;
  mouse_event.x = coords.x();
  mouse_event.y = coords.y();
  mouse_event.type = WebInputEvent::MOUSE_UP;

  MakePlatformMouseEvent platform_event(view, mouse_event);

  context_menu_allowed_ = true;
  bool handled =
      focused_frame->eventHandler()->sendContextMenuEvent(platform_event);
  context_menu_allowed_ = false;
  return handled;
}
#endif

bool WebViewImpl::KeyEventDefault(const WebKeyboardEvent& event) {
  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  switch (event.type) {
    case WebInputEvent::CHAR: {
#if defined(OS_WIN)
      // TODO(pinkerton): hook this up for non-win32
      if (event.key_code == VK_SPACE) {
        int key_code = ((event.modifiers & WebInputEvent::SHIFT_KEY) ?
                         VK_PRIOR : VK_NEXT);
        return ScrollViewWithKeyboard(key_code);
      }
#endif
      break;
    }

    case WebInputEvent::KEY_DOWN: {
      if (event.modifiers == WebInputEvent::CTRL_KEY) {
        switch (event.key_code) {
          case 'A':
            GetFocusedFrame()->SelectAll();
            return true;
#if defined(OS_WIN)
          case VK_INSERT:
#endif
          case 'C':
            GetFocusedFrame()->Copy();
            return true;
          // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
          // key combinations which affect scrolling. Safari is buggy in the
          // sense that it scrolls the page for all Ctrl+scrolling key
          // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
#if defined(OS_WIN)
          case VK_HOME:
          case VK_END:
#endif
            break;
          default:
            return false;
        }
      }
#if defined(OS_WIN)
      if (!event.system_key) {
        return ScrollViewWithKeyboard(event.key_code);
      }
#endif
      break;
    }

    default:
      break;
  }
  return false;
}

bool WebViewImpl::ScrollViewWithKeyboard(int key_code) {
  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  ScrollDirection scroll_direction;
  ScrollGranularity scroll_granularity;

#if defined(OS_WIN)
  switch (key_code) {
    case VK_LEFT:
      scroll_direction = ScrollLeft;
      scroll_granularity = ScrollByLine;
      break;
    case VK_RIGHT:
      scroll_direction = ScrollRight;
      scroll_granularity = ScrollByLine;
      break;
    case VK_UP:
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByLine;
      break;
    case VK_DOWN:
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByLine;
      break;
    case VK_HOME:
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByDocument;
      break;
    case VK_END:
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByDocument;
      break;
    case VK_PRIOR:  // page up
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByPage;
      break;
    case VK_NEXT:  // page down
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByPage;
      break;
    default:
      return false;
  }
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  scroll_direction = ScrollDown;
  scroll_granularity = ScrollByLine;
#endif

  bool scroll_handled =
      frame->eventHandler()->scrollOverflow(scroll_direction,
                                            scroll_granularity);
  Frame* current_frame = frame;
  while (!scroll_handled && current_frame) {
    scroll_handled = current_frame->view()->scroll(scroll_direction,
                                                   scroll_granularity);
    current_frame = current_frame->tree()->parent();
  }
  return scroll_handled;
}

Frame* WebViewImpl::GetFocusedWebCoreFrame() {
  if (!main_frame_ || !main_frame_->frame())
    return NULL;

  return
    main_frame_->frame()->page()->focusController()->focusedOrMainFrame();
}

// static
WebViewImpl* WebViewImpl::FromPage(WebCore::Page* page) {
  return WebFrameImpl::FromFrame(page->mainFrame())->webview_impl();
}

// WebView --------------------------------------------------------------------

bool WebViewImpl::ShouldClose() {
  // TODO(creis): This should really cause a recursive depth-first walk of all
  // frames in the tree, calling each frame's onbeforeunload.  At the moment,
  // we're consistent with Safari 3.1, not IE/FF.
  Frame* frame = page_->focusController()->focusedOrMainFrame();
  if (!frame)
    return true;

  return frame->shouldClose();
}

void WebViewImpl::Close() {
  // Do this first to prevent reentrant notifications from being sent to the
  // initiator of the close.
  delegate_ = NULL;

  // Initiate shutdown for the entire frameset.
  if (main_frame_) {
    // This will cause a lot of notifications to be sent.
    main_frame_->frame()->loader()->frameDetached();
    main_frame_ = NULL;
  }

  page_.reset();
}

WebViewDelegate* WebViewImpl::GetDelegate() {
  return delegate_;
}

WebFrame* WebViewImpl::GetMainFrame() {
  return main_frame_.get();
}

WebFrame* WebViewImpl::GetFocusedFrame() {
  Frame* frame = GetFocusedWebCoreFrame();
  return frame ? WebFrameImpl::FromFrame(frame) : NULL;
}

void WebViewImpl::SetFocusedFrame(WebFrame* frame) {
  if (!frame) {
    // Clears the focused frame if any.
    Frame* frame = GetFocusedWebCoreFrame();
    if (frame)
      frame->selection()->setFocused(false);
    return;
  }
  WebFrameImpl* frame_impl = static_cast<WebFrameImpl*>(frame);
  WebCore::Frame* webcore_frame = frame_impl->frame();
  webcore_frame->page()->focusController()->setFocusedFrame(webcore_frame);
}

WebFrame* WebViewImpl::GetFrameWithName(const std::wstring& name) {
  String name_str = webkit_glue::StdWStringToString(name);
  Frame* frame = main_frame_->frame()->tree()->find(name_str);
  return frame ? WebFrameImpl::FromFrame(frame) : NULL;
}

WebFrame* WebViewImpl::GetPreviousFrameBefore(WebFrame* frame, bool wrap) {
  WebFrameImpl* frame_impl = static_cast<WebFrameImpl*>(frame);
  WebCore::Frame* previous =
      frame_impl->frame()->tree()->traversePreviousWithWrap(wrap);
  return previous ? WebFrameImpl::FromFrame(previous) : NULL;
}

WebFrame* WebViewImpl::GetNextFrameAfter(WebFrame* frame, bool wrap) {
  WebFrameImpl* frame_impl = static_cast<WebFrameImpl*>(frame);
  WebCore::Frame* next =
      frame_impl->frame()->tree()->traverseNextWithWrap(wrap);
  return next ? WebFrameImpl::FromFrame(next) : NULL;
}

void WebViewImpl::Resize(const gfx::Size& new_size) {
  if (size_ == new_size)
    return;
  size_ = new_size;

  if (main_frame_->frameview()) {
    main_frame_->frameview()->resize(size_.width(), size_.height());
    main_frame_->frame()->sendResizeEvent();
  }

  if (delegate_) {
    gfx::Rect damaged_rect(0, 0, size_.width(), size_.height());
    delegate_->DidInvalidateRect(this, damaged_rect);
  }
}

void WebViewImpl::Layout() {
  if (main_frame_) {
    // In order for our child HWNDs (NativeWindowWidgets) to update properly,
    // they need to be told that we are updating the screen.  The problem is
    // that the native widgets need to recalculate their clip region and not
    // overlap any of our non-native widgets.  To force the resizing, call
    // setFrameGeometry().  This will be a quick operation for most frames, but
    // the NativeWindowWidgets will update a proper clipping region.
    FrameView* frameview = main_frame_->frameview();
    if (frameview)
      frameview->setFrameGeometry(frameview->frameGeometry());

    // setFrameGeometry may have the side-effect of causing existing page
    // layout to be invalidated, so layout needs to be called last.

    main_frame_->Layout();
  }
}

void WebViewImpl::Paint(gfx::PlatformCanvas* canvas, const gfx::Rect& rect) {
  if (main_frame_)
    main_frame_->Paint(canvas, rect);
}

// TODO(eseidel): g_current_input_event should be removed once
// ChromeClient:show() can get the current-event information from WebCore.
/* static */
const WebInputEvent* WebViewImpl::g_current_input_event = NULL;

bool WebViewImpl::HandleInputEvent(const WebInputEvent* input_event) {
  // If we've started a drag and drop operation, ignore input events until
  // we're done.
  if (doing_drag_and_drop_)
    return true;

  // TODO(eseidel): Remove g_current_input_event.
  // This only exists to allow ChromeClient::show() to know which mouse button
  // triggered a window.open event.
  // Safari must perform a similar hack, ours is in our WebKit glue layer
  // theirs is in the application.  This should go when WebCore can be fixed
  // to pass more event information to ChromeClient::show()
  g_current_input_event = input_event;

  bool handled = true;

  // TODO(jcampan): WebKit seems to always return false on mouse events
  // processing methods. For now we'll assume it has processed them (as we are
  // only interested in whether keyboard events are processed).
  switch (input_event->type) {
    case WebInputEvent::MOUSE_MOVE:
      MouseMove(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::MOUSE_LEAVE:
      MouseLeave(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::MOUSE_WHEEL:
      MouseWheel(*static_cast<const WebMouseWheelEvent*>(input_event));
      break;

    case WebInputEvent::MOUSE_DOWN:
    case WebInputEvent::MOUSE_DOUBLE_CLICK:
      MouseDown(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::MOUSE_UP:
      MouseUp(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::KEY_DOWN:
    case WebInputEvent::KEY_UP:
      handled = KeyEvent(*static_cast<const WebKeyboardEvent*>(input_event));
      break;

    case WebInputEvent::CHAR:
      handled = CharEvent(*static_cast<const WebKeyboardEvent*>(input_event));
      break;
    default:
      handled = false;
  }

  g_current_input_event = NULL;

  return handled;
}

void WebViewImpl::MouseCaptureLost() {
}

// TODO(darin): these navigation methods should be killed

void WebViewImpl::StopLoading() {
  main_frame_->StopLoading();
}

void WebViewImpl::SetBackForwardListSize(int size) {
  page_->backForwardList()->setCapacity(size);
}

void WebViewImpl::SetFocus(bool enable) {
  if (enable) {
    // Getting the focused frame will have the side-effect of setting the main
    // frame as the focused frame if it is not already focused.  Otherwise, if
    // there is already a focused frame, then this does nothing.
    GetFocusedFrame();
    if (main_frame_ && main_frame_->frame()) {
      Frame* frame = main_frame_->frame();
      if (!frame->selection()->isFocusedAndActive()) {
        // No one has focus yet, try to restore focus.
        RestoreFocus();
        frame->page()->focusController()->setActive(true);
      }
      Frame* focused_frame =
          frame->page()->focusController()->focusedOrMainFrame();
      frame->selection()->setFocused(frame == focused_frame);
    }
    ime_accept_events_ = true;
  } else {
    // Clear out who last had focus. If someone has focus, the refs will be
    // updated below.
    ReleaseFocusReferences();

    // Clear focus on the currently focused frame if any.

    if (!main_frame_)
      return;

    Frame* frame = main_frame_->frame();
    if (!frame)
      return;

    RefPtr<Frame> focused = frame->page()->focusController()->focusedFrame();
    if (focused.get()) {
      // Update the focus refs, this way we can give focus back appropriately.
      // It's entirely possible to have a focused document, but not a focused
      // node.
      RefPtr<Document> document = focused->document();
      last_focused_frame_ = focused;
      if (document.get()) {
        RefPtr<Node> focused_node = document->focusedNode();
        if (focused_node.get()) {
          // To workaround bug #792423, we do not blur the focused node.  This
          // should be reenabled when we merge a WebKit that has the fix for
          // http://bugs.webkit.org/show_bug.cgi?id=16928.
          // last_focused_node_ = focused_node;
          // document->setFocusedNode(NULL);
        }
      }
      frame->page()->focusController()->setFocusedFrame(0);
      // Finish an ongoing composition to delete the composition node.
      Editor* editor = focused->editor();
      if (editor && editor->hasComposition())
        editor->confirmComposition();
      ime_accept_events_ = false;
    }
    // Make sure the main frame doesn't think it has focus.
    if (frame != focused.get())
      frame->selection()->setFocused(false);
  }
}

// TODO(jcampan): http://b/issue?id=1157486 this is needed to work-around
// issues caused by the fix for bug #792423 and should be removed when that
// bug is fixed.
void WebViewImpl::StoreFocusForFrame(WebFrame* frame) {
  DCHECK(frame);

  // We only want to store focus info if we are the focused frame and if we have
  // not stored it already.
  WebCore::Frame* webcore_frame = static_cast<WebFrameImpl*>(frame)->frame();
  if (last_focused_frame_.get() != webcore_frame || last_focused_node_.get())
    return;

  // Clear out who last had focus. If someone has focus, the refs will be
  // updated below.
  ReleaseFocusReferences();

  last_focused_frame_ = webcore_frame;
  RefPtr<Document> document = last_focused_frame_->document();
  if (document.get()) {
    RefPtr<Node> focused_node = document->focusedNode();
    if (focused_node.get()) {
      last_focused_node_ = focused_node;
      document->setFocusedNode(NULL);
    }
  }
}

void WebViewImpl::ImeSetComposition(int string_type, int cursor_position,
                                    int target_start, int target_end,
                                    int string_length,
                                    const wchar_t *string_data) {
  Frame* focused = GetFocusedWebCoreFrame();
  if (!focused || !ime_accept_events_) {
    return;
  }
  Editor* editor = focused->editor();
  if (!editor)
    return;
  if (!editor->canEdit()) {
    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    if (!editor->hasComposition())
      return;
  }

  if (string_type == 0) {
    // A browser process sent an IPC message which does not contain a valid
    // string, which means an ongoing composition has been canceled.
    // If the ongoing composition has been canceled, replace the ongoing
    // composition string with an empty string and complete it.
    // TODO(hbono): Need to add a new function to cancel the ongoing
    // composition to WebCore::Editor?
    WebCore::String empty_string;
    editor->confirmComposition(empty_string);
  } else {
    // A browser process sent an IPC message which contains a string to be
    // displayed in this Editor object.
    // To display the given string, set the given string to the
    // m_compositionNode member of this Editor object and display it.
    // NOTE: An empty string (often sent by Chinese IMEs and Korean IMEs)
    // causes a panic in Editor::setComposition(), which deactivates the
    // m_frame.m_sel member of this Editor object, i.e. we can never display
    // composition strings in the m_compositionNode member.
    // (I have not been able to find good methods for re-activating it.)
    // Therefore, I have to prevent from calling Editor::setComposition()
    // with its first argument an empty string.
    if (string_length > 0) {
      if (target_start < 0) target_start = 0;
      if (target_end < 0) target_end = string_length;
      std::wstring temp(string_data, string_length);
      WebCore::String composition_string(webkit_glue::StdWStringToString(temp));
      // Create custom underlines.
      // To emphasize the selection, the selected region uses a solid black
      // for its underline while other regions uses a pale gray for theirs.
      WTF::Vector<WebCore::CompositionUnderline> underlines(3);
      underlines[0].startOffset = 0;
      underlines[0].endOffset = target_start;
      underlines[0].thick = true;
      underlines[0].color.setRGB(0xd3, 0xd3, 0xd3);
      underlines[1].startOffset = target_start;
      underlines[1].endOffset = target_end;
      underlines[1].thick = true;
      underlines[1].color.setRGB(0x00, 0x00, 0x00);
      underlines[2].startOffset = target_end;
      underlines[2].endOffset = string_length;
      underlines[2].thick = true;
      underlines[2].color.setRGB(0xd3, 0xd3, 0xd3);
      // When we use custom underlines, WebKit ("InlineTextBox.cpp" Line 282)
      // prevents from writing a text in between 'selectionStart' and
      // 'selectionEnd' somehow.
      // Therefore, we use the 'cursor_position' for these arguments so that
      // there are not any characters in the above region.
      editor->setComposition(composition_string, underlines,
                             cursor_position, cursor_position);
    }
#if defined(OS_WIN)
    // The given string is a result string, which means the ongoing
    // composition has been completed. I have to call the
    // Editor::confirmCompletion() and complete this composition.
    if (string_type == GCS_RESULTSTR) {
      editor->confirmComposition();
    }
#endif
  }
}

bool WebViewImpl::ImeUpdateStatus(bool* enable_ime, const void **id,
                                  int* x, int* y) {
  // Initialize the return values so that we can disable the IME attached
  // to a browser process when an error occurs while retrieving information
  // of the focused edit control.
  *enable_ime = false;
  *id = NULL;
  *x = -1;
  *y = -1;
  // Store the position of the bottom-left corner of the caret.
  // This process consists of the following four steps:
  //  1. Retrieve the selection controller of the focused frame;
  //  2. Retrieve the caret rectangle from the controller;
  //  3. Convert the rectangle, which is relative to the parent view, to the
  //     one relative to the client window, and;
  //  4. Store the position of its bottom-left corner.
  const Frame* focused = GetFocusedWebCoreFrame();
  if (!focused)
    return false;
  const Editor* editor = focused->editor();
  if (!editor || !editor->canEdit())
    return false;
  const SelectionController* controller = focused->selection();
  if (!controller)
    return false;
  const Node* node = controller->start().node();
  if (!node)
    return false;
  const FrameView* view = node->document()->view();
  if (!view)
    return false;
  IntRect rect = view->contentsToWindow(controller->caretRect());
  *x = rect.x();
  *y = rect.bottom();
  return true;
}

void WebViewImpl::RestoreFocus() {
  if (last_focused_frame_.get()) {
    if (last_focused_frame_->page()) {
      // last_focused_frame_ can be detached from the frame tree, thus,
      // its page can be null.
      last_focused_frame_->page()->focusController()->setFocusedFrame(
        last_focused_frame_.get());
    }
    if (last_focused_node_.get()) {
      // last_focused_node_ may be null, make sure it's valid before trying to
      // focus it.
      static_cast<Element*>(last_focused_node_.get())->focus();
    }
    // And clear out the refs.
    ReleaseFocusReferences();
  }
}

void WebViewImpl::SetInitialFocus(bool reverse) {
  if (page_.get()) {
    // So RestoreFocus does not focus anything when it is called.
    ReleaseFocusReferences();

    // Since we don't have a keyboard event, we'll create one.
    WebKeyboardEvent keyboard_event;
    keyboard_event.type = WebInputEvent::KEY_DOWN;
    if (reverse)
      keyboard_event.modifiers = WebInputEvent::SHIFT_KEY;
    // VK_TAB which is only defined on Windows.
    keyboard_event.key_code = 0x09;
    MakePlatformKeyboardEvent platform_event(keyboard_event);
    // We have to set the key type explicitly to avoid an assert in the
    // KeyboardEvent constructor.
    platform_event.SetKeyType(PlatformKeyboardEvent::RawKeyDown);
    RefPtr<KeyboardEvent> webkit_event = KeyboardEvent::create(platform_event, NULL);
    page()->focusController()->setInitialFocus(
        reverse ? WebCore::FocusDirectionBackward :
                  WebCore::FocusDirectionForward,
        webkit_event.get());
  }
}

bool WebViewImpl::FocusedFrameNeedsSpellchecking() {
  const WebCore::Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;
  const WebCore::Editor* editor = frame->editor();
  if (!editor)
    return false;
  const WebCore::Document* document = frame->document();
  if (!document)
    return false;
  const WebCore::Node* node = document->focusedNode();
  if (!node)
    return false;
  const WebCore::RenderObject* renderer = node->renderer();
  if (!renderer)
    return false;
  // We should also retrieve the contenteditable attribute of this element to
  // determine if this element needs spell-checking.
  const WebCore::EUserModify user_modify = renderer->style()->userModify();
  return (renderer->isTextArea() && editor->canEdit()) ||
         user_modify == WebCore::READ_WRITE ||
         user_modify == WebCore::READ_WRITE_PLAINTEXT_ONLY;
}

// Releases references used to restore focus.
void WebViewImpl::ReleaseFocusReferences() {
  if (last_focused_frame_.get()) {
    last_focused_frame_.release();
    last_focused_node_.release();
  }
}

bool WebViewImpl::DownloadImage(int id, const GURL& image_url, int image_size) {
  if (!main_frame_ || !main_frame_->frame())
    return false;
  image_fetchers_.insert(
      new ImageResourceFetcher(this, id, image_url, image_size));
  return true;
}

void WebViewImpl::SetPreferences(const WebPreferences& preferences) {
  if (!page_.get())
    return;

  // Keep a local copy of the preferences struct for GetPreferences.
  webprefs_ = preferences;

  Settings* settings = page_->settings();

  settings->setStandardFontFamily(webkit_glue::StdWStringToString(
    preferences.standard_font_family));
  settings->setFixedFontFamily(webkit_glue::StdWStringToString(
    preferences.fixed_font_family));
  settings->setSerifFontFamily(webkit_glue::StdWStringToString(
    preferences.serif_font_family));
  settings->setSansSerifFontFamily(webkit_glue::StdWStringToString(
    preferences.sans_serif_font_family));
  settings->setCursiveFontFamily(webkit_glue::StdWStringToString(
    preferences.cursive_font_family));
  settings->setFantasyFontFamily(webkit_glue::StdWStringToString(
    preferences.fantasy_font_family));
  settings->setDefaultFontSize(preferences.default_font_size);
  settings->setDefaultFixedFontSize(preferences.default_fixed_font_size);
  settings->setMinimumFontSize(preferences.minimum_font_size);
  settings->setMinimumLogicalFontSize(preferences.minimum_logical_font_size);
  settings->setDefaultTextEncodingName(webkit_glue::StdWStringToString(
    preferences.default_encoding));
  settings->setJavaScriptEnabled(preferences.javascript_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
    preferences.javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(
    preferences.loads_images_automatically);
  settings->setPluginsEnabled(preferences.plugins_enabled);
  settings->setDOMPasteAllowed(preferences.dom_paste_enabled);
  settings->setDeveloperExtrasEnabled(preferences.developer_extras_enabled);
  settings->setShrinksStandaloneImagesToFit(
    preferences.shrinks_standalone_images_to_fit);
  settings->setUsesUniversalDetector(preferences.uses_universal_detector);
  settings->setTextAreasAreResizable(preferences.text_areas_are_resizable);
  settings->setAllowScriptsToCloseWindows(
    preferences.allow_scripts_to_close_windows);
  if (preferences.user_style_sheet_enabled) {
    settings->setUserStyleSheetLocation(webkit_glue::GURLToKURL(
      preferences.user_style_sheet_location));
  } else {
    settings->setUserStyleSheetLocation(KURL());
  }
  settings->setUsesPageCache(preferences.uses_page_cache);

  // This setting affects the behavior of links in an editable region:
  // clicking the link should select it rather than navigate to it.
  // Safari uses the same default. It is unlikley an embedder would want to
  // change this, since it would break existing rich text editors.
  settings->setEditableLinkBehavior(WebCore::EditableLinkNeverLive);

  settings->setFontRenderingMode(NormalRenderingMode);
  settings->setJavaEnabled(preferences.java_enabled);

#if defined(OS_WIN)
  // RenderTheme is a singleton that needs to know the default font size to
  // draw some form controls.  We let it know each time the size changes.
  WebCore::RenderThemeWin::setDefaultFontSize(preferences.default_font_size);
#endif
}

const WebPreferences& WebViewImpl::GetPreferences() {
  return webprefs_;
}

// Set the encoding of the current main frame to the one selected by
// a user in the encoding menu.
void WebViewImpl::SetPageEncoding(const std::wstring& encoding_name) {
  if (!main_frame_)
    return;

  if (!encoding_name.empty()) {
    // only change override encoding, don't change default encoding
    // TODO(brettw) use std::string for encoding names.
    String new_encoding_name(webkit_glue::StdWStringToString(encoding_name));
    main_frame_->frame()->loader()->reloadAllowingStaleData(new_encoding_name);
  }
}

// Return the canonical encoding name of current main webframe in webview.
std::wstring WebViewImpl::GetMainFrameEncodingName() {
  if (!main_frame_)
    return std::wstring(L"");

  String encoding_name = main_frame_->frame()->loader()->encoding();
  return webkit_glue::StringToStdWString(encoding_name);
}

void WebViewImpl::ZoomIn(bool text_only) {
  Frame* frame = main_frame()->frame();
  double multiplier = std::min(std::pow(kTextSizeMultiplierRatio,
                                        zoom_level_ + 1),
                               kMaxTextSizeMultiplier);
  float zoom_factor = static_cast<float>(multiplier);
  if (zoom_factor != frame->zoomFactor()) {
    ++zoom_level_;
    frame->setZoomFactor(zoom_factor, text_only);
  }
}

void WebViewImpl::ZoomOut(bool text_only) {
  Frame* frame = main_frame()->frame();
  double multiplier = std::max(std::pow(kTextSizeMultiplierRatio,
                                        zoom_level_ - 1),
                               kMinTextSizeMultiplier);
  float zoom_factor = static_cast<float>(multiplier);
  if (zoom_factor != frame->zoomFactor()) {
    --zoom_level_;
    frame->setZoomFactor(zoom_factor, text_only);
  }
}

void WebViewImpl::ResetZoom() {
  // We don't change the zoom mode (text only vs. full page) here. We just want
  // to reset whatever is already set.
  zoom_level_ = 0;
  main_frame()->frame()->setZoomFactor(
      1.0f,
      main_frame()->frame()->isZoomFactorTextOnly());
}

void WebViewImpl::CopyImageAt(int x, int y) {
  IntPoint point = IntPoint(x, y);

  Frame* frame = main_frame_->frame();
  if (!frame)
    return;

  HitTestResult result =
      frame->eventHandler()->hitTestResultAtPoint(point, false);

  if (result.absoluteImageURL().isEmpty()) {
    // There isn't actually an image at these coordinates.  Might be because
    // the window scrolled while the context menu was open or because the page
    // changed itself between when we thought there was an image here and when
    // we actually tried to retreive the image.
    //
    // TODO: implement a cache of the most recent HitTestResult to avoid having
    //       to do two hit tests.
    return;
  }

  frame->editor()->copyImage(result);
}

void WebViewImpl::InspectElement(int x, int y) {
  if (x == -1 || y == -1) {
    page_->inspectorController()->inspect(NULL);
  } else {
    IntPoint point = IntPoint(x, y);
    HitTestResult result(point);

    if (Frame* frame = main_frame_->frame())
      result = frame->eventHandler()->hitTestResultAtPoint(point, false);

    if (!result.innerNonSharedNode())
      return;

    page_->inspectorController()->inspect(result.innerNonSharedNode());
  }
}

void WebViewImpl::ShowJavaScriptConsole() {
  page_->inspectorController()->showPanel(InspectorController::ConsolePanel);
}

void WebViewImpl::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y) {
  PlatformMouseEvent pme(IntPoint(client_x, client_y),
                         IntPoint(screen_x, screen_y),
                         NoButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  main_frame_->frame()->eventHandler()->dragSourceEndedAt(pme,
      DragOperationCopy);
}

void WebViewImpl::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  PlatformMouseEvent pme(IntPoint(client_x, client_y),
                         IntPoint(screen_x, screen_y),
                         LeftButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  main_frame_->frame()->eventHandler()->dragSourceMovedTo(pme);
}

void WebViewImpl::DragSourceSystemDragEnded() {
  page_->dragController()->dragEnded();
  DCHECK(doing_drag_and_drop_);
  doing_drag_and_drop_ = false;
}

bool WebViewImpl::DragTargetDragEnter(const WebDropData& drop_data,
    int client_x, int client_y, int screen_x, int screen_y) {
  DCHECK(!current_drop_data_.get());

  // Copy drop_data into current_drop_data_.
  WebDropData* drop_data_copy = new WebDropData;
  *drop_data_copy = drop_data;
  current_drop_data_.reset(drop_data_copy);

  DragData drag_data(reinterpret_cast<DragDataRef>(current_drop_data_.get()),
      IntPoint(client_x, client_y), IntPoint(screen_x, screen_y),
      kDropTargetOperation);
  DragOperation effect = page_->dragController()->dragEntered(&drag_data);
  return effect != DragOperationNone;
}

bool WebViewImpl::DragTargetDragOver(
    int client_x, int client_y, int screen_x, int screen_y) {
  DCHECK(current_drop_data_.get());
  DragData drag_data(reinterpret_cast<DragDataRef>(current_drop_data_.get()),
      IntPoint(client_x, client_y), IntPoint(screen_x, screen_y),
      kDropTargetOperation);
  DragOperation effect = page_->dragController()->dragUpdated(&drag_data);
  return effect != DragOperationNone;
}

void WebViewImpl::DragTargetDragLeave() {
  DCHECK(current_drop_data_.get());
  DragData drag_data(reinterpret_cast<DragDataRef>(current_drop_data_.get()),
      IntPoint(), IntPoint(), DragOperationNone);
  page_->dragController()->dragExited(&drag_data);
  current_drop_data_.reset(NULL);
}

void WebViewImpl::DragTargetDrop(
    int client_x, int client_y, int screen_x, int screen_y) {
  DCHECK(current_drop_data_.get());
  DragData drag_data(reinterpret_cast<DragDataRef>(current_drop_data_.get()),
      IntPoint(client_x, client_y), IntPoint(screen_x, screen_y),
      kDropTargetOperation);
  page_->dragController()->performDrag(&drag_data);
  current_drop_data_.reset(NULL);
}

SearchableFormData* WebViewImpl::CreateSearchableFormDataForFocusedNode() {
  if (!main_frame_)
    return NULL;

  Frame* frame = main_frame_->frame();
  if (!frame)
    return NULL;

  if (RefPtr<Frame> focused =
      frame->page()->focusController()->focusedFrame()) {
    RefPtr<Document> document = focused->document();
    if (document.get()) {
      RefPtr<Node> focused_node = document->focusedNode();
      if (focused_node.get() &&
          focused_node->nodeType() == Node::ELEMENT_NODE) {
        return SearchableFormData::Create(
          static_cast<Element*>(focused_node.get()));
      }
    }
  }
  return NULL;
}

void WebViewImpl::DidCommitLoad(bool* is_new_navigation) {
  if (is_new_navigation)
    *is_new_navigation = observed_new_navigation_;

#ifndef NDEBUG
  DCHECK(!observed_new_navigation_ ||
    main_frame_->frame()->loader()->documentLoader() == new_navigation_loader_);
  new_navigation_loader_ = NULL;
#endif
  observed_new_navigation_ = false;
}

void WebViewImpl::StartDragging(const WebDropData& drop_data) {
  if (delegate_) {
    DCHECK(!doing_drag_and_drop_);
    doing_drag_and_drop_ = true;
    delegate_->StartDragging(this, drop_data);
  }
}

const WebCore::Node* WebViewImpl::getInspectedNode(WebCore::Frame* frame) {
  DCHECK(frame);
  WebFrameImpl* webframe_impl = WebFrameImpl::FromFrame(frame);
  if (!webframe_impl)
    return NULL;

  return webframe_impl->inspected_node();
}

void WebViewImpl::ImageResourceDownloadDone(ImageResourceFetcher* fetcher,
                                            bool errored,
                                            const SkBitmap& image) {
  if (delegate_) {
    delegate_->DidDownloadImage(fetcher->id(), fetcher->image_url(), errored,
                                image);
  }
  DeleteImageResourceFetcher(fetcher);
}

//-----------------------------------------------------------------------------
// WebCore::WidgetClientWin

gfx::ViewHandle WebViewImpl::containingWindow() {
  return delegate_ ? delegate_->GetContainingWindow(this) : NULL;
}

void WebViewImpl::invalidateRect(const IntRect& damaged_rect) {
  if (delegate_)
    delegate_->DidInvalidateRect(this, gfx::Rect(damaged_rect.x(),
                                                 damaged_rect.y(),
                                                 damaged_rect.width(),
                                                 damaged_rect.height()));
}

void WebViewImpl::scrollRect(int dx, int dy, const IntRect& clip_rect) {
  if (delegate_)
    delegate_->DidScrollRect(this, dx, dy, gfx::Rect(clip_rect.x(),
                                                     clip_rect.y(),
                                                     clip_rect.width(),
                                                     clip_rect.height()));
}

void WebViewImpl::popupOpened(WebCore::Widget* widget,
                              const WebCore::IntRect& bounds) {
  if (!delegate_)
    return;

  WebWidgetImpl* webwidget =
      static_cast<WebWidgetImpl*>(delegate_->CreatePopupWidget(this));
  webwidget->Init(widget, gfx::Rect(bounds.x(), bounds.y(),
                                    bounds.width(), bounds.height()));
}

void WebViewImpl::popupClosed(WebCore::Widget* widget) {
  NOTREACHED() << "popupClosed called on a non-popup";
}

void WebViewImpl::setCursor(const WebCore::Cursor& cursor) {
#if defined(OS_WIN)
  // TODO(pinkerton): figure out the cursor delegate methods
  if (delegate_)
    delegate_->SetCursor(this, cursor.impl());
#endif
}

void WebViewImpl::setFocus() {
  if (delegate_)
    delegate_->Focus(this);
}

const SkBitmap* WebViewImpl::getPreloadedResourceBitmap(int resource_id) {
  if (!delegate_)
    return NULL;

  return delegate_->GetPreloadedResourceBitmap(resource_id);
}

void WebViewImpl::onScrollPositionChanged(WebCore::Widget* widget) {
  // Scroll position changes should be reflected in the session history.
  if (delegate_)
    delegate_->OnNavStateChanged(this);
}

const WTF::Vector<RefPtr<WebCore::Range> >* WebViewImpl::getTickmarks(
    WebCore::Frame* frame) {
  DCHECK(frame);
  WebFrameImpl* webframe_impl = WebFrameImpl::FromFrame(frame);
  if (!webframe_impl)
    return NULL;

  return &webframe_impl->tickmarks();
}

size_t WebViewImpl::getActiveTickmarkIndex(WebCore::Frame* frame) {
  DCHECK(frame);
  WebFrameImpl* webframe_impl = WebFrameImpl::FromFrame(frame);
  if (!webframe_impl)
    return kNoTickmark;

  // The mainframe can tell us if we are the frame with the active tick-mark.
  if (webframe_impl != main_frame_->active_tickmark_frame())
    return kNoTickmark;

  return webframe_impl->active_tickmark_index();
}

bool WebViewImpl::isHidden() {
  if (!delegate_)
    return true;

  return delegate_->IsHidden();
}

//-----------------------------------------------------------------------------
// WebCore::BackForwardListClient

void WebViewImpl::didAddHistoryItem(WebCore::HistoryItem* item) {
  // If WebCore adds a new HistoryItem, it means this is a new navigation
  // (ie, not a reload or back/forward).
  observed_new_navigation_ = true;
#ifndef NDEBUG
  new_navigation_loader_ = main_frame_->frame()->loader()->documentLoader();
#endif
  delegate_->DidAddHistoryItem();
}

void WebViewImpl::willGoToHistoryItem(WebCore::HistoryItem* item) {
  if (pending_history_item_) {
    if (item == pending_history_item_->GetHistoryItem()) {
      // Let the main frame know this HistoryItem is loading, so it can cache
      // any ExtraData when the DataSource is created.
      main_frame_->set_currently_loading_history_item(pending_history_item_);
      pending_history_item_ = 0;
    }
  }
}

WebCore::HistoryItem* WebViewImpl::itemAtIndex(int index) {
  if (!delegate_)
    return NULL;

  WebHistoryItem* item = delegate_->GetHistoryEntryAtOffset(index);
  if (!item)
    return NULL;

  // If someone has asked for a history item, we probably want to navigate to
  // it soon.  Keep track of it until willGoToHistoryItem is called.
  pending_history_item_ = static_cast<WebHistoryItemImpl*>(item);
  return pending_history_item_->GetHistoryItem();
}

void WebViewImpl::goToItemAtIndexAsync(int index) {
  if (delegate_)
    delegate_->GoToEntryAtOffsetAsync(index);
}

int WebViewImpl::backListCount() {
  if (!delegate_)
    return 0;

  return delegate_->GetHistoryBackListCount();
}

int WebViewImpl::forwardListCount() {
  if (!delegate_)
    return 0;

  return delegate_->GetHistoryForwardListCount();
}

void WebViewImpl::DeleteImageResourceFetcher(ImageResourceFetcher* fetcher) {
  DCHECK(image_fetchers_.find(fetcher) != image_fetchers_.end());
  image_fetchers_.erase(fetcher);

  // We're in the callback from the ImageResourceFetcher, best to delay
  // deletion.
  MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
}
