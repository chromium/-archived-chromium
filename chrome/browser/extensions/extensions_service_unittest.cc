// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif

namespace {

struct ExtensionsOrder {
  bool operator()(const Extension* a, const Extension* b) {
    return a->name() < b->name();
  }
};

static std::vector<std::string> GetErrors() {
  const std::vector<std::string>* errors =
      ExtensionErrorReporter::GetInstance()->GetErrors();
  std::vector<std::string> ret_val;

  for (std::vector<std::string>::const_iterator iter = errors->begin();
       iter != errors->end(); ++iter) {
    if (iter->find(".svn") == std::string::npos) {
      ret_val.push_back(*iter);
    }
  }

  // The tests rely on the errors being in a certain order, which can vary
  // depending on how filesystem iteration works.
  std::stable_sort(ret_val.begin(), ret_val.end());

  return ret_val;
}

}  // namespace

class ExtensionsServiceTest
  : public testing::Test, public NotificationObserver {
 public:
  ExtensionsServiceTest() : installed_(NULL) {
    registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_INSTALLED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::THEME_INSTALLED,
                   NotificationService::AllSources());

    // Create a temporary area in the registry to test external extensions.
    registry_path_ = "Software\\Google\\Chrome\\ExtensionsServiceTest_";
    registry_path_ += IntToString(
        static_cast<int>(base::Time::Now().ToDoubleT()));

    profile_.reset(new TestingProfile());
    service_ = new ExtensionsService(profile_.get(), &loop_, &loop_,
                                     registry_path_);
    service_->set_extensions_enabled(true);
    service_->set_show_extensions_disabled_notification(false);
    total_successes_ = 0;
  }

  static void SetUpTestCase() {
    ExtensionErrorReporter::Init(false);  // no noisy errors
  }

  virtual void SetUp() {
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSIONS_LOADED: {
        ExtensionList* list = Details<ExtensionList>(details).ptr();
        for (ExtensionList::iterator iter = list->begin(); iter != list->end();
             ++iter) {
          loaded_.push_back(*iter);
        }
        // The tests rely on the errors being in a certain order, which can vary
        // depending on how filesystem iteration works.
        std::stable_sort(loaded_.begin(), loaded_.end(), ExtensionsOrder());
        break;
      }

      case NotificationType::EXTENSION_UNLOADED:
        unloaded_id_ = Details<Extension>(details).ptr()->id();
        break;

      case NotificationType::EXTENSION_INSTALLED:
      case NotificationType::THEME_INSTALLED:
        installed_ = Details<Extension>(details).ptr();
        break;

      default:
        DCHECK(false);
    }
  }

  void SetExtensionsEnabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

  void TestInstallExtension(const FilePath& path,
                            bool should_succeed) {
    ASSERT_TRUE(file_util::PathExists(path));
    service_->InstallExtension(path);
    loop_.RunAllPending();
    std::vector<std::string> errors = GetErrors();
    if (should_succeed) {
      ++total_successes_;

      EXPECT_TRUE(installed_) << path.value();
      EXPECT_EQ(1u, loaded_.size()) << path.value();
      EXPECT_EQ(0u, errors.size()) << path.value();
      EXPECT_EQ(total_successes_, service_->extensions()->size()) <<
          path.value();
      if (loaded_.size() > 0) {
        EXPECT_TRUE(service_->GetExtensionByID(loaded_[0]->id())) << 
            path.value();
      }
      for (std::vector<std::string>::iterator err = errors.begin();
        err != errors.end(); ++err) {
        LOG(ERROR) << *err;
      }
    } else {
      EXPECT_FALSE(installed_) << path.value();
      EXPECT_EQ(1u, errors.size()) << path.value();
    }

    installed_ = NULL;
    loaded_.clear();
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<ExtensionsService> service_;
  size_t total_successes_;
  MessageLoop loop_;
  std::vector<Extension*> loaded_;
  std::string unloaded_id_;
  Extension* installed_;
  std::string registry_path_;

 private:
  NotificationRegistrar registrar_;
};

// Test loading good extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectorySuccess) {
  // Copy the test extensions into the test profile.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions");
  source_path = source_path.AppendASCII("good");

  FilePath dest_path = profile_->GetPath().AppendASCII("Extensions");
  file_util::CopyDirectory(source_path, dest_path, true); // recursive

  ASSERT_TRUE(service_->Init());
  loop_.RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  ASSERT_EQ(3u, loaded_.size());

  EXPECT_EQ(std::string("00123456789abcdef0123456789abcdef0123456"),
            loaded_[0]->id());
  EXPECT_EQ(std::string("My extension 1"),
            loaded_[0]->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            loaded_[0]->description());
  EXPECT_EQ(Extension::INTERNAL, loaded_[0]->location());
  EXPECT_TRUE(service_->GetExtensionByID(loaded_[0]->id()));
  EXPECT_EQ(3u, service_->extensions()->size());

  Extension* extension = loaded_[0];
  const UserScriptList& scripts = extension->content_scripts();
  const std::vector<std::string>& toolstrips = extension->toolstrips();
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(2u, scripts[0].url_patterns().size());
  EXPECT_EQ("http://*.google.com/*",
            scripts[0].url_patterns()[0].GetAsString());
  EXPECT_EQ("https://*.google.com/*",
            scripts[0].url_patterns()[1].GetAsString());
  EXPECT_EQ(2u, scripts[0].js_scripts().size());
  EXPECT_EQ(extension->path().AppendASCII("script1.js").value(),
            scripts[0].js_scripts()[0].path().value());
  EXPECT_EQ(extension->path().AppendASCII("script2.js").value(),
            scripts[0].js_scripts()[1].path().value());
  EXPECT_TRUE(extension->plugins_dir().empty());
  EXPECT_EQ(1u, scripts[1].url_patterns().size());
  EXPECT_EQ("http://*.news.com/*", scripts[1].url_patterns()[0].GetAsString());
  EXPECT_EQ(extension->path().AppendASCII("js_files").AppendASCII("script3.js")
      .value(), scripts[1].js_scripts()[0].path().value());
  const std::vector<URLPattern> permissions = extension->permissions();
  ASSERT_EQ(2u, permissions.size());
  EXPECT_EQ("http://*.google.com/*", permissions[0].GetAsString());
  EXPECT_EQ("https://*.google.com/*", permissions[1].GetAsString());
  ASSERT_EQ(2u, toolstrips.size());
  EXPECT_EQ("toolstrip1.html", toolstrips[0]);
  EXPECT_EQ("toolstrip2.html", toolstrips[1]);

  EXPECT_EQ(std::string("10123456789abcdef0123456789abcdef0123456"),
            loaded_[1]->id());
  EXPECT_EQ(std::string("My extension 2"), loaded_[1]->name());
  EXPECT_EQ(std::string(""), loaded_[1]->description());
  EXPECT_EQ(loaded_[1]->path().AppendASCII("npapi").value(),
            loaded_[1]->plugins_dir().value());
  EXPECT_EQ(loaded_[1]->GetResourceURL("background.html"),
            loaded_[1]->background_url());
  EXPECT_EQ(0u, loaded_[1]->content_scripts().size());
  EXPECT_EQ(Extension::INTERNAL, loaded_[1]->location());

  EXPECT_EQ(std::string("20123456789abcdef0123456789abcdef0123456"),
            loaded_[2]->id());
  EXPECT_EQ(std::string("My extension 3"), loaded_[2]->name());
  EXPECT_EQ(std::string(""), loaded_[2]->description());
  EXPECT_EQ(0u, loaded_[2]->content_scripts().size());
  EXPECT_EQ(Extension::INTERNAL, loaded_[2]->location());
};

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectoryFail) {
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions");
  source_path = source_path.AppendASCII("bad");

  FilePath dest_path = profile_->GetPath().AppendASCII("Extensions");
  file_util::CopyDirectory(source_path, dest_path, true); // recursive

  ASSERT_TRUE(service_->Init());
  loop_.RunAllPending();

  EXPECT_EQ(3u, GetErrors().size());
  EXPECT_EQ(0u, loaded_.size());

  EXPECT_TRUE(MatchPattern(GetErrors()[0],
      std::string("Could not load extension from '*'. * ") +
      JSONReader::kBadRootElementType)) << GetErrors()[0];

  EXPECT_TRUE(MatchPattern(GetErrors()[1],
      std::string("Could not load extension from '*'. ") +
      Extension::kMissingFileError)) << GetErrors()[1];

  EXPECT_TRUE(MatchPattern(GetErrors()[2],
      std::string("Could not load extension from '*'. ") +
      Extension::kInvalidManifestError)) << GetErrors()[2];
};

// Test that partially deleted extensions are cleaned up during startup
// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, CleanupOnStartup) {
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions");
  source_path = source_path.AppendASCII("good");

  FilePath dest_path = profile_->GetPath().AppendASCII("Extensions");
  file_util::CopyDirectory(source_path, dest_path, true); // recursive

  // Simulate that one of them got partially deleted by deling the
  // Current Version file.
  dest_path = dest_path.AppendASCII("extension1")
                       .AppendASCII(ExtensionsService::kCurrentVersionFileName);
  ASSERT_TRUE(file_util::Delete(dest_path, false));  // not recursive

  service_->Init();
  loop_.RunAllPending();

  // We should have only gotten two extensions now.
  EXPECT_EQ(2u, loaded_.size());

  // And extension1 dir should now be toast.
  dest_path = dest_path.DirName();
  ASSERT_FALSE(file_util::PathExists(dest_path));
}

// Test installing extensions.
TEST_F(ExtensionsServiceTest, InstallExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // Extensions not enabled.
  SetExtensionsEnabled(false);
  FilePath path = extensions_path.AppendASCII("good.crx");
  TestInstallExtension(path, false);
  SetExtensionsEnabled(true);

  // A simple extension that should install without error.
  path = extensions_path.AppendASCII("good.crx");
  TestInstallExtension(path, true);
  // TODO(erikkay): verify the contents of the installed extension.

  // An extension with page actions.
  path = extensions_path.AppendASCII("page_action.crx");
  TestInstallExtension(path, true);

  // 0-length extension file.
  path = extensions_path.AppendASCII("not_an_extension.crx");
  TestInstallExtension(path, false);

  // Bad magic number.
  path = extensions_path.AppendASCII("bad_magic.crx");
  TestInstallExtension(path, false);

  // Poorly formed JSON.
  path = extensions_path.AppendASCII("bad_json.crx");
  TestInstallExtension(path, false);

  // Incorrect zip hash.
  path = extensions_path.AppendASCII("bad_hash.crx");
  TestInstallExtension(path, false);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

TEST_F(ExtensionsServiceTest, InstallTheme) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A theme.
  FilePath path = extensions_path.AppendASCII("theme.crx");
  TestInstallExtension(path, true);

  // A theme when extensions are disabled.
  SetExtensionsEnabled(false);
  path = extensions_path.AppendASCII("theme2.crx");
  TestInstallExtension(path, true);
  SetExtensionsEnabled(true);

  // A theme with extension elements.
  path = extensions_path.AppendASCII("theme_with_extension.crx");
  TestInstallExtension(path, false);
}

// Test that when an extension version is reinstalled, nothing happens.
TEST_F(ExtensionsServiceTest, Reinstall) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());

  installed_ = NULL;
  loaded_.clear();
  ExtensionErrorReporter::GetInstance()->ClearErrors();

  // Reinstall the same version, nothing should happen.
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_FALSE(installed_);
  ASSERT_EQ(0u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
}

// Tests uninstalling normal extensions
TEST_F(ExtensionsServiceTest, UninstallExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  TestInstallExtension(path, true);

  // The directory should be there now.
  FilePath install_path = profile_->GetPath().AppendASCII("Extensions");
  const char* extension_id = "00123456789abcdef0123456789abcdef0123456";
  FilePath extension_path = install_path.AppendASCII(extension_id);
  EXPECT_TRUE(file_util::PathExists(extension_path));

  // Uninstall it.
  service_->UninstallExtension(extension_id);
  total_successes_ = 0;

  // We should get an unload notification.
  ASSERT_TRUE(unloaded_id_.length());
  EXPECT_EQ(extension_id, unloaded_id_);

  // The extension should not be in the service anymore.
  ASSERT_FALSE(service_->GetExtensionByID(extension_id));
  loop_.RunAllPending();

  // The directory should be gone.
  EXPECT_FALSE(file_util::PathExists(extension_path));

  // Try uinstalling one that doesn't have a Current Version file for some
  // reason.
  unloaded_id_.clear();
  TestInstallExtension(path, true);
  FilePath current_version_file =
      extension_path.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  EXPECT_TRUE(file_util::Delete(current_version_file, true));
  service_->UninstallExtension(extension_id);
  loop_.RunAllPending();
  EXPECT_FALSE(file_util::PathExists(extension_path));
}

// Tests loading single extensions (like --load-extension)
TEST_F(ExtensionsServiceTest, LoadExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath ext1 = extensions_path.AppendASCII("good").AppendASCII("extension1")
      .AppendASCII("1");
  service_->LoadExtension(ext1);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::LOAD, loaded_[0]->location());

  FilePath no_manifest = extensions_path.AppendASCII("bad")
      .AppendASCII("no_manifest").AppendASCII("1");
  service_->LoadExtension(no_manifest);
  loop_.RunAllPending();
  EXPECT_EQ(1u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());

  // Test uninstall
  std::string id = loaded_[0]->id();
  EXPECT_FALSE(unloaded_id_.length());
  service_->UninstallExtension(id);
  loop_.RunAllPending();
  EXPECT_EQ(id, unloaded_id_);
}

// Tests that we generate IDs when they are not specified in the manifest for
// --load-extension.
TEST_F(ExtensionsServiceTest, GenerateID) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath no_id_ext = extensions_path.AppendASCII("no_id");
  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  std::string id1 = loaded_[0]->id();
  ASSERT_EQ("0000000000000000000000000000000000000000", id1);
  ASSERT_EQ("chrome-extension://0000000000000000000000000000000000000000/",
            loaded_[0]->url().spec());

  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  std::string id2 = loaded_[1]->id();
  ASSERT_EQ("0000000000000000000000000000000000000001", id2);
  ASSERT_EQ("chrome-extension://0000000000000000000000000000000000000001/",
            loaded_[1]->url().spec());
}

// Tests the external installation feature
#if defined(OS_WIN)

TEST_F(ExtensionsServiceTest, ExternalInstall) {
  // Register a test extension externally.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");

  RegKey key;
  std::wstring reg_path = ASCIIToWide(registry_path_);
  reg_path += L"\\00123456789ABCDEF0123456789ABCDEF0123456";
  ASSERT_TRUE(key.Create(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_WRITE));
  ASSERT_TRUE(key.WriteValue(L"path", source_path.ToWStringHack().c_str()));
  ASSERT_TRUE(key.WriteValue(L"version", L"1.0.0.0"));

  // Start up the service, it should find our externally registered extension
  // and install it.
  service_->Init();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::EXTERNAL, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());

  // Reinit the service without changing anything. The extension should be
  // loaded again.
  loaded_.clear();
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  ASSERT_TRUE(key.WriteValue(L"path", source_path.ToWStringHack().c_str()));
  ASSERT_TRUE(key.WriteValue(L"version", L"1.0.0.1"));

  loaded_.clear();
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());

  // Uninstall the extension and reinit. Nothing should happen because the
  // preference should prevent us from reinstalling.
  std::string id = loaded_[0]->id();
  service_->UninstallExtension(id);
  loop_.RunAllPending();

  // The extension should also be gone from the install directory.
  FilePath install_path = profile_->GetPath().AppendASCII("Extensions")
                                             .AppendASCII(id);
  ASSERT_FALSE(file_util::PathExists(install_path));

  loaded_.clear();
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Now clear the preference, reinstall, then remove the reg key. The extension
  // should be uninstalled.
  profile_->GetPrefs()->GetMutableList(L"extensions.uninstalled_external_ids")
                      ->Clear();
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());

  RegKey parent_key;
  key.Open(HKEY_LOCAL_MACHINE, ASCIIToWide(registry_path_).c_str(), KEY_WRITE);
  key.DeleteKey(ASCIIToWide(id).c_str());
  loaded_.clear();
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
}

#else

// TODO(port)

#endif
