// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"

class Browser;
class DictionaryValue;
class Extension;
class ExtensionsServiceBackend;
class GURL;
class PrefService;
class Profile;
class ResourceDispatcherHost;
class SkBitmap;
class SiteInstance;
class UserScriptMaster;

typedef std::vector<Extension*> ExtensionList;


// Manages installed and running Chromium extensions.
class ExtensionsService
    : public base::RefCountedThreadSafe<ExtensionsService> {
 public:

   // TODO(port): Move Crx package definitions to ExtentionCreator. They are
   // currently here because ExtensionCreator is excluded on linux & mac.

   // The size of the magic character sequence at the beginning of each crx
   // file, in bytes. This should be a multiple of 4.
   static const size_t kExtensionHeaderMagicSize = 4;

   // The maximum size the crx parser will tolerate for a public key.
   static const size_t kMaxPublicKeySize = 1 << 16;

   // The maximum size the crx parser will tolerate for a signature.
   static const size_t kMaxSignatureSize = 1 << 16;

   // The magic character sequence at the beginning of each crx file.
   static const char kExtensionHeaderMagic[];

   // The current version of the crx format.
   static const uint32 kCurrentVersion = 2;

   // This header is the first data at the beginning of an extension. Its
   // contents are purposely 32-bit aligned so that it can just be slurped into
   // a struct without manual parsing.
   struct ExtensionHeader {
     char magic[kExtensionHeaderMagicSize];
     uint32 version;
     size_t key_size;  // The size of the public key, in bytes.
     size_t signature_size;  // The size of the signature, in bytes.
     // An ASN.1-encoded PublicKeyInfo structure follows.
     // The signature follows.
   };

  ExtensionsService(Profile* profile,
                    MessageLoop* frontend_loop,
                    MessageLoop* backend_loop,
                    const std::string& registry_path);
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
  Extension* GetExtensionByID(std::string id);

  // Gets a list of external extensions. If |external_extensions| is non-null,
  // a dictionary with all external extensions (including extensions installed
  // through the registry on Windows builds) and their preferences are
  // returned. If |killed_extensions| is non-null, a set of string IDs
  // containing all external extension IDs with the killbit set are returned.
  void GetExternalExtensions(DictionaryValue* external_extensions,
                             std::set<std::string>* killed_extensions);

  // Gets the settings for an extension from preferences. If the key doesn't
  // exist, this function creates it (don't need to check return for NULL).
  DictionaryValue* GetOrCreateExtensionPref(const std::wstring& extension_id);

  // Writes a preference value for a particular extension |extension_id| under
  // the |key| specified. If |schedule_save| is true, it will also ask the
  // preference system to schedule a save to disk.
  bool UpdateExtensionPref(const std::wstring& extension_id,
                           const std::wstring& key,
                           Value* data_value,
                           bool schedule_save);

  // The name of the file that the current active version number is stored in.
  static const char* kCurrentVersionFileName;

  void set_extensions_enabled(bool enabled) { extensions_enabled_ = enabled; }
  void set_show_extensions_prompts(bool enabled) {
    show_extensions_prompts_ = enabled;
  }

  bool extensions_enabled() { return extensions_enabled_; }
  bool show_extensions_prompts() {
    return show_extensions_prompts_;
  }

 private:
  // For OnExtensionLoaded, OnExtensionInstalled, and
  // OnExtensionVersionReinstalled.
  friend class ExtensionsServiceBackend;

  // Called by the backend when extensions have been loaded.
  void OnExtensionsLoaded(ExtensionList* extensions);

  // Called by the backend when an extensoin hsa been installed.
  void OnExtensionInstalled(Extension* extension,
      Extension::InstallType install_type);

  // Called by the backend when an external extension has been installed.
  void OnExternalExtensionInstalled(
      const std::string& id, Extension::Location location);

  // Called by the backend when an attempt was made to reinstall the same
  // version of an existing extension.
  void OnExtensionOverinstallAttempted(const std::string& id);

  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

  // Preferences for the owning profile.
  PrefService* prefs_;

  // The message loop to use with the backend.
  MessageLoop* backend_loop_;

  // The current list of installed extensions.
  ExtensionList extensions_;

  // The full path to the directory where extensions are installed.
  FilePath install_directory_;

  // Whether or not extensions are enabled.
  bool extensions_enabled_;

  // Whether to notify users when they attempt to install an extension.
  bool show_extensions_prompts_;

  // The backend that will do IO on behalf of this instance.
  scoped_refptr<ExtensionsServiceBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsService);
};

// Implements IO for the ExtensionsService.
// TODO(aa): This can probably move into the .cc file.
class ExtensionsServiceBackend
    : public base::RefCountedThreadSafe<ExtensionsServiceBackend> {
 public:
  // |rdh| can be NULL in the case of test environment.
  // |registry_path| can be NULL *except* in the case of the test environment,
  // where it is specified to a temp location.
  ExtensionsServiceBackend(const FilePath& install_directory,
                          ResourceDispatcherHost* rdh,
                          MessageLoop* frontend_loop,
                          const std::string& registry_path);

  // Loads extensions from the install directory. The extensions are assumed to
  // be unpacked in directories that are direct children of the specified path.
  // Errors are reported through ExtensionErrorReporter. On completion,
  // OnExtensionsLoaded() is called with any successfully loaded extensions.
  void LoadExtensionsFromInstallDirectory(
      scoped_refptr<ExtensionsService> frontend,
      DictionaryValue* extension_prefs);

  // Loads a single extension from |path| where |path| is the top directory of
  // a specific extension where its manifest file lives.
  // Errors are reported through ExtensionErrorReporter. On completion,
  // OnExtensionsLoadedFromDirectory() is called with any successfully loaded
  // extensions.
  // TODO(erikkay): It might be useful to be able to load a packed extension
  // (presumably into memory) without installing it.
  void LoadSingleExtension(const FilePath &path,
                           scoped_refptr<ExtensionsService> frontend);

  // Install the extension file at |extension_path|. Errors are reported through
  // ExtensionErrorReporter. OnExtensionInstalled is called in the frontend on
  // success.
  void InstallExtension(const FilePath& extension_path,
                        scoped_refptr<ExtensionsService> frontend);

  // Check externally updated extensions for updates and install if necessary.
  // Errors are reported through ExtensionErrorReporter. Succcess is not
  // reported.
  void CheckForExternalUpdates(std::set<std::string> ids_to_ignore,
                               DictionaryValue* extension_prefs,
                               scoped_refptr<ExtensionsService> frontend);

  // Deletes all versions of the extension from the filesystem. Note that only
  // extensions whose location() == INTERNAL can be uninstalled. Attempting to
  // uninstall other extensions will silently fail.
  void UninstallExtension(const std::string& extension_id);

 private:
  class UnpackerClient;
  friend class UnpackerClient;

  // Utility function to read an extension manifest and return it as a
  // DictionaryValue. If it fails, NULL is returned and |error| contains an
  // appropriate message.
  DictionaryValue* ReadManifest(FilePath manifest_path, std::string* error);

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

  // Validates the signature of the extension in |extension_path|. Returns true
  // and the public key (in |key|) if the signature validates, false otherwise.
  bool ValidateSignature(const FilePath& extension_path, std::string* key_out);

  // Finish installing an extension after it has been unpacked to
  // |temp_extension_dir| by our utility process.  If |expected_id| is not
  // empty, it's verified against the extension's manifest before installation.
  // |manifest| and |images| are parsed information from the extension that
  // we want to write to disk in the browser process.
  void OnExtensionUnpacked(
      const FilePath& extension_path,
      const FilePath& temp_extension_dir,
      const std::string expected_id,
      bool from_external,
      const DictionaryValue& manifest,
      const std::vector< Tuple2<SkBitmap, FilePath> >& images);

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const FilePath& extension_path,
                                const std::string& error);

  // Notify the frontend that extensions were loaded.
  void ReportExtensionsLoaded(ExtensionList* extensions);

  // Notify the frontend that there was an error installing an extension.
  void ReportExtensionInstallError(const FilePath& extension_path,
                                   const std::string& error);

  // Notify the frontend that an attempt was made (but not carried out) to
  // install the same version of an existing extension.
  void ReportExtensionOverinstallAttempted(const std::string& id);

  // Checks a set of strings (containing id's to ignore) in order to determine
  // if the extension should be installed.
  bool ShouldSkipInstallingExtension(const std::set<std::string>& ids_to_ignore,
                                     const std::string& id);

  // Installs the extension if the extension is a newer version or if the
  // extension hasn't been installed before.
  void CheckVersionAndInstallExtension(const std::string& id,
                                       const std::string& extension_version,
                                       const FilePath& extension_path,
                                       bool from_external);

  // Read the manifest from the front of the extension file.
  // Caller takes ownership of return value.
  DictionaryValue* ReadManifest(const FilePath& extension_path);

  // Reads the Current Version file from |dir| into |version_string|.
  bool ReadCurrentVersion(const FilePath& dir, std::string* version_string);

  // Look for an existing installation of the extension |id| & return
  // an InstallType that would result from installing |new_version_str|.
  Extension::InstallType CompareToInstalledVersion(const std::string& id,
      const std::string& new_version_str, std::string* current_version_str);

  // Does an existing installed extension need to be reinstalled.
  bool NeedsReinstall(const std::string& id,
                      const std::string& current_version);

  // Install the extension dir by moving it from |source| to |dest| safely.
  bool InstallDirSafely(const FilePath& source,
                        const FilePath& dest);

  // Update the CurrentVersion file in |dest_dir| to |version|.
  bool SetCurrentVersion(const FilePath& dest_dir,
                         std::string version);

  // For the extension in |version_path| with |id|, check to see if it's an
  // externally managed extension.  If so return true if it should be
  // uninstalled.
  bool CheckExternalUninstall(DictionaryValue* extension_prefs,
                              const FilePath& version_path,
                              const std::string& id);

  // Should an extension of |id| and |version| be installed?
  // Returns true if no extension of type |id| is installed or if |version|
  // is greater than the current installed version.
  bool ShouldInstall(const std::string& id, const std::string& version);

  // The name of a temporary directory to install an extension into for
  // validation before finalizing install.
  static const char* kTempExtensionName;

  // This is a naked pointer which is set by each entry point.
  // The entry point is responsible for ensuring lifetime.
  ExtensionsService* frontend_;

  // The top-level extensions directory being installed to.
  FilePath install_directory_;

  // We only need a pointer to this to pass along to other interfaces.
  ResourceDispatcherHost* resource_dispatcher_host_;

  // Whether errors result in noisy alerts.
  bool alert_on_error_;

  // The message loop to use to call the frontend.
  MessageLoop* frontend_loop_;

  // The path to look for externally registered extensions in. This is a
  // registry key on windows, but it could be a similar string for the
  // appropriate system on other platforms in the future.
  std::string registry_path_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
