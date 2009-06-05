// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/scoped_handle.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "base/thread.h"
#include "base/values.h"
#include "net/base/file_stream.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/zip.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "base/registry.h"
#include "base/win_util.h"
#endif

// ExtensionsService.

const char* ExtensionsService::kInstallDirectoryName = "Extensions";
const char* ExtensionsService::kCurrentVersionFileName = "Current Version";
const char* ExtensionsServiceBackend::kTempExtensionName = "TEMP_INSTALL";

namespace {
// Chromium Extension magic number
const char kExtensionFileMagic[] = "Cr24";

struct ExtensionHeader {
  char magic[sizeof(kExtensionFileMagic) - 1];
  uint32 version;
  size_t header_size;
  size_t manifest_size;
};

const size_t kZipHashBytes = 32;  // SHA-256
const size_t kZipHashHexBytes = kZipHashBytes * 2;  // Hex string is 2x size.

// A preference that keeps track of extension settings. This is a dictionary
// object read from the Preferences file, keyed off of extension id's.
const wchar_t kExternalExtensionsPref[] = L"extensions.settings";

// A preference keeping track of how the extension was installed.
const wchar_t kLocation[] = L"location";
const wchar_t kState[] = L"state";

// Registry key where registry defined extension installers live.
// TODO(port): Assuming this becomes a similar key into the appropriate
// platform system.
const char kRegistryExtensions[] = "Software\\Google\\Chrome\\Extensions";

#if defined(OS_WIN)

// Registry value of of that key that defines the path to the .crx file.
const wchar_t kRegistryExtensionPath[] = L"path";

// Registry value of that key that defines the current version of the .crx file.
const wchar_t kRegistryExtensionVersion[] = L"version";

#endif

// A marker file to indicate that an extension was installed from an external
// source.
const char kExternalInstallFile[] = "EXTERNAL_INSTALL";

// A temporary subdirectory where we unpack extensions.
const char* kUnpackExtensionDir = "TEMP_UNPACK";

// The version of the extension package that this code understands.
const uint32 kExpectedVersion = 1;
}

// This class coordinates an extension unpack task which is run in a separate
// process.  Results are sent back to this class, which we route to the
// ExtensionServiceBackend.
class ExtensionsServiceBackend::UnpackerClient
    : public UtilityProcessHost::Client {
 public:
  UnpackerClient(ExtensionsServiceBackend* backend,
                 const FilePath& extension_path,
                 const std::string& expected_id,
                 bool from_external)
    : backend_(backend), extension_path_(extension_path),
      expected_id_(expected_id), from_external_(from_external),
      got_response_(false) {
  }

  // Starts the unpack task.  We call back to the backend when the task is done,
  // or a problem occurs.
  void Start() {
    AddRef();  // balanced in OnUnpackExtensionReply()

    // TODO(mpcomplete): handle multiple installs
    FilePath temp_dir = backend_->install_directory_.AppendASCII(
        kUnpackExtensionDir);
    if (!file_util::CreateDirectory(temp_dir)) {
      backend_->ReportExtensionInstallError(extension_path_,
          "Failed to create temporary directory.");
      return;
    }

    temp_extension_path_ = temp_dir.Append(extension_path_.BaseName());
    if (!file_util::CopyFile(extension_path_, temp_extension_path_)) {
      backend_->ReportExtensionInstallError(extension_path_,
          "Failed to copy extension file to temporary directory.");
      return;
    }

    if (backend_->resource_dispatcher_host_) {
      ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(FROM_HERE,
          NewRunnableMethod(this, &UnpackerClient::StartProcessOnIOThread,
                            backend_->resource_dispatcher_host_,
                            MessageLoop::current()));
    } else {
      // Cheesy... but if we don't have a ResourceDispatcherHost, assume we're
      // in a unit test and run the unpacker directly in-process.
      ExtensionUnpacker unpacker(temp_extension_path_);
      if (unpacker.Run()) {
        OnUnpackExtensionSucceeded(*unpacker.parsed_manifest(),
                                   unpacker.decoded_images());
      } else {
        OnUnpackExtensionFailed(unpacker.error_message());
      }
    }
  }

 private:
  // UtilityProcessHost::Client
  virtual void OnProcessCrashed() {
    // Don't report crashes if they happen after we got a response.
    if (got_response_)
      return;

    OnUnpackExtensionFailed("Chrome crashed while trying to install.");
  }

  virtual void OnUnpackExtensionSucceeded(
      const DictionaryValue& manifest,
      const std::vector< Tuple2<SkBitmap, FilePath> >& images) {
    // The extension was unpacked to the temp dir inside our unpacking dir.
    FilePath extension_dir = temp_extension_path_.DirName().AppendASCII(
        ExtensionsServiceBackend::kTempExtensionName);
    backend_->OnExtensionUnpacked(extension_path_, extension_dir,
                                  expected_id_, from_external_,
                                  manifest, images);
    Cleanup();
  }

  virtual void OnUnpackExtensionFailed(const std::string& error_message) {
    backend_->ReportExtensionInstallError(extension_path_, error_message);
    Cleanup();
  }

  // Cleans up our temp directory.
  void Cleanup() {
    if (got_response_)
      return;

    got_response_ = true;
    file_util::Delete(temp_extension_path_.DirName(), true);
    Release();  // balanced in Run()
  }

  // Starts the utility process that unpacks our extension.
  void StartProcessOnIOThread(ResourceDispatcherHost* rdh,
                              MessageLoop* file_loop) {
    UtilityProcessHost* host = new UtilityProcessHost(rdh, this, file_loop);
    host->StartExtensionUnpacker(temp_extension_path_);
  }

  scoped_refptr<ExtensionsServiceBackend> backend_;

  // The path to the crx file that we're installing.
  FilePath extension_path_;

  // The path to the copy of the crx file in the temporary directory where we're
  // unpacking it.
  FilePath temp_extension_path_;

  // The ID we expect this extension to have, if any.
  std::string expected_id_;

  // True if this is being installed from an external source.
  bool from_external_;

  // True if we got a response from the utility process and have cleaned up
  // already.
  bool got_response_;
};

ExtensionsService::ExtensionsService(Profile* profile,
                                     MessageLoop* frontend_loop,
                                     MessageLoop* backend_loop,
                                     const std::string& registry_path)
    : prefs_(profile->GetPrefs()),
      backend_loop_(backend_loop),
      install_directory_(profile->GetPath().AppendASCII(kInstallDirectoryName)),
      extensions_enabled_(
          CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kEnableExtensions)),
      show_extensions_prompts_(true),
      backend_(new ExtensionsServiceBackend(
          install_directory_, g_browser_process->resource_dispatcher_host(),
          frontend_loop, registry_path)) {
  prefs_->RegisterDictionaryPref(kExternalExtensionsPref);
}

ExtensionsService::~ExtensionsService() {
  for (ExtensionList::iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    delete *iter;
  }
}

bool ExtensionsService::Init() {
  // Start up the extension event routers.
  ExtensionBrowserEventRouter::GetInstance()->Init();

  scoped_ptr<DictionaryValue> external_extensions(new DictionaryValue);
  GetExternalExtensions(external_extensions.get(), NULL);

  scoped_ptr< std::set<std::string> >
      killed_extensions(new std::set<std::string>);
  GetExternalExtensions(NULL, killed_extensions.get());

  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::CheckForExternalUpdates,
      *killed_extensions.get(),
      external_extensions.get(),
      scoped_refptr<ExtensionsService>(this)));

  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::LoadExtensionsFromInstallDirectory,
      scoped_refptr<ExtensionsService>(this),
      external_extensions.release()));

  return true;
}

void ExtensionsService::InstallExtension(const FilePath& extension_path) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::InstallExtension,
      extension_path,
      scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::UninstallExtension(const std::string& extension_id) {
  Extension* extension = NULL;

  ExtensionList::iterator iter;
  for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
    if ((*iter)->id() == extension_id) {
      extension = *iter;
      break;
    }
  }

  // Callers should not send us nonexistant extensions.
  CHECK(extension);

  // Remove the extension from our list.
  extensions_.erase(iter);

  // Tell other services the extension is gone.
  NotificationService::current()->Notify(NotificationType::EXTENSION_UNLOADED,
                                         NotificationService::AllSources(),
                                         Details<Extension>(extension));

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore.
  if (Extension::IsExternalLocation(extension->location())) {
    UpdateExtensionPref(ASCIIToWide(extension->id()), kState,
                        Value::CreateIntegerValue(Extension::KILLBIT), true);
  } else {
    UpdateExtensionPref(ASCIIToWide(extension->id()), kState,
                        Value::CreateIntegerValue(Extension::DISABLED), true);
  }

  // Tell the backend to start deleting installed extensions on the file thread.
  if (extension->location() == Extension::INTERNAL ||
      Extension::IsExternalLocation(extension->location())) {
    backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
        &ExtensionsServiceBackend::UninstallExtension, extension_id));
  }

  delete extension;
}

void ExtensionsService::LoadExtension(const FilePath& extension_path) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::LoadSingleExtension,
      extension_path, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::OnExtensionsLoaded(ExtensionList* new_extensions) {
  // Sync with manually loaded extensions. Otherwise we won't know about them
  // since they aren't installed in the normal way. Eventually, we want to not
  // load extensions at all from directory, but use the Extension preferences
  // as the truth for what is installed.
  DictionaryValue* pref = NULL;
  for (ExtensionList::const_iterator iter = new_extensions->begin();
       iter != new_extensions->end(); ++iter) {
    std::wstring extension_id = ASCIIToWide((*iter)->id());
    pref = GetOrCreateExtensionPref(extension_id);
    Extension::Location location;
    Extension::State state;
    if (!pref->GetInteger(kLocation, reinterpret_cast<int*>(&location)) ||
        !pref->GetInteger(kState, reinterpret_cast<int*>(&state))) {
      UpdateExtensionPref(extension_id,
          kLocation, Value::CreateIntegerValue(Extension::INTERNAL), false);
      UpdateExtensionPref(extension_id,
          kState, Value::CreateIntegerValue(Extension::ENABLED), false);
    } else {
      // The kill-bit only applies to External extensions so this check fails
      // for internal locations that have the kill-bit set. In other words,
      // the kill-bit cannot be set unless the extension is external.
      DCHECK(state != Extension::KILLBIT ||
             Extension::IsExternalLocation(location));
    }
  }

  // If extensions aren't enabled, we still want to add themes. However, themes
  // should not trigger EXTENSIONS_LOADED.
  // TODO(aa): This can be re-enabled when BUG 13128 is fixed.
  bool has_extension = false;
  for (ExtensionList::iterator iter = new_extensions->begin();
       iter != new_extensions->end(); ++iter) {
    if (extensions_enabled() || (*iter)->IsTheme()) {
      extensions_.push_back(*iter);
      if (!(*iter)->IsTheme())
        has_extension = true;
    }
  }

  if (has_extension) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSIONS_LOADED,
        NotificationService::AllSources(),
        Details<ExtensionList>(new_extensions));
  }
  delete new_extensions;
}

void ExtensionsService::OnExtensionInstalled(Extension* extension,
                                             bool update) {
  UpdateExtensionPref(ASCIIToWide(extension->id()), kState,
                      Value::CreateIntegerValue(Extension::ENABLED), false);
  UpdateExtensionPref(ASCIIToWide(extension->id()), kLocation,
                      Value::CreateIntegerValue(Extension::INTERNAL), true);

  // If the extension is a theme, tell the profile (and therefore ThemeProvider)
  // to apply it.
  if (extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        NotificationService::AllSources(),
        Details<Extension>(extension));
  } else {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_INSTALLED,
        NotificationService::AllSources(),
        Details<Extension>(extension));
  }
}

void ExtensionsService::OnExternalExtensionInstalled(
    const std::string& id, Extension::Location location) {
  DCHECK(Extension::IsExternalLocation(location));
  UpdateExtensionPref(ASCIIToWide(id), kState,
                      Value::CreateIntegerValue(Extension::ENABLED), false);
  UpdateExtensionPref(ASCIIToWide(id), kLocation,
                      Value::CreateIntegerValue(location), true);
}

void ExtensionsService::OnExtensionVersionReinstalled(const std::string& id) {
  Extension* extension = GetExtensionByID(id);
  if (extension && extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        NotificationService::AllSources(),
        Details<Extension>(extension));
  }
}

Extension* ExtensionsService::GetExtensionByID(std::string id) {
  for (ExtensionList::const_iterator iter = extensions_.begin();
      iter != extensions_.end(); ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }
  return NULL;
}

void ExtensionsService::GetExternalExtensions(
    DictionaryValue* external_extensions,
    std::set<std::string>* killed_extensions) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExternalExtensionsPref);
  if (!dict || dict->GetSize() == 0)
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    std::wstring key_name = *i;
    DCHECK(Extension::IdIsValid(WideToASCII(key_name)));
    DictionaryValue* extension = NULL;
    if (!dict->GetDictionary(key_name, &extension)) {
      NOTREACHED();
      continue;
    }

    // Check to see if the extension has been killed.
    Extension::State state;
    if (extension->GetInteger(kState, reinterpret_cast<int*>(&state)) &&
        state == static_cast<int>(Extension::KILLBIT)) {
      if (killed_extensions) {
        StringToLowerASCII(&key_name);
        killed_extensions->insert(WideToASCII(key_name));
      }
    }
    // Return all extensions found.
    if (external_extensions) {
      DictionaryValue* result =
          static_cast<DictionaryValue*>(extension->DeepCopy());
      StringToLowerASCII(&key_name);
      external_extensions->Set(key_name, result);
    }
  }
}

DictionaryValue* ExtensionsService::GetOrCreateExtensionPref(
    const std::wstring& extension_id) {
  DictionaryValue* dict = prefs_->GetMutableDictionary(kExternalExtensionsPref);
  DictionaryValue* extension = NULL;
  if (!dict->GetDictionary(extension_id, &extension)) {
    // Extension pref does not exist, create it.
    extension = new DictionaryValue();
    dict->Set(extension_id, extension);
  }

  return extension;
}

bool ExtensionsService::UpdateExtensionPref(const std::wstring& extension_id,
                                            const std::wstring& key,
                                            Value* data_value,
                                            bool schedule_save) {
  DictionaryValue* extension = GetOrCreateExtensionPref(extension_id);
  if (!extension->Set(key, data_value)) {
    NOTREACHED() << L"Cannot modify key: '" << key.c_str()
                 << "' for extension: '" << extension_id.c_str() << "'";
    return false;
  }

  if (schedule_save)
    prefs_->ScheduleSavePersistentPrefs();
  return true;
}

// ExtensionsServicesBackend

ExtensionsServiceBackend::ExtensionsServiceBackend(
    const FilePath& install_directory, ResourceDispatcherHost* rdh,
    MessageLoop* frontend_loop, const std::string& registry_path)
        : frontend_(NULL),
          install_directory_(install_directory),
          resource_dispatcher_host_(rdh),
          alert_on_error_(false),
          frontend_loop_(frontend_loop),
          registry_path_(registry_path) {
  // Default the registry path if unspecified.
  if (registry_path_.empty()) {
    registry_path_ = kRegistryExtensions;
  }
}

void ExtensionsServiceBackend::LoadExtensionsFromInstallDirectory(
    scoped_refptr<ExtensionsService> frontend,
    DictionaryValue* extension_prefs) {
  frontend_ = frontend;
  alert_on_error_ = false;
  scoped_ptr<DictionaryValue> external_extensions(extension_prefs);

#if defined(OS_WIN)
  // On POSIX, AbsolutePath() calls realpath() which returns NULL if
  // it does not exist.  Instead we absolute-ify after creation in
  // case that is needed.
  // TODO(port): does this really need to happen before
  // CreateDirectory() on Windows?
  if (!file_util::AbsolutePath(&install_directory_))
    NOTREACHED();
#endif

  scoped_ptr<ExtensionList> extensions(new ExtensionList);

  // Create the <Profile>/Extensions directory if it doesn't exist.
  if (!file_util::DirectoryExists(install_directory_)) {
    file_util::CreateDirectory(install_directory_);
    LOG(INFO) << "Created Extensions directory.  No extensions to install.";
    ReportExtensionsLoaded(extensions.release());
    return;
  }

#if !defined(OS_WIN)
  if (!file_util::AbsolutePath(&install_directory_))
    NOTREACHED();
#endif

  LOG(INFO) << "Loading installed extensions...";

  // Find all child directories in the install directory and load their
  // manifests. Post errors and results to the frontend.
  file_util::FileEnumerator enumerator(install_directory_,
                                       false,  // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath extension_path;
  for (extension_path = enumerator.Next(); !extension_path.value().empty();
       extension_path = enumerator.Next()) {
    std::string extension_id = WideToASCII(
        extension_path.BaseName().ToWStringHack());
    // The utility process might be in the middle of unpacking an extension, so
    // ignore the temp unpacking directory.
    if (extension_id == kUnpackExtensionDir)
      continue;

    // If there is no Current Version file, just delete the directory and move
    // on. This can legitimately happen when an uninstall does not complete, for
    // example, when a plugin is in use at uninstall time.
    FilePath current_version_path = extension_path.AppendASCII(
        ExtensionsService::kCurrentVersionFileName);
    if (!file_util::PathExists(current_version_path)) {
      LOG(INFO) << "Deleting incomplete install for directory "
                << WideToASCII(extension_path.ToWStringHack()) << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }

    std::string current_version;
    if (!ReadCurrentVersion(extension_path, &current_version))
      continue;

    Extension::Location location;
    DictionaryValue* pref = NULL;
    external_extensions->GetDictionary(ASCIIToWide(extension_id), &pref);
    if (!pref ||
        !pref->GetInteger(kLocation, reinterpret_cast<int*>(&location))) {
      location = Extension::INTERNAL;
    }
    FilePath version_path = extension_path.AppendASCII(current_version);
    if (Extension::IsExternalLocation(location) &&
        CheckExternalUninstall(external_extensions.get(),
                               version_path, extension_id)) {
      // TODO(erikkay): Possibly defer this operation to avoid slowing initial
      // load of extensions.
      UninstallExtension(extension_id);

      // No error needs to be reported.  The extension effectively doesn't
      // exist.
      continue;
    }

    Extension* extension = LoadExtension(version_path, true);  // require id
    if (extension)
      extensions->push_back(extension);
  }

  LOG(INFO) << "Done.";
  ReportExtensionsLoaded(extensions.release());
}

void ExtensionsServiceBackend::LoadSingleExtension(
    const FilePath& path_in, scoped_refptr<ExtensionsService> frontend) {
  frontend_ = frontend;

  // Explicit UI loads are always noisy.
  alert_on_error_ = true;

  FilePath extension_path = path_in;
  if (!file_util::AbsolutePath(&extension_path))
    NOTREACHED();

  LOG(INFO) << "Loading single extension from " <<
      WideToASCII(extension_path.BaseName().ToWStringHack());

  Extension* extension = LoadExtension(extension_path,
                                       false);  // don't require ID
  if (extension) {
    extension->set_location(Extension::LOAD);
    ExtensionList* extensions = new ExtensionList;
    extensions->push_back(extension);
    ReportExtensionsLoaded(extensions);
  }
}

Extension* ExtensionsServiceBackend::LoadExtension(
    const FilePath& extension_path, bool require_id) {
  FilePath manifest_path =
      extension_path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    ReportExtensionLoadError(extension_path, Extension::kInvalidManifestError);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(&error));
  if (!root.get()) {
    ReportExtensionLoadError(extension_path, error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionLoadError(extension_path, Extension::kInvalidManifestError);
    return NULL;
  }

  scoped_ptr<Extension> extension(new Extension(extension_path));
  if (!extension->InitFromValue(*static_cast<DictionaryValue*>(root.get()),
                                require_id, &error)) {
    ReportExtensionLoadError(extension_path, error);
    return NULL;
  }

  FilePath external_marker = extension_path.AppendASCII(kExternalInstallFile);
  if (file_util::PathExists(external_marker)) {
    extension->set_location(
      extension->ExternalExtensionInstallType(registry_path_));
  } else {
    extension->set_location(Extension::INTERNAL);
  }

  // Theme resource validation.
  if (extension->IsTheme()) {
    DictionaryValue* images_value = extension->GetThemeImages();
    DictionaryValue::key_iterator iter = images_value->begin_keys();
    while (iter != images_value->end_keys()) {
      std::string val;
      if (images_value->GetString(*iter , &val)) {
        FilePath image_path = extension->path().AppendASCII(val);
        if (!file_util::PathExists(image_path)) {
          ReportExtensionLoadError(extension_path,
              StringPrintf("Could not load '%s' for theme.",
              WideToUTF8(image_path.ToWStringHack()).c_str()));
          return NULL;
        }
      }
      ++iter;
    }

    // Themes cannot contain other extension types.
    return extension.release();
  }

  // Validate that claimed script resources actually exist.
  for (size_t i = 0; i < extension->content_scripts().size(); ++i) {
    const UserScript& script = extension->content_scripts()[i];

    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      const FilePath& path = script.js_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        ReportExtensionLoadError(extension_path,
            StringPrintf("Could not load '%s' for content script.",
            WideToUTF8(path.ToWStringHack()).c_str()));
        return NULL;
      }
    }

    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      const FilePath& path = script.css_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        ReportExtensionLoadError(extension_path,
            StringPrintf("Could not load '%s' for content script.",
            WideToUTF8(path.ToWStringHack()).c_str()));
        return NULL;
      }
    }
  }

  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    if (!file_util::PathExists(plugin.path)) {
      ReportExtensionLoadError(extension_path,
          StringPrintf("Could not load '%s' for plugin.",
          WideToUTF8(plugin.path.ToWStringHack()).c_str()));
      return NULL;
    }
  }

  // Validate icon location for page actions.
  const PageActionMap& page_actions = extension->page_actions();
  for (PageActionMap::const_iterator i(page_actions.begin());
       i != page_actions.end(); ++i) {
    PageAction* page_action = i->second;
    FilePath path = page_action->icon_path();
    if (!file_util::PathExists(path)) {
      ReportExtensionLoadError(extension_path,
          StringPrintf("Could not load icon '%s' for page action.",
          WideToUTF8(path.ToWStringHack()).c_str()));
      return NULL;
    }
  }

  return extension.release();
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  // TODO(port): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionList* extensions) {
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionsLoaded, extensions));
}

bool ExtensionsServiceBackend::ReadCurrentVersion(const FilePath& dir,
                                                  std::string* version_string) {
  FilePath current_version =
      dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  if (file_util::PathExists(current_version)) {
    if (file_util::ReadFileToString(current_version, version_string)) {
      TrimWhitespace(*version_string, TRIM_ALL, version_string);
      return true;
    }
  }
  return false;
}

bool ExtensionsServiceBackend::CheckCurrentVersion(
    const std::string& new_version_str,
    const std::string& current_version_str,
    const FilePath& dest_dir) {
  scoped_ptr<Version> current_version(
      Version::GetVersionFromString(current_version_str));
  scoped_ptr<Version> new_version(
      Version::GetVersionFromString(new_version_str));
  if (current_version->CompareTo(*new_version) >= 0) {
    // Verify that the directory actually exists.  If it doesn't we'll return
    // true so that the install code will repair the broken installation.
    // TODO(erikkay): A further step would be to verify that the extension
    // has actually loaded successfully.
    FilePath version_dir = dest_dir.AppendASCII(current_version_str);
    if (file_util::PathExists(version_dir)) {
      std::string id = WideToASCII(dest_dir.BaseName().ToWStringHack());
      StringToLowerASCII(&id);
      ReportExtensionVersionReinstalled(id);
      return false;
    }
  }
  return true;
}

bool ExtensionsServiceBackend::InstallDirSafely(const FilePath& source_dir,
                                                const FilePath& dest_dir) {
  if (file_util::PathExists(dest_dir)) {
    // By the time we get here, it should be safe to assume that this directory
    // is not currently in use (it's not the current active version).
    if (!file_util::Delete(dest_dir, true)) {
      ReportExtensionInstallError(source_dir,
          "Can't delete existing version directory.");
      return false;
    }
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent)) {
        ReportExtensionInstallError(source_dir,
                                    "Couldn't create extension directory.");
        return false;
      }
    }
  }
  if (!file_util::Move(source_dir, dest_dir)) {
    ReportExtensionInstallError(source_dir,
                                "Couldn't move temporary directory.");
    return false;
  }

  return true;
}

bool ExtensionsServiceBackend::SetCurrentVersion(const FilePath& dest_dir,
                                                 std::string version) {
  // Write out the new CurrentVersion file.
  // <profile>/Extension/<name>/CurrentVersion
  FilePath current_version =
      dest_dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  FilePath current_version_old =
    current_version.InsertBeforeExtension(FILE_PATH_LITERAL("_old"));
  if (file_util::PathExists(current_version_old)) {
    if (!file_util::Delete(current_version_old, false)) {
      ReportExtensionInstallError(dest_dir,
                                  "Couldn't remove CurrentVersion_old file.");
      return false;
    }
  }
  if (file_util::PathExists(current_version)) {
    if (!file_util::Move(current_version, current_version_old)) {
      ReportExtensionInstallError(dest_dir,
                                  "Couldn't move CurrentVersion file.");
      return false;
    }
  }
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  if (stream.Open(current_version, flags) != 0)
    return false;
  if (stream.Write(version.c_str(), version.size(), NULL) < 0) {
    // Restore the old CurrentVersion.
    if (file_util::PathExists(current_version_old)) {
      if (!file_util::Move(current_version_old, current_version)) {
        LOG(WARNING) << "couldn't restore " << current_version_old.value() <<
            " to " << current_version.value();

        // TODO(erikkay): This is an ugly state to be in.  Try harder?
      }
    }
    ReportExtensionInstallError(dest_dir,
                                "Couldn't create CurrentVersion file.");
    return false;
  }
  return true;
}

void ExtensionsServiceBackend::InstallExtension(
    const FilePath& extension_path, scoped_refptr<ExtensionsService> frontend) {
  LOG(INFO) << "Installing extension " << extension_path.value();

  frontend_ = frontend;
  alert_on_error_ = true;

  bool from_external = false;
  InstallOrUpdateExtension(extension_path, std::string(), from_external);
}

void ExtensionsServiceBackend::InstallOrUpdateExtension(
    const FilePath& extension_path, const std::string& expected_id,
    bool from_external) {
  UnpackerClient* client =
      new UnpackerClient(this, extension_path, expected_id, from_external);
  client->Start();
}

void ExtensionsServiceBackend::OnExtensionUnpacked(
    const FilePath& extension_path,
    const FilePath& temp_extension_dir,
    const std::string expected_id,
    bool from_external,
    const DictionaryValue& manifest,
    const std::vector< Tuple2<SkBitmap, FilePath> >& images) {
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(manifest,
                               true,  // require ID
                               &error)) {
    ReportExtensionInstallError(extension_path, "Invalid extension manifest.");
    return;
  }

  if (!frontend_->extensions_enabled() && !extension.IsTheme()) {
#if defined(OS_WIN)
    if (frontend_->show_extensions_prompts()) {
      win_util::MessageBox(GetActiveWindow(),
          L"Extensions are not enabled. Add --enable-extensions to the "
          L"command-line to enable extensions.\n\n"
          L"This is a temporary message and it will be removed when extensions "
          L"UI is finalized.",
          l10n_util::GetString(IDS_PRODUCT_NAME).c_str(), MB_OK);
    }
#endif
    ReportExtensionInstallError(extension_path,
        "Extensions are not enabled.");
    return;
  }

#if defined(OS_WIN)
  if (!extension.IsTheme() &&
      frontend_->show_extensions_prompts() &&
      win_util::MessageBox(GetActiveWindow(),
          L"Are you sure you want to install this extension?\n\n"
          L"This is a temporary message and it will be removed when extensions "
          L"UI is finalized.",
          l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
          MB_OKCANCEL) == IDOK) {
    ReportExtensionInstallError(extension_path,
        "User did not allow extension to be installed.");
    return;
  }
#endif

  // If an expected id was provided, make sure it matches.
  if (!expected_id.empty() && expected_id != extension.id()) {
    std::string error_msg = "ID in new extension manifest (";
    error_msg += extension.id();
    error_msg += ") does not match expected ID (";
    error_msg += expected_id;
    error_msg += ")";
    ReportExtensionInstallError(extension_path, error_msg);
    return;
  }

  // <profile>/Extensions/<id>
  FilePath dest_dir = install_directory_.AppendASCII(extension.id());
  std::string version = extension.VersionString();
  std::string current_version;
  bool was_update = false;
  if (ReadCurrentVersion(dest_dir, &current_version)) {
    if (!CheckCurrentVersion(version, current_version, dest_dir))
      return;
    was_update = true;
  }

  // Write our parsed manifest back to disk, to ensure it doesn't contain an
  // exploitable bug that can be used to compromise the browser.
  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(manifest)) {
    ReportExtensionInstallError(extension_path,
                                "Error serializing manifest.json.");
    return;
  }

  FilePath manifest_path =
      temp_extension_dir.AppendASCII(Extension::kManifestFilename);
  if (!file_util::WriteFile(manifest_path,
                            manifest_json.data(), manifest_json.size())) {
    ReportExtensionInstallError(extension_path, "Error saving manifest.json.");
    return;
  }

  // Write our parsed images back to disk as well.
  for (size_t i = 0; i < images.size(); ++i) {
    const SkBitmap& image = images[i].a;
    FilePath path = temp_extension_dir.Append(images[i].b);

    std::vector<unsigned char> image_data;
    // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
    // though they may originally be .jpg, etc.  Figure something out.
    // http://code.google.com/p/chromium/issues/detail?id=12459
    if (!PNGEncoder::EncodeBGRASkBitmap(image, false, &image_data)) {
      ReportExtensionInstallError(extension_path,
                                  "Error re-encoding theme image.");
      return;
    }

    // Note: we're overwriting existing files that the utility process wrote,
    // so we can be sure the directory exists.
    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    if (!file_util::WriteFile(path, image_data_ptr, image_data.size())) {
      ReportExtensionInstallError(extension_path, "Error saving theme image.");
      return;
    }
  }

  // <profile>/Extensions/<dir_name>/<version>
  FilePath version_dir = dest_dir.AppendASCII(version);

  // If anything fails after this, we want to delete the extension dir.
  ScopedTempDir scoped_version_dir;
  scoped_version_dir.Set(version_dir);

  if (!InstallDirSafely(temp_extension_dir, version_dir))
    return;

  if (!SetCurrentVersion(dest_dir, version))
    return;

  // To mark that this extension was installed from an external source, create a
  // zero-length file.  At load time, this is used to indicate that the
  // extension should be uninstalled.
  // TODO(erikkay): move this into per-extension config storage when it appears.
  if (from_external) {
    FilePath marker = version_dir.AppendASCII(kExternalInstallFile);
    file_util::WriteFile(marker, NULL, 0);
  }

  // Load the extension immediately and then report installation success. We
  // don't load extensions for external installs because external installation
  // occurs before the normal startup so we just let startup pick them up. We
  // notify on installation of external extensions because we need to update
  // the preferences for these extensions to reflect that they've just been
  // installed.
  if (!from_external) {
    Extension* extension = LoadExtension(version_dir, true);  // require id
    CHECK(extension);

    frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        frontend_, &ExtensionsService::OnExtensionInstalled, extension,
        was_update));

    // Only one extension, but ReportExtensionsLoaded can handle multiple,
    // so we need to construct a list.
    scoped_ptr<ExtensionList> extensions(new ExtensionList);
    extensions->push_back(extension);
    LOG(INFO) << "Done.";
    // Hand off ownership of the loaded extensions to the frontend.
    ReportExtensionsLoaded(extensions.release());
  } else {
    frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        frontend_, &ExtensionsService::OnExternalExtensionInstalled,
        extension.id(),
        extension.ExternalExtensionInstallType(registry_path_)));
  }

  scoped_version_dir.Take();
}

void ExtensionsServiceBackend::ReportExtensionInstallError(
    const FilePath& extension_path,  const std::string &error) {

  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message =
      StringPrintf("Could not install extension from '%s'. %s",
                   path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionVersionReinstalled(
    const std::string& id) {
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionVersionReinstalled, id));
}

bool ExtensionsServiceBackend::ShouldSkipInstallingExtension(
    const std::set<std::string>& ids_to_ignore,
    const std::string& id) {
  if (ids_to_ignore.find(id) != ids_to_ignore.end()) {
    LOG(INFO) << "Skipping uninstalled external extension " << id;
    return true;
  }
  return false;
}

void ExtensionsServiceBackend::CheckVersionAndInstallExtension(
    const std::string& id, const std::string& extension_version,
    const FilePath& extension_path, bool from_external) {
  if (ShouldInstall(id, extension_version))
    InstallOrUpdateExtension(FilePath(extension_path), id, from_external);
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
void ExtensionsServiceBackend::CheckForExternalUpdates(
    std::set<std::string> ids_to_ignore,
    DictionaryValue* extension_prefs,
    scoped_refptr<ExtensionsService> frontend) {
  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.
  alert_on_error_ = false;
  frontend_ = frontend;

  for (DictionaryValue::key_iterator i = extension_prefs->begin_keys();
       i != extension_prefs->end_keys(); ++i) {
    const std::wstring& extension_id = *i;
    if (ShouldSkipInstallingExtension(ids_to_ignore, WideToASCII(extension_id)))
      continue;

    DictionaryValue* extension = NULL;
    if (!extension_prefs->GetDictionary(extension_id, &extension)) {
      NOTREACHED() << "Cannot read extension " << extension_id.c_str()
                   << " from dictionary.";
      continue;
    }

    Extension::Location location;
    if (extension->GetInteger(kLocation, reinterpret_cast<int*>(&location)) &&
        location != Extension::EXTERNAL_PREF) {
      continue;
    }
    Extension::State state;
    if (extension->GetInteger(kState, reinterpret_cast<int*>(&state)) &&
        state == Extension::KILLBIT) {
      continue;
    }

    FilePath::StringType external_crx;
    std::string external_version;
    if (!extension->GetString(L"external_crx", &external_crx) ||
        !extension->GetString(L"external_version", &external_version)) {
      LOG(WARNING) << "Malformed extension dictionary for extension: "
                   << extension_id.c_str();
      continue;
    }

    bool from_external = true;
    CheckVersionAndInstallExtension(WideToASCII(extension_id), external_version,
                                    FilePath(external_crx), from_external);
  }

#if defined(OS_WIN)
  // TODO(port): Pull this out into an interface. That will also allow us to
  // test the behavior of external extensions.
  HKEY reg_root = HKEY_LOCAL_MACHINE;
  RegistryKeyIterator iterator(reg_root, ASCIIToWide(registry_path_).c_str());
  while (iterator.Valid()) {
    // Fold
    std::string id = StringToLowerASCII(WideToASCII(iterator.Name()));
    if (ShouldSkipInstallingExtension(ids_to_ignore, id)) {
      ++iterator;
      continue;
    }

    RegKey key;
    std::wstring key_path = ASCIIToWide(registry_path_);
    key_path.append(L"\\");
    key_path.append(iterator.Name());
    if (key.Open(reg_root, key_path.c_str())) {
      std::wstring extension_path;
      if (key.ReadValue(kRegistryExtensionPath, &extension_path)) {
        std::wstring extension_version;
        if (key.ReadValue(kRegistryExtensionVersion, &extension_version)) {
          bool from_external = true;
          CheckVersionAndInstallExtension(
              id, WideToASCII(extension_version), FilePath(extension_path),
              from_external);
        } else {
          // TODO(erikkay): find a way to get this into about:extensions
          LOG(WARNING) << "Missing value " << kRegistryExtensionVersion <<
              " for key " << key_path;
        }
      } else {
        // TODO(erikkay): find a way to get this into about:extensions
        LOG(WARNING) << "Missing value " << kRegistryExtensionPath <<
            " for key " << key_path;
      }
    }
    ++iterator;
  }
#endif
}

bool ExtensionsServiceBackend::CheckExternalUninstall(
    DictionaryValue* extension_prefs, const FilePath& version_path,
    const std::string& id) {
  // First check the preferences for the kill-bit.
  Extension::Location location = Extension::INVALID;
  DictionaryValue* extension = NULL;
  if (extension_prefs->GetDictionary(ASCIIToWide(id), &extension)) {
    Extension::State state;
    if (extension->GetInteger(kLocation, reinterpret_cast<int*>(&location)) &&
        location == Extension::EXTERNAL_PREF) {
      return extension->GetInteger(kState, reinterpret_cast<int*>(&state)) &&
             state == Extension::KILLBIT;
    }
  }

#if defined(OS_WIN)
  if (location == Extension::EXTERNAL_REGISTRY) {
    HKEY reg_root = HKEY_LOCAL_MACHINE;
    RegKey key;
    std::wstring key_path = ASCIIToWide(registry_path_);
    key_path.append(L"\\");
    key_path.append(ASCIIToWide(id));

    // If the key doesn't exist, then we should uninstall.
    return !key.Open(reg_root, key_path.c_str());
  }
#endif

  return false;
}

// Assumes that the extension isn't currently loaded or in use.
void ExtensionsServiceBackend::UninstallExtension(
    const std::string& extension_id) {
  // First, delete the Current Version file. If the directory delete fails, then
  // at least the extension won't be loaded again.
  FilePath extension_directory = install_directory_.AppendASCII(extension_id);

  if (!file_util::PathExists(extension_directory)) {
    LOG(WARNING) << "Asked to remove a non-existent extension " << extension_id;
    return;
  }

  FilePath current_version_file = extension_directory.AppendASCII(
      ExtensionsService::kCurrentVersionFileName);
  if (!file_util::PathExists(current_version_file)) {
    LOG(WARNING) << "Extension " << extension_id
                 << " does not have a Current Version file.";
  } else {
    if (!file_util::Delete(current_version_file, false)) {
      LOG(WARNING) << "Could not delete Current Version file for extension "
                   << extension_id;
      return;
    }
  }

  // Ok, now try and delete the entire rest of the directory. One major place
  // this can fail is if the extension contains a plugin (stupid plugins). It's
  // not a big deal though, because we'll notice next time we startup that the
  // Current Version file is gone and finish the delete then.
  if (!file_util::Delete(extension_directory, true)) {
    LOG(WARNING) << "Could not delete directory for extension "
                 << extension_id;
  }
}

bool ExtensionsServiceBackend::ShouldInstall(const std::string& id,
                                             const std::string& version) {
  FilePath dir(install_directory_.AppendASCII(id.c_str()));
  std::string current_version;
  if (ReadCurrentVersion(dir, &current_version)) {
    return CheckCurrentVersion(version, current_version, dir);
  }
  return true;
}
