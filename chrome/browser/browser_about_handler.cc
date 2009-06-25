// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_version_info.h"
#include "base/histogram.h"
#include "base/platform_thread.h"
#include "base/stats_table.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/tracked_objects.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/histogram_synchronizer.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/about_handler.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "webkit/glue/webkit_glue.h"
#ifdef CHROME_V8
#include "v8/include/v8.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/views/about_ipc_dialog.h"
#include "chrome/browser/views/about_network_dialog.h"
#endif

using base::Time;
using base::TimeDelta;

namespace {

// The paths used for the about pages.
const char kCachePath[] = "cache";
const char kDnsPath[] = "dns";
const char kHistogramsPath[] = "histograms";
const char kObjectsPath[] = "objects";
const char kMemoryRedirectPath[] = "memory-redirect";
const char kMemoryPath[] = "memory";
const char kPluginsPath[] = "plugins";
const char kStatsPath[] = "stats";
const char kVersionPath[] = "version";
const char kCreditsPath[] = "credits";
const char kTermsPath[] = "terms";
const char kLinuxSplash[] = "linux-splash";

// Points to the singleton AboutSource object, if any.
ChromeURLDataManager::DataSource* about_source = NULL;

// When you type about:memory, it actually loads an intermediate URL that
// redirects you to the final page. This avoids the problem where typing
// "about:memory" on the new tab page or any other page where a process
// transition would occur to the about URL will cause some confusion.
//
// The problem is that during the processing of the memory page, there are two
// processes active, the original and the destination one. This can create the
// impression that we're using more resources than we actually are. This
// redirect solves the problem by eliminating the process transition during the
// time that about memory is being computed.
std::string GetAboutMemoryRedirectResponse() {
  return "<meta http-equiv=\"refresh\" "
      "content=\"0;chrome://about/memory\">";
}

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
  DISALLOW_COPY_AND_ASSIGN(AboutSource);
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
  void AppendProcess(ListValue* child_data, ProcessMemoryInformation* info);
  void FinishAboutMemory();

  AboutSource* source_;
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(AboutMemoryHandler);
};

// Individual about handlers ---------------------------------------------------

std::string AboutCredits() {
  static const std::string credits_html =
      ResourceBundle::GetSharedInstance().GetDataResource(
          IDR_CREDITS_HTML);

  return credits_html;
}

std::string AboutDns() {
  std::string data;
  chrome_browser_net::DnsPrefetchGetHtmlInfo(&data);
  return data;
}

std::string AboutHistograms(const std::string& query) {
  TimeDelta wait_time = TimeDelta::FromMilliseconds(10000);

  HistogramSynchronizer* current_synchronizer =
      HistogramSynchronizer::CurrentSynchronizer();
  DCHECK(current_synchronizer != NULL);
  current_synchronizer->FetchRendererHistogramsSynchronously(wait_time);

  std::string data;
  StatisticsRecorder::WriteHTMLGraph(query, &data);
  return data;
}

std::string AboutLinuxSplash() {
  int resource_id = IDR_LINUX_SPLASH_HTML_CHROMIUM;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info == NULL) {
    DLOG(ERROR) << "Unable to create FileVersionInfo object";
  } else {
    if (version_info->is_official_build()) {
      resource_id = IDR_LINUX_SPLASH_HTML_CHROME;
    }
  }
  static const std::string linux_splash_html =
      ResourceBundle::GetSharedInstance().GetDataResource(resource_id);

  return linux_splash_html;
}

void AboutMemory(AboutSource* source, int request_id) {
  // The AboutMemoryHandler cleans itself up.
  new AboutMemoryHandler(source, request_id);
}

std::string AboutObjects(const std::string& query) {
  std::string data;
  tracked_objects::ThreadData::WriteHTML(query, &data);
  return data;
}

std::string AboutPlugins() {
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

std::string AboutStats() {
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
    while ((pos = name.find(".")) != std::string::npos)
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

std::string AboutTerms() {
  static const std::string terms_html =
      ResourceBundle::GetSharedInstance().GetDataResource(
          IDR_TERMS_HTML);

  return terms_html;
}

std::string AboutVersion() {
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
      UTF8ToWide(webkit_glue::GetUserAgent(GURL())));

  static const StringPiece version_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_VERSION_HTML));

  return jstemplate_builder::GetTemplateHtml(
      version_html, &localized_strings, "t" /* template root node id */);
}

// AboutSource -----------------------------------------------------------------

AboutSource::AboutSource()
    : DataSource(chrome::kAboutScheme, MessageLoop::current()) {
  // This should be a singleton.
  DCHECK(!about_source);
  about_source = this;

  // Add us to the global URL handler on the IO thread.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource, this));
}

AboutSource::~AboutSource() {
  about_source = NULL;
}

void AboutSource::StartDataRequest(const std::string& path_raw,
                                   int request_id) {
  std::string path = path_raw;
  std::string info;
  if (path.find("/") != std::string::npos) {
    size_t pos = path.find("/");
    info = path.substr(pos + 1, path.length() - (pos + 1));
    path = path.substr(0, pos);
  }
  path = StringToLowerASCII(path);

  std::string response;
  if (path == kDnsPath) {
    response = AboutDns();
  } else if (path == kHistogramsPath) {
    response = AboutHistograms(info);
  } else if (path == kMemoryPath) {
    AboutMemory(this, request_id);
    return;
  } else if (path == kMemoryRedirectPath) {
    response = GetAboutMemoryRedirectResponse();
  } else if (path == kObjectsPath) {
    response = AboutObjects(info);
  } else if (path == kPluginsPath) {
    response = AboutPlugins();
  } else if (path == kStatsPath) {
    response = AboutStats();
  } else if (path == kVersionPath || path.empty()) {
    response = AboutVersion();
  } else if (path == kCreditsPath) {
    response = AboutCredits();
  } else if (path == kTermsPath) {
    response = AboutTerms();
  }
#if defined(OS_LINUX)
  else if (path == kLinuxSplash) {
    response = AboutLinuxSplash();
  }
#endif

  FinishDataRequest(response, request_id);
}

void AboutSource::FinishDataRequest(const std::string& response,
                                    int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

// AboutMemoryHandler ----------------------------------------------------------

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

// Helper for AboutMemory to append memory usage information for all
// sub-processes (i.e. renderers, plugins) used by Chrome.
void AboutMemoryHandler::AppendProcess(ListValue* child_data,
                                       ProcessMemoryInformation* info) {
  DCHECK(child_data && info);

  // Append a new DictionaryValue for this renderer to our list.
  DictionaryValue* child = new DictionaryValue();
  child_data->Append(child);
  BindProcessMetrics(child, info);

  std::wstring child_label(ChildProcessInfo::GetTypeNameInEnglish(info->type));
  if (info->is_diagnostics)
    child_label.append(L" (diagnostics)");
  child->SetString(L"child_name", child_label);
  ListValue* titles = new ListValue();
  child->Set(L"titles", titles);
  for (size_t i = 0; i < info->titles.size(); ++i)
    titles->Append(new StringValue(info->titles[i]));
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
  ListValue* child_data = new ListValue();
  root.Set(L"child_data", child_data);

  ProcessData process = browser_processes[0];  // Chrome is the first browser.
  for (size_t index = 0; index < process.processes.size(); index++) {
    if (process.processes[index].type == ChildProcessInfo::BROWSER_PROCESS)
      BindProcessMetrics(browser_data, &process.processes[index]);
    else
      AppendProcess(child_data, &process.processes[index]);
  }

  // Get about_memory.html
  static const StringPiece memory_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_MEMORY_HTML));

  // Create jstemplate and return.
  std::string template_html = jstemplate_builder::GetTemplateHtml(
      memory_html, &root, "t" /* template root node id */);

  AboutSource* src = static_cast<AboutSource*>(source_);
  src->FinishDataRequest(template_html, request_id_);
}

}  // namespace

// -----------------------------------------------------------------------------

bool WillHandleBrowserAboutURL(GURL* url) {
  // We only handle about: schemes.
  if (!url->SchemeIs(chrome::kAboutScheme))
    return false;

  // about:blank is special. Frames are allowed to access about:blank,
  // but they are not allowed to access other types of about pages.
  // Just ignore the about:blank and let the TAB_CONTENTS_WEB handle it.
  if (LowerCaseEqualsASCII(url->spec(), chrome::kAboutBlankURL))
    return false;

  // Handle rewriting view-cache URLs. This allows us to load about:cache.
  if (LowerCaseEqualsASCII(url->spec(), chrome::kAboutCacheURL)) {
    // Create an mapping from about:cache to the view-cache: internal URL.
    *url = GURL(std::string(chrome::kViewCacheScheme) + ":");
    return true;
  }

  // Handle URL to crash the browser process.
  if (LowerCaseEqualsASCII(url->spec(), chrome::kAboutBrowserCrash)) {
    // Induce an intentional crash in the browser process.
    int* bad_pointer = NULL;
    *bad_pointer = 42;
    return true;
  }

  // There are a few about: URLs that we hand over to the renderer. If the
  // renderer wants them, don't do any rewriting.
  if (AboutHandler::WillHandle(*url))
    return false;

  // Anything else requires our special handler, make sure its initialized.
  // We only need to register the AboutSource once and it is kept globally.
  // There is currently no way to remove a data source.
  static bool initialized = false;
  if (!initialized) {
    about_source = new AboutSource();
    initialized = true;
  }

  // Special case about:memory to go through a redirect before ending up on
  // the final page. See GetAboutMemoryRedirectResponse above for why.
  if (LowerCaseEqualsASCII(url->path(), kMemoryPath)) {
    *url = GURL("chrome://about/memory-redirect");
    return true;
  }

  // Rewrite the about URL to use chrome:. WebKit treats all about URLS the
  // same (blank page), so if we want to display content, we need another
  // scheme.
  std::string about_url = "chrome://about/";
  about_url.append(url->path());
  *url = GURL(about_url);
  return true;
}

// This function gets called with the fixed-up chrome: URLs, so we have to
// compare against those instead of "about:blah".
bool HandleNonNavigationAboutURL(const GURL& url) {
  // About:network and IPC and currently buggy, so we disable it for official
  // builds.
#if defined(OS_WIN) && !defined(OFFICIAL_BUILD)
  if (LowerCaseEqualsASCII(url.spec(), chrome::kChromeUINetworkURL)) {
    // Run the dialog. This will re-use the existing one if it's already up.
    AboutNetworkDialog::RunDialog();
    return true;
  }

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (LowerCaseEqualsASCII(url.spec(), chrome::kChromeUIIPCURL)) {
    // Run the dialog. This will re-use the existing one if it's already up.
    AboutIPCDialog::RunDialog();
    return true;
  }
#endif

#else
  // TODO(port) Implement this.
#endif
  return false;
}
