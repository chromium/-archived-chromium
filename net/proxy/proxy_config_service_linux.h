// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_server.h"

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings from environment variables or gconf.
class ProxyConfigServiceLinux : public ProxyConfigService {
 public:

  // These are used to derive mocks for unittests.
  class EnvironmentVariableGetter {
   public:
    virtual ~EnvironmentVariableGetter() {}
    // Gets an environment variable's value and stores it in
    // result. Returns false if the key is unset.
    virtual bool Getenv(const char* variable_name, std::string* result) = 0;
  };

  class GConfSettingGetter {
   public:
    virtual ~GConfSettingGetter() {}

    // Initializes the class: obtains a gconf client, in the concrete
    // implementation. Returns true on success. Must be called before
    // using other methods.
    virtual bool Init() = 0;

    // Releases the gconf client, which clears cached directories and
    // stops notifications.
    virtual void Release() = 0;

    // Requests notification of gconf setting changes for proxy
    // settings. Returns true on success.
    virtual bool SetupNotification(void* callback_user_data) = 0;

    // Gets a string type value from gconf and stores it in
    // result. Returns false if the key is unset or on error. Must
    // only be called after a successful call to Init(), and not
    // after a failed call to SetupNotification() or after calling
    // Release().
    virtual bool GetString(const char* key, std::string* result) = 0;
    // Same thing for a bool typed value.
    virtual bool GetBoolean(const char* key, bool* result) = 0;
    // Same for an int typed value.
    virtual bool GetInt(const char* key, int* result) = 0;
    // And for a string list.
    virtual bool GetStringList(const char* key,
                               std::vector<std::string>* result) = 0;
  };

  // ProxyConfigServiceLinux is created on the UI thread, and
  // SetupAndFetchInitialConfig() is immediately called to
  // synchronously fetch the original configuration and setup gconf
  // notifications on the UI thread.
  //
  // Passed that point, it is accessed periodically through
  // GetProxyConfig() from the IO thread.
  //
  // gconf change notification callbacks can occur at any time and are
  // run on the UI thread. The new gconf settings are fetched on the
  // UI thread, and the new resulting proxy config is posted to the IO
  // thread through Delegate::SetNewProxyConfig().
  //
  // ProxyConfigServiceLinux is deleted from the IO thread.
  //
  // The substance of the ProxyConfigServiceLinux implementation is
  // wrapped in the Delegate ref counted class. On deleting the
  // ProxyConfigServiceLinux, Delegate::OnDestroy() is posted to the
  // UI thread where gconf notifications will be safely stopped before
  // releasing Delegate.

  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    // Constructor receives gconf and env var getter implementations
    // to use, and takes ownership of them.
    Delegate(EnvironmentVariableGetter* env_var_getter,
             GConfSettingGetter* gconf_getter);
    // Synchronously obtains the proxy configuration. If gconf is
    // used, also enables gconf notification for setting
    // changes. gconf must only be accessed from the thread running
    // the default glib main loop, and so this method must be called
    // from the UI thread. The message loop for the IO thread is
    // specified so that notifications can post tasks to it (and for
    // assertions).
    void SetupAndFetchInitialConfig(MessageLoop* glib_default_loop,
                                    MessageLoop* io_loop);
    // Resets cached_config_ and releases the gconf_getter_, making it
    // possible to call SetupAndFetchInitialConfig() again. Only used
    // in testing.
    void Reset();

    // Handler for gconf change notifications: fetches a new proxy
    // configuration from gconf settings, and if this config is
    // different than what we had before, posts a task to have it
    // stored in cached_config_.
    // Left public for simplicity.
    void OnCheckProxyConfigSettings();

    // Called from IO thread.
    int GetProxyConfig(ProxyConfig* config);

    // Posts a call to OnDestroy() to the UI thread. Called from
    // ProxyConfigServiceLinux's destructor.
    void PostDestroyTask();
    // Safely stops gconf notifications. Posted to the UI thread.
    void OnDestroy();

   private:
    // Obtains an environment variable's value. Parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromEnvVarForScheme(const char* variable,
                                     ProxyServer::Scheme scheme,
                                     ProxyServer* result_server);
    // As above but with scheme set to HTTP, for convenience.
    bool GetProxyFromEnvVar(const char* variable, ProxyServer* result_server);
    // Fills proxy config from environment variables. Returns true if
    // variables were found and the configuration is valid.
    bool GetConfigFromEnv(ProxyConfig* config);

    // Obtains host and port gconf settings and parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromGConf(const char* key_prefix, bool is_socks,
                           ProxyServer* result_server);
    // Fills proxy config from gconf. Returns true if settings were found
    // and the configuration is valid.
    bool GetConfigFromGConf(ProxyConfig* config);

    // Returns true if environment variables indicate that we are
    // running GNOME (and therefore we want to use gconf settings).
    bool ShouldTryGConf();

    // This method is posted from the UI thread to the IO thread to
    // carry the new config information.
    void SetNewProxyConfig(const ProxyConfig& new_config);

    scoped_ptr<EnvironmentVariableGetter> env_var_getter_;
    scoped_ptr<GConfSettingGetter> gconf_getter_;

    // Cached proxy configuration, to be returned by
    // GetProxyConfig. Initially populated from the UI thread, but
    // afterwards only accessed from the IO thread.
    ProxyConfig cached_config_;

    // A copy kept on the UI thread of the last seen proxy config, so as
    // to avoid posting a call to SetNewProxyConfig when we get a
    // notification but the config has not actually changed.
    ProxyConfig reference_config_;

    // The MessageLoop for the UI thread, aka main browser thread. This
    // thread is where we run the glib main loop (see
    // base/message_pump_glib.h). It is the glib default loop in the
    // sense that it runs the glib default context: as in the context
    // where sources are added by g_timeout_add and g_idle_add, and
    // returned by g_main_context_default. gconf uses glib timeouts and
    // idles and possibly other callbacks that will all be dispatched on
    // this thread. Since gconf is not thread safe, any use of gconf
    // must be done on the thread running this loop.
    MessageLoop* glib_default_loop_;
    // MessageLoop for the IO thread. GetProxyConfig() is called from
    // the thread running this loop.
    MessageLoop* io_loop_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Thin wrapper shell around Delegate.

  // Usual constructor
  ProxyConfigServiceLinux();
  // For testing: takes alternate gconf and env var getter implementations.
  ProxyConfigServiceLinux(EnvironmentVariableGetter* env_var_getter,
                          GConfSettingGetter* gconf_getter);

  virtual ~ProxyConfigServiceLinux() {
    delegate_->PostDestroyTask();
  }

  void SetupAndFetchInitialConfig(MessageLoop* glib_default_loop,
                                  MessageLoop* io_loop) {
    delegate_->SetupAndFetchInitialConfig(glib_default_loop, io_loop);
  }
  void Reset() {
    delegate_->Reset();
  }
  void OnCheckProxyConfigSettings() {
    delegate_->OnCheckProxyConfigSettings();
  }

  // ProxyConfigService methods:
  // Called from IO thread.
  virtual int GetProxyConfig(ProxyConfig* config) {
    return delegate_->GetProxyConfig(config);
  }

 private:
  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceLinux);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
