// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_

#include <vector>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension.h"

typedef std::vector<Extension*> ExtensionList;
class ExtensionsServiceBackend;
class UserScriptMaster;

// Interface for the frontend to implement. Typically, this will be
// ExtensionsService, but it can also be a test harness.
class ExtensionsServiceFrontendInterface
    : public base::RefCountedThreadSafe<ExtensionsServiceFrontendInterface> {
 public:
  virtual ~ExtensionsServiceFrontendInterface(){}

  // The message loop to invoke the frontend's methods on.
  virtual MessageLoop* GetMessageLoop() = 0;

  // Called when loading an extension fails.
  virtual void OnExtensionLoadError(const std::string& message) = 0;

  // Called with results from LoadExtensionsFromDirectory(). The frontend
  // takes ownership of the list.
  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions) = 0;
};


// Manages installed and running Chromium extensions.
class ExtensionsService : public ExtensionsServiceFrontendInterface {
 public:
  ExtensionsService(const FilePath& profile_directory,
                    UserScriptMaster* user_script_master);
  ~ExtensionsService();

  // Gets the list of currently installed extensions.
  const ExtensionList* extensions() const {
    return &extensions_;
  }

  // Initialize and start all installed extensions.
  bool Init();

  // ExtensionsServiceFrontendInterface
  virtual MessageLoop* GetMessageLoop();
  virtual void OnExtensionLoadError(const std::string& message);
  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions);

 private:
  // The name of the directory inside the profile where extensions are
  // installed to.
  static const FilePath::CharType* kInstallDirectoryName;

  // The message loop for the thread the ExtensionsService is running on.
  MessageLoop* message_loop_;

  // The backend that will do IO on behalf of this instance.
  scoped_refptr<ExtensionsServiceBackend> backend_;

  // The current list of installed extensions.
  ExtensionList extensions_;

  // The full path to the directory where extensions are installed.
  FilePath install_directory_;

  // The user script master for this profile.
  scoped_refptr<UserScriptMaster> user_script_master_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsService);
};

// Implements IO for the ExtensionsService.
// TODO(aa): Extract an interface out of this for testing the frontend, once the
// frontend has significant logic to test.
class ExtensionsServiceBackend
    : public base::RefCountedThreadSafe<ExtensionsServiceBackend> {
 public:
  ExtensionsServiceBackend(){};

  // Loads extensions from a directory. The extensions are assumed to be
  // unpacked in directories that are direct children of the specified path.
  // Errors are reported through OnExtensionLoadError(). On completion,
  // OnExtensionsLoadedFromDirectory() is called with any successfully loaded
  // extensions.
  bool LoadExtensionsFromDirectory(
      const FilePath &path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

 private:
  // Notify a frontend that there was an error loading an extension.
  void ReportExtensionLoadError(ExtensionsServiceFrontendInterface* frontend,
                                const std::wstring& path,
                                const std::string& error);

  // Notify a frontend that extensions were loaded.
  void ReportExtensionsLoaded(ExtensionsServiceFrontendInterface* frontend,
                              ExtensionList* extensions);

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
