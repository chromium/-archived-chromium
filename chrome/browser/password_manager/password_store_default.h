// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT

#include <map>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/webdata/web_data_service.h"

class Task;

// Simple password store implementation that delegates everything to
// the WebDatabase.
class PasswordStoreDefault : public PasswordStore,
                             public WebDataServiceConsumer {
 public:
  explicit PasswordStoreDefault(WebDataService* web_data_service);
  virtual ~PasswordStoreDefault();

 protected:
  // Implements PasswordStore interface.
  void AddLoginImpl(const PasswordForm& form);
  void UpdateLoginImpl(const PasswordForm& form);
  void RemoveLoginImpl(const PasswordForm& form);
  void GetLoginsImpl(GetLoginsRequest* request);

  // Called when a WebDataService method finishes.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  scoped_refptr<WebDataService> web_data_service_;

  // Methods in this class call async WebDataService methods.  This mapping
  // remembers which WebDataService request corresponds to which PasswordStore
  // request.
  typedef std::map<WebDataService::Handle, GetLoginsRequest*> PendingRequestMap;
  PendingRequestMap pending_requests_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT
