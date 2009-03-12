// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cert_store.h"

#include <algorithm>
#include <functional>

#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/stl_util-inl.h"

template <typename T>
struct MatchSecond {
  explicit MatchSecond(const T& t) : value(t) {}

  template<typename Pair>
  bool operator()(const Pair& p) const {
    return (value == p.second);
  }
  T value;
};

//  static
CertStore* CertStore::GetSharedInstance() {
  return Singleton<CertStore>::get();
}

CertStore::CertStore() : next_cert_id_(1) {
  // We watch for RenderProcess termination, as this is how we clear
  // certificates for now.
  // TODO(jcampan): we should be listening to events such as resource cached/
  //                removed from cache, and remove the cert when we know it
  //                is not used anymore.

  // TODO(tc): This notification observer never gets removed because the
  // CertStore is never deleted.
  NotificationService::current()->AddObserver(this,
      NotificationType::RENDERER_PROCESS_TERMINATED,
      NotificationService::AllSources());
}

CertStore::~CertStore() {
}

int CertStore::StoreCert(net::X509Certificate* cert, int process_id) {
  DCHECK(cert);
  AutoLock autoLock(cert_lock_);

  int cert_id;

  // Do we already know this cert?
  ReverseCertMap::iterator cert_iter = cert_to_id_.find(cert);
  if (cert_iter == cert_to_id_.end()) {
    cert_id = next_cert_id_++;
    cert->AddRef();
    id_to_cert_[cert_id] = cert;
    cert_to_id_[cert] = cert_id;
  } else {
    cert_id = cert_iter->second;
  }

  // Let's update process_id_to_cert_id_.
  if (std::find_if(process_id_to_cert_id_.lower_bound(process_id),
                   process_id_to_cert_id_.upper_bound(process_id),
                   MatchSecond<int>(cert_id)) ==
        process_id_to_cert_id_.upper_bound(process_id)) {
    process_id_to_cert_id_.insert(std::make_pair(process_id, cert_id));
  }

  // And cert_id_to_process_id_.
  if (std::find_if(cert_id_to_process_id_.lower_bound(cert_id),
                   cert_id_to_process_id_.upper_bound(cert_id),
                   MatchSecond<int>(process_id)) ==
        cert_id_to_process_id_.upper_bound(cert_id)) {
    cert_id_to_process_id_.insert(std::make_pair(cert_id, process_id));
  }

  return cert_id;
}

bool CertStore::RetrieveCert(int cert_id,
                             scoped_refptr<net::X509Certificate>* cert) {
  AutoLock autoLock(cert_lock_);

  CertMap::iterator iter = id_to_cert_.find(cert_id);
  if (iter == id_to_cert_.end())
    return false;
  *cert = iter->second;
  return true;
}

void CertStore::RemoveCertInternal(int cert_id) {
  CertMap::iterator cert_iter = id_to_cert_.find(cert_id);
  DCHECK(cert_iter != id_to_cert_.end());

  ReverseCertMap::iterator id_iter = cert_to_id_.find(cert_iter->second);
  DCHECK(id_iter != cert_to_id_.end());
  cert_to_id_.erase(id_iter);

  cert_iter->second->Release();
  id_to_cert_.erase(cert_iter);
}

void CertStore::RemoveCertsForRenderProcesHost(int process_id) {
  AutoLock autoLock(cert_lock_);

  // We iterate through all the cert ids for that process.
  IDMap::iterator ids_iter;
  for (ids_iter = process_id_to_cert_id_.lower_bound(process_id);
       ids_iter != process_id_to_cert_id_.upper_bound(process_id);) {
    int cert_id = ids_iter->second;
    // Remove this process from cert_id_to_process_id_.
    IDMap::iterator proc_iter =
        std::find_if(cert_id_to_process_id_.lower_bound(cert_id),
                     cert_id_to_process_id_.upper_bound(cert_id),
                     MatchSecond<int>(process_id));
    DCHECK(proc_iter != cert_id_to_process_id_.upper_bound(cert_id));
    cert_id_to_process_id_.erase(proc_iter);

    if (cert_id_to_process_id_.count(cert_id) == 0) {
      // This cert is not referenced by any process, remove it from id_to_cert_
      // and cert_to_id_.
      RemoveCertInternal(cert_id);
    }

    // Erase the current item but keep the iterator valid.
    process_id_to_cert_id_.erase(ids_iter++);
  }
}

void CertStore::Observe(NotificationType type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  DCHECK(rph);
  RemoveCertsForRenderProcesHost(rph->pid());
}
