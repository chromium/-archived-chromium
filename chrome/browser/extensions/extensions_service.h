// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_

#include <string>
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

  // Install the extension file at |extension_path|.  Will install as an
  // update if an older version is already installed.
  // For fresh installs, this method also causes the extension to be
  // immediately loaded.
  virtual void InstallExtension(const FilePath& extension_path) = 0;

  // Load the extension from the directory |extension_path|.
  virtual void LoadExtension(const FilePath& extension_path) = 0;

  // Called when loading an extension fails.
  virtual void OnExtensionLoadError(bool alert_on_error,
                                    const std::string& message) = 0;

  // Called with results from LoadExtensionsFromDirectory(). The frontend
  // takes ownership of the list.
  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions) = 0;

  // Called when installing an extension fails.
  virtual void OnExtensionInstallError(bool alert_on_error,
                                       const std::string& message) = 0;

  // Called with results from InstallExtension().
  // |is_update| is true if the installation was an update to an existing
  // installed extension rather than a new installation.
  virtual void OnExtensionInstalled(FilePath path, bool is_update) = 0;
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
  virtual void OnExtensionLoadError(bool alert_on_error,
                                    const std::string& message);
  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions);
  virtual void OnExtensionInstallError(bool alert_on_error,
                                       const std::string& message);
  virtual void OnExtensionInstalled(FilePath path, bool is_update);

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
  void LoadExtensionsFromDirectory(
      const FilePath &path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  // Errors are reported through OnExtensionLoadError(). On completion,
  // OnExtensionsLoadedFromDirectory() is called with any successfully loaded
  // extensions.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  void LoadSingleExtension(
      const FilePath &path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Install the extension file at extension_path to install_dir.
  // ReportExtensionInstallError is called on error.
  // ReportExtensionInstalled is called on success.
  void InstallExtension(
      const FilePath& extension_path,
      const FilePath& install_dir,
      bool alert_on_error,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Check externally updated extensions for updates and install if necessary.
  // ReportExtensionInstallError is called on error.
  // ReportExtensionInstalled is called on success.
  void CheckForExternalUpdates(
      const FilePath& install_dir,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

 private:
  // Load a single extension from |extension_path_|, the top directory of
  // a specific extension where its manifest file lives.
  Extension* LoadExtension();

  // Load a single extension from |extension_path_|, the top directory of
  // a versioned extension where its Current Version file lives.
  Extension* LoadExtensionCurrentVersion();

  // Install a crx file at |extension_path_| into |install_directory_|.
  // If |expected_id| is not empty, it's verified against the extension's
  // manifest before installationl.  If the extension is already installed,
  // install the new version only if its version number is greater than the
  // current installed version.
  void InstallOrUpdateExtension(const std::string& expected_id);

  // Notify a frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const std::string& error);

  // Notify a frontend that extensions were loaded.
  void ReportExtensionsLoaded(ExtensionList* extensions);

  // Notify a frontend that there was an error installing an extension.
  void ReportExtensionInstallError(const std::string& error);

  // Notify a frontend that extensions were installed.
  // |is_update| is true if this was an update to an existing extension.
  void ReportExtensionInstalled(FilePath path, bool is_update);

  // Read the manifest from the front of the extension file.
  // Caller takes ownership of return value.
  DictionaryValue* ReadManifest();

  // Reads the Current Version file from |dir| into |version_string|.
  bool ReadCurrentVersion(const FilePath& dir, std::string* version_string);

  // Check that the version to be installed is greater than the current
  // installed extension.
  bool CheckCurrentVersion(const std::string& version,
                           const std::string& current_version,
                           const FilePath& dest_dir);

  // Install the extension dir by moving it from |source| to |dest| safely.
  bool InstallDirSafely(const FilePath& source,
                        const FilePath& dest);

  // Update the CurrentVersion file in |dest_dir| to |version|.
  bool SetCurrentVersion(const FilePath& dest_dir,
                         std::string version);

  // For the extension at |path| with |id|, check to see if it's an
  // externally managed extension.  If so return true if it should be
  // uninstalled.
  bool CheckExternalUninstall(const FilePath& path, const std::string& id);

  // Deletes all versions of the extension from the filesystem.
  // |path| points at a specific extension version dir.
  void UninstallExtension(const FilePath& path);

  // Should an extension of |id| and |version| be installed?
  // Returns true if no extension of type |id| is installed or if |version|
  // is greater than the current installed version.
  bool ShouldInstall(const std::string& id, const std::string& version);

  // The name of a temporary directory to install an extension into for
  // validation before finalizing install.
  static const char* kTempExtensionName;

  // This is a naked pointer which is set by each entry point.
  // The entry point is responsible for ensuring lifetime.
  ExtensionsServiceFrontendInterface* frontend_;

  // The extension path being loaded or installed.
  FilePath extension_path_;

  // The top-level extensions directory being installed to.
  FilePath install_directory_;

  // Whether errors result in noisy alerts.
  bool alert_on_error_;

  // Whether the current install is from an external source.
  bool external_install_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
