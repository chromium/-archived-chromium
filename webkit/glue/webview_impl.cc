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
#include "CSSStyleSelector.h"
#include "Cursor.h"
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
#include "HTMLNames.h"
#include "HTMLInputElement.h"
#include "HitTestResult.h"
#include "Image.h"
#include "InspectorController.h"
#include "IntRect.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginInfoStore.h"
#include "PopupMenuChromium.h"
#include "PopupMenuClient.h"
#if defined(OS_WIN)
#include "RenderThemeChromiumWin.h"
#endif
#include "RenderView.h"
#include "ResourceHandle.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/clipboard_conversion.h"
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

// AutocompletePopupMenuClient
class AutocompletePopupMenuClient
    : public RefCounted<AutocompletePopupMenuClient>,
      public WebCore::PopupMenuClient {
 public:
  AutocompletePopupMenuClient(WebViewImpl* webview,
                              WebCore::HTMLInputElement* text_field,
                              const std::vector<std::wstring>& suggestions,
                              int default_suggestion_index)
      : text_field_(text_field),
        selected_index_(default_suggestion_index),
        webview_(webview) {
    SetSuggestions(suggestions);
  }
  virtual ~AutocompletePopupMenuClient() {
  }

  virtual void valueChanged(unsigned listIndex, bool fireEvents = true) {
    text_field_->setValue(suggestions_[listIndex]);
  }  

  virtual WebCore::String itemText(unsigned list_index) const {
    return suggestions_[list_index];
  }
  
  virtual bool itemIsEnabled(unsigned listIndex) const {
    return true;
  }

  virtual PopupMenuStyle itemStyle(unsigned listIndex) const {
    return menuStyle();
  }

  virtual PopupMenuStyle menuStyle() const {
    RenderStyle* style = text_field_->renderStyle() ?
                            text_field_->renderStyle() :
                            text_field_->computedStyle();
    return PopupMenuStyle(style->color(), Color::white, style->font(),
                          style->visibility() == VISIBLE);
  }

  virtual int clientInsetLeft() const {
    return 0;
  }
  virtual int clientInsetRight() const {
    return 0;
  }
  virtual int clientPaddingLeft() const {
#if defined(OS_WIN)
    return theme()->popupInternalPaddingLeft(text_field_->computedStyle());
#else
    NOTIMPLEMENTED();
    return 0;
#endif
  }
  virtual int clientPaddingRight() const {
#if defined(OS_WIN)
    return theme()->popupInternalPaddingRight(text_field_->computedStyle());
#else
    NOTIMPLEMENTED();
    return 0;
#endif
  }
  virtual int listSize() const {
    return suggestions_.size();
  }
  virtual int selectedIndex() const {
    return selected_index_;
  }
  virtual void hidePopup() {
    webview_->HideAutoCompletePopup();
  }
  virtual bool itemIsSeparator(unsigned listIndex) const {
    return false;
  }
  virtual bool itemIsLabel(unsigned listIndex) const {
    return false;
  }
  virtual bool itemIsSelected(unsigned listIndex) const {
    return false;
  }
  virtual bool shouldPopOver() const {
    return false;
  }
  virtual bool valueShouldChangeOnHotTrack() const {
    return false;
  }

  virtual FontSelector* fontSelector() const {
    return text_field_->document()->styleSelector()->fontSelector();
  }

  virtual void setTextFromItem(unsigned listIndex) {
    text_field_->setValue(suggestions_[listIndex]);
  }

  virtual HostWindow* hostWindow() const {
    return text_field_->document()->view()->hostWindow();
  }

  virtual PassRefPtr<Scrollbar> createScrollbar(
      ScrollbarClient* client,
      ScrollbarOrientation orientation,
      ScrollbarControlSize size) {
    RefPtr<Scrollbar> widget = Scrollbar::createNativeScrollbar(client, 
                                                                orientation,
                                                                size);
    return widget.release();
  }

  void SetSuggestions(const std::vector<std::wstring>& suggestions) {
    suggestions_.clear();
    for (std::vector<std::wstring>::const_iterator iter = suggestions.begin();
         iter != suggestions.end(); ++iter) {
      suggestions_.push_back(webkit_glue::StdWStringToString(*iter));
    }
    // Try to preserve selection if possible.
    if (selected_index_ >= static_cast<int>(suggestions.size()))
      selected_index_ = -1;
  }

  WebCore::HTMLInputElement* text_field() const {
    return text_field_.get();
  }

 private:
  RefPtr<WebCore::HTMLInputElement> text_field_;
  std::vector<WebCore::String> suggestions_;
  int selected_index_;
  WebViewImpl* webview_;
};

static const WebCore::PopupContainerSettings kAutocompletePopupSettings = {
  false,  // focusOnShow
  false,  // setTextOnIndexChange
  false,  // acceptOnAbandon
  true,   // loopSelectionNavigation
};

// WebView ----------------------------------------------------------------

/*static*/
WebView* WebView::Create(WebViewDelegate* delegate,
                         const WebPreferences& prefs) {
  WebViewImpl* instance = new WebViewImpl();
  instance->AddRef();
  instance->SetPreferences(prefs);

  // Here, we construct a new WebFrameImpl with a reference count of 0.  That
  // is bumped up to 1 by InitMainFrame.  The reference count is decremented
  // when the corresponding WebCore::Frame object is destroyed.
  WebFrameImpl* main_frame = new WebFrameImpl();
  main_frame->InitMainFrame(instance);

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
    : ALLOW_THIS_IN_INITIALIZER_LIST(back_forward_list_client_impl_(this)),
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

  page_->backForwardList()->setClient(&back_forward_list_client_impl_);

  // The group name identifies a namespace of pages.  I'm not sure how it's
  // intended to be used, but keeping all pages in the same group works for us.
  page_->setGroupName("default");
}

WebViewImpl::~WebViewImpl() {
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
  if (!main_frame() || !main_frame()->frameview())
    return;

  last_mouse_position_.SetPoint(event.x, event.y);

  // We call mouseMoved here instead of handleMouseMovedEvent because we need
  // our ChromeClientImpl to receive changes to the mouse position and
  // tooltip text, and mouseMoved handles all of that.
  main_frame()->frame()->eventHandler()->mouseMoved(
      MakePlatformMouseEvent(main_frame()->frameview(), event));
}

void WebViewImpl::MouseLeave(const WebMouseEvent& event) {
  // This event gets sent as the main frame is closing.  In that case, just
  // ignore it.
  if (!main_frame() || !main_frame()->frameview())
    return;

  delegate_->UpdateTargetURL(this, GURL());

  main_frame()->frame()->eventHandler()->handleMouseMoveEvent(
      MakePlatformMouseEvent(main_frame()->frameview(), event));
}

void WebViewImpl::MouseDown(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

  last_mouse_down_point_ = gfx::Point(event.x, event.y);
  // We need to remember who has focus, as if the user left-clicks an already
  // focused text-field, we may want to show the auto-fill popup.
  RefPtr<Node> focused_node;
  if (event.button == WebMouseEvent::BUTTON_LEFT)
    focused_node = GetFocusedNode();

  main_frame()->frame()->eventHandler()->handleMousePressEvent(
      MakePlatformMouseEvent(main_frame()->frameview(), event));

  if (focused_node.get() && focused_node == GetFocusedNode()) {
    // Already focused node was clicked, ShowAutofillForNode will determine
    // whether to show the autofill (typically, if the node is a text-field and
    // is empty).
    static_cast<EditorClientImpl*>(page_->editorClient())->
        ShowAutofillForNode(focused_node.get());
  }
}

void WebViewImpl::MouseContextMenu(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

  page_->contextMenuController()->clearContextMenu();

  MakePlatformMouseEvent pme(main_frame()->frameview(), event);

  // Find the right target frame. See issue 1186900.
  IntPoint doc_point(
      page_->mainFrame()->view()->windowToContents(pme.pos()));
  HitTestResult result =
      page_->mainFrame()->eventHandler()->hitTestResultAtPoint(
          doc_point, false);
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
  if (!main_frame() || !main_frame()->frameview())
    return;

  MouseCaptureLost();
  main_frame()->frame()->eventHandler()->handleMouseReleaseEvent(
      MakePlatformMouseEvent(main_frame()->frameview(), event));

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (event.button == WebMouseEvent::BUTTON_RIGHT)
    MouseContextMenu(event);
}

void WebViewImpl::MouseWheel(const WebMouseWheelEvent& event) {
  MakePlatformWheelEvent platform_event(main_frame()->frameview(), event);
  main_frame()->frame()->eventHandler()->handleWheelEvent(platform_event);
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

  // Give autocomplete a chance to consume the key events it is interested in.
  if (autocomplete_popup_ &&
      autocomplete_popup_->isInterestedInEventForKey(event.key_code)) {
    if (autocomplete_popup_->handleKeyEvent(MakePlatformKeyboardEvent(event))) {
#if defined(OS_WIN)
      // We need to ignore the next CHAR event after this otherwise pressing
      // enter when selecting an item in the menu will go to the page.
      if (WebInputEvent::KEY_DOWN == event.type)
        suppress_next_keypress_event_ = true;
#endif
      return true;
    }
  }

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

#if !defined(OS_MACOSX)
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
#else
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

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (evt.isSystemKey())
    return handler->handleAccessKey(evt);

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
  return page_.get() ? page_->focusController()->focusedOrMainFrame() : NULL;
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

  if (page_.get()) {
    // Initiate shutdown for the entire frameset.  This will cause a lot of
    // notifications to be sent.
    if (page_->mainFrame())
      page_->mainFrame()->loader()->frameDetached();
    page_.reset();
  }
}

WebViewDelegate* WebViewImpl::GetDelegate() {
  return delegate_;
}

WebFrame* WebViewImpl::GetMainFrame() {
  return main_frame();
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
  Frame* frame = page_->mainFrame()->tree()->find(name_str);
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

  if (main_frame()->frameview()) {
    main_frame()->frameview()->resize(size_.width(), size_.height());
    main_frame()->frame()->eventHandler()->sendResizeEvent();
  }

  if (delegate_) {
    gfx::Rect damaged_rect(0, 0, size_.width(), size_.height());
    delegate_->DidInvalidateRect(this, damaged_rect);
  }
}

void WebViewImpl::Layout() {
  WebFrameImpl* webframe = main_frame();
  if (webframe) {
    // In order for our child HWNDs (NativeWindowWidgets) to update properly,
    // they need to be told that we are updating the screen.  The problem is
    // that the native widgets need to recalculate their clip region and not
    // overlap any of our non-native widgets.  To force the resizing, call
    // setFrameRect().  This will be a quick operation for most frames, but
    // the NativeWindowWidgets will update a proper clipping region.
    FrameView* view = webframe->frameview();
    if (view)
      view->setFrameRect(view->frameRect());

    // setFrameRect may have the side-effect of causing existing page
    // layout to be invalidated, so layout needs to be called last.

    webframe->Layout();
  }
}

void WebViewImpl::Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect) {
  WebFrameImpl* webframe = main_frame();
  if (webframe)
    webframe->Paint(canvas, rect);
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
  main_frame()->StopLoading();
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
    if (page_.get() && page_->mainFrame()) {
      Frame* frame = page_->mainFrame();
      if (!frame->selection()->isFocusedAndActive()) {
        // No one has focus yet, try to restore focus.
        RestoreFocus();
        page_->focusController()->setActive(true);
      }
      Frame* focused_frame = page_->focusController()->focusedOrMainFrame();
      frame->selection()->setFocused(frame == focused_frame);
    }
    ime_accept_events_ = true;
  } else {
    HideAutoCompletePopup();

    // Clear out who last had focus. If someone has focus, the refs will be
    // updated below.
    ReleaseFocusReferences();

    // Clear focus on the currently focused frame if any.
    if (!page_.get())
      return;

    Frame* frame = page_->mainFrame();
    if (!frame)
      return;

    RefPtr<Frame> focused = page_->focusController()->focusedFrame();
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
      page_->focusController()->setFocusedFrame(0);

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

bool WebViewImpl::ImeSetComposition(int string_type,
                                    int cursor_position,
                                    int target_start,
                                    int target_end,
                                    const std::wstring& ime_string) {
  Frame* focused = GetFocusedWebCoreFrame();
  if (!focused || !ime_accept_events_) {
    return false;
  }
  Editor* editor = focused->editor();
  if (!editor)
    return false;
  if (!editor->canEdit()) {
    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    if (!editor->hasComposition())
      return false;
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
    if (ime_string.length() > 0) {
      if (target_start < 0) target_start = 0;
      if (target_end < 0) target_end = static_cast<int>(ime_string.length());
      WebCore::String composition_string(
          webkit_glue::StdWStringToString(ime_string));
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
      underlines[2].endOffset = static_cast<int>(ime_string.length());
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

  return editor->hasComposition();
}

bool WebViewImpl::ImeUpdateStatus(bool* enable_ime,
                                  const void** new_node,
                                  gfx::Rect* caret_rect) {
  // Store whether the selected node needs IME and the caret rectangle.
  // This process consists of the following four steps:
  //  1. Retrieve the selection controller of the focused frame;
  //  2. Retrieve the caret rectangle from the controller;
  //  3. Convert the rectangle, which is relative to the parent view, to the
  //     one relative to the client window, and;
  //  4. Store the converted rectangle.
  const Frame* focused = GetFocusedWebCoreFrame();
  if (!focused)
    return false;
  const Editor* editor = focused->editor();
  if (!editor || !editor->canEdit())
    return false;
  SelectionController* controller = focused->selection();
  if (!controller)
    return false;
  const Node* node = controller->start().node();
  if (!node)
    return false;
  *enable_ime = node->shouldUseInputMethod() &&
      !controller->isInPasswordField();
  const IntRect rect(controller->absoluteCaretBounds());
  caret_rect->SetRect(rect.x(), rect.y(), rect.width(), rect.height());
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
    RefPtr<KeyboardEvent> webkit_event = KeyboardEvent::create(platform_event,
                                                               NULL);
    page()->focusController()->setInitialFocus(
        reverse ? WebCore::FocusDirectionBackward :
                  WebCore::FocusDirectionForward,
        webkit_event.get());
  }
}

// Releases references used to restore focus.
void WebViewImpl::ReleaseFocusReferences() {
  if (last_focused_frame_.get()) {
    last_focused_frame_.release();
    last_focused_node_.release();
  }
}

bool WebViewImpl::DownloadImage(int id, const GURL& image_url, int image_size) {
  if (!page_.get())
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

  // Turn this on to cause WebCore to paint the resize corner for us.
  settings->setShouldPaintCustomScrollbars(true);

#if defined(OS_WIN)
  // RenderTheme is a singleton that needs to know the default font size to
  // draw some form controls.  We let it know each time the size changes.
  WebCore::RenderThemeChromiumWin::setDefaultFontSize(preferences.default_font_size);
#endif
}

const WebPreferences& WebViewImpl::GetPreferences() {
  return webprefs_;
}

// Set the encoding of the current main frame to the one selected by
// a user in the encoding menu.
void WebViewImpl::SetPageEncoding(const std::wstring& encoding_name) {
  if (!page_.get())
    return;

  if (!encoding_name.empty()) {
    // only change override encoding, don't change default encoding
    // TODO(brettw) use std::string for encoding names.
    String new_encoding_name(webkit_glue::StdWStringToString(encoding_name));
    page_->mainFrame()->loader()->reloadWithOverrideEncoding(new_encoding_name);
  }
}

// Return the canonical encoding name of current main webframe in webview.
std::wstring WebViewImpl::GetMainFrameEncodingName() {
  if (!page_.get())
    return std::wstring();

  String encoding_name = page_->mainFrame()->loader()->encoding();
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
  if (!page_.get())
    return;

  IntPoint point = IntPoint(x, y);

  Frame* frame = page_->mainFrame();

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
  if (!page_.get())
    return;

  if (x == -1 || y == -1) {
    page_->inspectorController()->inspect(NULL);
  } else {
    IntPoint point = IntPoint(x, y);
    HitTestResult result(point);

    result = page_->mainFrame()->eventHandler()->hitTestResultAtPoint(point, false);

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
  page_->mainFrame()->eventHandler()->dragSourceEndedAt(pme, DragOperationCopy);
}

void WebViewImpl::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  PlatformMouseEvent pme(IntPoint(client_x, client_y),
                         IntPoint(screen_x, screen_y),
                         LeftButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  page_->mainFrame()->eventHandler()->dragSourceMovedTo(pme);
}

void WebViewImpl::DragSourceSystemDragEnded() {
  page_->dragController()->dragEnded();
  DCHECK(doing_drag_and_drop_);
  doing_drag_and_drop_ = false;
}

bool WebViewImpl::DragTargetDragEnter(const WebDropData& drop_data,
    int client_x, int client_y, int screen_x, int screen_y) {
  DCHECK(!current_drop_data_.get());

  current_drop_data_ = webkit_glue::WebDropDataToChromiumDataObject(drop_data);

  DragData drag_data(current_drop_data_.get(), IntPoint(client_x, client_y),
      IntPoint(screen_x, screen_y), kDropTargetOperation);
  DragOperation effect = page_->dragController()->dragEntered(&drag_data);
  return effect != DragOperationNone;
}

bool WebViewImpl::DragTargetDragOver(
    int client_x, int client_y, int screen_x, int screen_y) {
  DCHECK(current_drop_data_.get());
  DragData drag_data(current_drop_data_.get(), IntPoint(client_x, client_y),
      IntPoint(screen_x, screen_y), kDropTargetOperation);
  DragOperation effect = page_->dragController()->dragUpdated(&drag_data);
  return effect != DragOperationNone;
}

void WebViewImpl::DragTargetDragLeave() {
  DCHECK(current_drop_data_.get());
  DragData drag_data(current_drop_data_.get(), IntPoint(), IntPoint(),
                     DragOperationNone);
  page_->dragController()->dragExited(&drag_data);
  current_drop_data_ = NULL;
}

void WebViewImpl::DragTargetDrop(
    int client_x, int client_y, int screen_x, int screen_y) {
  DCHECK(current_drop_data_.get());
  DragData drag_data(current_drop_data_.get(), IntPoint(client_x, client_y),
      IntPoint(screen_x, screen_y), kDropTargetOperation);
  page_->dragController()->performDrag(&drag_data);
  current_drop_data_ = NULL;
}

SearchableFormData* WebViewImpl::CreateSearchableFormDataForFocusedNode() {
  if (!page_.get())
    return NULL;

  if (RefPtr<Frame> focused = page_->focusController()->focusedFrame()) {
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

void WebViewImpl::AutofillSuggestionsForNode(
      int64 node_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestion_index) {
  if (!page_.get() || suggestions.empty()) {
    HideAutoCompletePopup();
    return;
  }

  DCHECK(default_suggestion_index < static_cast<int>(suggestions.size()));

  if (RefPtr<Frame> focused = page_->focusController()->focusedFrame()) {
    RefPtr<Document> document = focused->document();
    if (!document.get()) {
      HideAutoCompletePopup();
      return;
    }

    RefPtr<Node> focused_node = document->focusedNode();
    // If the node for which we queried the autofill suggestions is not the
    // focused node, then we have nothing to do.
    // TODO(jcampan): also check the carret is at the end and that the text has
    // not changed.
    if (!focused_node.get() ||
        reinterpret_cast<int64>(focused_node.get()) != node_id) {
      HideAutoCompletePopup();
      return;
    }

    if (!focused_node->hasTagName(WebCore::HTMLNames::inputTag)) {
      NOTREACHED();
      return;
    }

    WebCore::HTMLInputElement* input_elem =
        static_cast<WebCore::HTMLInputElement*>(focused_node.get());
    if (!autocomplete_popup_client_.get() ||
        autocomplete_popup_client_->text_field() != input_elem) {
      autocomplete_popup_client_ =
          adoptRef(new AutocompletePopupMenuClient(this, input_elem,
                                                   suggestions,
                                                   default_suggestion_index));
      // The autocomplete popup is not activated.  We need the page to still
      // have focus so the user can keep typing when the popup is showing.
      autocomplete_popup_ =
          WebCore::PopupContainer::create(autocomplete_popup_client_.get(),
                                          kAutocompletePopupSettings);
      autocomplete_popup_->show(focused_node->getRect(), 
                                page_->mainFrame()->view(), 0);
    } else {
      // There is already a popup, reuse it.
      autocomplete_popup_client_->SetSuggestions(suggestions);
      IntRect old_bounds = autocomplete_popup_->boundsRect();
      autocomplete_popup_->refresh();
      IntRect new_bounds = autocomplete_popup_->boundsRect();
      // Let's resize the backing window if necessary.
      if (old_bounds != new_bounds) {
        WebWidgetImpl* web_widget =
            static_cast<WebWidgetImpl*>(autocomplete_popup_->client());
        web_widget->delegate()->SetWindowRect(
            web_widget, webkit_glue::FromIntRect(new_bounds));
      }
    }
  }
}

void WebViewImpl::DidCommitLoad(bool* is_new_navigation) {
  if (is_new_navigation)
    *is_new_navigation = observed_new_navigation_;

#ifndef NDEBUG
  DCHECK(!observed_new_navigation_ ||
    page_->mainFrame()->loader()->documentLoader() == new_navigation_loader_);
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

// TODO(darin): Figure out what to do with these methods.
#if 0
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

bool WebViewImpl::isHidden() {
  if (!delegate_)
    return true;

  return delegate_->IsHidden();
}
#endif

void WebViewImpl::SetCurrentHistoryItem(WebCore::HistoryItem* item) {
  back_forward_list_client_impl_.SetCurrentHistoryItem(item);
}

WebCore::HistoryItem* WebViewImpl::GetPreviousHistoryItem() {
  return back_forward_list_client_impl_.GetPreviousHistoryItem();
}

void WebViewImpl::ObserveNewNavigation() {
  observed_new_navigation_ = true;
#ifndef NDEBUG
  new_navigation_loader_ = page_->mainFrame()->loader()->documentLoader();
#endif
}

void WebViewImpl::DeleteImageResourceFetcher(ImageResourceFetcher* fetcher) {
  DCHECK(image_fetchers_.find(fetcher) != image_fetchers_.end());
  image_fetchers_.erase(fetcher);

  // We're in the callback from the ImageResourceFetcher, best to delay
  // deletion.
  MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
}

void WebViewImpl::HideAutoCompletePopup() {
  if (autocomplete_popup_) {
    autocomplete_popup_->hidePopup();
    autocomplete_popup_.clear();
    autocomplete_popup_client_.clear();
  }
}

Node* WebViewImpl::GetFocusedNode() {
  Frame* frame = page_->focusController()->focusedFrame();
  if (!frame)
    return NULL;

  Document* document = frame->document();
  if (!document)
    return NULL;

  return document->focusedNode();
}
