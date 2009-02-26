// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_contents.h"

#include "chrome/browser/debugger/debugger_contents.h"
#include "chrome/browser/dom_ui/dev_tools_ui.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/url_constants.h"

// The path used in internal URLs to thumbnail data.
static const char kThumbnailPath[] = "thumb";

// The path used in internal URLs to favicon data.
static const char kFavIconPath[] = "favicon";

///////////////////////////////////////////////////////////////////////////////
// FavIconSource

FavIconSource::FavIconSource(Profile* profile)
    : DataSource(kFavIconPath, MessageLoop::current()), profile_(profile) {}

void FavIconSource::StartDataRequest(const std::string& path, int request_id) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle;
    if (path.size() > 8 && path.substr(0, 8) == "iconurl/") {
      handle = hs->GetFavIcon(
          GURL(path.substr(8)),
          &cancelable_consumer_,
          NewCallback(this, &FavIconSource::OnFavIconDataAvailable));
    } else {
      handle = hs->GetFavIconForURL(
          GURL(path),
          &cancelable_consumer_,
          NewCallback(this, &FavIconSource::OnFavIconDataAvailable));
    }
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    SendResponse(request_id, NULL);
  }
}

void FavIconSource::OnFavIconDataAvailable(
    HistoryService::Handle request_handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);

  if (know_favicon && data.get() && !data->data.empty()) {
    // Forward the data along to the networking system.
    SendResponse(request_id, data);
  } else {
    if (!default_favicon_.get()) {
      default_favicon_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_FAVICON, &default_favicon_->data);
    }

    SendResponse(request_id, default_favicon_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ThumbnailSource

ThumbnailSource::ThumbnailSource(Profile* profile)
    : DataSource(kThumbnailPath, MessageLoop::current()), profile_(profile) {}

void ThumbnailSource::StartDataRequest(const std::string& path,
                                       int request_id) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle = hs->GetPageThumbnail(
        GURL(path),
        &cancelable_consumer_,
        NewCallback(this, &ThumbnailSource::OnThumbnailDataAvailable));
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    // Tell the caller that no thumbnail is available.
    SendResponse(request_id, NULL);
  }
}

void ThumbnailSource::OnThumbnailDataAvailable(
    HistoryService::Handle request_handle,
    scoped_refptr<RefCountedBytes> data) {
  HistoryService* hs =
    profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);
  // Forward the data along to the networking system.
  if (data.get() && !data->data.empty()) {
    SendResponse(request_id, data);
  } else {
    if (!default_thumbnail_.get()) {
      default_thumbnail_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_THUMBNAIL, &default_thumbnail_->data);
    }

    SendResponse(request_id, default_thumbnail_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DOMUIContents

// This is the top-level URL handler for chrome-ui: URLs, and exposed in
// our header file. The individual DOMUIs provide a chrome-ui:// HTML source
// at the same host/path.
bool DOMUIContentsCanHandleURL(GURL* url,
                               TabContentsType* result_type) {
  // chrome-internal is a scheme we used to use for the new tab page.
  if (!url->SchemeIs(chrome::kChromeUIScheme) &&
      !url->SchemeIs(chrome::kChromeInternalScheme))
    return false;

  *result_type = TAB_CONTENTS_DOM_UI;
  return true;
}

DOMUIContents::DOMUIContents(Profile* profile,
                             SiteInstance* instance,
                             RenderViewHostFactory* render_view_factory)
    : WebContents(profile,
                  instance,
                  render_view_factory,
                  MSG_ROUTING_NONE,
                  NULL),
      current_ui_(NULL) {
  set_type(TAB_CONTENTS_DOM_UI);
}

DOMUIContents::~DOMUIContents() {
  if (current_ui_)
    delete current_ui_;
}

bool DOMUIContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // Be sure to enable DOM UI bindings on the RenderViewHost before
  // CreateRenderView is called.  Since a cross-site transition may be
  // involved, this may or may not be the same RenderViewHost that we had when
  // we were created.
  render_view_host->AllowDOMUIBindings();
  return WebContents::CreateRenderViewForRenderManager(render_view_host);
}

WebPreferences DOMUIContents::GetWebkitPrefs() {
  // Get the users preferences then force image loading to always be on.
  WebPreferences web_prefs = WebContents::GetWebkitPrefs();
  web_prefs.loads_images_automatically = true;
  web_prefs.javascript_enabled = true;

  return web_prefs;
}

void DOMUIContents::RenderViewCreated(RenderViewHost* render_view_host) {
  DCHECK(current_ui_);
  current_ui_->RenderViewCreated(render_view_host);
}

bool DOMUIContents::ShouldDisplayFavIcon() {
  if (current_ui_)
    return current_ui_->ShouldDisplayFavIcon();
  return true;
}

bool DOMUIContents::IsBookmarkBarAlwaysVisible() {
  if (current_ui_)
    return current_ui_->IsBookmarkBarAlwaysVisible();
  return false;
}

void DOMUIContents::SetInitialFocus() {
  if (current_ui_)
    current_ui_->SetInitialFocus();
}

bool DOMUIContents::ShouldDisplayURL() {
  if (current_ui_)
    return current_ui_->ShouldDisplayURL();
  return true;
}

void DOMUIContents::RequestOpenURL(const GURL& url, const GURL& referrer,
                                   WindowOpenDisposition disposition) {
  if (current_ui_)
    current_ui_->RequestOpenURL(url, referrer, disposition);
}

bool DOMUIContents::NavigateToPendingEntry(bool reload) {
  if (current_ui_) {
    // Shut down our existing DOMUI.
    delete current_ui_;
    current_ui_ = NULL;
  }

  // Set up a new DOMUI.
  NavigationEntry* pending_entry = controller()->GetPendingEntry();
  current_ui_ = GetDOMUIForURL(pending_entry->url());
  if (current_ui_)
    current_ui_->Init();
  else
    return false;

  // Let WebContents do whatever it's meant to do.
  return WebContents::NavigateToPendingEntry(reload);
}

void DOMUIContents::ProcessDOMUIMessage(const std::string& message,
                                        const std::string& content) {
  DCHECK(current_ui_);
  current_ui_->ProcessDOMUIMessage(message, content);
}

// static
const std::string DOMUIContents::GetScheme() {
  return chrome::kChromeUIScheme;
}

DOMUI* DOMUIContents::GetDOMUIForURL(const GURL &url) {
#if defined(OS_WIN)
// TODO(port): include this once these are converted to HTML
  if (url.host() == NewTabUI::GetBaseURL().host() ||
      url.SchemeIs(chrome::kChromeInternalScheme)) {
    return new NewTabUI(this);
  } else if (url.host() == HistoryUI::GetBaseURL().host()) {
    return new HistoryUI(this);
  } else if (url.host() == DownloadsUI::GetBaseURL().host()) {
    return new DownloadsUI(this);
  } else if (url.host() == DebuggerContents::GetBaseURL().host()) {
    return new DebuggerContents(this);
  } else if (url.host() == DevToolsUI::GetBaseURL().host()) {
    return new DevToolsUI(this);
  }
#else
  NOTIMPLEMENTED();
#endif
  return NULL;
}
