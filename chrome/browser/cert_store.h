// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_CERT_STORE_H_
#define CHROME_BROWSER_CERT_STORE_H_

#include <vector>
#include <map>

#include "base/lock.h"
#include "chrome/common/notification_service.h"
#include "net/base/x509_certificate.h"

// The purpose of the cert store is to provide an easy way to store/retrieve
// X509Certificate objects.  When stored, an X509Certificate object is
// associated with a RenderProcessHost.  If all the RenderProcessHosts
// associated with the cert have exited, the cert is removed from the store.
// This class is used by the SSLManager to keep track of the certs associated
// to loaded resources.
// It can be accessed from the UI and IO threads (it is thread-safe).
// Note that the cert ids will overflow if we register more than 2^32 - 1 certs
// in 1 browsing session (which is highly unlikely to happen).

class CertStore : public NotificationObserver {
 public:
  // Creates the singleton instance.  Should be called from the UI thread.
  static void Initialize();

  // Returns the singleton instance of the CertStore.
  static CertStore* GetSharedInstance();

  // Stores the specified cert and returns the id associated with it.  The cert
  // is associated to the specified RenderProcessHost.
  // When all the RenderProcessHosts associated with a cert have exited, the
  // cert is removed from the store.
  // Note: ids starts at 1.
  int StoreCert(net::X509Certificate* cert, int render_process_host_id);

  // Retrieves the previously stored cert associated with the specified
  // |cert_id| and set it in |cert|.  Returns false if no cert was found for
  // that id.
  bool RetrieveCert(int cert_id, scoped_refptr<net::X509Certificate>* cert);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  CertStore();
  ~CertStore();

  // Remove the specified cert from id_to_cert_ and cert_to_id_.
  void RemoveCert(int cert_id);

  // Removes all the certs associated with the specified process from the store.
  void RemoveCertsForRenderProcesHost(int render_process_host_id);

  static CertStore* instance_;

  typedef std::multimap<int, int> IDMap;
  typedef std::map<int, scoped_refptr<net::X509Certificate>> CertMap;
  typedef std::map<net::X509Certificate*, int, net::X509Certificate::LessThan>
      ReverseCertMap;

  IDMap process_id_to_cert_id_;
  IDMap cert_id_to_process_id_;

  CertMap id_to_cert_;
  ReverseCertMap cert_to_id_;

  int next_cert_id_;

  // This lock protects: process_to_ids_, id_to_processes_, id_to_cert_ and
  //                     cert_to_id_.
  Lock cert_lock_;

  DISALLOW_EVIL_CONSTRUCTORS(CertStore);
};

#endif  // CHROME_BROWSER_CERT_STORE_H_
