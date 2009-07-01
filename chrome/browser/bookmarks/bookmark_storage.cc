// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_storage.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/json_writer.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

using base::TimeTicks;

namespace {

// Extension used for backup files (copy of main file created during startup).
const FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

class BackupTask : public Task {
 public:
  explicit BackupTask(const FilePath& path) : path_(path) {
  }

  virtual void Run() {
    FilePath backup_path = path_.ReplaceExtension(kBackupExtension);
    file_util::CopyFile(path_, backup_path);
  }

 private:
  const FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(BackupTask);
};

class FileDeleteTask : public Task {
 public:
  explicit FileDeleteTask(const FilePath& path) : path_(path) {
  }

  virtual void Run() {
    file_util::Delete(path_, true);
  }

 private:
  const FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(FileDeleteTask);
};

}  // namespace

class BookmarkStorage::LoadTask : public Task {
 public:
  LoadTask(const FilePath& path,
           MessageLoop* loop,
           BookmarkStorage* storage,
           LoadDetails* details,
           bool persist_ids)
      : path_(path),
        loop_(loop),
        storage_(storage),
        details_(details),
        persist_ids_(persist_ids) {
  }

  virtual void Run() {
    bool bookmark_file_exists = file_util::PathExists(path_);
    if (bookmark_file_exists) {
      JSONFileValueSerializer serializer(path_);
      scoped_ptr<Value> root(serializer.Deserialize(NULL));

      if (root.get()) {
        // Building the index cane take a while, so we do it on the background
        // thread.
        int max_node_id = 0;
        BookmarkCodec codec(persist_ids_);
        TimeTicks start_time = TimeTicks::Now();
        codec.Decode(details_->bb_node(), details_->other_folder_node(),
                     &max_node_id, *root.get());
        details_->set_max_id(std::max(max_node_id, details_->max_id()));
        details_->set_computed_checksum(codec.computed_checksum());
        details_->set_stored_checksum(codec.stored_checksum());
        UMA_HISTOGRAM_TIMES("Bookmarks.DecodeTime",
                            TimeTicks::Now() - start_time);

        start_time = TimeTicks::Now();
        AddBookmarksToIndex(details_->bb_node());
        AddBookmarksToIndex(details_->other_folder_node());
        UMA_HISTOGRAM_TIMES("Bookmarks.CreateBookmarkIndexTime",
                            TimeTicks::Now() - start_time);
      }
    }

    if (loop_) {
      loop_->PostTask(FROM_HERE, NewRunnableMethod(
          storage_.get(), &BookmarkStorage::OnLoadFinished,
          bookmark_file_exists, path_));
    } else {
      storage_->OnLoadFinished(bookmark_file_exists, path_);
    }
  }

 private:
  // Adds node to the model's index, recursing through all children as well.
  void AddBookmarksToIndex(BookmarkNode* node) {
    if (node->is_url()) {
      details_->index()->Add(node);
    } else {
      for (int i = 0; i < node->GetChildCount(); ++i)
        AddBookmarksToIndex(node->GetChild(i));
    }
  }

  const FilePath path_;
  MessageLoop* loop_;
  scoped_refptr<BookmarkStorage> storage_;
  LoadDetails* details_;
  bool persist_ids_;

  DISALLOW_COPY_AND_ASSIGN(LoadTask);
};

// BookmarkStorage -------------------------------------------------------------

BookmarkStorage::BookmarkStorage(Profile* profile, BookmarkModel* model)
    : profile_(profile),
      model_(model),
      backend_thread_(g_browser_process->file_thread()),
      writer_(profile->GetPath().Append(chrome::kBookmarksFileName),
              backend_thread_),
      tmp_history_path_(
          profile->GetPath().Append(chrome::kHistoryBookmarksFileName)) {
  writer_.set_commit_interval(base::TimeDelta::FromMilliseconds(kSaveDelayMS));
  RunTaskOnBackendThread(new BackupTask(writer_.path()));
}

BookmarkStorage::~BookmarkStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void BookmarkStorage::LoadBookmarks(LoadDetails* details) {
  DCHECK(!details_.get());
  DCHECK(details);
  details_.reset(details);
  DoLoadBookmarks(writer_.path());
}

void BookmarkStorage::DoLoadBookmarks(const FilePath& path) {
  Task* task = new LoadTask(path,
                            backend_thread() ? MessageLoop::current() : NULL,
                            this,
                            details_.get(),
                            model_->PersistIDs());
  RunTaskOnBackendThread(task);
}

void BookmarkStorage::MigrateFromHistory() {
  // We need to wait until history has finished loading before reading
  // from generated bookmarks file.
  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history) {
    // This happens in unit tests.
    if (model_)
      model_->DoneLoading(details_.release());
    return;
  }
  if (!history->backend_loaded()) {
    // The backend isn't finished loading. Wait for it.
    notification_registrar_.Add(this, NotificationType::HISTORY_LOADED,
                                Source<Profile>(profile_));
  } else {
    DoLoadBookmarks(tmp_history_path_);
  }
}

void BookmarkStorage::OnHistoryFinishedWriting() {
  notification_registrar_.Remove(this, NotificationType::HISTORY_LOADED,
                                 Source<Profile>(profile_));

  // This is used when migrating bookmarks data from database to file.
  // History wrote the file for us, and now we want to load data from it.
  DoLoadBookmarks(tmp_history_path_);
}

void BookmarkStorage::ScheduleSave() {
  writer_.ScheduleWrite(this);
}

void BookmarkStorage::BookmarkModelDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();
  model_ = NULL;
}

bool BookmarkStorage::SerializeData(std::string* output) {
  BookmarkCodec codec(model_->PersistIDs());
  scoped_ptr<Value> value(codec.Encode(model_));
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*(value.get()));
}

void BookmarkStorage::OnLoadFinished(bool file_exists, const FilePath& path) {
  if (path == writer_.path() && !file_exists) {
    // The file doesn't exist. This means one of two things:
    // 1. A clean profile.
    // 2. The user is migrating from an older version where bookmarks were
    //    saved in history.
    // We assume step 2. If history had the bookmarks, it will write the
    // bookmarks to a file for us.
    MigrateFromHistory();
    return;
  }

  if (!model_)
    return;

  model_->DoneLoading(details_.release());

  if (path == tmp_history_path_) {
    // We just finished migration from history. Save now to new file,
    // after the model is created and done loading.
    SaveNow();

    // Clean up after migration from history.
    RunTaskOnBackendThread(new FileDeleteTask(tmp_history_path_));
  }
}

void BookmarkStorage::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::HISTORY_LOADED:
      OnHistoryFinishedWriting();
      break;

    default:
      NOTREACHED();
      break;
  }
}

bool BookmarkStorage::SaveNow() {
  if (!model_ || !model_->IsLoaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return false;
  }

  std::string data;
  if (!SerializeData(&data))
    return false;
  writer_.WriteNow(data);
  return true;
}

void BookmarkStorage::RunTaskOnBackendThread(Task* task) const {
  if (backend_thread()) {
    backend_thread()->message_loop()->PostTask(FROM_HERE, task);
  } else {
    task->Run();
    delete task;
  }
}
