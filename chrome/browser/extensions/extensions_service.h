// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/tuple.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/common/extensions/extension.h"

class Browser;
class DictionaryValue;
class Extension;
class ExtensionsServiceBackend;
class GURL;
class MessageLoop;
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

  // The name of the directory inside the profile where extensions are
  // installed to.
  static const char* kInstallDirectoryName;

  ExtensionsService(Profile* profile,
                    const CommandLine* command_line,
                    PrefService* prefs,
                    const FilePath& install_directory,
                    MessageLoop* frontend_loop,
                    MessageLoop* backend_loop);
  ~ExtensionsService();

  // Gets the list of currently installed extensions.
  const ExtensionList* extensions() const {
    return &extensions_;
  }

  // Initialize and start all installed extensions.
  void Init();

  // Install the extension file at |extension_path|.  Will install as an
  // update if an older version is already installed.
  // For fresh installs, this method also causes the extension to be
  // immediately loaded.
  void InstallExtension(const FilePath& extension_path);

  // A callback for when installs finish. If the Extension* parameter is
  // null then the install failed.
  typedef Callback2<const FilePath&, Extension*>::Type Callback;

  // Updates a currently-installed extension with the contents from
  // |extension_path|. The |alert_on_error| parameter controls whether the
  // user will be notified in the event of failure. If |callback| is non-null,
  // it will be called back when the update is finished (in success or failure).
  // This is useful to know when the service is done using |extension_path|.
  // Also, this takes ownership of |callback| if it's non-null.
  void UpdateExtension(const std::string& id, const FilePath& extension_path,
                       bool alert_on_error, Callback* callback);

  // Uninstalls the specified extension. Callers should only call this method
  // with extensions that exist.
  void UninstallExtension(const std::string& extension_id,
                          bool external_uninstall);

  // Load the extension from the directory |extension_path|.
  void LoadExtension(const FilePath& extension_path);

  // Load all known extensions (used by startup and testing code).
  void LoadAllExtensions();

  // Check for updates (or potentially new extensions from external providers)
  void CheckForUpdates();

  // Unload the specified extension.
  void UnloadExtension(const std::string& extension_id);

  // Unload all extensions.
  void UnloadAllExtensions();

  // Called only by testing.
  void ReloadExtensions();

  // Scan the extension directory and clean up the cruft.
  void GarbageCollectExtensions();

  // Lookup an extension by |id|.
  Extension* GetExtensionById(const std::string& id);

  // Lookup an extension by |url|.  This uses the host of the URL as the id.
  Extension* GetExtensionByURL(const GURL& url);

  // Clear all ExternalExtensionProviders.
  void ClearProvidersForTesting();

  // Sets an ExternalExtensionProvider for the service to use during testing.
  // |location| specifies what type of provider should be added.
  void SetProviderForTesting(Extension::Location location,
                             ExternalExtensionProvider* test_provider);

  // The name of the file that the current active version number is stored in.
  static const char* kCurrentVersionFileName;

  void SetExtensionsEnabled(bool enabled);
  bool extensions_enabled() { return extensions_enabled_; }

  void set_show_extensions_prompts(bool enabled) {
    show_extensions_prompts_ = enabled;
  }

  bool show_extensions_prompts() {
    return show_extensions_prompts_;
  }

  ExtensionPrefs* extension_prefs() { return extension_prefs_.get(); }

  // Whether the extension service is ready.
  bool is_ready() { return ready_; }

 private:
  // For OnExtensionLoaded, OnExtensionInstalled, and
  // OnExtensionVersionReinstalled.
  friend class ExtensionsServiceBackend;

  // Called by the backend when the initial extension load has completed.
  void OnLoadedInstalledExtensions();

  // Called by the backend when extensions have been loaded.
  void OnExtensionsLoaded(ExtensionList* extensions);

  // Called by the backend when an extension has been installed.
  void OnExtensionInstalled(const FilePath& path, Extension* extension,
      Extension::InstallType install_type);

  // Calls and then removes any registered install callback for |path|.
  void FireInstallCallback(const FilePath& path, Extension* extension);

  // Called by the backend when there was an error installing an extension.
  void OnExtenionInstallError(const FilePath& path);

  // Called by the backend when an attempt was made to reinstall the same
  // version of an existing extension.
  void OnExtensionOverinstallAttempted(const std::string& id,
                                       const FilePath& path);

  // Preferences for the owning profile.
  scoped_ptr<ExtensionPrefs> extension_prefs_;

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

  // Stores data we'll need to do callbacks as installs complete.
  typedef std::map<FilePath, linked_ptr<Callback> > CallbackMap;
  CallbackMap install_callbacks_;

  // Is the service ready to go?
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsService);
};

// Implements IO for the ExtensionsService.
// TODO(aa): This can probably move into the .cc file.
class ExtensionsServiceBackend
    : public base::RefCountedThreadSafe<ExtensionsServiceBackend>,
      public ExternalExtensionProvider::Visitor {
 public:
  // |rdh| can be NULL in the case of test environment.
  // |extension_prefs| contains a dictionary value that points to the extension
  // preferences.
  ExtensionsServiceBackend(const FilePath& install_directory,
                           ResourceDispatcherHost* rdh,
                           MessageLoop* frontend_loop,
                           bool extensions_enabled);

  virtual ~ExtensionsServiceBackend();

  void set_extensions_enabled(bool enabled) { extensions_enabled_ = enabled; }

  // Loads the installed extensions.
  // Errors are reported through ExtensionErrorReporter. On completion,
  // OnExtensionsLoaded() is called with any successfully loaded extensions.
  void LoadInstalledExtensions(scoped_refptr<ExtensionsService> frontend,
                               InstalledExtensions* installed);

  // Scans the extension installation directory to look for partially installed
  // or extensions to uninstall.
  void GarbageCollectExtensions(scoped_refptr<ExtensionsService> frontend);

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

  // Similar to InstallExtension, but |extension_path| must be an updated
  // version of an installed extension with id of |id|.
  void UpdateExtension(const std::string& id,
                       const FilePath& extension_path,
                       bool alert_on_error,
                       scoped_refptr<ExtensionsService> frontend);

  // Check externally updated extensions for updates and install if necessary.
  // Errors are reported through ExtensionErrorReporter. Succcess is not
  // reported.
  void CheckForExternalUpdates(std::set<std::string> ids_to_ignore,
                               scoped_refptr<ExtensionsService> frontend);

  // Deletes all versions of the extension from the filesystem. Note that only
  // extensions whose location() == INTERNAL can be uninstalled. Attempting to
  // uninstall other extensions will silently fail.
  void UninstallExtension(const std::string& extension_id);

  // Clear all ExternalExtensionProviders.
  void ClearProvidersForTesting();

  // Sets an ExternalExtensionProvider for the service to use during testing.
  // |location| specifies what type of provider should be added.
  void SetProviderForTesting(Extension::Location location,
                             ExternalExtensionProvider* test_provider);

  // ExternalExtensionProvider::Visitor implementation.
  virtual void OnExternalExtensionFound(const std::string& id,
                                        const Version* version,
                                        const FilePath& path);
 private:
  class UnpackerClient;
  friend class UnpackerClient;

  // Loads a single installed extension.
  void LoadInstalledExtension(const std::string& id, const FilePath& path,
                              Extension::Location location);

  // Utility function to read an extension manifest and return it as a
  // DictionaryValue. If it fails, NULL is returned and |error| contains an
  // appropriate message.
  DictionaryValue* ReadManifest(FilePath manifest_path, std::string* error);

  // Load a single extension from |extension_path|, the top directory of
  // a specific extension where its manifest file lives.
  Extension* LoadExtension(const FilePath& extension_path,
                           Extension::Location location,
                           bool require_id);

  // Load a single extension from |extension_path|, the top directory of
  // a versioned extension where its Current Version file lives.
  Extension* LoadExtensionCurrentVersion(const FilePath& extension_path);

  // Install a crx file at |extension_path|. If |expected_id| is not empty, it's
  // verified against the extension's manifest before installation. If the
  // extension is already installed, install the new version only if its version
  // number is greater than the current installed version.
  void InstallOrUpdateExtension(const FilePath& extension_path,
                                const std::string& expected_id);

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
  void ReportExtensionOverinstallAttempted(const std::string& id,
                                           const FilePath& path);

  // Checks a set of strings (containing id's to ignore) in order to determine
  // if the extension should be installed.
  bool ShouldSkipInstallingExtension(const std::set<std::string>& ids_to_ignore,
                                     const std::string& id);

  // Installs the extension if the extension is a newer version or if the
  // extension hasn't been installed before.
  void CheckVersionAndInstallExtension(const std::string& id,
                                       const Version* extension_version,
                                       const FilePath& extension_path);

  // Lookup an external extension by |id| by going through all registered
  // external extension providers until we find a provider that contains an
  // extension that matches. If |version| is not NULL, the extension version
  // will be returned (caller is responsible for deleting that pointer).
  // |location| can also be null, if not needed. Returns true if extension is
  // found, false otherwise.
  bool LookupExternalExtension(const std::string& id,
                               Version** version,
                               Extension::Location* location);

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
  bool CheckExternalUninstall(const std::string& id,
                              Extension::Location location);

  // Should an extension of |id| and |version| be installed?
  // Returns true if no extension of type |id| is installed or if |version|
  // is greater than the current installed version.
  bool ShouldInstall(const std::string& id, const Version* version);

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

  // Whether non-theme extensions are enabled (themes and externally registered
  // extensions are always enabled).
  bool extensions_enabled_;

  // A map of all external extension providers.
  typedef std::map<Extension::Location,
                   linked_ptr<ExternalExtensionProvider> > ProviderMap;
  ProviderMap external_extension_providers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsServiceBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_H_
