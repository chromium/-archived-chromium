// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/downloads_ui.h"

#include "base/gfx/png_encoder.h"
#include "base/string_piece.h"
#include "base/thread.h"
#include "base/time_format.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/fileicon_source.h"
#if defined(OS_WIN)
// TODO(port): re-enable when download_util is ported
#include "chrome/browser/download/download_util.h"
#endif
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/time_format.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

// DownloadsUI is accessible from chrome-ui://downloads.
static const char kDownloadsHost[] = "downloads";

// Maximum number of downloads to show. TODO(glen): Remove this and instead
// stuff the downloads down the pipe slowly.
static const int kMaxDownloads = 150;

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUIHTMLSource::DownloadsUIHTMLSource()
    : DataSource(kDownloadsHost, MessageLoop::current()) {
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

  localized_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       L"rtl" : L"ltr");

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

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsDOMHandler
//
///////////////////////////////////////////////////////////////////////////////

// Sort DownloadItems into descending order by their start time.
class DownloadItemSorter : public std::binary_function<DownloadItem*,
                                                       DownloadItem*,
                                                       bool> {
 public:
  bool operator()(const DownloadItem* lhs, const DownloadItem* rhs) {
    return lhs->start_time() > rhs->start_time();
  }
};

DownloadsDOMHandler::DownloadsDOMHandler(DOMUI* dom_ui, DownloadManager* dlm)
    : DOMMessageHandler(dom_ui),
      search_text_(),
      download_manager_(dlm) {
  dom_ui_->RegisterMessageCallback("getDownloads",
      NewCallback(this, &DownloadsDOMHandler::HandleGetDownloads));
  dom_ui_->RegisterMessageCallback("openFile",
      NewCallback(this, &DownloadsDOMHandler::HandleOpenFile));

  dom_ui_->RegisterMessageCallback("drag",
    NewCallback(this, &DownloadsDOMHandler::HandleDrag));

  dom_ui_->RegisterMessageCallback("saveDangerous",
    NewCallback(this, &DownloadsDOMHandler::HandleSaveDangerous));
  dom_ui_->RegisterMessageCallback("discardDangerous",
    NewCallback(this, &DownloadsDOMHandler::HandleDiscardDangerous));
  dom_ui_->RegisterMessageCallback("show",
    NewCallback(this, &DownloadsDOMHandler::HandleShow));
  dom_ui_->RegisterMessageCallback("togglepause",
    NewCallback(this, &DownloadsDOMHandler::HandlePause));
  dom_ui_->RegisterMessageCallback("resume",
    NewCallback(this, &DownloadsDOMHandler::HandlePause));
  dom_ui_->RegisterMessageCallback("cancel",
    NewCallback(this, &DownloadsDOMHandler::HandleCancel));

  // Create our fileicon data source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
                        &ChromeURLDataManager::AddDataSource,
                        new FileIconSource()));
}

DownloadsDOMHandler::~DownloadsDOMHandler() {
  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
}

// DownloadsDOMHandler, public: -----------------------------------------------

void DownloadsDOMHandler::Init() {
  download_manager_->AddObserver(this);
}

void DownloadsDOMHandler::OnDownloadUpdated(DownloadItem* download) {
  // Get the id for the download. Our downloads are sorted latest to first,
  // and the id is the index into that list. We should be careful of sync
  // errors between the UI and the download_items_ list (we may wish to use
  // something other than 'id').
  OrderedDownloads::iterator it = find(download_items_.begin(),
    download_items_.end(),
    download);
  if (it == download_items_.end())
    return;
  const int id = static_cast<int>(it - download_items_.begin());

  ListValue results_value;
  results_value.Append(CreateDownloadItemValue(download, id));
  dom_ui_->CallJavascriptFunction(L"downloadUpdated", results_value);
}

// A download has started or been deleted. Query our DownloadManager for the
// current set of downloads, which will call us back in SetDownloads once it
// has retrieved them.
void DownloadsDOMHandler::ModelChanged() {
  ClearDownloadItems();
  download_manager_->GetDownloads(this, search_text_);
}

void DownloadsDOMHandler::SetDownloads(
    std::vector<DownloadItem*>& downloads) {
  ClearDownloadItems();

  // Swap new downloads in.
  download_items_.swap(downloads);
  sort(download_items_.begin(), download_items_.end(), DownloadItemSorter());

  // Scan for any in progress downloads and add ourself to them as an observer.
  for (OrderedDownloads::iterator it = download_items_.begin();
       it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin()) > kMaxDownloads)
      break;

    DownloadItem* download = *it;
    if (download->state() == DownloadItem::IN_PROGRESS) {
      // We want to know what happens as the download progresses.
      download->AddObserver(this);
    } else if (download->safety_state() == DownloadItem::DANGEROUS) {
      // We need to be notified when the user validates the dangerous download.
      download->AddObserver(this);
    }
  }

  SendCurrentDownloads();
}

void DownloadsDOMHandler::HandleGetDownloads(const Value* value) {
  std::wstring new_search = ExtractStringValue(value);
  if (search_text_.compare(new_search) != 0) {
    search_text_ = new_search;
    ClearDownloadItems();
    download_manager_->GetDownloads(this, search_text_);
  } else {
    SendCurrentDownloads();
  }
}

void DownloadsDOMHandler::HandleOpenFile(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    download_manager_->OpenDownloadInShell(file, NULL);
}

void DownloadsDOMHandler::HandleDrag(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file) {
    IconManager* im = g_browser_process->icon_manager();
    SkBitmap* icon = im->LookupIcon(file->full_path().ToWStringHack(),
        IconLoader::NORMAL);
    download_util::DragDownload(file, icon);
  }
}

void DownloadsDOMHandler::HandleSaveDangerous(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    download_manager_->DangerousDownloadValidated(file);
}

void DownloadsDOMHandler::HandleDiscardDangerous(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->Remove(true);
}

void DownloadsDOMHandler::HandleShow(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
      download_manager_->ShowDownloadInShell(file);
}

void DownloadsDOMHandler::HandlePause(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->TogglePause();
}

void DownloadsDOMHandler::HandleCancel(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->Cancel(true);
}

// DownloadsDOMHandler, private: ----------------------------------------------

void DownloadsDOMHandler::SendCurrentDownloads() {
  ListValue results_value;
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    int index = static_cast<int>(it - download_items_.begin());
    if (index > kMaxDownloads)
      break;
    results_value.Append(CreateDownloadItemValue(*it,index));
  }

  dom_ui_->CallJavascriptFunction(L"downloadsList", results_value);
}

DictionaryValue* DownloadsDOMHandler::CreateDownloadItemValue(
    DownloadItem* download, int id) {
  DictionaryValue* file_value = new DictionaryValue();

  file_value->SetInteger(L"started",
    static_cast<int>(download->start_time().ToTimeT()));
  file_value->SetInteger(L"id", id);
  file_value->SetString(L"file_path", download->full_path().ToWStringHack());
  file_value->SetString(L"file_name", download->GetFileName().ToWStringHack());
  file_value->SetString(L"url", download->url().spec());

  if (download->state() == DownloadItem::IN_PROGRESS) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString(L"state", L"DANGEROUS");
    } else if (download->is_paused()) {
      file_value->SetString(L"state", L"PAUSED");
    } else {
      file_value->SetString(L"state", L"IN_PROGRESS");
    }
    file_value->SetInteger(L"speed",
        static_cast<int>(download->CurrentSpeed()));
    file_value->SetInteger(L"percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger(L"received",
        static_cast<int>(download->received_bytes()));
  } else if (download->state() == DownloadItem::CANCELLED) {
    file_value->SetString(L"state", L"CANCELLED");
  } else if (download->state() == DownloadItem::COMPLETE) {
    if (download->safety_state() == DownloadItem::DANGEROUS) {
      file_value->SetString(L"state", L"DANGEROUS");
    } else {
      file_value->SetString(L"state", L"COMPLETE");
    }
  }

  file_value->SetInteger(L"total",
      static_cast<int>(download->total_bytes()));

  return file_value;
}

void DownloadsDOMHandler::ClearDownloadItems() {
  // Clear out old state and remove self as observer for each download.
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  download_items_.clear();
}

DownloadItem* DownloadsDOMHandler::GetDownloadById(int id) {
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin() == id)) {
      return (*it);
    }
  }

  return NULL;
}

DownloadItem* DownloadsDOMHandler::GetDownloadByValue(const Value* value) {
  int id;
  if (ExtractIntegerValue(value, &id)) {
    return GetDownloadById(id);
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(DOMUIContents* contents) : DOMUI(contents) {
}

void DownloadsUI::Init() {
  DownloadManager* dlm = get_profile()->GetDownloadManager();

  DownloadsDOMHandler* handler = new DownloadsDOMHandler(this, dlm);
  AddMessageHandler(handler);
  handler->Init();

  DownloadsUIHTMLSource* html_source = new DownloadsUIHTMLSource();

  // Set up the chrome-ui://downloads/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource,
          html_source));
}

// static
GURL DownloadsUI::GetBaseURL() {
  std::string url = DOMUIContents::GetScheme();
  url += "://";
  url += kDownloadsHost;
  return GURL(url);
}
