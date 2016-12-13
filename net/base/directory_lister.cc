// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/directory_lister.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "net/base/net_errors.h"

namespace net {

static const int kFilesPerEvent = 8;

class DirectoryDataEvent : public Task {
 public:
  explicit DirectoryDataEvent(DirectoryLister* d)
      : lister(d), count(0), error(0) {
  }

  void Run() {
    if (count) {
      lister->OnReceivedData(data, count);
    } else {
      lister->OnDone(error);
    }
  }

  scoped_refptr<DirectoryLister> lister;
  file_util::FileEnumerator::FindInfo data[kFilesPerEvent];
  int count, error;
};

DirectoryLister::DirectoryLister(const FilePath& dir,
                                 DirectoryListerDelegate* delegate)
    : dir_(dir),
      delegate_(delegate),
      message_loop_(NULL),
      thread_(NULL),
      canceled_(false) {
  DCHECK(!dir.value().empty());
}

DirectoryLister::~DirectoryLister() {
  if (thread_) {
    PlatformThread::Join(thread_);
  }
}

bool DirectoryLister::Start() {
  // spawn a thread to enumerate the specified directory

  // pass events back to the current thread
  message_loop_ = MessageLoop::current();
  DCHECK(message_loop_) << "calling thread must have a message loop";

  AddRef();  // the thread will release us when it is done

  if (!PlatformThread::Create(0, this, &thread_)) {
    Release();
    return false;
  }

  return true;
}

void DirectoryLister::Cancel() {
  canceled_ = true;

  if (thread_) {
    PlatformThread::Join(thread_);
    thread_ = NULL;
  }
}

void DirectoryLister::ThreadMain() {
  DirectoryDataEvent* e = new DirectoryDataEvent(this);

  if (!file_util::DirectoryExists(dir_)) {
    e->error = net::ERR_FILE_NOT_FOUND;
    message_loop_->PostTask(FROM_HERE, e);
    Release();
    return;
  }

  file_util::FileEnumerator file_enum(dir_, false,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::INCLUDE_DOT_DOT));

  while (!canceled_ && !(file_enum.Next().value().empty())) {
    file_enum.GetFindInfo(&e->data[e->count]);

    if (++e->count == kFilesPerEvent) {
      message_loop_->PostTask(FROM_HERE, e);
      e = new DirectoryDataEvent(this);
    }
  }

  if (e->count > 0) {
    message_loop_->PostTask(FROM_HERE, e);
    e = new DirectoryDataEvent(this);
  }

  // Notify done
  Release();
  message_loop_->PostTask(FROM_HERE, e);
}

void DirectoryLister::OnReceivedData(
    const file_util::FileEnumerator::FindInfo* data, int count) {
  // Since the delegate can clear itself during the OnListFile callback, we
  // need to null check it during each iteration of the loop.  Similarly, it is
  // necessary to check the canceled_ flag to avoid sending data to a delegate
  // who doesn't want anymore.
  for (int i = 0; !canceled_ && delegate_ && i < count; ++i)
    delegate_->OnListFile(data[i]);
}

void DirectoryLister::OnDone(int error) {
  // If canceled, we need to report some kind of error, but don't overwrite the
  // error condition if it is already set.
  if (!error && canceled_)
    error = net::ERR_ABORTED;

  if (delegate_)
    delegate_->OnListDone(error);
}

}  // namespace net
