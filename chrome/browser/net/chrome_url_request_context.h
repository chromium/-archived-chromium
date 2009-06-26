// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/common/net/cookie_monster_sqlite.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_service.h"
#include "net/url_request/url_request_context.h"

class Profile;
class CommandLine;
namespace net {
class ProxyConfig;
}

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
  typedef std::map<std::string, FilePath> ExtensionPaths;

  // Create an instance for use with an 'original' (non-OTR) profile. This is
  // expected to get called on the UI thread.
  static ChromeURLRequestContext* CreateOriginal(
      Profile* profile, const FilePath& cookie_store_path,
      const FilePath& disk_cache_path, int cache_size);

  // Create an instance for an original profile for media. This is expected to
  // get called on UI thread. This method takes a profile and reuses the
  // 'original' URLRequestContext for common files.
  static ChromeURLRequestContext* CreateOriginalForMedia(Profile *profile,
      const FilePath& disk_cache_path, int cache_size);

  // Create an instance for an original profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContext* CreateOriginalForExtensions(Profile *profile,
      const FilePath& cookie_store_path);

  // Create an instance for use with an OTR profile. This is expected to get
  // called on the UI thread.
  static ChromeURLRequestContext* CreateOffTheRecord(Profile* profile);

  // Create an instance of request context for OTR profile for extensions.
  static ChromeURLRequestContext* CreateOffTheRecordForExtensions(
      Profile* profile);

  // Clean up UI thread resources. This is expected to get called on the UI
  // thread before the instance is deleted on the IO thread.
  void CleanupOnUIThread();

  // Gets the path to the directory for the specified extension.
  FilePath GetPathForExtension(const std::string& id);

  // Gets the path to the directory user scripts are stored in.
  FilePath user_script_dir_path() const {
    return user_script_dir_path_;
  }

  virtual const std::string& GetUserAgent(const GURL& url) const;

  virtual bool interceptCookie(const URLRequest* request, std::string* cookie);

  virtual bool allowSendingCookies(const URLRequest* request) const;

private:
  // Private constructor, use the static factory methods instead. This is
  // expected to be called on the UI thread.
  ChromeURLRequestContext(Profile* profile);

  // Create a request context for media resources from a regular request
  // context. This helper method is called from CreateOriginalForMedia and
  // CreateOffTheRecordForMedia.
  static ChromeURLRequestContext* CreateRequestContextForMedia(Profile* profile,
      const FilePath& disk_cache_path, int cache_size, bool off_the_record);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Callback for when the accept language changes.
  void OnAcceptLanguageChange(std::string accept_language);

  // Callback for when the cookie policy changes.
  void OnCookiePolicyChange(net::CookiePolicy::Type type);

  // Callback for when new extensions are loaded.
  void OnNewExtensions(ExtensionPaths* new_paths);

  // Callback for when an extension is unloaded.
  void OnUnloadedExtension(const std::string& id);

  // Destructor.
  virtual ~ChromeURLRequestContext();

  NotificationRegistrar registrar_;

  // Maps extension IDs to paths on disk. This is initialized in the
  // construtor and updated when extensions changed.
  ExtensionPaths extension_paths_;

  // Path to the directory user scripts are stored in.
  FilePath user_script_dir_path_;

  scoped_ptr<SQLitePersistentCookieStore> cookie_db_;
  PrefService* prefs_;
  bool is_media_;
  bool is_off_the_record_;
};

// Creates a proxy configuration using the overrides specified on the command
// line. Returns NULL if the system defaults should be used instead.
net::ProxyConfig* CreateProxyConfig(const CommandLine& command_line);
