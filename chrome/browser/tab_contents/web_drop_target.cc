// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlobj.h>

#include "chrome/browser/tab_contents/web_drop_target.h"

#include "app/os_exchange_data.h"
#include "base/clipboard_util.h"
#include "base/gfx/point.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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
  explicit InterstitialDropTarget(TabContents* tab_contents)
      : tab_contents_(tab_contents) {}

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
      tab_contents_->OpenURL(GURL(url), GURL(), CURRENT_TAB,
                             PageTransition::AUTO_BOOKMARK);
      return GetPreferredDropEffect(effect);
    }
    return DROPEFFECT_NONE;
  }

 private:
  TabContents* tab_contents_;

  DISALLOW_EVIL_CONSTRUCTORS(InterstitialDropTarget);
};

///////////////////////////////////////////////////////////////////////////////
// WebDropTarget, public:

WebDropTarget::WebDropTarget(HWND source_hwnd, TabContents* tab_contents)
    : BaseDropTarget(source_hwnd),
      tab_contents_(tab_contents),
      current_rvh_(NULL),
      is_drop_target_(false),
      interstitial_drop_target_(new InterstitialDropTarget(tab_contents)) {
}

WebDropTarget::~WebDropTarget() {
}

DWORD WebDropTarget::OnDragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  current_rvh_ = tab_contents_->render_view_host();

  // Don't pass messages to the renderer if an interstitial page is showing
  // because we don't want the interstitial page to navigate.  Instead,
  // pass the messages on to a separate interstitial DropTarget handler.
  if (tab_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDragEnter(data_object, effect);

  // TODO(tc): PopulateWebDropData can be slow depending on what is in the
  // IDataObject.  Maybe we can do this in a background thread.
  WebDropData drop_data(GetDragIdentity());
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  if (drop_data.url.is_empty())
    OSExchangeData::GetPlainTextURL(data_object, &drop_data.url);

  is_drop_target_ = true;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  tab_contents_->render_view_host()->DragTargetDragEnter(drop_data,
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
  DCHECK(current_rvh_);
  if (current_rvh_ != tab_contents_->render_view_host())
    OnDragEnter(data_object, key_state, cursor_position, effect);

  if (tab_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDragOver(data_object, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  tab_contents_->render_view_host()->DragTargetDragOver(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  if (!is_drop_target_)
    return DROPEFFECT_NONE;

  return GetPreferredDropEffect(effect);
}

void WebDropTarget::OnDragLeave(IDataObject* data_object) {
  DCHECK(current_rvh_);
  if (current_rvh_ != tab_contents_->render_view_host())
    return;

  if (tab_contents_->showing_interstitial_page()) {
    interstitial_drop_target_->OnDragLeave(data_object);
  } else {
    tab_contents_->render_view_host()->DragTargetDragLeave();
  }
}

DWORD WebDropTarget::OnDrop(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect) {
  DCHECK(current_rvh_);
  if (current_rvh_ != tab_contents_->render_view_host())
    OnDragEnter(data_object, key_state, cursor_position, effect);

  if (tab_contents_->showing_interstitial_page())
    interstitial_drop_target_->OnDragOver(data_object, effect);

  if (tab_contents_->showing_interstitial_page())
    return interstitial_drop_target_->OnDrop(data_object, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  tab_contents_->render_view_host()->DragTargetDrop(
      gfx::Point(client_pt.x, client_pt.y),
      gfx::Point(cursor_position.x, cursor_position.y));

  current_rvh_ = NULL;

  // We lie and always claim that the drop operation didn't happen because we
  // don't want to wait for the renderer to respond.
  return DROPEFFECT_NONE;
}
