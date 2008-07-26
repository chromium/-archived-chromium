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

// Contains the history backend wrapper around the in-memory URL database. This
// object maintains an in-memory cache of the subset of history required to do
// in-line autocomplete.
//
// It is created on the history thread and passed to the main thread where
// operations can be completed synchronously. It listenes for notifications
// from the "regular" history backend and keeps itself in sync.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H__
#define CHROME_BROWSER_HISTORY_IN_MEMORY_HISTORY_BACKEND_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/common/notification_service.h"

class HistoryDatabase;
class Profile;

namespace history {

class InMemoryDatabase;

class InMemoryHistoryBackend : public NotificationObserver {
 public:
  InMemoryHistoryBackend();
  ~InMemoryHistoryBackend();

  // Initializes with data from the given history database.
  bool Init(const std::wstring& history_filename);

  // Does initialization work when this object is attached to the history
  // system on the main thread. The argument is the profile with which the
  // attached history service is under.
  void AttachToHistoryService(Profile* profile);

  // Returns the underlying database associated with this backend. The current
  // autocomplete code was written fro this, but it should probably be removed
  // so that it can deal directly with this object, rather than the DB.
  InMemoryDatabase* db() const {
    return db_.get();
  }

  // Notification callback.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  FRIEND_TEST(HistoryBackendTest, DeleteAll);

  // Handler for NOTIFY_HISTORY_TYPED_URLS_MODIFIED.
  void OnTypedURLsModified(const URLsModifiedDetails& details);

  // Handler for NOTIFY_HISTORY_URLS_DELETED.
  void OnURLsDeleted(const URLsDeletedDetails& details);

  // Handler for NOTIFY_URLS_STARRED.
  void OnURLsStarred(const URLsStarredDetails& details);

  scoped_ptr<InMemoryDatabase> db_;

  // The profile that this object is attached. May be NULL before
  // initialization.
  Profile* profile_;

  // Set when this object has registered for notifications. This is done so we
  // know whether to unregister (the initialization may have failed, so we
  // may be destroyed before attaching to the main thread.
  bool registered_for_notifications_;

  DISALLOW_EVIL_CONSTRUCTORS(InMemoryHistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_IN_MEMORY_HISTORY_BACKEND_H__
