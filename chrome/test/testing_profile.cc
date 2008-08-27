// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_profile.h"

#include "chrome/browser/history/history_backend.h"
#include "chrome/common/chrome_constants.h"

TestingProfile::TestingProfile()
    : start_time_(Time::Now()), has_history_service_(false) {
  PathService::Get(base::DIR_TEMP, &path_);
  file_util::AppendToPath(&path_, L"TestingProfilePath");
  file_util::Delete(path_, true);
  file_util::CreateDirectory(path_);
}

TestingProfile::~TestingProfile() {
  DestroyHistoryService();
  file_util::Delete(path_, true);
}

void TestingProfile::CreateHistoryService(bool delete_file) {
  if (history_service_.get())
    history_service_->Cleanup();

  history_service_ = NULL;

  if (delete_file) {
    std::wstring path = GetPath();
    file_util::AppendToPath(&path, chrome::kHistoryFilename);
    file_util::Delete(path, false);
  }
  history_service_ = new HistoryService(this);
  history_service_->Init(GetPath(), bookmark_bar_model_.get());
}

void TestingProfile::DestroyHistoryService() {
  if (!history_service_.get())
    return;

  history_service_->NotifyRenderProcessHostDestruction(0);
  history_service_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
  history_service_->Cleanup();
  history_service_ = NULL;

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  MessageLoop::current()->Run();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();
}

void TestingProfile::CreateBookmarkBarModel(bool delete_file) {
  // Nuke the model first, that way we're sure it's done writing to disk.
  bookmark_bar_model_.reset(NULL);

  if (delete_file) {
    std::wstring path = GetPath();
    file_util::AppendToPath(&path, chrome::kBookmarksFileName);
    file_util::Delete(path, false);
  }
  bookmark_bar_model_.reset(new BookmarkBarModel(this));
  if (history_service_.get()) {
    history_service_->history_backend_->bookmark_service_ =
        bookmark_bar_model_.get();
    history_service_->history_backend_->expirer_.bookmark_service_ =
        bookmark_bar_model_.get();
  }
  bookmark_bar_model_->Load();
}

void TestingProfile::CreateTemplateURLModel() {
  template_url_model_.reset(new TemplateURLModel(this));
}

