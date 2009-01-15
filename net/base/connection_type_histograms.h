// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CONNECTION_TYPE_HISTOGRAMS_H_
#define NET_BASE_CONNECTION_TYPE_HISTOGRAMS_H_

// The UpdateConnectionTypeHistograms function collects statistics related
// to the number of MD5 certificates that our users are encountering.  The
// information will help us decide when it is fine for browsers to stop
// supporting MD5 certificates, in light of the recent MD5 certificate
// collision attack (see "MD5 considered harmful today: Creating a rogue CA
// certificate" at http://www.win.tue.nl/hashclash/rogue-ca/).

namespace net {

enum ConnectionType {
  CONNECTION_ANY = 0,      // Any connection, SSL or not
  CONNECTION_SSL = 1,      // An SSL connection
  CONNECTION_SSL_MD5 = 2,  // An SSL connection with an MD5 certificate in
                           // the certificate chain (excluding root)
  CONNECTION_SSL_MD2 = 3,  // An SSL connection with an MD2 certificate in
                           // the certificate chain (excluding root)
  CONNECTION_SSL_MD4 = 4,  // An SSL connection with an MD4 certificate in
                           // the certificate chain (excluding root)
  CONNECTION_SSL_MD5_CA = 5,  // An SSL connection with an MD5 CA certificate
                              // in the certificate chain (excluding root)
  NUM_OF_CONNECTION_TYPES
};

void UpdateConnectionTypeHistograms(ConnectionType type);

}  // namespace net

#endif  // NET_BASE_CONNECTION_TYPE_HISTOGRAMS_H_
