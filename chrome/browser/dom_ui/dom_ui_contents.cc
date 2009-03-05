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
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/url_constants.h"

#include "grit/generated_resources.h"

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
      current_ui_(NULL),
      current_url_(GURL()) {
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
  if (InitCurrentUI(false))
    return current_ui_->ShouldDisplayFavIcon();
  return true;
}

bool DOMUIContents::IsBookmarkBarAlwaysVisible() {
  if (InitCurrentUI(false))
    return current_ui_->IsBookmarkBarAlwaysVisible();
  return false;
}

void DOMUIContents::SetInitialFocus(bool reverse) {
  if (InitCurrentUI(false))
    current_ui_->SetInitialFocus(reverse);
  else
    TabContents::SetInitialFocus(reverse);
}

const string16& DOMUIContents::GetTitle() const {
  // Workaround for new tab page - we may be asked for a title before
  // the content is ready, and we don't even want to display a 'loading...'
  // message, so we force it here.
  if (controller()->GetActiveEntry() &&
      controller()->GetActiveEntry()->url().host() ==
      NewTabUI::GetBaseURL().host()) {
    static string16* newtab_title = new string16(WideToUTF16Hack(
        l10n_util::GetString(IDS_NEW_TAB_TITLE)));
    return *newtab_title;
  }
  return WebContents::GetTitle();
}

bool DOMUIContents::ShouldDisplayURL() {
  if (InitCurrentUI(false))
    return current_ui_->ShouldDisplayURL();
  return TabContents::ShouldDisplayURL();
}

void DOMUIContents::RequestOpenURL(const GURL& url, const GURL& referrer,
                                   WindowOpenDisposition disposition) {
  if (InitCurrentUI(false))
    current_ui_->RequestOpenURL(url, referrer, disposition);
  else
    WebContents::RequestOpenURL(url, referrer, disposition);
}

bool DOMUIContents::NavigateToPendingEntry(bool reload) {
  InitCurrentUI(reload);
  // Let WebContents do whatever it's meant to do.
  return WebContents::NavigateToPendingEntry(reload);
}

void DOMUIContents::ProcessDOMUIMessage(const std::string& message,
                                        const std::string& content) {
  DCHECK(current_ui_);
  current_ui_->ProcessDOMUIMessage(message, content);
}

bool DOMUIContents::InitCurrentUI(bool reload) {
  if (!controller()->GetActiveEntry())
    return false;

  GURL url = controller()->GetActiveEntry()->url();

  if (url.is_empty() || !url.is_valid())
    return false;

  if (reload || url != current_url_) {
    // Shut down our existing DOMUI.
    delete current_ui_;
    current_ui_ = NULL;

    // Set up a new DOMUI.
    current_ui_ = GetDOMUIForURL(url);
    if (current_ui_) {
      current_ui_->Init();
      current_url_ = url;
      return true;
    }
  } else if (current_ui_) {
    return true;
  }

  return false;
}

// static
const std::string DOMUIContents::GetScheme() {
  return chrome::kChromeUIScheme;
}

DOMUI* DOMUIContents::GetDOMUIForURL(const GURL &url) {
  if (url.host() == NewTabUI::GetBaseURL().host() ||
      url.SchemeIs(chrome::kChromeInternalScheme)) {
    return new NewTabUI(this);
  }
  if (url.host() == HistoryUI::GetBaseURL().host()) {
    return new HistoryUI(this);
  }
  if (url.host() == DownloadsUI::GetBaseURL().host()) {
    return new DownloadsUI(this);
  }
#if defined(OS_WIN)
// TODO(port): include this once these are converted to HTML
  if (url.host() == ExtensionsUI::GetBaseURL().host()) {
    return new ExtensionsUI(this);
  }
  if (url.host() == DebuggerContents::GetBaseURL().host()) {
    return new DebuggerContents(this);
  }
  if (url.host() == DevToolsUI::GetBaseURL().host()) {
    return new DevToolsUI(this);
  }
#else
  NOTIMPLEMENTED();
#endif
  return NULL;
}
