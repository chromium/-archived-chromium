// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>
#include <shlobj.h>

#include "chrome/browser/web_drop_target.h"

#include "base/clipboard_util.h"
#include "base/gfx/point.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/os_exchange_data.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// A helper method for getting the preferred drop effect.
DWORD GetPreferredDropEffect(DWORD effect) {
  if (effect & DROPEFFECT_COPY)
    return DROPEFFECT_COPY;
  if (effect & DROPEFFECT_LINK)
    return DROPEFFECT_LINK;
  if (effect & DROPEFFECT_MOVE)
    return DROPEFFECT_MOVE;
  return DROPEFFECT_NONE;
}

}  // anonymous namespace

// InterstitialDropTarget is like a BaseDropTarget implementation that
// WebDropTarget passes through to if an interstitial is showing.  Rather than
// passing messages on to the renderer, we just check to see if there's a link
// in the drop data and handle links as navigations.
class InterstitialDropTarget {
 public:
  explicit InterstitialDropTarget(WebContents* web_contents)
      : web_contents_(web_contents) {}

  DWORD OnDragEnter(IDataObject* data_object, DWORD effect) {
    return ClipboardUtil::HasUrl(data_object) ? GetPreferredDropEffect(effect)
                                              : DROPEFFECT_NONE;
  }

  DWORD OnDragOver(IDataObject* data_object, DWORD effect) {
    return ClipboardUtil::HasUrl(data_object) ? GetPreferredDropEffect(effect)
                                              : DROPEFFECT_NONE;
  }

  void OnDragLeave(IDataObject* data_object) {
  }

  DWORD OnDrop(IDataObject* data_object, DWORD effect) {
    if (ClipboardUtil::HasUrl(data_object)) {
      std::wstring url;
      std::wstring title;
      ClipboardUtil::GetUrl(data_object, &url, &title);
      web_contents_->OpenURL(GURL(url), CURRENT_TAB,
                             PageTransition::AUTO_BOOKMARK);
      return GetPreferredDropEffect(effect);
    }
    return DROPEFFECT_NONE;
  }

 private:
  WebContents* web_contents_;

  DISALLOW_EVIL_CONSTRUCTORS(InterstitialDropTarget);
};

///////////////////////////////////////////////////////////////////////////////
// WebDropTarget, public:

WebDropTarget::WebDropTarget(HWND source_hwnd, WebContents* web_contents)
    : BaseDropTarget(source_hwnd),
      web_contents_(web_contents),
      is_drop_target_(false),
      interstitial_drop_target_(new InterstitialDropTarget(web_contents)) {
}

WebDropTarget::~WebDropTarget() {
}

DWORD WebDropTarget::OnDragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  // Don't pass messages to the renderer if an interstitial page is showing
  // because we don't want the interstitial page to navigate.  Instead,
  // pass the messages on to a separate interstitial DropTarget handler.
  if (web_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDragEnter(data_object, effect);

  // TODO(tc): PopulateWebDropData is kind of slow, maybe we can do this in a
  // background thread.
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  if (drop_data.url.is_empty())
    OSExchangeData::GetPlainTextURL(data_object, &drop_data.url);

  is_drop_target_ = true;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  web_contents_->DragTargetDragEnter(drop_data,
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  // We lie here and always return a DROPEFFECT because we don't want to
  // wait for the IPC call to return.
  return GetPreferredDropEffect(effect);
}

DWORD WebDropTarget::OnDragOver(IDataObject* data_object,
                                DWORD key_state,
                                POINT cursor_position,
                                DWORD effect) {
  if (web_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDragOver(data_object, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  web_contents_->DragTargetDragOver(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  if (!is_drop_target_)
    return DROPEFFECT_NONE;

  return GetPreferredDropEffect(effect);
}

void WebDropTarget::OnDragLeave(IDataObject* data_object) {
  if (web_contents_->showing_interstitial_page()) {
    interstitial_drop_target_->OnDragLeave(data_object);
  } else {
    web_contents_->DragTargetDragLeave();
  }
}


DWORD WebDropTarget::OnDrop(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect) {
  if (web_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDrop(data_object, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  web_contents_->DragTargetDrop(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  // We lie and always claim that the drop operation didn't happen because we
  // don't want to wait for the renderer to respond.
  return DROPEFFECT_NONE;
}
