// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_H__
#define NET_BASE_SSL_CONFIG_SERVICE_H__

#include "base/time.h"

namespace net {

// A collection of SSL-related configuration settings.
struct SSLConfig {
  // Default to no revocation checking.
  // Default to SSL 2.0 off, SSL 3.0 on, and TLS 1.0 on.
  SSLConfig()
      : rev_checking_enabled(false), ssl2_enabled(false),
        ssl3_enabled(true), tls1_enabled(true) {
  }

  bool rev_checking_enabled;  // True if server certificate revocation
                              // checking is enabled.
  bool ssl2_enabled;  // True if SSL 2.0 is enabled.
  bool ssl3_enabled;  // True if SSL 3.0 is enabled.
  bool tls1_enabled;  // True if TLS 1.0 is enabled.
};

// This class is responsible for getting and setting the SSL configuration.
//
// We think the SSL configuration settings should apply to all applications
// used by the user.  We consider IE's Internet Options as the de facto
// system-wide network configuration settings, so we just use the values
// from IE's Internet Settings registry key.
class SSLConfigService {
 public:
  SSLConfigService();
  explicit SSLConfigService(base::TimeTicks now);  // Used for testing.
  ~SSLConfigService() { }

  // Get the current SSL configuration settings.  Can be called on any
  // thread.
  static bool GetSSLConfigNow(SSLConfig* config);

  // Setters.  Can be called on any thread.
  static void SetRevCheckingEnabled(bool enabled);
  static void SetSSL2Enabled(bool enabled);

  // Get the (cached) SSL configuration settings that are fresh within 10
  // seconds.  This is cheaper than GetSSLConfigNow and is suitable when
  // we don't need the absolutely current configuration settings.  This
  // method is not thread-safe, so it must be called on the same thread.
  void GetSSLConfig(SSLConfig* config) {
    GetSSLConfigAt(config, base::TimeTicks::Now());
  }

  // Used for testing.
  void GetSSLConfigAt(SSLConfig* config, base::TimeTicks now);

 private:
  void UpdateConfig(base::TimeTicks now);

  // We store the IE SSL config and the time that we fetched it.
  SSLConfig config_info_;
  base::TimeTicks config_time_;

  DISALLOW_EVIL_CONSTRUCTORS(SSLConfigService);
};

}  // namespace net

#endif  // NET_BASE_SSL_CONFIG_SERVICE_H__
