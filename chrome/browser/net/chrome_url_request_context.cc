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
#include "chrome/common/pref_names.h"
#include "net/http/http_cache.h"
#include "net/proxy/proxy_service.h"
#include "webkit/glue/webkit_glue.h"

// Sets up proxy info if it was specified, otherwise returns NULL. The
// returned pointer MUST be deleted by the caller if non-NULL.
static net::ProxyInfo* CreateProxyInfo() {
  net::ProxyInfo* proxy_info = NULL;

  CommandLine command_line;
  if (command_line.HasSwitch(switches::kProxyServer)) {
    proxy_info = new net::ProxyInfo();
    const std::wstring& proxy_server =
        command_line.GetSwitchValue(switches::kProxyServer);
    proxy_info->UseNamedProxy(WideToASCII(proxy_server));
  }

  return proxy_info;
}

// static
ChromeURLRequestContext* ChromeURLRequestContext::CreateOriginal(
    Profile* profile, const std::wstring& cookie_store_path,
    const std::wstring& disk_cache_path) {
  DCHECK(!profile->IsOffTheRecord());
  ChromeURLRequestContext* context = new ChromeURLRequestContext(profile);

  scoped_ptr<net::ProxyInfo> proxy_info(CreateProxyInfo());
  context->proxy_service_ = net::ProxyService::Create(proxy_info.get());

  net::HttpCache* cache =
      new net::HttpCache(context->proxy_service_, disk_cache_path, 0);

  CommandLine command_line;
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

  // setup cookie store
  if (!context->cookie_store_) {
    DCHECK(!cookie_store_path.empty());
    context->cookie_db_.reset(new SQLitePersistentCookieStore(
        cookie_store_path, g_browser_process->db_thread()->message_loop()));
    context->cookie_store_ = new net::CookieMonster(context->cookie_db_.get());
  }

  return context;
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

ChromeURLRequestContext::ChromeURLRequestContext(Profile* profile)
    : prefs_(profile->GetPrefs()),
      is_off_the_record_(profile->IsOffTheRecord()) {
  user_agent_ = webkit_glue::GetUserAgent();

  // set up Accept-Language and Accept-Charset header values
  // TODO(jungshik) : This may slow down http requests. Perhaps,
  // we have to come up with a better way to set up these values.
  accept_language_ = WideToASCII(prefs_->GetString(prefs::kAcceptLanguages));
  accept_charset_ = WideToASCII(prefs_->GetString(prefs::kDefaultCharset));
  accept_charset_ += ",*,utf-8";

  cookie_policy_.SetType(net::CookiePolicy::FromInt(
      prefs_->GetInteger(prefs::kCookieBehavior)));

  const ExtensionList* extensions =
      profile->GetExtensionsService()->extensions();
  for (ExtensionList::const_iterator iter = extensions->begin();
      iter != extensions->end(); ++iter) {
    extension_paths_[(*iter)->id()] = (*iter)->path();
  }

  user_script_dir_path_ = profile->GetUserScriptMaster()->user_script_dir();

  prefs_->AddPrefObserver(prefs::kAcceptLanguages, this);
  prefs_->AddPrefObserver(prefs::kCookieBehavior, this);  

  NotificationService::current()->AddObserver(
      this, NOTIFY_EXTENSIONS_LOADED, NotificationService::AllSources());
}

// NotificationObserver implementation.
void ChromeURLRequestContext::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (NOTIFY_PREF_CHANGED == type) {
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
  } else if (NOTIFY_EXTENSIONS_LOADED == type) {
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
      this, NOTIFY_EXTENSIONS_LOADED, NotificationService::AllSources());
}

FilePath ChromeURLRequestContext::GetPathForExtension(const std::string& id) {
  ExtensionPaths::iterator iter = extension_paths_.find(id);
  if (iter != extension_paths_.end()) {
    return iter->second;
  } else {
    return FilePath();
  }
}

void ChromeURLRequestContext::OnAcceptLanguageChange(std::string accept_language) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  accept_language_ = accept_language;
}

void ChromeURLRequestContext::OnCookiePolicyChange(net::CookiePolicy::Type type) {
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

  NotificationService::current()->Notify(NOTIFY_URL_REQUEST_CONTEXT_RELEASED,
                                         Source<URLRequestContext>(this),
                                         NotificationService::NoDetails());

  delete cookie_store_;
  delete http_transaction_factory_;

  // Do not delete the proxy service in the case of OTR, as it is owned by the
  // original URLRequestContext.
  if (!is_off_the_record_)
    delete proxy_service_;
}
