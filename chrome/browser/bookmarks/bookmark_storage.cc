// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_storage.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json_writer.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/json_value_serializer.h"

namespace {

// Extension used for backup files (copy of main file created during startup).
const FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// Extension for the temporary file. We write to the temp file than move to
// kBookmarksFileName.
const FilePath::CharType kTmpExtension[] = FILE_PATH_LITERAL("tmp");

// How often we save.
const int kSaveDelayMS = 2500;

}  // namespace

// BookmarkStorage -------------------------------------------------------------

BookmarkStorage::BookmarkStorage(Profile* profile, BookmarkModel* model)
    : model_(model),
      ALLOW_THIS_IN_INITIALIZER_LIST(save_factory_(this)),
      backend_thread_(g_browser_process->file_thread()) {
  FilePath path = profile->GetPath().Append(chrome::kBookmarksFileName);
  FilePath tmp_history_path =
      profile->GetPath().Append(chrome::kHistoryBookmarksFileName);
  backend_ = new BookmarkStorageBackend(path, tmp_history_path);
}

void BookmarkStorage::LoadBookmarks(bool load_from_history) {
  if (!backend_thread()) {
    backend_->Read(scoped_refptr<BookmarkStorage>(this), NULL,
                   load_from_history);
  } else {
    backend_thread()->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(backend_.get(), &BookmarkStorageBackend::Read,
                          scoped_refptr<BookmarkStorage>(this),
                          MessageLoop::current(), load_from_history));
  }
}

void BookmarkStorage::ScheduleSave() {
  if (!backend_thread()) {
    SaveNow();
  } else if (save_factory_.empty()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, save_factory_.NewRunnableMethod(&BookmarkStorage::SaveNow),
        kSaveDelayMS);
  }
}

void BookmarkStorage::BookmarkModelDeleted() {
  if (!save_factory_.empty()) {
    // There's a pending save. We need to save now as otherwise by the time
    // SaveNow is invoked the model is gone.
    save_factory_.RevokeAll();
    SaveNow();
  }
  model_ = NULL;
}

void BookmarkStorage::LoadedBookmarks(Value* root_value,
                                      bool bookmark_file_exists,
                                      bool loaded_from_history) {
  scoped_ptr<Value> value_ref(root_value);

  if (model_) {
    if (root_value) {
      BookmarkCodec codec;
      codec.Decode(model_, *root_value);
    }
    model_->OnBookmarkStorageLoadedBookmarks(bookmark_file_exists,
                                             loaded_from_history);
  }
}

void BookmarkStorage::SaveNow() {
  if (!model_ || !model_->IsLoaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return;
  }

  BookmarkCodec codec;
  Value* value = codec.Encode(model_);
  // The backend deletes value in write.
  if (!backend_thread()) {
    backend_->Write(value);
  } else {
    backend_thread()->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(backend_.get(), &BookmarkStorageBackend::Write,
                          value));
  }
}

// BookmarkStorageBackend ------------------------------------------------------

BookmarkStorageBackend::BookmarkStorageBackend(
    const FilePath& path,
    const FilePath& tmp_history_path)
    : path_(path.ToWStringHack()),
      tmp_history_path_(tmp_history_path.ToWStringHack()) {
  // Make a backup of the current file.
  FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  file_util::CopyFile(path, backup_path);
}

void BookmarkStorageBackend::Write(Value* value) {
  DCHECK(value);

  // We own Value.
  scoped_ptr<Value> value_ref(value);

  std::string content;
  JSONWriter::Write(value, true, &content);

  // Write to a temp file, then rename.
  // TODO(port): this code was all written to use wstrings; needs cleaning up
  // for FilePath.
  FilePath tmp_file_filepath =
      FilePath::FromWStringHack(path_).ReplaceExtension(kTmpExtension);
  std::wstring tmp_file = tmp_file_filepath.ToWStringHack();

  int bytes_written = file_util::WriteFile(tmp_file, content.c_str(),
                                           static_cast<int>(content.length()));
  if (bytes_written != -1) {
    if (!file_util::Move(tmp_file, path_)) {
      // Rename failed. Try again on the off chance someone has locked either
      // file and hope we're successful the second time through.
      bool move_result = file_util::Move(tmp_file, path_);
      DCHECK(move_result);
      if (!move_result)
        LOG(WARNING) << " writing bookmarks failed, result=" << move_result;
      else
        LOG(INFO) << "wrote bookmarks, file=" << path_;
    } else {
      LOG(INFO) << "wrote bookmarks, file=" << path_;
    }
    // Nuke the history file so that we don't attempt to load from it again.
    file_util::Delete(tmp_history_path_, false);
  } else {
    LOG(WARNING) << " writing bookmarks failed, bytes written=" <<
        bytes_written;
  }
}

void BookmarkStorageBackend::Read(scoped_refptr<BookmarkStorage> service,
                                  MessageLoop* message_loop,
                                  bool load_from_history) {
  const std::wstring& path = load_from_history ? tmp_history_path_ : path_;
  bool bookmark_file_exists = file_util::PathExists(path);
  Value* root = NULL;
  if (bookmark_file_exists) {
    JSONFileValueSerializer serializer(path);
    root = serializer.Deserialize(NULL);
  }

  // BookmarkStorage takes ownership of root.
  if (message_loop) {
    message_loop->PostTask(FROM_HERE, NewRunnableMethod(
        service.get(), &BookmarkStorage::LoadedBookmarks, root,
        bookmark_file_exists, load_from_history));
  } else {
    service->LoadedBookmarks(root, bookmark_file_exists, load_from_history);
  }
}
