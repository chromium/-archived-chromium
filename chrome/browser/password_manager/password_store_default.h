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
// TODO(stuartmorgan): This is a temprorary shim for Windows from the new
// PasswordStore interface to the old model of storing passwords in the
// WebDatabase. This should be replaced by a self-contained Windows
// implementation once PasswordStore is completed.
class PasswordStoreDefault : public PasswordStore,
                             public WebDataServiceConsumer {
 public:
  explicit PasswordStoreDefault(WebDataService* web_data_service);
  virtual ~PasswordStoreDefault() {}

 protected:
  // Implements PasswordStore interface.
  void AddLoginImpl(const PasswordForm& form);
  void UpdateLoginImpl(const PasswordForm& form);
  void RemoveLoginImpl(const PasswordForm& form);
  void GetLoginsImpl(GetLoginsRequest* request);

  // Called when a WebDataService method finishes.
  // Overrides must call RemovePendingWebDataServiceRequest.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Keeps track of a pending WebDataService request, and the original
  // GetLogins request it is associated with.
  // Increases the reference count of |this|, so must be balanced with a
  // call to RemovePendingWebDataServiceRequest.
  void AddPendingWebDataServiceRequest(WebDataService::Handle handle,
                                       GetLoginsRequest* request);
  // Removes a WebDataService request from the list of pending requests,
  // and reduces the reference count of |this|.
  void RemovePendingWebDataServiceRequest(WebDataService::Handle handle);
  // Returns the GetLoginsRequest associated with |handle|.
  GetLoginsRequest* GetLoginsRequestForWebDataServiceRequest(
      WebDataService::Handle handle);

  scoped_refptr<WebDataService> web_data_service_;

 private:
  // Methods in this class call async WebDataService methods.  This mapping
  // remembers which WebDataService request corresponds to which PasswordStore
  // request.
  typedef std::map<WebDataService::Handle, GetLoginsRequest*> PendingRequestMap;
  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT
