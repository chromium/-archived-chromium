// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERT_STORE_H_
#define CHROME_BROWSER_CERT_STORE_H_

#include <map>

#include "base/lock.h"
#include "base/singleton.h"
#include "chrome/common/notification_registrar.h"
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
  friend struct DefaultSingletonTraits<CertStore>;

  CertStore();
  ~CertStore();

  // Remove the specified cert from id_to_cert_ and cert_to_id_.
  // NOTE: the caller (RemoveCertsForRenderProcesHost) must hold cert_lock_.
  void RemoveCertInternal(int cert_id);

  // Removes all the certs associated with the specified process from the store.
  void RemoveCertsForRenderProcesHost(int render_process_host_id);

  typedef std::multimap<int, int> IDMap;
  typedef std::map<int, scoped_refptr<net::X509Certificate> > CertMap;
  typedef std::map<net::X509Certificate*, int, net::X509Certificate::LessThan>
      ReverseCertMap;

  NotificationRegistrar registrar_;

  IDMap process_id_to_cert_id_;
  IDMap cert_id_to_process_id_;

  CertMap id_to_cert_;
  ReverseCertMap cert_to_id_;

  int next_cert_id_;

  // This lock protects: process_to_ids_, id_to_processes_, id_to_cert_ and
  //                     cert_to_id_.
  Lock cert_lock_;

  DISALLOW_COPY_AND_ASSIGN(CertStore);
};

#endif  // CHROME_BROWSER_CERT_STORE_H_
