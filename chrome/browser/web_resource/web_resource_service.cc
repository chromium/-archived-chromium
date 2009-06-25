// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_resource/web_resource_service.h"

#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

const wchar_t* WebResourceService::kWebResourceTitle = L"title";
const wchar_t* WebResourceService::kWebResourceURL = L"url";

class WebResourceService::WebResourceFetcher
    : public URLFetcher::Delegate {
 public:
  explicit WebResourceFetcher(WebResourceService* web_resource_service) :
      ALLOW_THIS_IN_INITIALIZER_LIST(fetcher_factory_(this)),
      web_resource_service_(web_resource_service) {
  }

  // Delay initial load of resource data into cache so as not to interfere
  // with startup time.
  void StartAfterDelay(int delay_ms) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      fetcher_factory_.NewRunnableMethod(&WebResourceFetcher::StartFetch),
                                         delay_ms);
  }

  // Initializes the fetching of data from the resource server.  Data
  // load calls OnURLFetchComplete.
  void StartFetch() {
    // First, put our next cache load on the MessageLoop.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      fetcher_factory_.NewRunnableMethod(&WebResourceFetcher::StartFetch),
                                         kCacheUpdateDelay);
    // If we are still fetching data, exit.
    if (web_resource_service_->in_fetch_)
      return;

    url_fetcher_.reset(new URLFetcher(GURL(
        WideToUTF8(web_resource_service_->web_resource_server_)),
        URLFetcher::GET, this));
    // Do not let url fetcher affect existing state in profile (by setting
    // cookies, for example.
    url_fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES);
    url_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
    url_fetcher_->Start();
  }

  // From URLFetcher::Delegate.
  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data) {
    // Delete the URLFetcher when this function exits.
    scoped_ptr<URLFetcher> clean_up_fetcher(url_fetcher_.release());

    // Don't parse data if attempt to download was unsuccessful.
    // Stop loading new web resource data, and silently exit.
    if (!status.is_success() || (response_code != 200))
      return;

    web_resource_service_->UpdateResourceCache(data);
  }

 private:
  // So that we can delay our start so as not to affect start-up time; also,
  // so that we can schedule future cache updates.
  ScopedRunnableMethodFactory<WebResourceFetcher> fetcher_factory_;

  // The tool that fetches the url data from the server.
  scoped_ptr<URLFetcher> url_fetcher_;

  // Our owner and creator.
  scoped_ptr<WebResourceService> web_resource_service_;
};

// This class coordinates a web resource unpack and parse task which is run in
// a separate process.  Results are sent back to this class and routed to
// the WebResourceService.
class WebResourceService::UnpackerClient
    : public UtilityProcessHost::Client {
 public:
  UnpackerClient(WebResourceService* web_resource_service,
                 const std::string& json_data)
    : web_resource_service_(web_resource_service),
      json_data_(json_data) {
  }

  void Start() {
    AddRef();  // balanced in Cleanup.

    if (web_resource_service_->resource_dispatcher_host_) {
      ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(FROM_HERE,
          NewRunnableMethod(this, &UnpackerClient::StartProcessOnIOThread,
                            web_resource_service_->resource_dispatcher_host_,
                            MessageLoop::current()));
    } else {
      // TODO(mrc): unit tests here.
    }
  }

 private:
  virtual void OnUnpackWebResourceSucceeded(const ListValue& parsed_json) {
    web_resource_service_->OnWebResourceUnpacked(parsed_json);
    Release();
  }

  virtual void OnUnpackWebResourceFailed(const std::string& error_message) {
    web_resource_service_->EndFetch();
    Release();
  }

  void StartProcessOnIOThread(ResourceDispatcherHost* rdh,
                              MessageLoop* file_loop) {
    UtilityProcessHost* host = new UtilityProcessHost(rdh, this, file_loop);
    // TODO(mrc): get proper file path when we start using web resources
    // that need to be unpacked.
    host->StartWebResourceUnpacker(json_data_);
  }

  scoped_refptr<WebResourceService> web_resource_service_;

  // Holds raw JSON string.
  const std::string& json_data_;
};

// TODO(mrc): make into a changeable preference.
const wchar_t* WebResourceService::kDefaultResourceServer =
    L"http://www.google.com/labs/popgadget/world?view=json";

const char* WebResourceService::kResourceDirectoryName =
    "Resources";

WebResourceService::WebResourceService(Profile* profile,
                                       MessageLoop* backend_loop) :
    prefs_(profile->GetPrefs()),
    web_resource_dir_(profile->GetPath().AppendASCII(kResourceDirectoryName)),
    backend_loop_(backend_loop),
    in_fetch_(false) {
  Init();
}

WebResourceService::~WebResourceService() { }

void WebResourceService::Init() {
  resource_dispatcher_host_ = g_browser_process->resource_dispatcher_host();
  web_resource_fetcher_ = new WebResourceFetcher(this);
  prefs_->RegisterStringPref(prefs::kNTPTipsCacheUpdate, L"0");
  // TODO(mrc): make sure server name is valid.
  web_resource_server_ = prefs_->HasPrefPath(prefs::kNTPTipsServer) ?
      prefs_->GetString(prefs::kNTPTipsServer) :
      kDefaultResourceServer;
}

void WebResourceService::EndFetch() {
  in_fetch_ = false;
}

void WebResourceService::OnWebResourceUnpacked(const ListValue& parsed_json) {
  // Get dictionary of cached preferences.
  web_resource_cache_ =
      prefs_->GetMutableDictionary(prefs::kNTPTipsCache);
  ListValue::const_iterator wr_iter = parsed_json.begin();
  int wr_counter = 0;

  // These values store the data for each new web resource item.
  std::wstring result_snippet;
  std::wstring result_url;
  std::wstring result_source;
  std::wstring result_title;
  std::wstring result_title_type;
  std::wstring result_thumbnail;

  // Iterate through newly parsed preferences, replacing stale cache with
  // new data.
  // TODO(mrc): make this smarter, so it actually only replaces stale data,
  // instead of overwriting the whole thing every time.
  while (wr_iter != parsed_json.end() &&
         wr_counter < kMaxResourceCacheSize) {
    // Each item is stored in the form of a dictionary.
    // See tips_handler.h for format (this will change until
    // tip services are solidified!).
    if (!(*wr_iter)->IsType(Value::TYPE_DICTIONARY))
      continue;
    DictionaryValue* wr_dict =
        static_cast<DictionaryValue*>(*wr_iter);

    // Get next space for resource in prefs file.
    Value* current_wr;
    std::wstring wr_counter_str = IntToWString(wr_counter);
    // Create space if it doesn't exist yet.
    if (!web_resource_cache_->Get(wr_counter_str, &current_wr) ||
      !current_wr->IsType(Value::TYPE_DICTIONARY)) {
        current_wr = new DictionaryValue();
        web_resource_cache_->Set(wr_counter_str, current_wr);
    }
    DictionaryValue* wr_cache_dict =
      static_cast<DictionaryValue*>(current_wr);

    // Update the resource cache.
    if (wr_dict->GetString(kWebResourceURL, &result_url))
      wr_cache_dict->SetString(kWebResourceURL, result_url);
    if (wr_dict->GetString(kWebResourceTitle, &result_title))
      wr_cache_dict->SetString(kWebResourceTitle, result_title);

    wr_counter++;
    wr_iter++;
  }
  EndFetch();
}

void WebResourceService::StartAfterDelay() {
  int64 delay = kStartResourceFetchDelay;
  // Check whether we have ever put a value in the web resource cache;
  // if so, pull it out and see if it's time to update again.
  if (prefs_->HasPrefPath(prefs::kNTPTipsCacheUpdate)) {
    std::wstring last_update_pref =
      prefs_->GetString(prefs::kNTPTipsCacheUpdate);
    int64 ms_since_update =
        (base::Time::Now() - base::Time::FromDoubleT(
        StringToDouble(WideToASCII(last_update_pref)))).InMilliseconds();

    delay = kStartResourceFetchDelay +
        (ms_since_update > kCacheUpdateDelay ?
         0 : kCacheUpdateDelay - ms_since_update);
  }

  // Start fetch and wait for UpdateResourceCache.
  DCHECK(delay >= kStartResourceFetchDelay &&
         delay <= kCacheUpdateDelay);
  web_resource_fetcher_->StartAfterDelay(static_cast<int>(delay));
}

void WebResourceService::UpdateResourceCache(const std::string& json_data) {
  UnpackerClient* client = new UnpackerClient(this, json_data);
  client->Start();

  // Update resource server and cache update time in preferences.
  prefs_->SetString(prefs::kNTPTipsCacheUpdate,
      DoubleToWString(base::Time::Now().ToDoubleT()));
  prefs_->SetString(prefs::kNTPTipsServer, web_resource_server_);
}

