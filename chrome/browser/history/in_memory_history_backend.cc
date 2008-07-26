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

#include "chrome/browser/history/in_memory_history_backend.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/profile.h"

namespace history {

// If a page becomes starred we use this id in place of the real starred id.
// See note in OnURLsStarred.
static const StarID kBogusStarredID = 0x0FFFFFFF;

InMemoryHistoryBackend::InMemoryHistoryBackend()
    : profile_(NULL),
      registered_for_notifications_(false) {
}

InMemoryHistoryBackend::~InMemoryHistoryBackend() {
  if (registered_for_notifications_) {
    NotificationService* service = NotificationService::current();

    Source<Profile> source(profile_);
    service->RemoveObserver(this, NOTIFY_HISTORY_URL_VISITED, source);
    service->RemoveObserver(this, NOTIFY_HISTORY_TYPED_URLS_MODIFIED, source);
    service->RemoveObserver(this, NOTIFY_HISTORY_URLS_DELETED, source);
    service->RemoveObserver(this, NOTIFY_URLS_STARRED, source);
  }
}

bool InMemoryHistoryBackend::Init(const std::wstring& history_filename) {
  db_.reset(new InMemoryDatabase);
  return db_->InitFromDisk(history_filename);
}

void InMemoryHistoryBackend::AttachToHistoryService(Profile* profile) {
  if (!db_.get()) {
    NOTREACHED();
    return;
  }

  // We only want notifications for the associated profile.
  profile_ = profile;
  Source<Profile> source(profile_);

  // TODO(evanm): this is currently necessitated by generate_profile, which
  // runs without a browser process. generate_profile should really create
  // a browser process, at which point this check can then be nuked.
  if (!g_browser_process)
    return;

  // Register for the notifications we care about.
  // TODO(tc): Make a ScopedNotificationObserver so we don't have to remember
  // to remove these manually.
  registered_for_notifications_ = true;
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NOTIFY_HISTORY_URL_VISITED, source);
  service->AddObserver(this, NOTIFY_HISTORY_TYPED_URLS_MODIFIED, source);
  service->AddObserver(this, NOTIFY_HISTORY_URLS_DELETED, source);
  service->AddObserver(this, NOTIFY_URLS_STARRED, source);
}

void InMemoryHistoryBackend::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_HISTORY_URL_VISITED: {
      Details<history::URLVisitedDetails> visited_details(details);
      if (visited_details->row.typed_count() > 0) {
        URLsModifiedDetails modified_details;
        modified_details.changed_urls.push_back(visited_details->row);
        OnTypedURLsModified(modified_details);
      }
      break;
    }
    case NOTIFY_HISTORY_TYPED_URLS_MODIFIED:
      OnTypedURLsModified(
          *Details<history::URLsModifiedDetails>(details).ptr());
      break;
    case NOTIFY_HISTORY_URLS_DELETED:
      OnURLsDeleted(*Details<history::URLsDeletedDetails>(details).ptr());
      break;
    case NOTIFY_URLS_STARRED:
      OnURLsStarred(*Details<history::URLsStarredDetails>(details).ptr());
      break;
    default:
      // For simplicity, the unit tests send us all notifications, even when
      // we haven't registered for them, so don't assert here.
      break;
  }
}

void InMemoryHistoryBackend::OnTypedURLsModified(
    const URLsModifiedDetails& details) {
  DCHECK(db_.get());

  // Add or update the URLs.
  //
  // TODO(brettw) currently the rows in the in-memory database don't match the
  // IDs in the main database. This sucks. Instead of Add and Remove, we should
  // have Sync(), which would take the ID if it's given and add it.
  std::vector<history::URLRow>::const_iterator i;
  for (i = details.changed_urls.begin();
       i != details.changed_urls.end(); i++) {
    URLID id = db_->GetRowForURL(i->url(), NULL);
    if (id)
      db_->UpdateURLRow(id, *i);
    else
      db_->AddURL(*i);
  }
}

void InMemoryHistoryBackend::OnURLsDeleted(const URLsDeletedDetails& details) {
  DCHECK(db_.get());

  if (details.all_history) {
    // When all history is deleted, the individual URLs won't be listed. Just
    // create a new database to quickly clear everything out.
    db_.reset(new InMemoryDatabase);
    if (!db_->InitFromScratch())
      db_.reset();
    return;
  }

  // Delete all matching URLs in our database.
  for (std::set<GURL>::const_iterator i = details.urls.begin();
       i != details.urls.end(); ++i) {
    URLID id = db_->GetRowForURL(*i, NULL);
    if (id) {
      // We typically won't have most of them since we only have a subset of
      // history, so ignore errors.
      db_->DeleteURLRow(id);
    }
  }
}

void InMemoryHistoryBackend::OnURLsStarred(
    const history::URLsStarredDetails& details) {
  DCHECK(db_.get());

  for (std::set<GURL>::const_iterator i = details.changed_urls.begin();
       i != details.changed_urls.end(); ++i) {
    URLRow row;
    if (db_->GetRowForURL(*i, &row)) {
      // NOTE: We currently don't care about the star id from the in memory
      // db, so that we use a fake value. If this does become a problem,
      // then the notification will have to propagate the star id.
      row.set_star_id(details.starred ? kBogusStarredID : 0);
      db_->UpdateURLRow(row.id(), row);
    }
  }
}

}  // namespace history
