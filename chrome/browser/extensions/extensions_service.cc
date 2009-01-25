// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "base/file_util.h"
#include "base/values.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"

// ExtensionsService

const FilePath::CharType* ExtensionsService::kInstallDirectoryName =
    FILE_PATH_LITERAL("Extensions");

ExtensionsService::ExtensionsService(const FilePath& profile_directory,
                                     UserScriptMaster* user_script_master)
    : message_loop_(MessageLoop::current()),
      backend_(new ExtensionsServiceBackend),
      install_directory_(profile_directory.Append(kInstallDirectoryName)),
      user_script_master_(user_script_master) {
}

ExtensionsService::~ExtensionsService() {
  for (ExtensionList::iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    delete *iter;
  }
}

bool ExtensionsService::Init() {
  // TODO(aa): This message loop should probably come from a backend
  // interface, similar to how the message loop for the frontend comes
  // from the frontend interface.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::LoadExtensionsFromDirectory,
          install_directory_,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
  // TODO(aa): Load extensions from other registered directories.

  return true;
}

MessageLoop* ExtensionsService::GetMessageLoop() {
  return message_loop_;
}

void ExtensionsService::OnExtensionsLoadedFromDirectory(
    ExtensionList* new_extensions) {
  extensions_.insert(extensions_.end(), new_extensions->begin(),
                     new_extensions->end());

  // Tell UserScriptMaster about any scripts in the loaded extensions.
  for (ExtensionList::iterator extension = extensions_.begin();
       extension != extensions_.end(); ++extension) {
    const UserScriptList& scripts = (*extension)->user_scripts();
    for (UserScriptList::const_iterator script = scripts.begin();
         script != scripts.end(); ++script) {
      user_script_master_->AddLoneScript(*script);
    }
  }

  // Tell UserScriptMaster to also watch the extensions directory for changes
  // and then kick off the first scan.
  // TODO(aa): This should go away when we implement the --extension flag, since
  // developing scripts in the Extensions directory will no longer be a common
  // use-case.
  user_script_master_->AddWatchedPath(install_directory_);
  user_script_master_->StartScan();

  NotificationService::current()->Notify(NOTIFY_EXTENSIONS_LOADED,
      NotificationService::AllSources(),
      Details<ExtensionList>(new_extensions));

  delete new_extensions;
}

void ExtensionsService::OnExtensionLoadError(const std::string& error) {
  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(WARNING) << error;
}


// ExtensionsServicesBackend

bool ExtensionsServiceBackend::LoadExtensionsFromDirectory(
    const FilePath& path_in,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  FilePath path = path_in;
  if (!file_util::AbsolutePath(&path))
    NOTREACHED();

  // Find all child directories in the install directory and load their
  // manifests. Post errors and results to the frontend.
  scoped_ptr<ExtensionList> extensions(new ExtensionList);
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (FilePath child_path = enumerator.Next(); !child_path.value().empty();
       child_path = enumerator.Next()) {
    FilePath manifest_path =
        child_path.AppendASCII(Extension::kManifestFilename);
    if (!file_util::PathExists(manifest_path)) {
      ReportExtensionLoadError(frontend.get(), child_path.ToWStringHack(),
                               Extension::kInvalidManifestError);
      continue;
    }

    JSONFileValueSerializer serializer(manifest_path.ToWStringHack());
    std::string error;
    scoped_ptr<Value> root(serializer.Deserialize(&error));
    if (!root.get()) {
      ReportExtensionLoadError(frontend.get(), child_path.ToWStringHack(),
                               error);
      continue;
    }

    if (!root->IsType(Value::TYPE_DICTIONARY)) {
      ReportExtensionLoadError(frontend.get(), child_path.ToWStringHack(),
                               Extension::kInvalidManifestError);
      continue;
    }

    scoped_ptr<Extension> extension(new Extension(child_path));
    if (!extension->InitFromValue(*static_cast<DictionaryValue*>(root.get()),
                                  &error)) {
      ReportExtensionLoadError(frontend.get(), child_path.ToWStringHack(),
                               error);
      continue;
    }

    extensions->push_back(extension.release());
  }

  ReportExtensionsLoaded(frontend.get(), extensions.release());
  return true;
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    ExtensionsServiceFrontendInterface *frontend, const std::wstring& path,
    const std::string &error) {
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     WideToASCII(path).c_str(), error.c_str());
  frontend->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend, &ExtensionsServiceFrontendInterface::OnExtensionLoadError,
      message));
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionsServiceFrontendInterface *frontend, ExtensionList* extensions) {
  frontend->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend,
      &ExtensionsServiceFrontendInterface::OnExtensionsLoadedFromDirectory,
      extensions));
}
