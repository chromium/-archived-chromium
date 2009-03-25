// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_service.h"
#include "webkit/glue/webkit_glue.h"

// Sets up proxy info if overrides were specified on the command line.
// Otherwise returns NULL (meaning we should use the system defaults).
// The caller is responsible for deleting returned pointer.
static net::ProxyInfo* CreateProxyInfo(const CommandLine& command_line) {
  net::ProxyInfo* proxy_info = NULL;

  if (command_line.HasSwitch(switches::kProxyServer)) {
    proxy_info = new net::ProxyInfo();
    const std::wstring& proxy_server =
        command_line.GetSwitchValue(switches::kProxyServer);
    proxy_info->UseNamedProxy(WideToASCII(proxy_server));
  }

  return proxy_info;
}

// Create a proxy service according to the options on command line.
static net::ProxyService* CreateProxyService(URLRequestContext* context,
                                             const CommandLine& command_line) {
  scoped_ptr<net::ProxyInfo> proxy_info(CreateProxyInfo(command_line));

  bool use_v8 = command_line.HasSwitch(switches::kV8ProxyResolver);
  if (use_v8 && command_line.HasSwitch(switches::kSingleProcess)) {
    // See the note about V8 multithreading in net/proxy/proxy_resolver_v8.h
    // to understand why we have this limitation.
    LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    use_v8 = false;  // Fallback to non-v8 implementation.
  }

  return use_v8 ?
    net::ProxyService::CreateUsingV8Resolver(proxy_info.get(), context) :
    net::ProxyService::Create(proxy_info.get());
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOriginal(
    Profile* profile, const FilePath& cookie_store_path,
    const FilePath& disk_cache_path) {
  DCHECK(!profile->IsOffTheRecord());
  ChromeURLRequestContext* context = new ChromeURLRequestContext(profile);

  context->proxy_service_ = CreateProxyService(
      context, *CommandLine::ForCurrentProcess());

  net::HttpCache* cache =
      new net::HttpCache(context->proxy_service_,
                         disk_cache_path.ToWStringHack(), 0);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = chrome::kRecordModeEnabled &&
                     command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = command_line.HasSwitch(switches::kPlaybackMode);

  if (record_mode || playback_mode) {
    // Don't use existing cookies and use an in-memory store.
    context->cookie_store_ = new net::CookieMonster();
    cache->set_mode(
        record_mode ? net::HttpCache::RECORD : net::HttpCache::PLAYBACK);
  }
  context->http_transaction_factory_ = cache;

  // The kNewFtp switch is Windows specific only because we have multiple FTP
  // implementations on Windows.
#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kNewFtp))
    context->ftp_transaction_factory_ = new net::FtpNetworkLayer;
#else
  context->ftp_transaction_factory_ = new net::FtpNetworkLayer;
#endif

  // setup cookie store
  if (!context->cookie_store_) {
    DCHECK(!cookie_store_path.empty());
    context->cookie_db_.reset(new SQLitePersistentCookieStore(
        cookie_store_path.ToWStringHack(),
        g_browser_process->db_thread()->message_loop()));
    context->cookie_store_ = new net::CookieMonster(context->cookie_db_.get());
  }

  return context;
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOriginalForMedia(
    Profile* profile, const FilePath& disk_cache_path) {
  DCHECK(!profile->IsOffTheRecord());
  return CreateRequestContextForMedia(profile, disk_cache_path, false);
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOffTheRecord(
    Profile* profile) {
  DCHECK(profile->IsOffTheRecord());
  ChromeURLRequestContext* context = new ChromeURLRequestContext(profile);

  // Share the same proxy service as the original profile. This proxy
  // service's lifespan is dependent on the lifespan of the original profile,
  // which we reference (see above).
  context->proxy_service_ =
      profile->GetOriginalProfile()->GetRequestContext()->proxy_service();

  context->http_transaction_factory_ =
      new net::HttpCache(context->proxy_service_, 0);
  context->cookie_store_ = new net::CookieMonster;

  return context;
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOffTheRecordForMedia(
  Profile* profile, const FilePath& disk_cache_path) {
  // TODO(hclam): since we don't have an implementation of disk cache backend
  // for media files in OTR mode, we create a request context just like the
  // original one.
  DCHECK(profile->IsOffTheRecord());
  return CreateRequestContextForMedia(profile, disk_cache_path, true);
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateRequestContextForMedia(
    Profile* profile, const FilePath& disk_cache_path, bool off_the_record) {
  URLRequestContext* original_context =
      profile->GetOriginalProfile()->GetRequestContext();
  ChromeURLRequestContext* context = new ChromeURLRequestContext(profile);
  // Share the same proxy service of the common profile.
  context->proxy_service_ = original_context->proxy_service();
  // Also share the cookie store of the common profile.
  context->cookie_store_ = original_context->cookie_store();

  // Create a media cache with maximum size of kint32max (2GB).
  // TODO(hclam): make the maximum size of media cache configurable.
  net::HttpCache* original_cache =
      original_context->http_transaction_factory()->GetCache();
  net::HttpCache* cache;
  if (original_cache) {
    // Try to reuse HttpNetworkSession in the original context, assuming that
    // HttpTransactionFactory (network_layer()) of HttpCache is implemented
    // by HttpNetworkLayer so we can reuse HttpNetworkSession within it. This
    // assumption will be invalid if the original HttpCache is constructed with
    // HttpCache(HttpTransactionFactory*, disk_cache::Backend*) constructor.
    net::HttpNetworkLayer* original_network_layer =
        static_cast<net::HttpNetworkLayer*>(original_cache->network_layer());
    cache = new net::HttpCache(original_network_layer->GetSession(),
                               disk_cache_path.ToWStringHack(), kint32max);
  } else {
    // If original HttpCache doesn't exist, simply construct one with a whole
    // new set of network stack.
    cache = new net::HttpCache(original_context->proxy_service(),
                               disk_cache_path.ToWStringHack(), kint32max);
  }

  // Set the cache type to media.
  if (off_the_record) {
    cache->set_type(net::TEMP_MEDIA_CACHE);
  } else {
    cache->set_type(net::MEDIA_CACHE);
  }

  context->http_transaction_factory_ = cache;
  return context;
}

ChromeURLRequestContext::ChromeURLRequestContext(Profile* profile)
    : prefs_(profile->GetPrefs()),
      is_off_the_record_(profile->IsOffTheRecord()) {
  // Set up Accept-Language and Accept-Charset header values
  accept_language_ = net::HttpUtil::GenerateAcceptLanguageHeader(
      WideToASCII(prefs_->GetString(prefs::kAcceptLanguages)));
  accept_charset_ = net::HttpUtil::GenerateAcceptCharsetHeader(
      WideToASCII(prefs_->GetString(prefs::kDefaultCharset)));

  cookie_policy_.SetType(net::CookiePolicy::FromInt(
      prefs_->GetInteger(prefs::kCookieBehavior)));

  if (profile->GetExtensionsService()) {
    const ExtensionList* extensions =
        profile->GetExtensionsService()->extensions();
    for (ExtensionList::const_iterator iter = extensions->begin();
        iter != extensions->end(); ++iter) {
      extension_paths_[(*iter)->id()] = (*iter)->path();
    }
  }

  if (profile->GetUserScriptMaster())
    user_script_dir_path_ = profile->GetUserScriptMaster()->user_script_dir();

  prefs_->AddPrefObserver(prefs::kAcceptLanguages, this);
  prefs_->AddPrefObserver(prefs::kCookieBehavior, this);

  NotificationService::current()->AddObserver(
      this, NotificationType::EXTENSIONS_LOADED,
      NotificationService::AllSources());
}

// NotificationObserver implementation.
void ChromeURLRequestContext::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (NotificationType::PREF_CHANGED == type) {
    std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
    PrefService* prefs = Source<PrefService>(source).ptr();
    DCHECK(pref_name_in && prefs);
    if (*pref_name_in == prefs::kAcceptLanguages) {
      std::string accept_language =
          WideToASCII(prefs->GetString(prefs::kAcceptLanguages));
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &ChromeURLRequestContext::OnAcceptLanguageChange,
                            accept_language));
    } else if (*pref_name_in == prefs::kCookieBehavior) {
      net::CookiePolicy::Type type = net::CookiePolicy::FromInt(
          prefs_->GetInteger(prefs::kCookieBehavior));
      g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &ChromeURLRequestContext::OnCookiePolicyChange,
                            type));
    }
  } else if (NotificationType::EXTENSIONS_LOADED == type) {
    ExtensionPaths* new_paths = new ExtensionPaths;
    ExtensionList* extensions = Details<ExtensionList>(details).ptr();
    DCHECK(extensions);
    for (ExtensionList::const_iterator iter = extensions->begin();
         iter != extensions->end(); ++iter) {
      new_paths->insert(ExtensionPaths::value_type((*iter)->id(),
                                                   (*iter)->path()));
    }
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ChromeURLRequestContext::OnNewExtensions,
                          new_paths));
  } else {
    NOTREACHED();
  }
}

void ChromeURLRequestContext::CleanupOnUIThread() {
  // Unregister for pref notifications.
  prefs_->RemovePrefObserver(prefs::kAcceptLanguages, this);
  prefs_->RemovePrefObserver(prefs::kCookieBehavior, this);
  prefs_ = NULL;

  NotificationService::current()->RemoveObserver(
      this, NotificationType::EXTENSIONS_LOADED,
      NotificationService::AllSources());
}

FilePath ChromeURLRequestContext::GetPathForExtension(const std::string& id) {
  ExtensionPaths::iterator iter = extension_paths_.find(id);
  if (iter != extension_paths_.end()) {
    return iter->second;
  } else {
    return FilePath();
  }
}

const std::string& ChromeURLRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

void ChromeURLRequestContext::OnAcceptLanguageChange(
    std::string accept_language) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  accept_language_ =
      net::HttpUtil::GenerateAcceptLanguageHeader(accept_language);
}

void ChromeURLRequestContext::OnCookiePolicyChange(
    net::CookiePolicy::Type type) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  cookie_policy_.SetType(type);
}

void ChromeURLRequestContext::OnNewExtensions(ExtensionPaths* new_paths) {
  extension_paths_.insert(new_paths->begin(), new_paths->end());
  delete new_paths;
}

ChromeURLRequestContext::~ChromeURLRequestContext() {
  DCHECK(NULL == prefs_);

  NotificationService::current()->Notify(
      NotificationType::URL_REQUEST_CONTEXT_RELEASED,
      Source<URLRequestContext>(this),
      NotificationService::NoDetails());

  delete cookie_store_;
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;

  // Do not delete the proxy service in the case of OTR, as it is owned by the
  // original URLRequestContext.
  if (!is_off_the_record_)
    delete proxy_service_;
}
