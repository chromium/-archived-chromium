// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE

#include <set>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "webkit/glue/password_form.h"

class Profile;
class Task;

class PasswordStoreConsumer {
 public:
  virtual ~PasswordStoreConsumer() {}
  // Call this when the request is finished. If there are no results, call it
  // anyway with an empty vector.
  virtual void OnPasswordStoreRequestDone(
      int handle, const std::vector<webkit_glue::PasswordForm*>& result) = 0;
};

// Interface for storing form passwords in a platform-specific secure way.
// The login request/manipulation API is not threadsafe.
class PasswordStore : public base::RefCountedThreadSafe<PasswordStore> {
 public:
  PasswordStore();
  virtual ~PasswordStore() {}

  // Reimplement this to add custom initialization. Always call this too.
  virtual bool Init();

  // TODO(stuartmorgan): These are only virtual to support the shim version of
  // password_store_default; once that is fixed, they can become non-virtual.

  // Adds the given PasswordForm to the secure password store asynchronously.
  virtual void AddLogin(const webkit_glue::PasswordForm& form);
  // Updates the matching PasswordForm in the secure password store (async).
  virtual void UpdateLogin(const webkit_glue::PasswordForm& form);
  // Removes the matching PasswordForm from the secure password store (async).
  virtual void RemoveLogin(const webkit_glue::PasswordForm& form);
  // Searches for a matching PasswordForm and returns a handle so the async
  // request can be tracked. Implement the PasswordStoreConsumer interface to
  // be notified on completion.
  virtual int GetLogins(const webkit_glue::PasswordForm& form,
                        PasswordStoreConsumer* consumer);

  // Cancels a previous GetLogins query (async)
  virtual void CancelLoginsQuery(int handle);

 protected:
  // Simple container class that represents a GetLogins request.
  // Created in GetLogins and passed to GetLoginsImpl.
  struct GetLoginsRequest {
    GetLoginsRequest(const webkit_glue::PasswordForm& f,
                     PasswordStoreConsumer* c,
                     int handle);

    // The query form that was originally passed to GetLogins
    webkit_glue::PasswordForm form;
    // The consumer to notify when this GetLogins request is complete
    PasswordStoreConsumer* consumer;
    // A unique handle for the request
    int handle;
    // The message loop that the GetLogins request was made from.  We send the
    // result back to the consumer in this same message loop.
    MessageLoop* message_loop;

    DISALLOW_COPY_AND_ASSIGN(GetLoginsRequest);
  };

  // Schedule the given task to be run in the PasswordStore's own thread.
  void ScheduleTask(Task* task);

  // These will be run in PasswordStore's own thread.
  // Synchronous implementation to add the given login.
  virtual void AddLoginImpl(const webkit_glue::PasswordForm& form) = 0;
  // Synchronous implementation to update the given login.
  virtual void UpdateLoginImpl(const webkit_glue::PasswordForm& form) = 0;
  // Synchronous implementation to remove the given login.
  virtual void RemoveLoginImpl(const webkit_glue::PasswordForm& form) = 0;
  // Should find all PasswordForms with the same signon_realm. The results
  // will then be scored by the PasswordFormManager. Once they are found
  // (or not), the consumer should be notified.
  virtual void GetLoginsImpl(GetLoginsRequest* request) = 0;

  // Notifies the consumer that GetLoginsImpl() is complete.
  void NotifyConsumer(GetLoginsRequest* request,
                      const std::vector<webkit_glue::PasswordForm*> forms);

  // Next handle to return from GetLogins() to allow callers to track
  // their request.
  int handle_;

  // Thread that the synchronous methods are run in.
  scoped_ptr<base::Thread> thread_;

 private:
  // Called by NotifyConsumer, but runs in the consumer's thread.  Will not
  // call the consumer if the request was canceled.  This extra layer is here so
  // that PasswordStoreConsumer doesn't have to be reference counted (we assume
  // consumers will cancel their requests before they are destroyed).
  void NotifyConsumerImpl(PasswordStoreConsumer* consumer, int handle,
                          const std::vector<webkit_glue::PasswordForm*> forms);

  // List of pending request handles.  Handles are removed from the set when
  // they finish or are canceled.
  std::set<int> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE
