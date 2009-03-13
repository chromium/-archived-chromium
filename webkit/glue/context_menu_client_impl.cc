// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ContextMenu.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "Widget.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/context_menu_client_impl.h"

#include "base/string_util.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/webresponse.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/webview_impl.h"

#include "base/word_iterator.h"

namespace {

// Helper function to determine whether text is a single word or a sentence.
bool IsASingleWord(const std::wstring& text) {
  WordIterator iter(text, WordIterator::BREAK_WORD);
  int word_count = 0;
  if (!iter.Init()) return false;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_count++;
      if (word_count > 1) // More than one word.
        return false;
    }
  }

  // Check for 0 words.
  if (!word_count)
    return false;

  // Has a single word.
  return true;
}

// Helper function to get misspelled word on which context menu
// is to be evolked. This function also sets the word on which context menu
// has been evoked to be the selected word, as required.
std::wstring GetMisspelledWord(const WebCore::ContextMenu* default_menu,
                               WebCore::Frame* selected_frame) {
  std::wstring misspelled_word_string;

  // First select from selectedText to check for multiple word selection.
  misspelled_word_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
      false);

  // Don't provide suggestions for multiple words.
  if (!misspelled_word_string.empty() &&
      !IsASingleWord(misspelled_word_string))
    return L"";

  WebCore::HitTestResult hit_test_result = selected_frame->eventHandler()->
      hitTestResultAtPoint(default_menu->hitTestResult().point(), true);
  WebCore::Node* inner_node = hit_test_result.innerNode();
  WebCore::VisiblePosition pos(inner_node->renderer()->positionForPoint(
      hit_test_result.localPoint()));

  WebCore::VisibleSelection selection;
  if (pos.isNotNull()) {
    selection = WebCore::VisibleSelection(pos);
    selection.expandUsingGranularity(WebCore::WordGranularity);
  }

  if (selection.isRange()) {
    selected_frame->setSelectionGranularity(WebCore::WordGranularity);
  }

  if (selected_frame->shouldChangeSelection(selection))
    selected_frame->selection()->setSelection(selection);

  misspelled_word_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
                                      false);

  // If misspelled word is empty, then that portion should not be selected.
  // Set the selection to that position only, and do not expand.
  if (misspelled_word_string.empty()) {
    selection = WebCore::VisibleSelection(pos);
    selected_frame->selection()->setSelection(selection);
  }

  return misspelled_word_string;
}

}  // namespace

ContextMenuClientImpl::~ContextMenuClientImpl() {
}

void ContextMenuClientImpl::contextMenuDestroyed() {
  delete this;
}

// Figure out the URL of a page or subframe. Returns |page_type| as the type,
// which indicates page or subframe, or ContextNode::NONE if the URL could not
// be determined for some reason.
static ContextNode GetTypeAndURLFromFrame(WebCore::Frame* frame,
                                          GURL* url,
                                          ContextNode page_node) {
  ContextNode node;
  if (frame) {
    WebCore::DocumentLoader* dl = frame->loader()->documentLoader();
    if (dl) {
      WebDataSource* ds = WebDataSourceImpl::FromLoader(dl);
      if (ds) {
        node = page_node;
        *url = ds->HasUnreachableURL() ? ds->GetUnreachableURL()
                                       : ds->GetRequest().GetURL();
      }
    }
  }
  return node;
}

WebCore::PlatformMenuDescription
    ContextMenuClientImpl::getCustomMenuFromDefaultItems(
        WebCore::ContextMenu* default_menu) {
  // Displaying the context menu in this function is a big hack as we don't
  // have context, i.e. whether this is being invoked via a script or in
  // response to user input (Mouse event WM_RBUTTONDOWN,
  // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
  // in response to the above input events before popping up the context menu.
  if (!webview_->context_menu_allowed())
    return NULL;

  WebCore::HitTestResult r = default_menu->hitTestResult();
  WebCore::Frame* selected_frame = r.innerNonSharedNode()->document()->frame();

  WebCore::IntPoint menu_point =
      selected_frame->view()->contentsToWindow(r.point());

  ContextNode node;

  // Links, Images and Image-Links take preference over all else.
  WebCore::KURL link_url = r.absoluteLinkURL();
  if (!link_url.isEmpty()) {
    node.type |= ContextNode::LINK;
  }
  WebCore::KURL image_url = r.absoluteImageURL();
  if (!image_url.isEmpty()) {
    node.type |= ContextNode::IMAGE;
  }

  // If it's not a link, an image or an image link, show a selection menu or a
  // more generic page menu.
  std::wstring selection_text_string;
  std::wstring misspelled_word_string;
  GURL frame_url;
  GURL page_url;
  std::string security_info;

  std::wstring frame_encoding;
  // Send the frame and page URLs in any case.
  ContextNode frame_node = ContextNode(ContextNode::NONE);
  ContextNode page_node =
      GetTypeAndURLFromFrame(webview_->main_frame()->frame(),
                             &page_url,
                             ContextNode(ContextNode::PAGE));
  if (selected_frame != webview_->main_frame()->frame()) {
    frame_node = GetTypeAndURLFromFrame(selected_frame,
                                        &frame_url,
                                        ContextNode(ContextNode::FRAME));
    frame_encoding = webkit_glue::StringToStdWString(
        selected_frame->loader()->encoding());
  }

  if (r.isSelected()) {
    node.type |= ContextNode::SELECTION;
    selection_text_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
      false);
  }

  if (r.isContentEditable()) {
    node.type |= ContextNode::EDITABLE;
    if (webview_->GetFocusedWebCoreFrame()->editor()->
        isContinuousSpellCheckingEnabled()) {
      misspelled_word_string = GetMisspelledWord(default_menu,
                                                 selected_frame);
    }
  }

  if (node.type == ContextNode::NONE) {
    if (selected_frame != webview_->main_frame()->frame()) {
      node = frame_node;
    } else {
      node = page_node;
    }
  }

  // Now retrieve the security info.
  WebCore::DocumentLoader* dl = selected_frame->loader()->documentLoader();
  WebDataSource* ds = WebDataSourceImpl::FromLoader(dl);
  if (ds) {
    const WebResponse& response = ds->GetResponse();
    security_info = response.GetSecurityInfo();
  }

  int edit_flags = ContextNode::CAN_DO_NONE;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canUndo())
    edit_flags |= ContextNode::CAN_UNDO;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canRedo())
    edit_flags |= ContextNode::CAN_REDO;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canCut())
    edit_flags |= ContextNode::CAN_CUT;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canCopy())
    edit_flags |= ContextNode::CAN_COPY;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canPaste())
    edit_flags |= ContextNode::CAN_PASTE;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canDelete())
    edit_flags |= ContextNode::CAN_DELETE;
  // We can always select all...
  edit_flags |= ContextNode::CAN_SELECT_ALL;

  WebViewDelegate* d = webview_->delegate();
  if (d) {
    d->ShowContextMenu(webview_,
                       node,
                       menu_point.x(),
                       menu_point.y(),
                       webkit_glue::KURLToGURL(link_url),
                       webkit_glue::KURLToGURL(image_url),
                       page_url,
                       frame_url,
                       selection_text_string,
                       misspelled_word_string,
                       edit_flags,
                       security_info);
  }
  return NULL;
}

void ContextMenuClientImpl::contextMenuItemSelected(
    WebCore::ContextMenuItem*, const WebCore::ContextMenu*) {
}

void ContextMenuClientImpl::downloadURL(const WebCore::KURL&) {
}

void ContextMenuClientImpl::copyImageToClipboard(const WebCore::HitTestResult&) {
}

void ContextMenuClientImpl::searchWithGoogle(const WebCore::Frame*) {
}

void ContextMenuClientImpl::lookUpInDictionary(WebCore::Frame*) {
}

void ContextMenuClientImpl::speak(const WebCore::String&) {
}

void ContextMenuClientImpl::stopSpeaking() {
}

bool ContextMenuClientImpl::shouldIncludeInspectElementItem() {
    return false;  // TODO(jackson): Eventually include the inspector context menu item
}

#if defined(OS_MACOSX)
void ContextMenuClientImpl::searchWithSpotlight() {
  // TODO(pinkerton): write this
}
#endif
