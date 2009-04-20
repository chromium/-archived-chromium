// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
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

    // GDK/GTK+ thread-safety: multi-threaded programs coordinate
    // around a global lock. See
    // http://library.gnome.org/devel/gdk/unstable/gdk-Threads.html
    // and http://research.operationaldynamics.com/blogs/andrew/software/gnome-desktop/gtk-thread-awareness.html
    // The following methods are used to grab said lock around any
    // actual gconf usage: including calls to Init() and any of the
    // Get* methods.
    virtual void Enter() = 0;
    virtual void Leave() = 0;

    // Initializes the class: obtains a gconf client, in the concrete
    // implementation. Returns true on success. Must be called before
    // any of the Get* methods, and the Get* methods must not be used
    // if this method returns false. This method may be called again,
    // whether or not it succeeded before.
    virtual bool InitIfNeeded() = 0;

    // Gets a string type value from gconf and stores it in result. Returns
    // false if the key is unset or on error.
    virtual bool GetString(const char* key, std::string* result) = 0;
    // Same thing for a bool typed value.
    virtual bool GetBoolean(const char* key, bool* result) = 0;
    // Same for an int typed value.
    virtual bool GetInt(const char* key, int* result) = 0;
    // And for a string list.
    virtual bool GetStringList(const char* key,
                               std::vector<std::string>* result) = 0;
  };

  // Usual constructor
  ProxyConfigServiceLinux();
  // For testing: pass in alternate gconf and env var getter implementations.
  // ProxyConfigServiceLinux takes ownership of the getter objects.
  ProxyConfigServiceLinux(EnvironmentVariableGetter* env_var_getter,
                          GConfSettingGetter* gconf_getter);

  // ProxyConfigService methods:
  virtual int GetProxyConfig(ProxyConfig* config);

 private:
  // Obtains an environment variable's value. Parses a proxy server
  // specification from it and puts it in result. Returns true if the
  // requested variable is defined and the value valid.
  bool GetProxyFromEnvVarForScheme(const char* variable,
                                   ProxyServer::Scheme scheme,
                                   ProxyServer* result_server);
  // As above but with scheme set to HTTP, for convenience.
  bool GetProxyFromEnvVar(const char* variable, ProxyServer* result_server);
  // Parses entries from the value of the no_proxy env var, and stuffs
  // them into config->proxy_bypass.
  void ParseNoProxyList(const std::string& no_proxy, ProxyConfig* config);
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

  scoped_ptr<EnvironmentVariableGetter> env_var_getter_;
  scoped_ptr<GConfSettingGetter> gconf_getter_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceLinux);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
