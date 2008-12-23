// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/cookie_monster_sqlite.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"
#include "net/url_request/url_request_context.h"

class Profile;

// A URLRequestContext subclass used by the browser. This can be used to store
// extra information about requests, beyond what is supported by the base
// URLRequestContext class.
//
// All methods are expected to be called on the IO thread except the
// constructor and factories (CreateOriginal, CreateOffTheRecord), which are
// expected to be called on the UI thread.
class ChromeURLRequestContext : public URLRequestContext,
                                public NotificationObserver {
 public:
  // Create an instance for use with an 'original' (non-OTR) profile. This is
  // expected to get called on the UI thread.
  static ChromeURLRequestContext* CreateOriginal(
      Profile* profile, const std::wstring& cookie_store_path,
      const std::wstring& disk_cache_path);

  // Create an instance for use with an OTR profile. This is expected to get
  // called on the UI thread.
  static ChromeURLRequestContext* CreateOffTheRecord(Profile* profile);

  // Clean up UI thread resources. This is expected to get called on the UI
  // thread before the instance is deleted on the IO thread.
  void CleanupOnUIThread();

 private:
  // Private constructor, use the static factory methods instead. This is
  // expected to be called on the UI thread.
  ChromeURLRequestContext(Profile* profile);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Callback for when the accept language changes.
  void OnAcceptLanguageChange(std::string accept_language);

  // Callback for when the cookie policy changes.
  void OnCookiePolicyChange(net::CookiePolicy::Type type);

  // Destructor.
  virtual ~ChromeURLRequestContext();

  scoped_ptr<SQLitePersistentCookieStore> cookie_db_;
  PrefService* prefs_;
  bool is_off_the_record_;
};
