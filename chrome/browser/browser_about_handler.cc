// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>
#include <vector>

#include "base/file_version_info.h"
#include "base/histogram.h"
#include "base/image_util.h"
#include "base/process_util.h"
#include "base/stats_table.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/tracked_objects.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/ipc_status_view.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/renderer/about_handler.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webkit_glue.h"
#ifdef CHROME_V8
#include "v8/include/v8.h"
#endif

#include "chromium_strings.h"
#include "generated_resources.h"

// The URL scheme used for the about ui.
static const char kAboutScheme[] = "about";

// The paths used for the about pages.
static const char kCachePath[] = "cache";
static const char kDnsPath[] = "dns";
static const char kHistogramsPath[] = "histograms";
static const char kObjectsPath[] = "objects";
static const char kMemoryPath[] = "memory";
static const char kPluginsPath[] = "plugins";
static const char kStatsPath[] = "stats";
static const char kVersionPath[] = "version";
static const char kCreditsPath[] = "credits";
static const char kTermsPath[] = "terms";

class AboutSource : public ChromeURLDataManager::DataSource {
 public:
  // Creates our datasource.
  AboutSource();
  virtual ~AboutSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

  // Send the response data.
  void FinishDataRequest(const std::string& html, int request_id);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(AboutSource);
};

// Handling about:memory is complicated enough to encapsulate it's
// related methods into a single class.
class AboutMemoryHandler : public MemoryDetails {
 public:
  AboutMemoryHandler(AboutSource* source, int request_id);

  virtual void OnDetailsAvailable();

 private:
  void BindProcessMetrics(DictionaryValue* data,
                          ProcessMemoryInformation* info);
  void GetTabContentsTitles(RenderProcessHost* rph, ListValue* titles);
  void AppendProcess(ListValue* renderers, ProcessMemoryInformation* info);
  void FinishAboutMemory();

  AboutSource* source_;
  int request_id_;
  DISALLOW_EVIL_CONSTRUCTORS(AboutMemoryHandler);
};

AboutSource::AboutSource()
    : DataSource(kAboutScheme, MessageLoop::current()) {
}

AboutSource::~AboutSource() {
}

void AboutSource::StartDataRequest(const std::string& path_raw,
                                   int request_id) {
  std::string path = path_raw;
  std::string info;
  if (path.find("/") != -1) {
    size_t pos = path.find("/");
    info = path.substr(pos + 1, path.length() - (pos + 1));
    path = path.substr(0, pos);
  }
  path = StringToLowerASCII(path);

  std::string response;
  if (path == kDnsPath) {
    response = BrowserAboutHandler::AboutDns();
  } else if (path == kHistogramsPath) {
    response = BrowserAboutHandler::AboutHistograms(info);
  } else if (path == kMemoryPath) {
    BrowserAboutHandler::AboutMemory(this, request_id);
    return;
  } else if (path == kObjectsPath) {
    response = BrowserAboutHandler::AboutObjects(info);
  } else if (path == kPluginsPath) {
    response = BrowserAboutHandler::AboutPlugins();
  } else if (path == kStatsPath) {
    response = BrowserAboutHandler::AboutStats();
  } else if (path == kVersionPath || path.empty()) {
    response = BrowserAboutHandler::AboutVersion();
  } else if (path == kCreditsPath) {
    response = BrowserAboutHandler::AboutCredits();
  } else if (path == kTermsPath) {
    response = BrowserAboutHandler::AboutTerms();
  }
  FinishDataRequest(response, request_id);
}

void AboutSource::FinishDataRequest(const std::string& response,
                                    int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

// This is the top-level URL handler for chrome-internal: URLs, and exposed in
// our header file.
bool BrowserAboutHandler::MaybeHandle(GURL* url,
                                      TabContentsType* result_type) {
  if (!url->SchemeIs(kAboutScheme))
    return false;

  // about:blank is special.  Frames are allowed to access about:blank,
  // but they are not allowed to access other types of about pages.
  // Just ignore the about:blank and let the TAB_CONTENTS_WEB handle it.
  if (StringToLowerASCII(url->path()) == "blank") {
    return false;
  }

  // We create an about:cache mapping to the view-cache: internal URL.
  if (StringToLowerASCII(url->path()) == kCachePath) {
    *url = GURL("view-cache:");
    *result_type = TAB_CONTENTS_WEB;
    return true;
  }

  if (LowerCaseEqualsASCII(url->path(), "network")) {
    // about:network doesn't have an internal protocol, so don't modify |url|.
    *result_type = TAB_CONTENTS_NETWORK_STATUS_VIEW;
    return true;
  }

#ifdef IPC_MESSAGE_LOG_ENABLED
  if ((LowerCaseEqualsASCII(url->path(), "ipc")) &&
      (IPCStatusView::current() == NULL)) {
    // about:ipc doesn't have an internal protocol, so don't modify |url|.
    *result_type = TAB_CONTENTS_IPC_STATUS_VIEW;
    return true;
  }
#endif

  if (LowerCaseEqualsASCII(url->path(), "internets")) {
    // about:internets doesn't have an internal protocol, so don't modify |url|.
    *result_type = TAB_CONTENTS_ABOUT_INTERNETS_STATUS_VIEW;
    return true;
  }

  // There are a few about URLs that we hand over to the renderer.
  // If the renderer wants them, let it have them.
  if (AboutHandler::WillHandle(*url))
    return false;

  *result_type = TAB_CONTENTS_ABOUT_UI;
  std::string about_url = "chrome://about/";
  about_url.append(url->path());
  *url = GURL(about_url);
  return true;
}

BrowserAboutHandler::BrowserAboutHandler(Profile* profile,
                           SiteInstance* instance,
                           RenderViewHostFactory* render_view_factory) :
  WebContents(profile, instance, render_view_factory, MSG_ROUTING_NONE, NULL) {
  set_type(TAB_CONTENTS_ABOUT_UI);

  // We only need to register the AboutSource once and it is
  // kept globally.  There is currently no way to remove a data source.
  static bool initialized = false;
  if (!initialized) {
    about_source_ = new AboutSource();
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(&chrome_url_data_manager,
            &ChromeURLDataManager::AddDataSource,
            about_source_));
    initialized = true;
  }
}

bool BrowserAboutHandler::SupportsURL(GURL* url) {
  // Enable this tab contents to access javascript urls.
  if (url->SchemeIs("javascript"))
    return true;
  return WebContents::SupportsURL(url);
}


// static
std::string BrowserAboutHandler::AboutVersion() {
  // Strings used in the JsTemplate file.
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_ABOUT_VERSION_TITLE));
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info == NULL) {
    DLOG(ERROR) << "Unable to create FileVersionInfo object";
    return std::string();
  }

  std::wstring webkit_version = UTF8ToWide(webkit_glue::GetWebKitVersion());
#ifdef CHROME_V8
  const char* v8_vers = v8::V8::GetVersion();
  std::wstring js_version = UTF8ToWide(v8_vers);
  std::wstring js_engine = L"V8";
#else
  std::wstring js_version = webkit_version;
  std::wstring js_engine = L"JavaScriptCore";
#endif

  localized_strings.SetString(L"name",
      l10n_util::GetString(IDS_PRODUCT_NAME));
  localized_strings.SetString(L"version", version_info->file_version());
  localized_strings.SetString(L"js_engine", js_engine);
  localized_strings.SetString(L"js_version", js_version);
  localized_strings.SetString(L"webkit_version", webkit_version);
  localized_strings.SetString(L"company",
      l10n_util::GetString(IDS_ABOUT_VERSION_COMPANY_NAME));
  localized_strings.SetString(L"copyright",
      l10n_util::GetString(IDS_ABOUT_VERSION_COPYRIGHT));
  localized_strings.SetString(L"cl", version_info->last_change());
  if (version_info->is_official_build()) {
    localized_strings.SetString(L"official",
      l10n_util::GetString(IDS_ABOUT_VERSION_OFFICIAL));
  } else {
    localized_strings.SetString(L"official",
      l10n_util::GetString(IDS_ABOUT_VERSION_UNOFFICIAL));
  }
  localized_strings.SetString(L"useragent",
      UTF8ToWide(webkit_glue::GetUserAgent()));

  static const StringPiece version_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_VERSION_HTML));

  return jstemplate_builder::GetTemplateHtml(
      version_html, &localized_strings, "t" /* template root node id */);
}

// static
std::string BrowserAboutHandler::AboutCredits() {
  static const std::string credits_html = 
      ResourceBundle::GetSharedInstance().GetDataResource(
          IDR_CREDITS_HTML);

  return credits_html;
}

// static
std::string BrowserAboutHandler::AboutTerms() {
  static const std::string terms_html = 
      ResourceBundle::GetSharedInstance().GetDataResource(
          IDR_TERMS_HTML);

  return terms_html;
}

// static
std::string BrowserAboutHandler::AboutPlugins() {
  // Strings used in the JsTemplate file.
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_TITLE));
  localized_strings.SetString(L"headingPlugs",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_HEADING_PLUGS));
  localized_strings.SetString(L"headingNoPlugs",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_HEADING_NOPLUGS));
  localized_strings.SetString(L"filename",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_FILENAME_LABEL));
  localized_strings.SetString(L"mimetype",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_MIMETYPE_LABEL));
  localized_strings.SetString(L"description",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_DESCRIPTION_LABEL));
  localized_strings.SetString(L"suffixes",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_SUFFIX_LABEL));
  localized_strings.SetString(L"enabled",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_ENABLED_LABEL));
  localized_strings.SetString(L"enabled_yes",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_ENABLED_YES));
  localized_strings.SetString(L"enabled_no",
      l10n_util::GetString(IDS_ABOUT_PLUGINS_ENABLED_NO));

  static const StringPiece plugins_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_PLUGINS_HTML));

  return jstemplate_builder::GetTemplateHtml(
      plugins_html, &localized_strings, "t" /* template root node id */);
}

// static
std::string BrowserAboutHandler::AboutHistograms(const std::string query) {
  std::string data;
  StatisticsRecorder::WriteHTMLGraph(query, &data);
  return data;
}

// static
std::string BrowserAboutHandler::AboutObjects(const std::string& query) {
  std::string data;
  tracked_objects::ThreadData::WriteHTML(query, &data);
  return data;
}

// static
std::string BrowserAboutHandler::AboutDns() {
  std::string data;
  chrome_browser_net::DnsPrefetchGetHtmlInfo(&data);
  return data;
}

// static
std::string BrowserAboutHandler::AboutStats() {
  // We keep the DictionaryValue tree live so that we can do delta
  // stats computations across runs.
  static DictionaryValue root;

  StatsTable* table = StatsTable::current();
  if (!table)
    return std::string();

  // We maintain two lists - one for counters and one for timers.
  // Timers actually get stored on both lists.
  ListValue* counters;
  if (!root.GetList(L"counters", &counters)) {
    counters = new ListValue();
    root.Set(L"counters", counters);
  }

  ListValue* timers;
  if (!root.GetList(L"timers", &timers)) {
    timers = new ListValue();
    root.Set(L"timers", timers);
  }

  // NOTE: Counters start at index 1.
  for (int index = 1; index <= table->GetMaxCounters(); index++) {
    // Get the counter's full name
    std::string full_name = table->GetRowName(index);
    if (full_name.length() == 0)
      break;
    DCHECK(full_name[1] == ':');
    char counter_type = full_name[0];
    std::string name = full_name.substr(2);

    // JSON doesn't allow '.' in names.
    size_t pos;
    while ((pos = name.find(".")) != -1)
      name.replace(pos, 1, ":");

    // Try to see if this name already exists.
    DictionaryValue* counter = NULL;
    for (size_t scan_index = 0;
         scan_index < counters->GetSize(); scan_index++) {
      DictionaryValue* dictionary;
      if (counters->GetDictionary(scan_index, &dictionary)) {
        std::wstring scan_name;
        if (dictionary->GetString(L"name", &scan_name) &&
            WideToASCII(scan_name) == name) {
          counter = dictionary;
        }
      } else {
        NOTREACHED();  // Should always be there
      }
    }

    if (counter == NULL) {
      counter = new DictionaryValue();
      counter->SetString(L"name", ASCIIToWide(name));
      counters->Append(counter);
    }

    switch (counter_type) {
      case 'c':
        {
          int new_value = table->GetRowValue(index);
          int prior_value = 0;
          int delta = 0;
          if (counter->GetInteger(L"value", &prior_value)) {
            delta = new_value - prior_value;
          }
          counter->SetInteger(L"value", new_value);
          counter->SetInteger(L"delta", delta);
        }
        break;
      case 'm':
        {
          // TODO(mbelshe): implement me.
        }
        break;
      case 't':
        {
          int time = table->GetRowValue(index);
          counter->SetInteger(L"time", time);

          // Store this on the timers list as well.
          timers->Append(counter);
        }
        break;
      default:
        NOTREACHED();
    }
  }

  // Get about_stats.html
  static const StringPiece stats_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_STATS_HTML));

  // Create jstemplate and return.
  std::string data = jstemplate_builder::GetTemplateHtml(
      stats_html, &root, "t" /* template root node id */);

  // Clear the timer list since we stored the data in the timers list as well.
  for (int index = static_cast<int>(timers->GetSize())-1; index >= 0;
       index--) {
    Value* value;
    timers->Remove(index, &value);
    // We don't care about the value pointer; it's still tracked
    // on the counters list.
  }

  return data;
}

AboutMemoryHandler::AboutMemoryHandler(AboutSource* source, int request_id)
  : source_(source),
    request_id_(request_id) {
  StartFetch();
}

// Helper for AboutMemory to bind results from a ProcessMetrics object
// to a DictionaryValue. Fills ws_usage and comm_usage so that the objects
// can be used in caller's scope (e.g for appending to a net total).
void AboutMemoryHandler::BindProcessMetrics(DictionaryValue* data,
      ProcessMemoryInformation* info) {
  DCHECK(data && info);

  // Bind metrics to dictionary.
  data->SetInteger(L"ws_priv", static_cast<int>(info->working_set.priv));
  data->SetInteger(L"ws_shareable",
    static_cast<int>(info->working_set.shareable));
  data->SetInteger(L"ws_shared", static_cast<int>(info->working_set.shared));
  data->SetInteger(L"comm_priv", static_cast<int>(info->committed.priv));
  data->SetInteger(L"comm_map", static_cast<int>(info->committed.mapped));
  data->SetInteger(L"comm_image", static_cast<int>(info->committed.image));
  data->SetInteger(L"pid", info->pid);
  data->SetString(L"version", info->version);
  data->SetInteger(L"processes", info->num_processes);
}

// Helper for AboutMemory to iterate over a RenderProcessHost's listeners
// and retrieve the tab titles.
void AboutMemoryHandler::GetTabContentsTitles(RenderProcessHost* rph,
                                              ListValue* titles) {
  RenderProcessHost::listeners_iterator iter;
  // NOTE: This is a bit dangerous.  We know that for now, listeners
  //       are always RenderWidgetHosts.  But in theory, they don't
  //       have to be.
  for (iter = rph->listeners_begin(); iter != rph->listeners_end(); iter++) {
    RenderWidgetHost* widget = static_cast<RenderWidgetHost*>(iter->second);
    DCHECK(widget);
    if (!widget || !widget->IsRenderView())
      continue;

    RenderViewHost* host = static_cast<RenderViewHost*>(widget);
    TabContents* contents = static_cast<WebContents*>(host->delegate());
    DCHECK(contents);
    if (!contents)
      continue;

    std::wstring title = contents->GetTitle();
    StringValue* val = NULL;
    if (!title.length())
      title = L"Untitled";
    val = new StringValue(title);
    titles->Append(val);
  }
}

// Helper for AboutMemory to append memory usage information for all
// sub-processes (renderers & plugins) used by chrome.
void AboutMemoryHandler::AppendProcess(ListValue* renderers,
                                       ProcessMemoryInformation* info) {
  DCHECK(renderers && info);

  // Append a new DictionaryValue for this renderer to our list.
  DictionaryValue* renderer = new DictionaryValue();
  renderers->Append(renderer);
  BindProcessMetrics(renderer, info);

  // Now get more information about the process.

  // First, figure out if it is a renderer.
  RenderProcessHost::iterator renderer_iter;
  for (renderer_iter = RenderProcessHost::begin(); renderer_iter !=
       RenderProcessHost::end(); ++renderer_iter) {
    if (renderer_iter->second->process().pid() == info->pid)
      break;
  }
  if (renderer_iter != RenderProcessHost::end()) {
    std::wstring renderer_label(L"Tab ");
    renderer_label.append(FormatNumber(renderer_iter->second->host_id()));
    if (info->is_diagnostics)
      renderer_label.append(L" (diagnostics)");
    renderer->SetString(L"renderer_id", renderer_label);
    ListValue* titles = new ListValue();
    renderer->Set(L"titles", titles);
    GetTabContentsTitles(renderer_iter->second, titles);
    return;
  }

  // Figure out if this is a plugin process.
  for (size_t index = 0; index < plugins()->size(); ++index) {
    if (info->pid == (*plugins())[index].pid) {
      // It is a plugin!
      std::wstring label(L"Plug-in");
      std::wstring name;
      renderer->SetString(L"renderer_id", label);
      FileVersionInfo* version_info =
          FileVersionInfo::CreateFileVersionInfo(
              (*plugins())[index].plugin_path);
      if (version_info)
        name = version_info->product_name();
      ListValue* titles = new ListValue();
      renderer->Set(L"titles", titles);
      titles->Append(new StringValue(name));
      return;
    }
  }
}


void AboutMemoryHandler::OnDetailsAvailable() {
  // the root of the JSON hierarchy for about:memory jstemplate
  DictionaryValue root;
  ListValue* browsers = new ListValue();
  root.Set(L"browsers", browsers);

  ProcessData* browser_processes = processes();

  // Aggregate per-process data into browser summary data.
  std::wstring log_string;
  for (int index = 0; index < MemoryDetails::MAX_BROWSERS; index++) {
    if (browser_processes[index].processes.size() == 0)
      continue;

    // Sum the information for the processes within this browser.
    ProcessMemoryInformation aggregate;
    ProcessMemoryInformationList::iterator iterator;
    iterator = browser_processes[index].processes.begin();
    aggregate.pid = iterator->pid;
    aggregate.version = iterator->version;
    while (iterator != browser_processes[index].processes.end()) {
      if (!iterator->is_diagnostics ||
          browser_processes[index].processes.size() == 1) {
        aggregate.working_set.priv += iterator->working_set.priv;
        aggregate.working_set.shared += iterator->working_set.shared;
        aggregate.working_set.shareable += iterator->working_set.shareable;
        aggregate.committed.priv += iterator->committed.priv;
        aggregate.committed.mapped += iterator->committed.mapped;
        aggregate.committed.image += iterator->committed.image;
        aggregate.num_processes++;
      }
      ++iterator;
    }
    DictionaryValue* browser_data = new DictionaryValue();
    browsers->Append(browser_data);
    browser_data->SetString(L"name", browser_processes[index].name);

    BindProcessMetrics(browser_data, &aggregate);

    // We log memory info as we record it.
    if (log_string.length() > 0)
      log_string.append(L", ");
    log_string.append(browser_processes[index].name);
    log_string.append(L", ");
    log_string.append(Int64ToWString(aggregate.working_set.priv));
    log_string.append(L", ");
    log_string.append(Int64ToWString(aggregate.working_set.shared));
    log_string.append(L", ");
    log_string.append(Int64ToWString(aggregate.working_set.shareable));
  }
  if (log_string.length() > 0)
    LOG(INFO) << "memory: " << log_string;


  // Set the browser & renderer detailed process data.
  DictionaryValue* browser_data = new DictionaryValue();
  root.Set(L"browzr_data", browser_data);
  ListValue* renderer_data = new ListValue();
  root.Set(L"renderer_data", renderer_data);

  DWORD browser_pid = GetCurrentProcessId();
  ProcessData process = browser_processes[0];  // Chrome is the first browser.
  for (size_t index = 0; index < process.processes.size(); index++) {
    if (process.processes[index].pid == browser_pid)
      BindProcessMetrics(browser_data, &process.processes[index]);
    else
      AppendProcess(renderer_data, &process.processes[index]);
  }

  // Get about_memory.html
  static const StringPiece memory_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_MEMORY_HTML));

  // Create jstemplate and return.
  std::string template_html = jstemplate_builder::GetTemplateHtml(
      memory_html, &root, "t" /* template root node id */);

  AboutSource* about_source = static_cast<AboutSource*>(source_);
  about_source->FinishDataRequest(template_html, request_id_);
}

// static
void BrowserAboutHandler::AboutMemory(AboutSource* source, int request_id) {
  // The AboutMemoryHandler cleans itself up.
  new AboutMemoryHandler(source, request_id);
}

