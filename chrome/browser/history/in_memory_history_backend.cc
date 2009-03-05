// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_history_backend.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"

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
    service->RemoveObserver(this, NotificationType::HISTORY_URL_VISITED,
                            source);
    service->RemoveObserver(this, NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                            source);
    service->RemoveObserver(this, NotificationType::HISTORY_URLS_DELETED,
                            source);
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
  service->AddObserver(this, NotificationType::HISTORY_URL_VISITED, source);
  service->AddObserver(this,
                       NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                       source);
  service->AddObserver(this, NotificationType::HISTORY_URLS_DELETED, source);
}

void InMemoryHistoryBackend::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::HISTORY_URL_VISITED: {
      Details<history::URLVisitedDetails> visited_details(details);
      if (visited_details->row.typed_count() > 0) {
        URLsModifiedDetails modified_details;
        modified_details.changed_urls.push_back(visited_details->row);
        OnTypedURLsModified(modified_details);
      }
      break;
    }
    case NotificationType::HISTORY_TYPED_URLS_MODIFIED:
      OnTypedURLsModified(
          *Details<history::URLsModifiedDetails>(details).ptr());
      break;
    case NotificationType::HISTORY_URLS_DELETED:
      OnURLsDeleted(*Details<history::URLsDeletedDetails>(details).ptr());
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

}  // namespace history

