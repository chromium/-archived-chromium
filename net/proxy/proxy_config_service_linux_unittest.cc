// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "net/proxy/proxy_config_service_linux.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_common_unittest.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Set of values for all environment variables that we might
// query. NULL represents an unset variable.
struct EnvVarValues {
  // The strange capitalization is so that the field matches the
  // environment variable name exactly.
  const char *GNOME_DESKTOP_SESSION_ID, *DESKTOP_SESSION,
      *auto_proxy, *all_proxy,
      *http_proxy, *https_proxy, *ftp_proxy,
      *SOCKS_SERVER, *SOCKS_VERSION,
      *no_proxy;
};

// Undo macro pollution from GDK includes (from message_loop.h).
#undef TRUE
#undef FALSE

// So as to distinguish between an unset gconf boolean variable and
// one that is false.
enum BoolSettingValue {
  UNSET = 0, TRUE, FALSE
};

// Set of values for all gconf settings that we might query.
struct GConfValues {
  // strings
  const char *mode, *autoconfig_url,
      *http_host, *secure_host, *ftp_host, *socks_host;
  // integers
  int http_port, secure_port, ftp_port, socks_port;
  // booleans
  BoolSettingValue use_proxy, same_proxy, use_auth;
  // string list
  std::vector<std::string> ignore_hosts;
};

// Mapping from a setting name to the location of the corresponding
// value (inside a EnvVarValues or GConfValues struct).
template<typename value_type>
struct SettingsTable {
  typedef std::map<std::string, value_type*> map_type;

  // Gets the value from its location
  value_type Get(const std::string& key) {
    typename map_type::const_iterator it = settings.find(key);
    // In case there's a typo or the unittest becomes out of sync.
    CHECK(it != settings.end()) << "key " << key << " not found";
    value_type* value_ptr = it->second;
    return *value_ptr;
  }

  map_type settings;
};

class MockEnvironmentVariableGetter
    : public ProxyConfigServiceLinux::EnvironmentVariableGetter {
 public:
  MockEnvironmentVariableGetter() {
#define ENTRY(x) table.settings[#x] = &values.x
    ENTRY(GNOME_DESKTOP_SESSION_ID);
    ENTRY(DESKTOP_SESSION);
    ENTRY(auto_proxy);
    ENTRY(all_proxy);
    ENTRY(http_proxy);
    ENTRY(https_proxy);
    ENTRY(ftp_proxy);
    ENTRY(no_proxy);
    ENTRY(SOCKS_SERVER);
    ENTRY(SOCKS_VERSION);
#undef ENTRY
    Reset();
  }

  // Zeros all environment values.
  void Reset() {
    EnvVarValues zero_values = { 0 };
    values = zero_values;
  }

  virtual bool Getenv(const char* variable_name, std::string* result) {
    const char* env_value = table.Get(variable_name);
    if (env_value) {
      // Note that the variable may be defined but empty.
      *result = env_value;
      return true;
    }
    return false;
  }

  // Intentionally public, for convenience when setting up a test.
  EnvVarValues values;

 private:
  SettingsTable<const char*> table;
};

class MockGConfSettingGetter
    : public ProxyConfigServiceLinux::GConfSettingGetter {
 public:
  MockGConfSettingGetter() {
#define ENTRY(key, field) \
      strings_table.settings["/system/" key] = &values.field
    ENTRY("proxy/mode", mode);
    ENTRY("proxy/autoconfig_url", autoconfig_url);
    ENTRY("http_proxy/host", http_host);
    ENTRY("proxy/secure_host", secure_host);
    ENTRY("proxy/ftp_host", ftp_host);
    ENTRY("proxy/socks_host", socks_host);
#undef ENTRY
#define ENTRY(key, field) \
      ints_table.settings["/system/" key] = &values.field
    ENTRY("http_proxy/port", http_port);
    ENTRY("proxy/secure_port", secure_port);
    ENTRY("proxy/ftp_port", ftp_port);
    ENTRY("proxy/socks_port", socks_port);
#undef ENTRY
#define ENTRY(key, field) \
      bools_table.settings["/system/" key] = &values.field
    ENTRY("http_proxy/use_http_proxy", use_proxy);
    ENTRY("http_proxy/use_same_proxy", same_proxy);
    ENTRY("http_proxy/use_authentication", use_auth);
#undef ENTRY
    string_lists_table.settings["/system/http_proxy/ignore_hosts"] =
      &values.ignore_hosts;
    Reset();
  }

  // Zeros all environment values.
  void Reset() {
    GConfValues zero_values = { 0 };
    values = zero_values;
  }

  virtual bool Init() {
    return true;
  }

  virtual void Release() {}

  virtual bool SetupNotification(void* callback_user_data) {
    return true;
  }

  virtual bool GetString(const char* key, std::string* result) {
    const char* value = strings_table.Get(key);
    if (value) {
      *result = value;
      return true;
    }
    return false;
  }

  virtual bool GetInt(const char* key, int* result) {
    // We don't bother to distinguish unset keys from 0 values.
    *result = ints_table.Get(key);
    return true;
  }

  virtual bool GetBoolean(const char* key, bool* result) {
    BoolSettingValue value = bools_table.Get(key);
    switch (value) {
    case UNSET:
      return false;
    case TRUE:
      *result = true;
      break;
    case FALSE:
      *result = false;
    }
    return true;
  }

  virtual bool GetStringList(const char* key,
                             std::vector<std::string>* result) {
    *result = string_lists_table.Get(key);
    // We don't bother to distinguish unset keys from empty lists.
    return !result->empty();
  }

  // Intentionally public, for convenience when setting up a test.
  GConfValues values;

 private:
  SettingsTable<const char*> strings_table;
  SettingsTable<int> ints_table;
  SettingsTable<BoolSettingValue> bools_table;
  SettingsTable<std::vector<std::string> > string_lists_table;
};

}  // namespace
}  // namespace net

// This helper class runs ProxyConfigServiceLinux::GetProxyConfig() on
// the IO thread and synchronously waits for the result.
// Some code duplicated from proxy_script_fetcher_unittest.cc.
class SynchConfigGetter {
 public:
  explicit SynchConfigGetter(net::ProxyConfigServiceLinux* config_service)
      : event_(false, false),
        io_thread_("IO_Thread"),
        config_service_(config_service) {
    // Start an IO thread.
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    // Make sure the thread started.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SynchConfigGetter::Init));
    Wait();
  }

  ~SynchConfigGetter() {
    // Cleanup the IO thread.
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SynchConfigGetter::Cleanup));
    Wait();
  }

  // Does a reset, gconf setup and initial fetch of the proxy config,
  // all on the calling thread (meant to be the thread with the
  // default glib main loop, which is the UI thread).
  void SetupAndInitialFetch() {
    config_service_->Reset();
    config_service_->SetupAndFetchInitialConfig(
        MessageLoop::current(), io_thread_.message_loop());
  }
  // Synchronously gets the proxy config.
  int SyncGetProxyConfig(net::ProxyConfig* config) {
    io_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SynchConfigGetter::GetConfigOnIOThread));
    Wait();
    *config = proxy_config_;
    return get_config_result_;
  }

 private:
  // [Runs on |io_thread_|]
  void Init() {
    event_.Signal();
  }

  // Calls GetProxyConfig, running on |io_thread_|] Signals |event_|
  // on completion.
  void GetConfigOnIOThread() {
    get_config_result_ = config_service_->GetProxyConfig(&proxy_config_);
    event_.Signal();
  }

  // [Runs on |io_thread_|] Signals |event_| on cleanup completion.
  void Cleanup() {
    MessageLoop::current()->RunAllPending();
    event_.Signal();
  }

  void Wait() {
    event_.Wait();
    event_.Reset();
  }

  base::WaitableEvent event_;
  base::Thread io_thread_;

  net::ProxyConfigServiceLinux* config_service_;

  // The config obtained by |io_thread_| and read back by the main
  // thread.
  net::ProxyConfig proxy_config_;
  int get_config_result_;  // Return value from GetProxyConfig().
};

template<>
void RunnableMethodTraits<SynchConfigGetter>::RetainCallee(
    SynchConfigGetter* remover) {}
template<>
void RunnableMethodTraits<SynchConfigGetter>::ReleaseCallee(
    SynchConfigGetter* remover) {}

namespace net {

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) StringPrintf("at line %d <%s>", __LINE__, desc)

TEST(ProxyConfigServiceLinuxTest, BasicGConfTest) {
  MockEnvironmentVariableGetter* env_getter =
      new MockEnvironmentVariableGetter;
  MockGConfSettingGetter* gconf_getter = new MockGConfSettingGetter;
  ProxyConfigServiceLinux service(env_getter, gconf_getter);
  // This env var indicates we are running Gnome and should consult gconf.
  env_getter->values.GNOME_DESKTOP_SESSION_ID = "defined";

  std::vector<std::string> empty_ignores;

  std::vector<std::string> google_ignores;
  google_ignores.push_back("*.google.com");

  // Inspired from proxy_config_service_win_unittest.cc.
  // Very neat, but harder to track down failures though.
  const struct {
    // Short description to identify the test
    std::string description;

    // Input.
    GConfValues values;

    // Expected outputs (fields of the ProxyConfig).
    bool auto_detect;
    GURL pac_url;
    ProxyConfig::ProxyRules proxy_rules;
    const char* proxy_bypass_list;  // newline separated
    bool bypass_local_names;
  } tests[] = {
    {
      TEST_DESC("No proxying"),
      { // Input.
        "none",                   // mode
        "",                       // autoconfig_url
        "", "", "", "",           // hosts
        0, 0, 0, 0,               // ports
        FALSE, FALSE, FALSE,      // use, same, auth
        empty_ignores,            // ignore_hosts
      },

      // Expected result.
      false,                      // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    {
      TEST_DESC("Auto detect"),
      { // Input.
        "auto",                   // mode
        "",                       // autoconfig_url
        "", "", "", "",           // hosts
        0, 0, 0, 0,               // ports
        FALSE, FALSE, FALSE,      // use, same, auth
        empty_ignores,            // ignore_hosts
      },

      // Expected result.
      true,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    {
      TEST_DESC("Valid PAC url"),
      { // Input.
        "auto",                      // mode
        "http://wpad/wpad.dat",      // autoconfig_url
        "", "", "", "",              // hosts
        0, 0, 0, 0,                  // ports
        FALSE, FALSE, FALSE,         // use, same, auth
        empty_ignores,               // ignore_hosts
      },

      // Expected result.
      false,                         // auto_detect
      GURL("http://wpad/wpad.dat"),  // pac_url
      ProxyConfig::ProxyRules(),     // proxy_rules
      "",                            // proxy_bypass_list
      false,                         // bypass_local_names
    },

    {
      TEST_DESC("Invalid PAC url"),
      { // Input.
        "auto",                      // mode
        "wpad.dat",                  // autoconfig_url
        "", "", "", "",              // hosts
        0, 0, 0, 0,                  // ports
        FALSE, FALSE, FALSE,         // use, same, auth
        empty_ignores,               // ignore_hosts
      },

      // Expected result.
      false,                         // auto_detect
      GURL(),                        // pac_url
      ProxyConfig::ProxyRules(),     // proxy_rules
      "",                            // proxy_bypass_list
      false,                         // bypass_local_names
    },

    {
      TEST_DESC("Single-host in proxy list"),
      { // Input.
        "manual",                              // mode
        "",                                    // autoconfig_url
        "www.google.com", "", "", "",          // hosts
        80, 0, 0, 0,                           // ports
        TRUE, TRUE, FALSE,                     // use, same, auth
        empty_ignores,                         // ignore_hosts
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("www.google.com"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("use_http_proxy is honored"),
      { // Input.
        "manual",                              // mode
        "",                                    // autoconfig_url
        "www.google.com", "", "", "",          // hosts
        80, 0, 0, 0,                           // ports
        FALSE, TRUE, FALSE,                    // use, same, auth
        empty_ignores,                         // ignore_hosts
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      ProxyConfig::ProxyRules(),               // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("use_http_proxy and use_same_proxy are optional"),
      { // Input.
        "manual",                                     // mode
        "",                                           // autoconfig_url
        "www.google.com", "", "", "",                 // hosts
        80, 0, 0, 0,                                  // ports
        UNSET, UNSET, FALSE,                          // use, same, auth
        empty_ignores,                                // ignore_hosts
      },

      // Expected result.
      false,                                          // auto_detect
      GURL(),                                         // pac_url
      MakeProxyPerSchemeRules("www.google.com",       // proxy_rules
                              "", ""),
      "",                                             // proxy_bypass_list
      false,                                          // bypass_local_names
    },

    {
      TEST_DESC("Single-host, different port"),
      { // Input.
        "manual",                                     // mode
        "",                                           // autoconfig_url
        "www.google.com", "", "", "",                 // hosts
        88, 0, 0, 0,                                  // ports
        TRUE, TRUE, FALSE,                            // use, same, auth
        empty_ignores,                                // ignore_hosts
      },

      // Expected result.
      false,                                          // auto_detect
      GURL(),                                         // pac_aurl
      MakeSingleProxyRules("www.google.com:88"),      // proxy_rules
      "",                                             // proxy_bypass_list
      false,                                          // bypass_local_names
    },

    {
      TEST_DESC("Per-scheme proxy rules"),
      { // Input.
        "manual",                              // mode
        "",                                           // autoconfig_url
        "www.google.com",                             // http_host
        "www.foo.com",                                // secure_host
        "ftpfoo.com",                                 // ftp
        "",                                           // socks
        88, 110, 121, 0,                              // ports
        TRUE, FALSE, FALSE,                           // use, same, auth
        empty_ignores,                                // ignore_hosts
      },

      // Expected result.
      false,                                          // auto_detect
      GURL(),                                         // pac_url
      MakeProxyPerSchemeRules("www.google.com:88",    // proxy_rules
                              "www.foo.com:110",
                              "ftpfoo.com:121"),
      "",                                             // proxy_bypass_list
      false,                                          // bypass_local_names
    },

    {
      TEST_DESC("socks"),
      { // Input.
        "manual",                                     // mode
        "",                                           // autoconfig_url
        "", "", "", "socks.com",                      // hosts
        0, 0, 0, 99,                                  // ports
        TRUE, FALSE, FALSE,                           // use, same, auth
        empty_ignores,                                // ignore_hosts
      },

      // Expected result.
      false,                                          // auto_detect
      GURL(),                                         // pac_url
      MakeSingleProxyRules("socks4://socks.com:99"),  // proxy_rules
      "",                                             // proxy_bypass_list
      false,                                          // bypass_local_names
    },

    {
      TEST_DESC("Bypass *.google.com"),
      { // Input.
        "manual",                                     // mode
        "",                                           // autoconfig_url
        "www.google.com", "", "", "",                 // hosts
        80, 0, 0, 0,                                  // ports
        TRUE, TRUE, FALSE,                            // use, same, auth
        google_ignores,                               // ignore_hosts
      },

      false,                                          // auto_detect
      GURL(),                                         // pac_url
      MakeSingleProxyRules("www.google.com"),         // proxy_rules
      "*.google.com\n",                               // proxy_bypass_list
      false,                                          // bypass_local_names
    },
  };

  SynchConfigGetter sync_config_getter(&service);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%d] %s", i, tests[i].description.c_str()));
    ProxyConfig config;
    gconf_getter->values = tests[i].values;
    sync_config_getter.SetupAndInitialFetch();
    sync_config_getter.SyncGetProxyConfig(&config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect);
    EXPECT_EQ(tests[i].pac_url, config.pac_url);
    EXPECT_EQ(tests[i].proxy_bypass_list,
              FlattenProxyBypass(config.proxy_bypass));
    EXPECT_EQ(tests[i].bypass_local_names, config.proxy_bypass_local_names);
    EXPECT_EQ(tests[i].proxy_rules, config.proxy_rules);
  }
}

TEST(ProxyConfigServiceLinuxTest, BasicEnvTest) {
  MockEnvironmentVariableGetter* env_getter =
      new MockEnvironmentVariableGetter;
  MockGConfSettingGetter* gconf_getter = new MockGConfSettingGetter;
  ProxyConfigServiceLinux service(env_getter, gconf_getter);

  // Inspired from proxy_config_service_win_unittest.cc.
  const struct {
    // Short description to identify the test
    std::string description;

    // Input.
    EnvVarValues values;

    // Expected outputs (fields of the ProxyConfig).
    bool auto_detect;
    GURL pac_url;
    ProxyConfig::ProxyRules proxy_rules;
    const char* proxy_bypass_list;  // newline separated
    bool bypass_local_names;
  } tests[] = {
    {
      TEST_DESC("No proxying"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        NULL,  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        "*",  // no_proxy
      },

      // Expected result.
      false,                      // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    {
      TEST_DESC("Auto detect"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        "",  // auto_proxy
        NULL,  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      true,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    {
      TEST_DESC("Valid PAC url"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        "http://wpad/wpad.dat",  // auto_proxy
        NULL,  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                         // auto_detect
      GURL("http://wpad/wpad.dat"),  // pac_url
      ProxyConfig::ProxyRules(),     // proxy_rules
      "",                            // proxy_bypass_list
      false,                         // bypass_local_names
    },

    {
      TEST_DESC("Invalid PAC url"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        "wpad.dat",  // auto_proxy
        NULL,  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                       // auto_detect
      GURL(),                     // pac_url
      ProxyConfig::ProxyRules(),  // proxy_rules
      "",                         // proxy_bypass_list
      false,                      // bypass_local_names
    },

    {
      TEST_DESC("Single-host in proxy list"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "www.google.com",  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("www.google.com"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("Single-host, different port"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "www.google.com:99",  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("www.google.com:99"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("Tolerate a scheme"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "http://www.google.com:99",  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("www.google.com:99"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("Per-scheme proxy rules"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        NULL,  // all_proxy
        "www.google.com:80", "www.foo.com:110", "ftpfoo.com:121",  // per-proto
        NULL, NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeProxyPerSchemeRules("www.google.com", "www.foo.com:110",
                              "ftpfoo.com:121"),
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("socks"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "",  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        "socks.com:888", NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("socks4://socks.com:888"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("socks5"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "",  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        "socks.com:888", "5",  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("socks5://socks.com:888"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("socks default port"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "",  // all_proxy
        NULL, NULL, NULL,  // per-proto proxies
        "socks.com", NULL,  // SOCKS
        NULL,  // no_proxy
      },

      // Expected result.
      false,                                   // auto_detect
      GURL(),                                  // pac_url
      MakeSingleProxyRules("socks4://socks.com"),  // proxy_rules
      "",                                      // proxy_bypass_list
      false,                                   // bypass_local_names
    },

    {
      TEST_DESC("bypass"),
      { // Input.
        NULL, NULL,  // *DESKTOP*
        NULL,  // auto_proxy
        "www.google.com",  // all_proxy
        NULL, NULL, NULL,  // per-proto
        NULL, NULL,  // SOCKS
        ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8",  // no_proxy
      },

      false,                      // auto_detect
      GURL(),                     // pac_url
      MakeSingleProxyRules("www.google.com"),  // proxy_rules
      // proxy_bypass_list
      "*.google.com\n*foo.com:99\n1.2.3.4:22\n127.0.0.1/8\n",
      false,                        // bypass_local_names
    },
  };

  SynchConfigGetter sync_config_getter(&service);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%d] %s", i, tests[i].description.c_str()));
    ProxyConfig config;
    env_getter->values = tests[i].values;
    sync_config_getter.SetupAndInitialFetch();
    sync_config_getter.SyncGetProxyConfig(&config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect);
    EXPECT_EQ(tests[i].pac_url, config.pac_url);
    EXPECT_EQ(tests[i].proxy_bypass_list,
              FlattenProxyBypass(config.proxy_bypass));
    EXPECT_EQ(tests[i].bypass_local_names, config.proxy_bypass_local_names);
    EXPECT_EQ(tests[i].proxy_rules, config.proxy_rules);
  }
}

// Verify that we fall back on consulting the environment when
// GNOME-specific environment variables aren't available.
TEST(ProxyConfigServiceLinuxTest, FallbackOnEnv) {
  MockEnvironmentVariableGetter* env_getter =
      new MockEnvironmentVariableGetter;
  MockGConfSettingGetter* gconf_getter = new MockGConfSettingGetter;
  ProxyConfigServiceLinux service(env_getter, gconf_getter);

  // Imagine we're:
  // 1) Running a non-GNOME desktop session:
  env_getter->values.DESKTOP_SESSION = "default";
  // 2) Have settings in gconf.
  gconf_getter->values.mode = "auto";
  gconf_getter->values.autoconfig_url = "http://incorrect/wpad.dat";
  // 3) But we have a proxy-specifying environment variable set:
  env_getter->values.auto_proxy = "http://correct/wpad.dat";

  ProxyConfig config;

  SynchConfigGetter sync_config_getter(&service);
  sync_config_getter.SetupAndInitialFetch();
  sync_config_getter.SyncGetProxyConfig(&config);

  // Then we expect the environment variable to win.
  EXPECT_EQ(GURL(env_getter->values.auto_proxy), config.pac_url);
}

TEST(ProxyConfigServiceLinuxTest, GconfNotification) {
  MockEnvironmentVariableGetter* env_getter =
      new MockEnvironmentVariableGetter;
  MockGConfSettingGetter* gconf_getter = new MockGConfSettingGetter;
  ProxyConfigServiceLinux service(env_getter, gconf_getter);
  ProxyConfig config;
  SynchConfigGetter sync_config_getter(&service);

  // Use gconf configuration.
  env_getter->values.GNOME_DESKTOP_SESSION_ID = "defined";

  // Start with no proxy.
  gconf_getter->values.mode = "none";
  sync_config_getter.SetupAndInitialFetch();
  sync_config_getter.SyncGetProxyConfig(&config);
  EXPECT_FALSE(config.auto_detect);

  // Now set to auto-detect.
  gconf_getter->values.mode = "auto";
  // Simulate gconf notification callback.
  service.OnCheckProxyConfigSettings();
  sync_config_getter.SyncGetProxyConfig(&config);
  EXPECT_TRUE(config.auto_detect);
}

}  // namespace net
