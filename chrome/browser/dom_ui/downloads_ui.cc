// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/downloads_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_piece.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/downloads_dom_handler.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
// TODO(port): re-enable when download_util is ported
#include "chrome/browser/download/download_util.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif


namespace {

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class DownloadsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  DownloadsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsUIHTMLSource);
};

DownloadsUIHTMLSource::DownloadsUIHTMLSource()
    : DataSource(chrome::kChromeUIDownloadsHost, MessageLoop::current()) {
}

void DownloadsUIHTMLSource::StartDataRequest(const std::string& path,
                                             int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_DOWNLOAD_TITLE));
  localized_strings.SetString(L"searchbutton",
      l10n_util::GetString(IDS_DOWNLOAD_SEARCH_BUTTON));
  localized_strings.SetString(L"no_results",
      l10n_util::GetString(IDS_DOWNLOAD_SEARCH_BUTTON));
  localized_strings.SetString(L"searchresultsfor",
      l10n_util::GetString(IDS_DOWNLOAD_SEARCHRESULTSFOR));
  localized_strings.SetString(L"downloads",
      l10n_util::GetString(IDS_DOWNLOAD_TITLE));
  localized_strings.SetString(L"clear_all",
      l10n_util::GetString(IDS_DOWNLOAD_LINK_CLEAR_ALL));

  // Status.
  localized_strings.SetString(L"status_cancelled",
      l10n_util::GetString(IDS_DOWNLOAD_TAB_CANCELLED));
  localized_strings.SetString(L"status_paused",
      l10n_util::GetString(IDS_DOWNLOAD_PROGRESS_PAUSED));

  // Dangerous file.
  localized_strings.SetString(L"danger_desc",
      l10n_util::GetStringF(IDS_PROMPT_DANGEROUS_DOWNLOAD, L"%s"));
  localized_strings.SetString(L"danger_save",
      l10n_util::GetString(IDS_SAVE_DOWNLOAD));
  localized_strings.SetString(L"danger_discard",
      l10n_util::GetString(IDS_DISCARD_DOWNLOAD));

  // Controls.
  localized_strings.SetString(L"control_pause",
      l10n_util::GetString(IDS_DOWNLOAD_LINK_PAUSE));
  localized_strings.SetString(L"control_showinfolder",
      l10n_util::GetString(IDS_DOWNLOAD_LINK_SHOW));
  localized_strings.SetString(L"control_cancel",
      l10n_util::GetString(IDS_DOWNLOAD_LINK_CANCEL));
  localized_strings.SetString(L"control_resume",
      l10n_util::GetString(IDS_DOWNLOAD_LINK_RESUME));

  SetFontAndTextDirection(&localized_strings);

  static const StringPiece downloads_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DOWNLOADS_HTML));
  const std::string full_html = jstemplate_builder::GetTemplateHtml(
      downloads_html, &localized_strings, "t");

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(TabContents* contents) : DOMUI(contents) {
  DownloadManager* dlm = GetProfile()->GetOriginalProfile()->
      GetDownloadManager();

  DownloadsDOMHandler* handler = new DownloadsDOMHandler(this, dlm);
  AddMessageHandler(handler);
  handler->Init();

  DownloadsUIHTMLSource* html_source = new DownloadsUIHTMLSource();

  // Set up the chrome://downloads/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource,
          html_source));
}
