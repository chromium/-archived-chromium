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
#include "CSSValueKeywords.h"
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
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginInfoStore.h"
#include "PopupMenuChromium.h"
#include "PopupMenuClient.h"
#if defined(OS_WIN)
#include "RenderThemeChromiumWin.h"
#else
#include "RenderTheme.h"
#endif
#include "RenderView.h"
#include "ResourceHandle.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/gfx/rect.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/glue/chrome_client_impl.h"
#include "webkit/glue/context_menu_client_impl.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/dragclient_impl.h"
#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/inspector_client_impl.h"
#include "webkit/glue/searchable_form_data.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webdevtoolsagent.h"
#include "webkit/glue/webdevtoolsclient.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webwidget_impl.h"

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;

using WebKit::WebDragData;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebSize;

using webkit_glue::ImageResourceFetcher;

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
static const double kTextSizeMultiplierRatio = 1.2;
static const double kMinTextSizeMultiplier = 0.5;
static const double kMaxTextSizeMultiplier = 3.0;

// The group name identifies a namespace of pages.  Page group is used on OSX
// for some programs that use HTML views to display things that don't seem like
// web pages to the user (so shouldn't have visited link coloring).  We only use
// one page group.
static const char* kPageGroupName = "default";

// The webcore drag operation type when something is trying to be dropped on
// the webview.  These values are taken from Apple's windows port.
static const WebCore::DragOperation kDropTargetOperation =
    static_cast<WebCore::DragOperation>(DragOperationCopy | DragOperationLink);

// AutocompletePopupMenuClient
class AutocompletePopupMenuClient : public WebCore::PopupMenuClient {
 public:
  AutocompletePopupMenuClient(WebViewImpl* webview) : text_field_(NULL),
                                                      selected_index_(0),
                                                      webview_(webview) {
  }

  void Init(WebCore::HTMLInputElement* text_field,
            const std::vector<std::wstring>& suggestions,
            int default_suggestion_index) {
    DCHECK(default_suggestion_index < static_cast<int>(suggestions.size()));
    text_field_ = text_field;
    selected_index_ = default_suggestion_index;
    SetSuggestions(suggestions);

    FontDescription font_description;
    webview_->theme()->systemFont(CSSValueWebkitControl, font_description);
    // Use a smaller font size to match IE/Firefox.
    // TODO(jcampan): http://crbug.com/7376 use the system size instead of a
    //                fixed font size value.
    font_description.setComputedSize(12.0);
    Font font(font_description, 0, 0);
    font.update(text_field->document()->styleSelector()->fontSelector());
    style_.reset(new PopupMenuStyle(Color::black, Color::white, font, true,
        Length(WebCore::Fixed), LTR));
  }

  virtual ~AutocompletePopupMenuClient() {
  }

  // WebCore::PopupMenuClient implementation.
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
    return *style_;
  }

  virtual PopupMenuStyle menuStyle() const {
    return *style_;
  }

  virtual int clientInsetLeft() const {
    return 0;
  }
  virtual int clientInsetRight() const {
    return 0;
  }
  virtual int clientPaddingLeft() const {
    // Bug http://crbug.com/7708 seems to indicate the style can be NULL.
    WebCore::RenderStyle* style = GetTextFieldStyle();
    return style ? webview_->theme()->popupInternalPaddingLeft(style) : 0;
  }
  virtual int clientPaddingRight() const {
    // Bug http://crbug.com/7708 seems to indicate the style can be NULL.
    WebCore::RenderStyle* style = GetTextFieldStyle();
    return style ? webview_->theme()->popupInternalPaddingRight(style) : 0;
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

  // AutocompletePopupMenuClient specific methods:
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

  void RemoveItemAtIndex(int index) {
    DCHECK(index >= 0 && index < static_cast<int>(suggestions_.size()));
    suggestions_.erase(suggestions_.begin() + index);
  }

  WebCore::HTMLInputElement* text_field() const {
    return text_field_.get();
  }

  WebCore::RenderStyle* GetTextFieldStyle() const {
    WebCore::RenderStyle* style = text_field_->computedStyle();
    if (!style) {
      // It seems we can only have an NULL style in a TextField if the node is
      // dettached, in which case we the popup shoud not be showing.
      NOTREACHED() << "Please report this in http://crbug.com/7708 and include "
                      "the page you were visiting.";
    }
    return style;
  }

 private:
  RefPtr<WebCore::HTMLInputElement> text_field_;
  std::vector<WebCore::String> suggestions_;
  int selected_index_;
  WebViewImpl* webview_;
  scoped_ptr<PopupMenuStyle> style_;
};

// Note that focusOnShow is false so that the autocomplete popup is shown not
// activated.  We need the page to still have focus so the user can keep typing
// while the popup is showing.
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

  // NOTE: The WebFrameImpl takes a reference to itself within InitMainFrame
  // and releases that reference once the corresponding Frame is destroyed.
  scoped_refptr<WebFrameImpl> main_frame = new WebFrameImpl();

  // Set the delegate before initializing the frame, so that notifications like
  // DidCreateDataSource make their way to the client.
  instance->delegate_ = delegate;
  main_frame->InitMainFrame(instance);

  WebDevToolsAgentDelegate* tools_delegate =
      delegate->GetWebDevToolsAgentDelegate();
  if (tools_delegate) {
    instance->devtools_agent_.reset(
      new WebDevToolsAgentImpl(instance, tools_delegate));
  }

  // Restrict the access to the local file system
  // (see WebView.mm WebView::_commonInitializationWithFrameName).
  FrameLoader::setLocalLoadPolicy(
      FrameLoader::AllowLocalLoadsForLocalOnly);
  return instance;
}

// static
void WebView::UpdateVisitedLinkState(uint64 link_hash) {
  WebCore::Page::visitedStateChanged(
      WebCore::PageGroup::pageGroup(kPageGroupName), link_hash);
}

// static
void WebView::ResetVisitedLinkState() {
  WebCore::Page::allVisitedStateChanged(
      WebCore::PageGroup::pageGroup(kPageGroupName));
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
      ignore_input_events_(false),
      suppress_next_keypress_event_(false),
      window_open_disposition_(IGNORE_ACTION),
      ime_accept_events_(true),
      drag_target_dispatch_(false),
      drag_identity_(0),
      drop_effect_(DROP_EFFECT_DEFAULT),
      drop_accept_(false),
      autocomplete_popup_showing_(false),
      is_transparent_(false) {
  // WebKit/win/WebView.cpp does the same thing, except they call the
  // KJS specific wrapper around this method. We need to have threading
  // initialized because CollatorICU requires it.
  WTF::initializeThreading();

  // set to impossible point so we always get the first mouse pos
  last_mouse_position_ = WebPoint(-1, -1);

  // the page will take ownership of the various clients
  page_.reset(new Page(new ChromeClientImpl(this),
                       new ContextMenuClientImpl(this),
                       new EditorClientImpl(this),
                       new DragClientImpl(this),
                       new WebInspectorClient(this)));

  page_->backForwardList()->setClient(&back_forward_list_client_impl_);
  page_->setGroupName(kPageGroupName);
}

WebViewImpl::~WebViewImpl() {
  DCHECK(page_ == NULL);
  for (std::set<ImageResourceFetcher*>::iterator i = image_fetchers_.begin();
       i != image_fetchers_.end(); ++i) {
    delete *i;
  }
}

RenderTheme* WebViewImpl::theme() const {
  return page_.get() ? page_->theme() : RenderTheme::defaultTheme().get();
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

  last_mouse_position_ = WebPoint(event.x, event.y);

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

  // If a text field that has focus is clicked again, we should display the
  // autocomplete popup.
  RefPtr<Node> clicked_node;
  if (event.button == WebMouseEvent::ButtonLeft) {
    RefPtr<Node> focused_node = GetFocusedNode();
    if (focused_node.get() &&
        webkit_glue::NodeToHTMLInputElement(focused_node.get())) {
      IntPoint point(event.x, event.y);
      HitTestResult result(point);
      result = page_->mainFrame()->eventHandler()->hitTestResultAtPoint(point,
                                                                        false);
      if (result.innerNonSharedNode() == focused_node) {
        // Already focused text field was clicked, let's remember this.  If
        // focus has not changed after the mouse event is processed, we'll
        // trigger the autocomplete.
        clicked_node = focused_node;
      }
    }
  }

  main_frame()->frame()->eventHandler()->handleMousePressEvent(
      MakePlatformMouseEvent(main_frame()->frameview(), event));

  if (clicked_node.get() && clicked_node == GetFocusedNode()) {
    // Focus has not changed, show the autocomplete popup.
    static_cast<EditorClientImpl*>(page_->editorClient())->
        ShowFormAutofillForNode(clicked_node.get());
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  // On Windows, we handle it on mouse up, not down.
#if defined(OS_MACOSX)
  if (event.button == WebMouseEvent::ButtonRight ||
      (event.button == WebMouseEvent::ButtonLeft &&
       event.modifiers & WebMouseEvent::ControlKey)) {
    MouseContextMenu(event);
  }
#elif defined(OS_LINUX)
  if (event.button == WebMouseEvent::ButtonRight)
    MouseContextMenu(event);
#endif
}

void WebViewImpl::MouseContextMenu(const WebMouseEvent& event) {
  if (!main_frame() || !main_frame()->frameview())
    return;

  page_->contextMenuController()->clearContextMenu();

  MakePlatformMouseEvent pme(main_frame()->frameview(), event);

  // Find the right target frame. See issue 1186900.
  HitTestResult result = HitTestResultForWindowPos(pme.pos());
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

#if defined(OS_WIN)
  // Dispatch the contextmenu event regardless of if the click was swallowed.
  // On Mac/Linux, we handle it on mouse down, not up.
  if (event.button == WebMouseEvent::ButtonRight)
    MouseContextMenu(event);
#endif

#if defined(OS_LINUX)
  // If the event was a middle click, attempt to copy text into the focused
  // frame.
  //
  // This code is in the mouse up handler. There is some debate about putting
  // this here, as opposed to the mouse down handler.
  //   xterm: pastes on up.
  //   GTK: pastes on down.
  //   Firefox: pastes on up.
  //   Midori: couldn't paste at all with 0.1.2
  //
  // There is something of a webcompat angle to this well, as highlighted by
  // crbug.com/14608. Pages can clear text boxes 'onclick' and, if we paste on
  // down then the text is pasted just before the onclick handler runs and
  // clears the text box. So it's important this happens after the
  // handleMouseReleaseEvent() earlier in this function
  if (event.button == WebMouseEvent::ButtonMiddle) {
    Frame* focused = GetFocusedWebCoreFrame();
    if (!focused)
      return;
    Editor* editor = focused->editor();
    if (!editor || !editor->canEdit())
      return;

    delegate_->PasteFromSelectionClipboard();
  }
#endif
}

void WebViewImpl::MouseWheel(const WebMouseWheelEvent& event) {
  MakePlatformWheelEvent platform_event(main_frame()->frameview(), event);
  main_frame()->frame()->eventHandler()->handleWheelEvent(platform_event);
}

bool WebViewImpl::KeyEvent(const WebKeyboardEvent& event) {
  DCHECK((event.type == WebInputEvent::RawKeyDown) ||
         (event.type == WebInputEvent::KeyDown) ||
         (event.type == WebInputEvent::KeyUp));

  // Please refer to the comments explaining the suppress_next_keypress_event_
  // member.
  // The suppress_next_keypress_event_ is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  // Give autocomplete a chance to consume the key events it is interested in.
  if (AutocompleteHandleKeyEvent(event))
    return true;

  Frame* frame = GetFocusedWebCoreFrame();
  if (!frame)
    return false;

  EventHandler* handler = frame->eventHandler();
  if (!handler)
    return KeyEventDefault(event);

#if defined(OS_WIN)
  // TODO(pinkerton): figure out these keycodes on non-windows
  if (((event.modifiers == 0) && (event.windowsKeyCode == VK_APPS)) ||
      ((event.modifiers == WebInputEvent::ShiftKey) &&
       (event.windowsKeyCode == VK_F10))) {
    SendContextMenuEvent(event);
    return true;
  }
#endif

  MakePlatformKeyboardEvent evt(event);

  if (WebInputEvent::RawKeyDown == event.type) {
    if (handler->keyEvent(evt) && !evt.isSystemKey()) {
      suppress_next_keypress_event_ = true;
      return true;
    }
  } else {
    if (handler->keyEvent(evt)) {
      return true;
    }
  }

  return KeyEventDefault(event);
}

bool WebViewImpl::AutocompleteHandleKeyEvent(const WebKeyboardEvent& event) {
  if (!autocomplete_popup_showing_ ||
      // Home and End should be left to the text field to process.
      event.windowsKeyCode == base::VKEY_HOME ||
      event.windowsKeyCode == base::VKEY_END) {
    return false;
  }

  // Pressing delete triggers the removal of the selected suggestion from the
  // DB.
  if (event.windowsKeyCode == base::VKEY_DELETE &&
      autocomplete_popup_->selectedIndex() != -1) {
    Node* node = GetFocusedNode();
    if (!node || (node->nodeType() != WebCore::Node::ELEMENT_NODE)) {
      NOTREACHED();
      return false;
    }
    WebCore::Element* element = static_cast<WebCore::Element*>(node);
    if (!element->hasLocalName(WebCore::HTMLNames::inputTag)) {
      NOTREACHED();
      return false;
    }

    int selected_index = autocomplete_popup_->selectedIndex();
    WebCore::HTMLInputElement* input_element =
        static_cast<WebCore::HTMLInputElement*>(element);
    std::wstring name = webkit_glue::StringToStdWString(input_element->name());
    std::wstring value = webkit_glue::StringToStdWString(
        autocomplete_popup_client_->itemText(selected_index ));
    delegate()->RemoveStoredAutofillEntry(name, value);
    // Update the entries in the currently showing popup to reflect the
    // deletion.
    autocomplete_popup_client_->RemoveItemAtIndex(selected_index);
    RefreshAutofillPopup();
    return false;
  }

  if (!autocomplete_popup_->isInterestedInEventForKey(event.windowsKeyCode))
    return false;

  if (autocomplete_popup_->handleKeyEvent(MakePlatformKeyboardEvent(event))) {
#if defined(OS_WIN)
      // We need to ignore the next Char event after this otherwise pressing
      // enter when selecting an item in the menu will go to the page.
      if (WebInputEvent::RawKeyDown == event.type)
        suppress_next_keypress_event_ = true;
#endif
      return true;
    }

  return false;
}

bool WebViewImpl::CharEvent(const WebKeyboardEvent& event) {
  DCHECK(event.type == WebInputEvent::Char);

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

    RefPtr<Range> selection = main_frame->selection()->toNormalizedRange();
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
  mouse_event.button = WebMouseEvent::ButtonRight;
  mouse_event.x = coords.x();
  mouse_event.y = coords.y();
  mouse_event.type = WebInputEvent::MouseUp;

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
    case WebInputEvent::Char: {
      if (event.windowsKeyCode == VKEY_SPACE) {
        int key_code = ((event.modifiers & WebInputEvent::ShiftKey) ?
                         VKEY_PRIOR : VKEY_NEXT);
        return ScrollViewWithKeyboard(key_code);
      }
      break;
    }

#if defined(OS_WIN) || defined(OS_LINUX)
    case WebInputEvent::RawKeyDown: {
#else
    case WebInputEvent::KeyDown: {
#endif
      if (event.modifiers == WebInputEvent::ControlKey) {
        switch (event.windowsKeyCode) {
          case 'A':
            GetFocusedFrame()->SelectAll();
            return true;
          case VKEY_INSERT:
          case 'C':
            GetFocusedFrame()->Copy();
            return true;
          // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
          // key combinations which affect scrolling. Safari is buggy in the
          // sense that it scrolls the page for all Ctrl+scrolling key
          // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
          case VKEY_HOME:
          case VKEY_END:
            break;
          default:
            return false;
        }
      }
      if (!event.isSystemKey) {
        return ScrollViewWithKeyboard(event.windowsKeyCode);
      }
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

  switch (key_code) {
    case VKEY_LEFT:
      scroll_direction = ScrollLeft;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_RIGHT:
      scroll_direction = ScrollRight;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_UP:
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_DOWN:
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByLine;
      break;
    case VKEY_HOME:
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByDocument;
      break;
    case VKEY_END:
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByDocument;
      break;
    case VKEY_PRIOR:  // page up
      scroll_direction = ScrollUp;
      scroll_granularity = ScrollByPage;
      break;
    case VKEY_NEXT:  // page down
      scroll_direction = ScrollDown;
      scroll_granularity = ScrollByPage;
      break;
    default:
      return false;
  }

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
  return WebFrameImpl::FromFrame(page->mainFrame())->GetWebViewImpl();
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
  if (page_.get()) {
    // Initiate shutdown for the entire frameset.  This will cause a lot of
    // notifications to be sent.
    if (page_->mainFrame())
      page_->mainFrame()->loader()->frameDetached();
    page_.reset();
  }

  // Should happen after page_.reset().
  if (devtools_agent_.get())
    devtools_agent_.reset(NULL);

  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  delegate_ = NULL;

  Release();  // Balances AddRef from WebView::Create
}

WebViewDelegate* WebViewImpl::GetDelegate() {
  return delegate_;
}

void WebViewImpl::SetDelegate(WebViewDelegate* delegate) {
  delegate_ = delegate;
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

void WebViewImpl::Resize(const WebSize& new_size) {
  if (size_ == new_size)
    return;
  size_ = new_size;

  if (main_frame()->frameview()) {
    main_frame()->frameview()->resize(size_.width, size_.height);
    main_frame()->frame()->eventHandler()->sendResizeEvent();
  }

  if (delegate_) {
    WebRect damaged_rect(0, 0, size_.width, size_.height);
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

void WebViewImpl::Paint(skia::PlatformCanvas* canvas, const WebRect& rect) {
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

  if (ignore_input_events_)
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
    case WebInputEvent::MouseMove:
      MouseMove(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::MouseLeave:
      MouseLeave(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::MouseWheel:
      MouseWheel(*static_cast<const WebMouseWheelEvent*>(input_event));
      break;

    case WebInputEvent::MouseDown:
      MouseDown(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::MouseUp:
      MouseUp(*static_cast<const WebMouseEvent*>(input_event));
      break;

    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      handled = KeyEvent(*static_cast<const WebKeyboardEvent*>(input_event));
      break;

    case WebInputEvent::Char:
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

void WebViewImpl::ClearFocusedNode() {
  if (!page_.get())
    return;

  RefPtr<Frame> frame = page_->mainFrame();
  if (!frame.get())
    return;

  RefPtr<Document> document = frame->document();
  if (!document.get())
    return;

  RefPtr<Node> old_focused_node = document->focusedNode();

  // Clear the focused node.
  document->setFocusedNode(NULL);

  if (!old_focused_node.get())
    return;

  // If a text field has focus, we need to make sure the selection controller
  // knows to remove selection from it. Otherwise, the text field is still
  // processing keyboard events even though focus has been moved to the page and
  // keystrokes get eaten as a result.
  if (old_focused_node->hasTagName(HTMLNames::textareaTag) ||
      (old_focused_node->hasTagName(HTMLNames::inputTag) &&
       static_cast<HTMLInputElement*>(old_focused_node.get())->isTextField())) {
    // Clear the selection.
    SelectionController* selection = frame->selection();
    selection->clear();
  }
}

void WebViewImpl::SetFocus(bool enable) {
  page_->focusController()->setFocused(enable);
  if (enable) {
    // Note that we don't call setActive() when disabled as this cause extra
    // focus/blur events to be dispatched.
    page_->focusController()->setActive(true);
    ime_accept_events_ = true;
  } else {
    HideAutoCompletePopup();

    // Clear focus on the currently focused frame if any.
    if (!page_.get())
      return;

    Frame* frame = page_->mainFrame();
    if (!frame)
      return;

    RefPtr<Frame> focused_frame = page_->focusController()->focusedFrame();
    if (focused_frame.get()) {
      // Finish an ongoing composition to delete the composition node.
      Editor* editor = focused_frame->editor();
      if (editor && editor->hasComposition())
        editor->confirmComposition();
      ime_accept_events_ = false;
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

  // We should verify the parent node of this IME composition node are
  // editable because JavaScript may delete a parent node of the composition
  // node. In this case, WebKit crashes while deleting texts from the parent
  // node, which doesn't exist any longer.
  PassRefPtr<Range> range = editor->compositionRange();
  if (range) {
    const Node* node = range->startPosition().node();
    if (!node || !node->isContentEditable())
      return false;
  }

  if (string_type == -1) {
    // A browser process sent an IPC message which does not contain a valid
    // string, which means an ongoing composition has been canceled.
    // If the ongoing composition has been canceled, replace the ongoing
    // composition string with an empty string and complete it.
    WebCore::String empty_string;
    WTF::Vector<WebCore::CompositionUnderline> empty_underlines;
    editor->setComposition(empty_string, empty_underlines, 0, 0);
  } else {
    // A browser process sent an IPC message which contains a string to be
    // displayed in this Editor object.
    // To display the given string, set the given string to the
    // m_compositionNode member of this Editor object and display it.
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
    // The given string is a result string, which means the ongoing
    // composition has been completed. I have to call the
    // Editor::confirmCompletion() and complete this composition.
    if (string_type == 1) {
      editor->confirmComposition();
    }
  }

  return editor->hasComposition();
}

bool WebViewImpl::ImeUpdateStatus(bool* enable_ime,
                                  WebRect* caret_rect) {
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
  const FrameView* view = node->document()->view();
  if (!view)
    return false;

  *caret_rect = webkit_glue::IntRectToWebRect(
      view->contentsToWindow(controller->absoluteCaretBounds()));
  return true;
}

void WebViewImpl::SetTextDirection(WebTextDirection direction) {
  // The Editor::setBaseWritingDirection() function checks if we can change
  // the text direction of the selected node and updates its DOM "dir"
  // attribute and its CSS "direction" property.
  // So, we just call the function as Safari does.
  const Frame* focused = GetFocusedWebCoreFrame();
  if (!focused)
    return;

  Editor* editor = focused->editor();
  if (!editor || !editor->canEdit())
    return;

  switch (direction) {
    case WEB_TEXT_DIRECTION_DEFAULT:
      editor->setBaseWritingDirection(WebCore::NaturalWritingDirection);
      break;

    case WEB_TEXT_DIRECTION_LTR:
      editor->setBaseWritingDirection(WebCore::LeftToRightWritingDirection);
      break;

    case WEB_TEXT_DIRECTION_RTL:
      editor->setBaseWritingDirection(WebCore::RightToLeftWritingDirection);
      break;

    default:
      NOTIMPLEMENTED();
      break;
  }
}

void WebViewImpl::SetInitialFocus(bool reverse) {
  if (page_.get()) {
    // Since we don't have a keyboard event, we'll create one.
    WebKeyboardEvent keyboard_event;
    keyboard_event.type = WebInputEvent::RawKeyDown;
    if (reverse)
      keyboard_event.modifiers = WebInputEvent::ShiftKey;
    // VK_TAB which is only defined on Windows.
    keyboard_event.windowsKeyCode = 0x09;
    MakePlatformKeyboardEvent platform_event(keyboard_event);
    RefPtr<KeyboardEvent> webkit_event =
        KeyboardEvent::create(platform_event, NULL);
    page()->focusController()->setInitialFocus(
        reverse ? WebCore::FocusDirectionBackward :
                  WebCore::FocusDirectionForward,
        webkit_event.get());
  }
}

bool WebViewImpl::DownloadImage(int id, const GURL& image_url,
                                int image_size) {
  if (!page_.get())
    return false;
  image_fetchers_.insert(new ImageResourceFetcher(
      image_url, main_frame(), id, image_size,
      NewCallback(this, &WebViewImpl::OnImageFetchComplete)));
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
  settings->setWebSecurityEnabled(preferences.web_security_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
    preferences.javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(
    preferences.loads_images_automatically);
  settings->setPluginsEnabled(preferences.plugins_enabled);
  settings->setDOMPasteAllowed(preferences.dom_paste_enabled);
  settings->setDeveloperExtrasEnabled(preferences.developer_extras_enabled);
  settings->setShrinksStandaloneImagesToFit(
    preferences.shrinks_standalone_images_to_fit);
  settings->setUsesEncodingDetector(preferences.uses_universal_detector);
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
  settings->setDownloadableBinaryFontsEnabled(preferences.remote_fonts_enabled);
  settings->setXSSAuditorEnabled(preferences.xss_auditor_enabled);

  // This setting affects the behavior of links in an editable region:
  // clicking the link should select it rather than navigate to it.
  // Safari uses the same default. It is unlikley an embedder would want to
  // change this, since it would break existing rich text editors.
  settings->setEditableLinkBehavior(WebCore::EditableLinkNeverLive);

  settings->setFontRenderingMode(NormalRenderingMode);
  settings->setJavaEnabled(preferences.java_enabled);

  // Turn this on to cause WebCore to paint the resize corner for us.
  settings->setShouldPaintCustomScrollbars(true);

  // Mitigate attacks from local HTML files by not granting file:// URLs
  // universal access.
  settings->setAllowUniversalAccessFromFileURLs(false);

  // We prevent WebKit from checking if it needs to add a "text direction"
  // submenu to a context menu. it is not only because we don't need the result
  // but also because it cause a possible crash in Editor::hasBidiSelection().
  settings->setTextDirectionSubmenuInclusionBehavior(
      TextDirectionSubmenuNeverIncluded);

#if defined(OS_WIN)
  // RenderTheme is a singleton that needs to know the default font size to
  // draw some form controls.  We let it know each time the size changes.
  WebCore::RenderThemeChromiumWin::setDefaultFontSize(
      preferences.default_font_size);
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

  HitTestResult result = HitTestResultForWindowPos(IntPoint(x, y));

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

  page_->mainFrame()->editor()->copyImage(result);
}

void WebViewImpl::InspectElement(int x, int y) {
  if (!page_.get())
    return;

  if (x == -1 || y == -1) {
    page_->inspectorController()->inspect(NULL);
  } else {
    HitTestResult result = HitTestResultForWindowPos(IntPoint(x, y));

    if (!result.innerNonSharedNode())
      return;

    page_->inspectorController()->inspect(result.innerNonSharedNode());
  }
}

void WebViewImpl::ShowJavaScriptConsole() {
  page_->inspectorController()->showPanel(InspectorController::ConsolePanel);
}

void WebViewImpl::DragSourceEndedAt(
    const WebPoint& client_point,
    const WebPoint& screen_point) {
  PlatformMouseEvent pme(webkit_glue::WebPointToIntPoint(client_point),
                         webkit_glue::WebPointToIntPoint(screen_point),
                         NoButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  page_->mainFrame()->eventHandler()->dragSourceEndedAt(pme, DragOperationCopy);
}

void WebViewImpl::DragSourceMovedTo(
    const WebPoint& client_point,
    const WebPoint& screen_point) {
  PlatformMouseEvent pme(webkit_glue::WebPointToIntPoint(client_point),
                         webkit_glue::WebPointToIntPoint(screen_point),
                         LeftButton, MouseEventMoved, 0, false, false, false,
                         false, 0);
  page_->mainFrame()->eventHandler()->dragSourceMovedTo(pme);
}

void WebViewImpl::DragSourceSystemDragEnded() {
  page_->dragController()->dragEnded();
  DCHECK(doing_drag_and_drop_);
  doing_drag_and_drop_ = false;
}

bool WebViewImpl::DragTargetDragEnter(
    const WebDragData& web_drag_data,
    int identity,
    const WebPoint& client_point,
    const WebPoint& screen_point) {
  DCHECK(!current_drag_data_.get());

  current_drag_data_ =
      webkit_glue::WebDragDataToChromiumDataObject(web_drag_data);
  drag_identity_ = identity;

  DragData drag_data(
      current_drag_data_.get(),
      webkit_glue::WebPointToIntPoint(client_point),
      webkit_glue::WebPointToIntPoint(screen_point),
      kDropTargetOperation);

  drop_effect_ = DROP_EFFECT_DEFAULT;
  drag_target_dispatch_ = true;
  DragOperation effect = page_->dragController()->dragEntered(&drag_data);
  drag_target_dispatch_ = false;

  if (drop_effect_ != DROP_EFFECT_DEFAULT)
    return drop_accept_ = (drop_effect_ != DROP_EFFECT_NONE);
  return drop_accept_ = (effect != DragOperationNone);
}

bool WebViewImpl::DragTargetDragOver(
    const WebPoint& client_point,
    const WebPoint& screen_point) {
  DCHECK(current_drag_data_.get());

  DragData drag_data(
      current_drag_data_.get(),
      webkit_glue::WebPointToIntPoint(client_point),
      webkit_glue::WebPointToIntPoint(screen_point),
      kDropTargetOperation);

  drop_effect_ = DROP_EFFECT_DEFAULT;
  drag_target_dispatch_ = true;
  DragOperation effect = page_->dragController()->dragUpdated(&drag_data);
  drag_target_dispatch_ = false;

  if (drop_effect_ != DROP_EFFECT_DEFAULT)
    return drop_accept_ = (drop_effect_ != DROP_EFFECT_NONE);
  return drop_accept_ = (effect != DragOperationNone);
}

void WebViewImpl::DragTargetDragLeave() {
  DCHECK(current_drag_data_.get());

  DragData drag_data(
      current_drag_data_.get(),
      IntPoint(),
      IntPoint(),
      kDropTargetOperation);

  drag_target_dispatch_ = true;
  page_->dragController()->dragExited(&drag_data);
  drag_target_dispatch_ = false;

  current_drag_data_ = NULL;
  drop_effect_ = DROP_EFFECT_DEFAULT;
  drop_accept_ = false;
  drag_identity_ = 0;
}

void WebViewImpl::DragTargetDrop(
    const WebPoint& client_point,
    const WebPoint& screen_point) {
  DCHECK(current_drag_data_.get());

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview drop_accept_ state is true.

  if (!drop_accept_) {  // IPC RACE CONDITION: do not allow this drop.
    DragTargetDragLeave();
    return;
  }

  DragData drag_data(
      current_drag_data_.get(),
      webkit_glue::WebPointToIntPoint(client_point),
      webkit_glue::WebPointToIntPoint(screen_point),
      kDropTargetOperation);

  drag_target_dispatch_ = true;
  page_->dragController()->performDrag(&drag_data);
  drag_target_dispatch_ = false;

  current_drag_data_ = NULL;
  drop_effect_ = DROP_EFFECT_DEFAULT;
  drop_accept_ = false;
  drag_identity_ = 0;
}

int32 WebViewImpl::GetDragIdentity() {
  if (drag_target_dispatch_)
    return drag_identity_;
  return 0;
}

bool WebViewImpl::SetDropEffect(bool accept) {
  if (drag_target_dispatch_) {
    drop_effect_ = accept ? DROP_EFFECT_COPY : DROP_EFFECT_NONE;
    return true;
  } else {
    return false;
  }
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

    // The first time the autocomplete is shown we'll create the client and the
    // popup.
    if (!autocomplete_popup_client_.get())
      autocomplete_popup_client_.reset(new AutocompletePopupMenuClient(this));
    autocomplete_popup_client_->Init(input_elem,
                                     suggestions,
                                     default_suggestion_index);
    if (!autocomplete_popup_.get()) {
      autocomplete_popup_ =
          WebCore::PopupContainer::create(autocomplete_popup_client_.get(),
                                          kAutocompletePopupSettings);
    }

    if (autocomplete_popup_showing_) {
      autocomplete_popup_client_->SetSuggestions(suggestions);
      RefreshAutofillPopup();
    } else {
      autocomplete_popup_->show(focused_node->getRect(),
                                focused_node->ownerDocument()->view(), 0);
      autocomplete_popup_showing_ = true;
    }
  }
}

WebDevToolsAgent* WebViewImpl::GetWebDevToolsAgent() {
  return GetWebDevToolsAgentImpl();
}

WebDevToolsAgentImpl* WebViewImpl::GetWebDevToolsAgentImpl() {
  return devtools_agent_.get();
}

void WebViewImpl::SetIsTransparent(bool is_transparent) {
  // Set any existing frames to be transparent.
  WebCore::Frame* frame = page_->mainFrame();
  while (frame) {
    frame->view()->setTransparent(is_transparent);
    frame = frame->tree()->traverseNext();
  }

  // Future frames check this to know whether to be transparent.
  is_transparent_ = is_transparent;
}

bool WebViewImpl::GetIsTransparent() const {
  return is_transparent_;
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

void WebViewImpl::StartDragging(const WebDragData& drag_data) {
  if (delegate_) {
    DCHECK(!doing_drag_and_drop_);
    doing_drag_and_drop_ = true;
    delegate_->StartDragging(this, drag_data);
  }
}

void WebViewImpl::OnImageFetchComplete(ImageResourceFetcher* fetcher,
                                       const SkBitmap& image) {
  if (delegate_) {
    delegate_->DidDownloadImage(fetcher->id(), fetcher->image_url(),
                                image.isNull(), image);
  }
  DeleteImageResourceFetcher(fetcher);
}

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
  if (autocomplete_popup_showing_) {
    autocomplete_popup_->hidePopup();
    autocomplete_popup_showing_ = false;
  }
}

void WebViewImpl::SetIgnoreInputEvents(bool new_value) {
  DCHECK(ignore_input_events_ != new_value);
  ignore_input_events_ = new_value;
}

WebCore::Node* WebViewImpl::GetNodeForWindowPos(int x, int y) {
  HitTestResult result = HitTestResultForWindowPos(IntPoint(x, y));
  return result.innerNonSharedNode();
}

void WebViewImpl::HideAutofillPopup() {
  HideAutoCompletePopup();
}

void WebViewImpl::RefreshAutofillPopup() {
  DCHECK(autocomplete_popup_showing_);

  // Hide the popup if it has become empty.
  if (autocomplete_popup_client_->listSize() == 0) {
    HideAutoCompletePopup();
    return;
  }

  IntRect old_bounds = autocomplete_popup_->boundsRect();
  autocomplete_popup_->refresh();
  IntRect new_bounds = autocomplete_popup_->boundsRect();
  // Let's resize the backing window if necessary.
  if (old_bounds != new_bounds) {
    WebWidgetImpl* web_widget =
        static_cast<WebWidgetImpl*>(autocomplete_popup_->client());
    web_widget->delegate()->SetWindowRect(
        web_widget, webkit_glue::IntRectToWebRect(new_bounds));
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

HitTestResult WebViewImpl::HitTestResultForWindowPos(const IntPoint& pos) {
  IntPoint doc_point(
      page_->mainFrame()->view()->windowToContents(pos));
  return page_->mainFrame()->eventHandler()->
      hitTestResultAtPoint(doc_point, false);
}
