// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include "base/values.h"

class Browser;
class Extension;
class ExtensionsServiceBackend;
class GURL;
class Profile;
class ResourceDispatcherHost;
class SiteInstance;
class UserScriptMaster;
typedef std::vector<Extension*> ExtensionList;

// Interface for the frontend to implement. Typically, this will be
// ExtensionsService, but it can also be a test harness.
class ExtensionsServiceFrontendInterface
    : public base::RefCountedThreadSafe<ExtensionsServiceFrontendInterface> {
 public:
  virtual ~ExtensionsServiceFrontendInterface() {}

  // The message loop to invoke the frontend's methods on.
  virtual MessageLoop* GetMessageLoop() = 0;

  // Called when extensions are loaded by the backend. The frontend takes
  // ownership of the list.
  virtual void OnExtensionsLoaded(ExtensionList* extensions) = 0;

  // Called with results from InstallExtension().
  // |is_update| is true if the installation was an update to an existing
  // installed extension rather than a new installation.
  virtual void OnExtensionInstalled(Extension* extension, bool is_update) = 0;

  // Called when an existing extension is installed by the user. We may wish to
  // notify the user about the prior existence of the extension, or take some
  // action using the fact that the user chose to reinstall the extension as a
  // signal (for example, setting the default theme to the extension).
  virtual void OnExtensionVersionReinstalled(const std::string& id) = 0;
};


// Manages installed and running Chromium extensions.
class ExtensionsService : public ExtensionsServiceFrontendInterface {
 public:
  ExtensionsService(Profile* profile, UserScriptMaster* user_script_master);
  ~ExtensionsService();

  // Gets the list of currently installed extensions.
  const ExtensionList* extensions() const {
    return &extensions_;
  }

  // Initialize and start all installed extensions.
  bool Init();

  // Install the extension file at |extension_path|.  Will install as an
  // update if an older version is already installed.
  // For fresh installs, this method also causes the extension to be
  // immediately loaded.
  void InstallExtension(const FilePath& extension_path);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist and are "internal".
  void UninstallExtension(const std::string& extension_id);

  // Load the extension from the directory |extension_path|.
  void LoadExtension(const FilePath& extension_path);

  // Lookup an extension by |id|.
  virtual Extension* GetExtensionByID(std::string id);

  // ExtensionsServiceFrontendInterface
  virtual MessageLoop* GetMessageLoop();
  virtual void OnExtensionsLoaded(ExtensionList* extensions);
  virtual void OnExtensionInstalled(Extension* extension, bool is_update);
  virtual void OnExtensionVersionReinstalled(const std::string& id);

  // The name of the file that the current active version number is stored in.
  static const char* kCurrentVersionFileName;

 private:
  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

  // The message loop for the thread the ExtensionsService is running on.
  MessageLoop* message_loop_;

  // The current list of installed extensions.
  ExtensionList extensions_;

  // The full path to the directory where extensions are installed.
  FilePath install_directory_;

  // The backend that will do IO on behalf of this instance.
  scoped_refptr<ExtensionsServiceBackend> backend_;

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
  explicit ExtensionsServiceBackend(const FilePath& install_directory,
                                    ResourceDispatcherHost* rdh)
      : install_directory_(install_directory),
        resource_dispatcher_host_(rdh) {}

  // Loads extensions from the install directory. The extensions are assumed to
  // be unpacked in directories that are direct children of the specified path.
  // Errors are reported through ExtensionErrorReporter. On completion,
  // OnExtensionsLoaded() is called with any successfully loaded extensions.
  void LoadExtensionsFromInstallDirectory(
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  // Errors are reported through ExtensionErrorReporter. On completion,
  // OnExtensionsLoadedFromDirectory() is called with any successfully loaded
  // extensions.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  void LoadSingleExtension(
      const FilePath &path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Install the extension file at |extension_path|. Errors are reported through
  // ExtensionErrorReporter. ReportExtensionInstalled is called on success.
  void InstallExtension(
      const FilePath& extension_path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Check externally updated extensions for updates and install if necessary.
  // Errors are reported through ExtensionErrorReporter.
  // ReportExtensionInstalled is called on success.
  void CheckForExternalUpdates(
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend);

  // Deletes all versions of the extension from the filesystem. Note that only
  // extensions whose location() == INTERNAL can be uninstalled. Attempting to
  // uninstall other extensions will silently fail.
  void UninstallExtension(const std::string& extension_id);

 private:
  class UnpackerClient;
  friend class UnpackerClient;

  // Load a single extension from |extension_path|, the top directory of
  // a specific extension where its manifest file lives.
  Extension* LoadExtension(const FilePath& extension_path, bool require_id);

  // Load a single extension from |extension_path|, the top directory of
  // a versioned extension where its Current Version file lives.
  Extension* LoadExtensionCurrentVersion(const FilePath& extension_path);

  // Install a crx file at |extension_path|. If |expected_id| is not empty, it's
  // verified against the extension's manifest before installation. If
  // |from_external| is true, this extension install is from an external source,
  // ie the Windows registry, and will be marked as such. If the extension is
  // already installed, install the new version only if its version number is
  // greater than the current installed version.
  void InstallOrUpdateExtension(const FilePath& extension_path,
                                const std::string& expected_id,
                                bool from_external);

  // Finish installing an extension after it has been unpacked to
  // |temp_extension_dir| by our utility process.  If |expected_id| is not
  // empty, it's verified against the extension's manifest before installation.
  void OnExtensionUnpacked(const FilePath& extension_path,
                           const FilePath& temp_extension_dir,
                           const std::string expected_id,
                           bool from_external);

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error);

  // Notify the frontend that extensions were loaded.
  void ReportExtensionsLoaded(ExtensionList* extensions);

  // Notify the frontend that there was an error installing an extension.
  void ReportExtensionInstallError(const FilePath& extension_path,
                                   const std::string& error);

  // Notify the frontend that the extension had already been installed.
  void ReportExtensionVersionReinstalled(const std::string& id);

  // Notify the frontend that extensions were installed.
  // |is_update| is true if this was an update to an existing extension.
  void ReportExtensionInstalled(const FilePath& path, bool is_update);

  // Read the manifest from the front of the extension file.
  // Caller takes ownership of return value.
  DictionaryValue* ReadManifest(const FilePath& extension_path);

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

  // The top-level extensions directory being installed to.
  FilePath install_directory_;

  // We only need a pointer to this to pass along to other interfaces.
  ResourceDispatcherHost* resource_dispatcher_host_;

  // Whether errors result in noisy alerts.
  bool alert_on_error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
