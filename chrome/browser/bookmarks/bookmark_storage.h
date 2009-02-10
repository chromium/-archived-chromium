// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_

#include "base/ref_counted.h"
#include "base/task.h"

class BookmarkModel;
class BookmarkStorageBackend;
class FilePath;
class Profile;
class MessageLoop;
class Value;

namespace base {
class Thread;
}

// BookmarkStorage handles reading/write the bookmark bar model. The
// BookmarkModel uses the BookmarkStorage to load bookmarks from disk, as well
// as notifying the BookmarkStorage every time the model changes.
//
// Internally BookmarkStorage uses BookmarkCodec to do the actual read/write.

class BookmarkStorage : public base::RefCountedThreadSafe<BookmarkStorage> {
  friend class BookmarkStorageBackend;

 public:
  // Creates a BookmarkStorage for the specified model
  BookmarkStorage(Profile* profile, BookmarkModel* model);

  // Loads the bookmarks into the model, notifying the model when done. If
  // load_from_history is true, the bookmarks are loaded from the file written
  // by history (StarredURLDatabase).
  void LoadBookmarks(bool load_from_history);

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

 private:
  // Callback from backend with the results of the bookmark file.
  void LoadedBookmarks(Value* root_value,
                       bool bookmark_file_exists,
                       bool loaded_from_history);

  // Schedules a save on the backend thread.
  void SaveNow();

  // Returns the thread the backend is run on.
  base::Thread* backend_thread() const { return backend_thread_; }

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkModel* model_;

  // Used to delay saves.
  ScopedRunnableMethodFactory<BookmarkStorage> save_factory_;

  // The backend handles actual reading/writing to disk.
  scoped_refptr<BookmarkStorageBackend> backend_;

  // Thread read/writing is run on. This comes from the profile, and is null
  // during testing.
  base::Thread* backend_thread_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

// Used to save the bookmarks on the file thread.
class BookmarkStorageBackend :
    public base::RefCountedThreadSafe<BookmarkStorageBackend> {
 public:
  explicit BookmarkStorageBackend(const FilePath& path,
                                  const FilePath& tmp_history_path);

  // Writes the specified value to disk. This takes ownership of |value| and
  // deletes it when done.
  void Write(Value* value);

  // Reads the bookmarks from kBookmarksFileName. Notifies |service| with
  // the results on the specified MessageLoop.
  void Read(scoped_refptr<BookmarkStorage> service,
            MessageLoop* message_loop,
            bool load_from_history);

 private:
  // Path we read/write to.
  const std::wstring path_;

  // Path bookmarks are read from if asked to load from history file.
  const std::wstring tmp_history_path_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorageBackend);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
