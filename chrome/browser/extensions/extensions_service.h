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

  // Install the extension file at |extension_path|.
  virtual void InstallExtension(const FilePath& extension_path) = 0;

  // Load the extension from the directory |extension_path|.
  virtual void LoadExtension(const FilePath& extension_path) = 0;

  // Called when loading an extension fails.
  virtual void OnExtensionLoadError(const std::string& message) = 0;

  // Called with results from LoadExtensionsFromDirectory(). The frontend
  // takes ownership of the list.
  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions) = 0;

  // Called when installing an extension fails.
  virtual void OnExtensionInstallError(const std::string& message) = 0;

  // Called with results from InstallExtension().
  virtual void OnExtensionInstalled(FilePath path) = 0;
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
  virtual void InstallExtension(const FilePath& extension_path);
  virtual void LoadExtension(const FilePath& extension_path);
  virtual void OnExtensionLoadError(const std::string& message);
  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions);
  virtual void OnExtensionInstallError(const std::string& message);
  virtual void OnExtensionInstalled(FilePath path);

  // The name of the file that the current active version number is stored in.
  static const char* kCurrentVersionFileName;

 private:
  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

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

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  // Errors are reported through OnExtensionLoadError(). On completion,
  // OnExtensionsLoadedFromDirectory() is called with any successfully loaded
  // extensions.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  bool LoadSingleExtension(
      const FilePath &path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Install the extension file at extension_path to install_dir.
  // ReportExtensionInstallError is called on error.
  // ReportExtensionInstalled is called on success.
  bool InstallExtension(
      const FilePath& extension_path,
      const FilePath& install_dir,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

 private:
  // Load a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  Extension* LoadExtension(
      const FilePath &path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Notify a frontend that there was an error loading an extension.
  void ReportExtensionLoadError(ExtensionsServiceFrontendInterface* frontend,
                                const FilePath& path,
                                const std::string& error);

  // Notify a frontend that extensions were loaded.
  void ReportExtensionsLoaded(ExtensionsServiceFrontendInterface* frontend,
                              ExtensionList* extensions);

  // Notify a frontend that there was an error installing an extension.
  void ReportExtensionInstallError(ExtensionsServiceFrontendInterface* frontend,
                                   const FilePath& path,
                                   const std::string& error);

  // Notify a frontend that extensions were installed.
  void ReportExtensionInstalled(ExtensionsServiceFrontendInterface* frontend,
                                FilePath path);

  // Read the manifest from the front of the extension file.
  DictionaryValue* ReadManifest(const FilePath& extension_path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Reads the Current Version file from |extension_path|.
  bool ReadCurrentVersion(const FilePath& extension_path,
                          std::string* version_string);

  // Check that the version to be installed is > the current installed
  // extension.
  bool CheckCurrentVersion(const FilePath& extension_path,
      const std::string& version,
      const FilePath& dest_dir,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Unzip the extension into |dest_dir|.
  bool UnzipExtension(const FilePath& extension_path,
      const FilePath& dest_dir,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Install the extension dir by moving it from |source| to |dest| safely.
  bool InstallDirSafely(const FilePath& extension_path,
      const FilePath& source, const FilePath& dest,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Update the CurrentVersion file in |dest_dir| to |version|.
  bool SetCurrentVersion(const FilePath& extension_path,
      const FilePath& dest_dir,
      std::string version,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // The name of a temporary directory to install an extension into for
  // validation before finalizing install.
  static const char* kTempExtensionName;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
