// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_linux.h"

#include <gconf/gconf-client.h>
#include <gdk/gdk.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "googleurl/src/url_canon.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"

namespace net {

namespace {

class EnvironmentVariableGetterImpl
    : public ProxyConfigServiceLinux::EnvironmentVariableGetter {
 public:
  virtual bool Getenv(const char* variable_name, std::string* result) {
    const char* env_value = ::getenv(variable_name);
    if (env_value) {
      // Note that the variable may be defined but empty.
      *result = env_value;
      return true;
    }
    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (first_char >= 'a' && first_char <= 'z')
      alternate_case_var = StringToUpperASCII(std::string(variable_name));
    else if (first_char >= 'A' && first_char <= 'Z')
      alternate_case_var = StringToLowerASCII(std::string(variable_name));
    else
      return false;
    env_value = ::getenv(alternate_case_var.c_str());
    if (env_value) {
      *result = env_value;
      return true;
    }
    return false;
  }
};

// Given a proxy hostname from a setting, returns that hostname with
// an appropriate proxy server scheme prefix.
// scheme indicates the desired proxy scheme: usually http, with
// socks 4 or 5 as special cases.
std::string FixupProxyHostScheme(ProxyServer::Scheme scheme,
                                 std::string host) {
  if (scheme == ProxyServer::SCHEME_SOCKS4 &&
      StartsWithASCII(host, "socks5://", false)) {
    // We default to socks 4, but if the user specifically set it to
    // socks5://, then use that.
    scheme = ProxyServer::SCHEME_SOCKS5;
  }
  // Strip the scheme if any.
  std::string::size_type colon = host.find("://");
  if (colon != std::string::npos)
    host = host.substr(colon + 3);
  // If a username and perhaps password are specified, give a warning.
  std::string::size_type at_sign = host.find("@");
  // Should this be supported?
  if (at_sign != std::string::npos) {
    LOG(ERROR) << "ProxyConfigServiceLinux: proxy authentication "
        "not supported";
    // Disregard the authentication parameters and continue with this hostname.
    host = host.substr(at_sign + 1);
  }
  // If this is a socks proxy, prepend a scheme so as to tell
  // ProxyServer. This also allows ProxyServer to choose the right
  // default port.
  if (scheme == ProxyServer::SCHEME_SOCKS4)
    host = "socks4://" + host;
  else if (scheme == ProxyServer::SCHEME_SOCKS5)
    host = "socks5://" + host;
  return host;
}

}  // namespace

bool ProxyConfigServiceLinux::GetProxyFromEnvVarForScheme(
    const char* variable, ProxyServer::Scheme scheme,
    ProxyServer* result_server) {
  std::string env_value;
  if (env_var_getter_->Getenv(variable, &env_value)) {
    if (!env_value.empty()) {
      env_value = FixupProxyHostScheme(scheme, env_value);
      ProxyServer proxy_server = ProxyServer::FromURI(env_value);
      if (proxy_server.is_valid() && !proxy_server.is_direct()) {
        *result_server = proxy_server;
        return true;
      } else {
        LOG(ERROR) << "ProxyConfigServiceLinux: failed to parse "
                   << "environment variable " << variable;
      }
    }
  }
  return false;
}

bool ProxyConfigServiceLinux::GetProxyFromEnvVar(
    const char* variable, ProxyServer* result_server) {
  return GetProxyFromEnvVarForScheme(variable, ProxyServer::SCHEME_HTTP,
                                     result_server);
}

namespace {

// Returns true if the given string represents an IP address.
bool IsIPAddress(const std::string& domain) {
  // From GURL::HostIsIPAddress()
  url_canon::RawCanonOutputT<char, 128> ignored_output;
  url_parse::Component ignored_component;
  url_parse::Component domain_comp(0, domain.size());
  return url_canon::CanonicalizeIPAddress(domain.c_str(), domain_comp,
                                          &ignored_output,
                                          &ignored_component);
}

}  // namespace

void ProxyConfigServiceLinux::ParseNoProxyList(const std::string& no_proxy,
                                               ProxyConfig* config) {
  if (no_proxy.empty())
    return;
  // Traditional semantics:
  // A single "*" is specifically allowed and unproxies anything.
  // "*" wildcards other than a single "*" entry are not universally
  // supported. We will support them, as we get * wildcards for free
  // (see MatchPattern() called from ProxyService::ShouldBypassProxyForURL()).
  // no_proxy is a comma-separated list of <trailing_domain>[:<port>].
  // If no port is specified then any port matches.
  // The historical definition has trailing_domain match using a simple
  // string "endswith" test, so that the match need not correspond to a
  // "." boundary. For example: "google.com" matches "igoogle.com" too.
  // Seems like that could be confusing, but we'll obey tradition.
  // IP CIDR patterns are supposed to be supported too. We intend
  // to do this in proxy_service.cc, but it's currently a TODO.
  // See: http://crbug.com/9835.
  StringTokenizer no_proxy_list(no_proxy, ",");
  while (no_proxy_list.GetNext()) {
    std::string bypass_entry = no_proxy_list.token();
    TrimWhitespaceASCII(bypass_entry, TRIM_ALL, &bypass_entry);
    if (bypass_entry.empty())
      continue;
    if (bypass_entry.at(0) != '*') {
      // Insert a wildcard * to obtain an endsWith match, unless the
      // entry looks like it might be an IP or CIDR.
      // First look for either a :<port> or CIDR mask length suffix.
      std::string::const_iterator begin = bypass_entry.begin();
      std::string::const_iterator scan = bypass_entry.end() - 1;
      while (scan > begin && IsAsciiDigit(*scan))
        --scan;
      std::string potential_ip;
      if (*scan == '/' || *scan == ':')
        potential_ip = std::string(begin, scan - 1);
      else
        potential_ip = bypass_entry;
      if (!IsIPAddress(potential_ip)) {
        // Do insert a wildcard.
        bypass_entry.insert(0, "*");
      }
      // TODO(sdoyon): When CIDR matching is implemented in
      // proxy_service.cc, consider making config->proxy_bypass more
      // sophisticated to avoid parsing out the string on every
      // request.
    }
    config->proxy_bypass.push_back(bypass_entry);
  }
}

bool ProxyConfigServiceLinux::GetConfigFromEnv(ProxyConfig* config) {
  // Check for automatic configuration first, in
  // "auto_proxy". Possibly only the "environment_proxy" firefox
  // extension has ever used this, but it still sounds like a good
  // idea.
  std::string auto_proxy;
  if (env_var_getter_->Getenv("auto_proxy", &auto_proxy)) {
    if (auto_proxy.empty()) {
      // Defined and empty => autodetect
      config->auto_detect = true;
    } else {
      // specified autoconfig URL
      config->pac_url = GURL(auto_proxy);
    }
    return true;
  }
  // "all_proxy" is a shortcut to avoid defining {http,https,ftp}_proxy.
  ProxyServer proxy_server;
  if (GetProxyFromEnvVar("all_proxy", &proxy_server)) {
    config->proxy_rules.type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
    config->proxy_rules.single_proxy = proxy_server;
  } else {
    bool have_http = GetProxyFromEnvVar("http_proxy", &proxy_server);
    if (have_http)
      config->proxy_rules.proxy_for_http = proxy_server;
    // It would be tempting to let http_proxy apply for all protocols
    // if https_proxy and ftp_proxy are not defined. Googling turns up
    // several documents that mention only http_proxy. But then the
    // user really might not want to proxy https. And it doesn't seem
    // like other apps do this. So we will refrain.
    bool have_https = GetProxyFromEnvVar("https_proxy", &proxy_server);
    if (have_https)
      config->proxy_rules.proxy_for_https = proxy_server;
    bool have_ftp = GetProxyFromEnvVar("ftp_proxy", &proxy_server);
    if (have_ftp)
      config->proxy_rules.proxy_for_ftp = proxy_server;
    if (have_http || have_https || have_ftp) {
      // mustn't change type unless some rules are actually set.
      config->proxy_rules.type = ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
    }
  }
  if (config->proxy_rules.empty()) {
    // If the above were not defined, try for socks.
    ProxyServer::Scheme scheme = ProxyServer::SCHEME_SOCKS4;
    std::string env_version;
    if (env_var_getter_->Getenv("SOCKS_VERSION", &env_version)
        && env_version.compare("5") == 0)
      scheme = ProxyServer::SCHEME_SOCKS5;
    if (GetProxyFromEnvVarForScheme("SOCKS_SERVER", scheme, &proxy_server)) {
      config->proxy_rules.type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
      config->proxy_rules.single_proxy = proxy_server;
    }
  }
  // Look for the proxy bypass list.
  std::string no_proxy;
  env_var_getter_->Getenv("no_proxy", &no_proxy);
  if (config->proxy_rules.empty()) {
    // Having only "no_proxy" set, presumably to "*", makes it
    // explicit that env vars do specify a configuration: having no
    // rules specified only means the user explicitly asks for direct
    // connections.
    return !no_proxy.empty();
  }
  ParseNoProxyList(no_proxy, config);
  return true;
}

namespace {

class GConfSettingGetterImpl
    : public ProxyConfigServiceLinux::GConfSettingGetter {
 public:
  GConfSettingGetterImpl() : client_(NULL) {}
  virtual ~GConfSettingGetterImpl() {
    if (client_)
      g_object_unref(client_);
  }

  virtual void Enter() {
    gdk_threads_enter();
  }
  virtual void Leave() {
    gdk_threads_leave();
  }

  virtual bool InitIfNeeded() {
    if (!client_) {
      Enter();
      client_ = gconf_client_get_default();
      Leave();
      // It's not clear whether/when this can return NULL.
      if (!client_)
        LOG(ERROR) << "ProxyConfigServiceLinux: Unable to create "
            "a gconf client";
    }
    return client_ != NULL;
  }

  virtual bool GetString(const char* key, std::string* result) {
    CHECK(client_);
    GError* error = NULL;
    gchar* value = gconf_client_get_string(client_, key, &error);
    if (HandleGError(error, key))
      return false;
    if (!value)
      return false;
    *result = value;
    g_free(value);
    return true;
  }
  virtual bool GetBoolean(const char* key, bool* result) {
    CHECK(client_);
    GError* error = NULL;
    // We want to distinguish unset values from values defaulting to
    // false. For that we need to use the type-generic
    // gconf_client_get() rather than gconf_client_get_bool().
    GConfValue* gconf_value = gconf_client_get(client_, key, &error);
    if (HandleGError(error, key))
      return false;
    if (!gconf_value) {
      // Unset.
      return false;
    }
    if (gconf_value->type != GCONF_VALUE_BOOL) {
      gconf_value_free(gconf_value);
      return false;
    }
    gboolean bool_value = gconf_value_get_bool(gconf_value);
    *result = static_cast<bool>(bool_value);
    gconf_value_free(gconf_value);
    return true;
  }
  virtual bool GetInt(const char* key, int* result) {
    CHECK(client_);
    GError* error = NULL;
    int value = gconf_client_get_int(client_, key, &error);
    if (HandleGError(error, key))
      return false;
    // We don't bother to distinguish an unset value because callers
    // don't care. 0 is returned if unset.
    *result = value;
    return true;
  }
  virtual bool GetStringList(const char* key,
                             std::vector<std::string>* result) {
    CHECK(client_);
    GError* error = NULL;
    GSList* list = gconf_client_get_list(client_, key,
                                         GCONF_VALUE_STRING, &error);
    if (HandleGError(error, key))
      return false;
    if (!list) {
      // unset
      return false;
    }
    for (GSList *it = list; it; it = it->next) {
      result->push_back(static_cast<char*>(it->data));
      g_free(it->data);
    }
    g_slist_free(list);
    return true;
  }

 private:
  // Logs and frees a glib error. Returns false if there was no error
  // (error is NULL).
  bool HandleGError(GError* error, const char* key) {
    if (error != NULL) {
      LOG(ERROR) << "ProxyConfigServiceLinux: error getting gconf value for "
                 << key << ": " << error->message;
      g_error_free(error);
      return true;
    }
    return false;
  }

  GConfClient* client_;

  DISALLOW_COPY_AND_ASSIGN(GConfSettingGetterImpl);
};

}  // namespace

bool ProxyConfigServiceLinux::GetProxyFromGConf(
    const char* key_prefix, bool is_socks, ProxyServer* result_server) {
  std::string key(key_prefix);
  std::string host;
  if (!gconf_getter_->GetString((key + "host").c_str(), &host)
      || host.empty()) {
    // Unset or empty.
    return false;
  }
  // Check for an optional port.
  int port;
  gconf_getter_->GetInt((key + "port").c_str(), &port);
  if (port != 0) {
    // If a port is set and non-zero:
    host += ":" + IntToString(port);
  }
  host = FixupProxyHostScheme(
      is_socks ? ProxyServer::SCHEME_SOCKS4 : ProxyServer::SCHEME_HTTP,
      host);
  ProxyServer proxy_server = ProxyServer::FromURI(host);
  if (proxy_server.is_valid()) {
    *result_server = proxy_server;
    return true;
  }
  return false;
}

bool ProxyConfigServiceLinux::GetConfigFromGConf(ProxyConfig* config) {
  std::string mode;
  if (!gconf_getter_->GetString("/system/proxy/mode", &mode)) {
    // We expect this to always be set, so if we don't see it then we
    // probably have a gconf problem, and so we don't have a valid
    // proxy config.
    return false;
  }
  if (mode.compare("none") == 0)
    // Specifically specifies no proxy.
    return true;

  if (mode.compare("auto") == 0) {
    // automatic proxy config
    std::string pac_url_str;
    if (gconf_getter_->GetString("/system/proxy/autoconfig_url",
                                 &pac_url_str)) {
      if (!pac_url_str.empty()) {
        GURL pac_url(pac_url_str);
        if (!pac_url.is_valid())
          return false;
        config->pac_url = pac_url;
        return true;
      }
    }
    config->auto_detect = true;
    return true;
  }

  if (mode.compare("manual") != 0) {
    // Mode is unrecognized.
    return false;
  }
  bool use_http_proxy;
  if (gconf_getter_->GetBoolean("/system/http_proxy/use_http_proxy",
                                &use_http_proxy)
      && !use_http_proxy) {
    // Another master switch for some reason. If set to false, then no
    // proxy. But we don't panic if the key doesn't exist.
    return true;
  }

  bool same_proxy = false;
  // Indicates to use the http proxy for all protocols. This one may
  // not exist (presumably on older versions), assume false in that
  // case.
  gconf_getter_->GetBoolean("/system/http_proxy/use_same_proxy",
                            &same_proxy);

  ProxyServer proxy_server;
  if (!same_proxy) {
    // Try socks.
    if (GetProxyFromGConf("/system/proxy/socks_", true, &proxy_server)) {
      // gconf settings do not appear to distinguish between socks
      // version. We default to version 4.
      config->proxy_rules.type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
      config->proxy_rules.single_proxy = proxy_server;
    }
  }
  if (config->proxy_rules.empty()) {
    bool have_http = GetProxyFromGConf("/system/http_proxy/", false,
                                       &proxy_server);
    if (same_proxy) {
      if (have_http) {
        config->proxy_rules.type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
        config->proxy_rules.single_proxy = proxy_server;
      }
    } else {
      // Protocol specific settings.
      if (have_http)
        config->proxy_rules.proxy_for_http = proxy_server;
      bool have_secure = GetProxyFromGConf("/system/proxy/secure_", false,
                                           &proxy_server);
      if (have_secure)
        config->proxy_rules.proxy_for_https = proxy_server;
      bool have_ftp = GetProxyFromGConf("/system/proxy/ftp_", false,
                                        &proxy_server);
      if (have_ftp)
        config->proxy_rules.proxy_for_ftp = proxy_server;
      if (have_http || have_secure || have_ftp)
        config->proxy_rules.type =
            ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
    }
  }

  if (config->proxy_rules.empty()) {
    // Manual mode but we couldn't parse any rules.
    return false;
  }

  // Check for authentication, just so we can warn.
  bool use_auth;
  gconf_getter_->GetBoolean("/system/http_proxy/use_authentication",
                            &use_auth);
  if (use_auth)
    LOG(ERROR) << "ProxyConfigServiceLinux: proxy authentication "
        "not supported";

  // Now the bypass list.
  gconf_getter_->GetStringList("/system/http_proxy/ignore_hosts",
                               &config->proxy_bypass);
  // Note that there are no settings with semantics corresponding to
  // config->proxy_bypass_local_names.

  return true;
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux(
    EnvironmentVariableGetter* env_var_getter,
    GConfSettingGetter* gconf_getter)
    : env_var_getter_(env_var_getter), gconf_getter_(gconf_getter) {
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux()
    : env_var_getter_(new EnvironmentVariableGetterImpl()),
      gconf_getter_(new GConfSettingGetterImpl()) {
}

int ProxyConfigServiceLinux::GetProxyConfig(ProxyConfig* config) {
  // GNOME_DESKTOP_SESSION_ID being defined is a good indication that
  // we are probably running under GNOME.
  // Note: KDE_FULL_SESSION is a corresponding env var to recognize KDE.
  std::string dummy, desktop_session;
  bool ok = false;
  if (env_var_getter_->Getenv("GNOME_DESKTOP_SESSION_ID", &dummy)
      || (env_var_getter_->Getenv("DESKTOP_SESSION", &desktop_session)
          && desktop_session.compare("gnome"))) {
    // Get settings from gconf.
    //
    // I (sdoyon) would have liked to prioritize environment variables
    // and only fallback to gconf if env vars were unset. But
    // gnome-terminal "helpfully" sets http_proxy and no_proxy, and it
    // does so even if the proxy mode is set to auto, which would
    // mislead us.
    //
    // We could introduce a CHROME_PROXY_OBEY_ENV_VARS variable...??
    if (gconf_getter_->InitIfNeeded()) {
      gconf_getter_->Enter();
      ok = GetConfigFromGConf(config);
      gconf_getter_->Leave();
      if (ok)
        LOG(INFO) << "ProxyConfigServiceLinux: obtained proxy setting "
            "from gconf";
      // If gconf proxy mode is "none", meaning direct, then we take
      // that to be a valid config and will not check environment variables.
      // The alternative would have been to look for a proxy whereever
      // we can find one.
      //
      // TODO(sdoyon): Consider wiring in the gconf notification
      // system. Cache this result config to return on subsequent calls,
      // and only call GetConfigFromGConf() when we know things have
      // actually changed.
    }
  }
  // An implementation for KDE settings would be welcome here.
  if (!ok) {
    ok = GetConfigFromEnv(config);
    if (ok)
      LOG(INFO) << "ProxyConfigServiceLinux: obtained proxy setting "
          "from environment variables";
  }
  return ok ? OK : ERR_FAILED;
}

}  // namespace net
