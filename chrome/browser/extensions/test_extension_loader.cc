// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_loader.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// How long to wait for the extension to load before giving up.
const int kLoadTimeoutMs = 5000;

}  // namespace

TestExtensionLoader::TestExtensionLoader(Profile* profile)
    : profile_(profile),
      extension_(NULL),
      loading_extension_id_(NULL) {
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
      NotificationService::AllSources());

  profile_->GetExtensionsService()->Init();
  DCHECK(profile_->GetExtensionsService()->extensions()->empty()); 
}

Extension* TestExtensionLoader::Load(const char* extension_id,
                                     const FilePath& path) {
  loading_extension_id_ = extension_id;

  // Load the extension.
  profile_->GetExtensionsService()->LoadExtension(path);

  // Wait for the load to complete.  Stick a QuitTask into the message loop
  // with the timeout so it will exit if the extension never loads.
  extension_ = NULL;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      new MessageLoop::QuitTask, kLoadTimeoutMs);
  ui_test_utils::RunMessageLoop();

  return extension_;
}

void TestExtensionLoader::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::EXTENSIONS_LOADED) {
    ExtensionList* extensions = Details<ExtensionList>(details).ptr();
    for (size_t i = 0; i < (*extensions).size(); i++) {
      if ((*extensions)[i]->id() == loading_extension_id_) {
        extension_ = (*extensions)[i];
        MessageLoopForUI::current()->Quit();
        break;
      }
    }
  } else {
    NOTREACHED();
  }
}
