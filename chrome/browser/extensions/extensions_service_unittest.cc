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
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/external_extension_provider.h"
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

namespace {

// Extension ids used during testing.
const char* const all_zero = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char* const zero_n_one = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab";
const char* const good0 = "behllobkkfkfnphdnhnkndlbkcpglgmj";
const char* const good1 = "hpiknbiabeeppbpihjehijgoemciehgk";
const char* const good2 = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
const char* const good_crx = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char* const page_action = "kemkhnabegjkabakmlcaafgikalipenj";
const char* const theme_crx = "iamefpfkojoapidjnbafmgkgncegbkad";
const char* const theme2_crx = "pjpgmfcmabopnnfonnhmdjglfpjjfkbf";

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

class MockExtensionProvider : public ExternalExtensionProvider {
 public:
  explicit MockExtensionProvider(Extension::Location location)
    : location_(location) {}
  virtual ~MockExtensionProvider() {}

  void UpdateOrAddExtension(const std::string& id,
                            const std::string& version,
                            FilePath path) {
    extension_map_[id] = std::make_pair(version, path);
  }

  void RemoveExtension(const std::string& id) {
    extension_map_.erase(id);
  }

  // ExternalExtensionProvider implementation:
  virtual void VisitRegisteredExtension(
      Visitor* visitor, const std::set<std::string>& ids_to_ignore) const {
    for (DataMap::const_iterator i = extension_map_.begin();
         i != extension_map_.end(); ++i) {
      if (ids_to_ignore.find(i->first) != ids_to_ignore.end())
        continue;
      scoped_ptr<Version> version;
      version.reset(Version::GetVersionFromString(i->second.first));

      visitor->OnExternalExtensionFound(
          i->first, version.get(), i->second.second);
    }
  }

  virtual Version* RegisteredVersion(std::string id,
                                     Extension::Location* location) const {
    DataMap::const_iterator it = extension_map_.find(id);
    if (it == extension_map_.end())
      return NULL;

    if (location)
      *location = location_;
    return Version::GetVersionFromString(it->second.first);
  }

 private:
  typedef std::map< std::string, std::pair<std::string, FilePath> > DataMap;
  DataMap extension_map_;
  Extension::Location location_;
};

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

    profile_.reset(new TestingProfile());
    service_ = new ExtensionsService(profile_.get(), &loop_, &loop_);
    service_->set_extensions_enabled(true);
    service_->set_show_extensions_prompts(false);

    // When we start up, we want to make sure there is no external provider,
    // since the ExtensionService on Windows will use the Registry as a default
    // provider and if there is something already registered there then it will
    // interfere with the tests. Those tests that need an external provider
    // will register one specifically.
    service_->ClearProvidersForTesting();

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

      case NotificationType::EXTENSION_UNLOADED: {
        Extension* e = Details<Extension>(details).ptr();
        unloaded_id_ = e->id();
        ExtensionList::iterator i =
            std::find(loaded_.begin(), loaded_.end(), e);
        // TODO(erikkay) fix so this can be an assert.  Right now the tests
        // are manually calling clear() on loaded_, so this isn't doable.
        if (i == loaded_.end())
          return;
        loaded_.erase(i);
        break;
      }
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

  void SetMockExternalProvider(Extension::Location location,
                               ExternalExtensionProvider* provider) {
    service_->SetProviderForTesting(location, provider);
  }

 protected:
  void InstallExtension(const FilePath& path,
                        bool should_succeed) {
    ASSERT_TRUE(file_util::PathExists(path));
    service_->InstallExtension(path);
    loop_.RunAllPending();
    std::vector<std::string> errors = GetErrors();
    if (should_succeed) {
      ++total_successes_;

      EXPECT_TRUE(installed_) << path.value();

      ASSERT_EQ(1u, loaded_.size()) << path.value();
      EXPECT_EQ(0u, errors.size()) << path.value();
      EXPECT_EQ(total_successes_, service_->extensions()->size()) <<
          path.value();
      EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id())) <<
          path.value();
      for (std::vector<std::string>::iterator err = errors.begin();
        err != errors.end(); ++err) {
        LOG(ERROR) << *err;
      }
    } else {
      EXPECT_FALSE(installed_) << path.value();
      EXPECT_EQ(0u, loaded_.size()) << path.value();
      EXPECT_EQ(1u, errors.size()) << path.value();
    }

    installed_ = NULL;
    loaded_.clear();
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }

  void ValidatePrefKeyCount(size_t count) {
    DictionaryValue* dict =
        profile_->GetPrefs()->GetMutableDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL);
    EXPECT_EQ(count, dict->GetSize());
  }

  void ValidatePref(std::string extension_id,
                    std::wstring pref_path,
                    int must_equal) {
    std::wstring msg = L" while checking: ";
    msg += ASCIIToWide(extension_id);
    msg += L" ";
    msg += pref_path;
    msg += L" == ";
    msg += IntToWString(must_equal);

    const DictionaryValue* dict =
        profile_->GetPrefs()->GetDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(ASCIIToWide(extension_id), &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    int val;
    pref->GetInteger(pref_path, &val);
    EXPECT_EQ(must_equal, val) << msg;
  }

  void SetPref(std::string extension_id, std::wstring pref_path, int value) {
    std::wstring msg = L" while setting: ";
    msg += ASCIIToWide(extension_id);
    msg += L" ";
    msg += pref_path;
    msg += L" = ";
    msg += IntToWString(value);

    const DictionaryValue* dict =
        profile_->GetPrefs()->GetMutableDictionary(L"extensions.settings");
    ASSERT_TRUE(dict != NULL) << msg;
    DictionaryValue* pref = NULL;
    ASSERT_TRUE(dict->GetDictionary(ASCIIToWide(extension_id), &pref)) << msg;
    EXPECT_TRUE(pref != NULL) << msg;
    pref->SetInteger(pref_path, value);
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<ExtensionsService> service_;
  size_t total_successes_;
  MessageLoop loop_;
  ExtensionList loaded_;
  std::string unloaded_id_;
  Extension* installed_;

 private:
  NotificationRegistrar registrar_;
};

// TODO(erikkay) this test and the next need to be replaced with equivalent
// versions that load from prefs.
// Test loading good extensions from the profile directory.
TEST_F(ExtensionsServiceTest, DISABLED_LoadAllExtensionsFromDirectorySuccess) {
  // Copy the test extensions into the test profile.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions");
  source_path = source_path.AppendASCII("good");

  FilePath dest_path = profile_->GetPath().AppendASCII("Extensions");
  file_util::CopyDirectory(source_path, dest_path, true);  // Recursive.

  service_->Init();
  loop_.RunAllPending();

  std::vector<std::string> errors = GetErrors();
  for (std::vector<std::string>::iterator err = errors.begin();
    err != errors.end(); ++err) {
    LOG(ERROR) << *err;
  }
  ASSERT_EQ(3u, loaded_.size());

  EXPECT_EQ(std::string(good0), loaded_[0]->id());
  EXPECT_EQ(std::string("My extension 1"),
            loaded_[0]->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            loaded_[0]->description());
  EXPECT_EQ(Extension::INTERNAL, loaded_[0]->location());
  EXPECT_TRUE(service_->GetExtensionById(loaded_[0]->id()));
  EXPECT_EQ(3u, service_->extensions()->size());

  ValidatePrefKeyCount(3);
  ValidatePref(good0, L"state", Extension::ENABLED);
  ValidatePref(good0, L"location", Extension::INTERNAL);
  ValidatePref(good1, L"state", Extension::ENABLED);
  ValidatePref(good1, L"location", Extension::INTERNAL);
  ValidatePref(good2, L"state", Extension::ENABLED);
  ValidatePref(good2, L"location", Extension::INTERNAL);

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
  EXPECT_TRUE(extension->plugins().empty());
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

  EXPECT_EQ(std::string(good1), loaded_[1]->id());
  EXPECT_EQ(std::string("My extension 2"), loaded_[1]->name());
  EXPECT_EQ(std::string(""), loaded_[1]->description());
  EXPECT_EQ(loaded_[1]->GetResourceURL("background.html"),
            loaded_[1]->background_url());
  EXPECT_EQ(0u, loaded_[1]->content_scripts().size());
  EXPECT_EQ(2u, loaded_[1]->plugins().size());
  EXPECT_EQ(loaded_[1]->path().AppendASCII("content_plugin.dll").value(),
            loaded_[1]->plugins()[0].path.value());
  EXPECT_TRUE(loaded_[1]->plugins()[0].is_public);
  EXPECT_EQ(loaded_[1]->path().AppendASCII("extension_plugin.dll").value(),
            loaded_[1]->plugins()[1].path.value());
  EXPECT_FALSE(loaded_[1]->plugins()[1].is_public);
  EXPECT_EQ(Extension::INTERNAL, loaded_[1]->location());

  EXPECT_EQ(std::string(good2), loaded_[2]->id());
  EXPECT_EQ(std::string("My extension 3"), loaded_[2]->name());
  EXPECT_EQ(std::string(""), loaded_[2]->description());
  EXPECT_EQ(0u, loaded_[2]->content_scripts().size());
  EXPECT_EQ(Extension::INTERNAL, loaded_[2]->location());
};

// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, DISABLED_LoadAllExtensionsFromDirectoryFail) {
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions");
  source_path = source_path.AppendASCII("bad");

  FilePath dest_path = profile_->GetPath().AppendASCII("Extensions");
  file_util::CopyDirectory(source_path, dest_path, true);  // Recursive.

  service_->Init();
  loop_.RunAllPending();

  EXPECT_EQ(3u, GetErrors().size());
  EXPECT_EQ(0u, loaded_.size());

  // Make sure the dictionary is empty.
  ValidatePrefKeyCount(0);

  EXPECT_TRUE(MatchPattern(GetErrors()[0],
      std::string("Could not load extension from '*'. * ") +
      JSONReader::kBadRootElementType)) << GetErrors()[0];

  EXPECT_TRUE(MatchPattern(GetErrors()[1],
      std::string("Could not load extension from '*'. ") +
      Extension::kInvalidManifestError)) << GetErrors()[1];

  EXPECT_TRUE(MatchPattern(GetErrors()[2],
      std::string("Could not load extension from '*'. ") +
      Extension::kMissingFileError)) << GetErrors()[2];
};

// Test that partially deleted extensions are cleaned up during startup
// Test loading bad extensions from the profile directory.
TEST_F(ExtensionsServiceTest, CleanupOnStartup) {
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions");
  source_path = source_path.AppendASCII("good");

  FilePath dest_path = profile_->GetPath().AppendASCII("Extensions");
  file_util::CopyDirectory(source_path, dest_path, true);  // Recursive.

  // Simulate that one of them got partially deleted by deling the
  // Current Version file.
  FilePath vers = dest_path.AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII(ExtensionsService::kCurrentVersionFileName);
  ASSERT_TRUE(file_util::Delete(vers, false));  // not recursive

  service_->Init();
  loop_.RunAllPending();

  file_util::FileEnumerator dirs(dest_path, false,
                                 file_util::FileEnumerator::DIRECTORIES);
  size_t count = 0;
  while (!dirs.Next().empty())
    count++;

  // We should have only gotten two extensions now.
  EXPECT_EQ(2u, count);

  // And extension1 dir should now be toast.
  vers = vers.DirName();
  ASSERT_FALSE(file_util::PathExists(vers));
}

// Test installing extensions.
TEST_F(ExtensionsServiceTest, InstallExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // Extensions not enabled.
  SetExtensionsEnabled(false);
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, false);
  SetExtensionsEnabled(true);

  ValidatePrefKeyCount(0);

  // A simple extension that should install without error.
  path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);
  // TODO(erikkay): verify the contents of the installed extension.

  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::INTERNAL);

  // An extension with page actions.
  path = extensions_path.AppendASCII("page_action.crx");
  InstallExtension(path, true);
  ValidatePrefKeyCount(++pref_count);
  ValidatePref(page_action, L"state", Extension::ENABLED);
  ValidatePref(page_action, L"location", Extension::INTERNAL);

  // Bad signature.
  path = extensions_path.AppendASCII("bad_signature.crx");
  InstallExtension(path, false);

  // 0-length extension file.
  path = extensions_path.AppendASCII("not_an_extension.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // Bad magic number.
  path = extensions_path.AppendASCII("bad_magic.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

#if defined(OS_WIN)  // TODO(port)
// Test Packaging and installing an extension.
// TODO(rafaelw): add more tests for failure cases.
TEST_F(ExtensionsServiceTest, PackExtension) {
  SetExtensionsEnabled(true);

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath input_directory = extensions_path.AppendASCII("good")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj").AppendASCII("1.0.0.0");

  FilePath output_directory;
  file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_"),
      &output_directory);
  FilePath crx_path(output_directory.AppendASCII("ex1.crx"));
  FilePath privkey_path(output_directory.AppendASCII("privkey.pem"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, FilePath(),
      privkey_path));

  std::string priv_key;
  file_util::ReadFileToString(crx_path, &priv_key);
  LOG(INFO) << "Private key:\n" << priv_key;

  ASSERT_TRUE(file_util::PathExists(privkey_path));
  InstallExtension(crx_path, true);

  file_util::Delete(crx_path, false);
  file_util::Delete(privkey_path, false);
}

// Test Packaging and installing an extension using an openssl generated key.
// The openssl is generated with the following:
// > openssl genrsa -out privkey.pem 1024
// > openssl pkcs8 -topk8 -nocrypt -in privkey.pem -out privkey_asn1.pem
// The privkey.pem is a PrivateKey, and the pcks8 -topk8 creates a
// PrivateKeyInfo ASN.1 structure, we our RSAPrivateKey expects.
TEST_F(ExtensionsServiceTest, PackExtensionOpenSSLKey) {
  SetExtensionsEnabled(true);

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath input_directory = extensions_path.AppendASCII("good")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj").AppendASCII("1.0.0.0");
  FilePath privkey_path(extensions_path.AppendASCII(
      "openssl_privkey_asn1.pem"));
  ASSERT_TRUE(file_util::PathExists(privkey_path));

  FilePath output_directory;
  file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_"),
      &output_directory);
  FilePath crx_path(output_directory.AppendASCII("ex1.crx"));

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(input_directory, crx_path, privkey_path,
      FilePath()));

  InstallExtension(crx_path, true);

  file_util::Delete(crx_path, false);
}
#endif  // defined(OS_WIN)

TEST_F(ExtensionsServiceTest, InstallTheme) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A theme.
  FilePath path = extensions_path.AppendASCII("theme.crx");
  InstallExtension(path, true);
  int pref_count = 0;
  ValidatePrefKeyCount(++pref_count);
  ValidatePref(theme_crx, L"state", Extension::ENABLED);
  ValidatePref(theme_crx, L"location", Extension::INTERNAL);

  // A theme when extensions are disabled. Themes can be installed even though
  // extensions are disabled.
  SetExtensionsEnabled(false);
  path = extensions_path.AppendASCII("theme2.crx");
  InstallExtension(path, true);
  ValidatePrefKeyCount(++pref_count);
  ValidatePref(theme2_crx, L"state", Extension::ENABLED);
  ValidatePref(theme2_crx, L"location", Extension::INTERNAL);
  SetExtensionsEnabled(true);

  // A theme with extension elements. Themes cannot have extension elements so
  // this test should fail.
  path = extensions_path.AppendASCII("theme_with_extension.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);

  // A theme with image resources missing (misspelt path).
  path = extensions_path.AppendASCII("theme_missing_image.crx");
  InstallExtension(path, false);
  ValidatePrefKeyCount(pref_count);
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
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::INTERNAL);

  installed_ = NULL;
  loaded_.clear();
  ExtensionErrorReporter::GetInstance()->ClearErrors();

  // Reinstall the same version, nothing should happen.
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_FALSE(installed_);
  ASSERT_EQ(0u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::INTERNAL);
}

// Test upgrading a signed extension.
TEST_F(ExtensionsServiceTest, UpgradeSignedGood) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ASSERT_EQ(0u, GetErrors().size());

  // Upgrade to version 2.0
  path = extensions_path.AppendASCII("good2.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ASSERT_EQ(0u, GetErrors().size());
}

// Test upgrading a signed extension with a bad signature.
TEST_F(ExtensionsServiceTest, UpgradeSignedBad) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath path = extensions_path.AppendASCII("good.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_TRUE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(0u, GetErrors().size());
  installed_ = NULL;

  // Try upgrading with a bad signature. This should fail during the unpack,
  // because the key will not match the signature.
  path = extensions_path.AppendASCII("good2_bad_signature.crx");
  service_->InstallExtension(path);
  loop_.RunAllPending();

  ASSERT_FALSE(installed_);
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(1u, GetErrors().size());
}

// Tests uninstalling normal extensions
TEST_F(ExtensionsServiceTest, UninstallExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  // A simple extension that should install without error.
  FilePath path = extensions_path.AppendASCII("good.crx");
  InstallExtension(path, true);

  // The directory should be there now.
  FilePath install_path = profile_->GetPath().AppendASCII("Extensions");
  const char* extension_id = good_crx;
  FilePath extension_path = install_path.AppendASCII(extension_id);
  EXPECT_TRUE(file_util::PathExists(extension_path));

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::INTERNAL);

  // Uninstall it.
  service_->UninstallExtension(extension_id);
  total_successes_ = 0;

  // We should get an unload notification.
  ASSERT_TRUE(unloaded_id_.length());
  EXPECT_EQ(extension_id, unloaded_id_);

  ValidatePrefKeyCount(0);

  // The extension should not be in the service anymore.
  ASSERT_FALSE(service_->GetExtensionById(extension_id));
  loop_.RunAllPending();

  // The directory should be gone.
  EXPECT_FALSE(file_util::PathExists(extension_path));

  // Try uinstalling one that doesn't have a Current Version file for some
  // reason.
  unloaded_id_.clear();
  InstallExtension(path, true);
  FilePath current_version_file =
      extension_path.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  EXPECT_TRUE(file_util::Delete(current_version_file, true));
  service_->UninstallExtension(extension_id);
  loop_.RunAllPending();
  EXPECT_FALSE(file_util::PathExists(extension_path));

  ValidatePrefKeyCount(0);
}

// Tests loading single extensions (like --load-extension)
TEST_F(ExtensionsServiceTest, LoadExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath ext1 = extensions_path.AppendASCII("good")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");
  service_->LoadExtension(ext1);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(Extension::LOAD, loaded_[0]->location());
  EXPECT_EQ(1u, service_->extensions()->size());

  // --load-extension doesn't add entries to prefs
  ValidatePrefKeyCount(0);

  FilePath no_manifest = extensions_path.AppendASCII("bad")
      .AppendASCII("no_manifest").AppendASCII("1");
  service_->LoadExtension(no_manifest);
  loop_.RunAllPending();
  EXPECT_EQ(1u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  EXPECT_EQ(1u, service_->extensions()->size());

  // Test uninstall.
  std::string id = loaded_[0]->id();
  EXPECT_FALSE(unloaded_id_.length());
  service_->UninstallExtension(id);
  loop_.RunAllPending();
  EXPECT_EQ(id, unloaded_id_);
  ASSERT_EQ(0u, loaded_.size());
  EXPECT_EQ(0u, service_->extensions()->size());
}

// Tests that we generate IDs when they are not specified in the manifest for
// --load-extension.
TEST_F(ExtensionsServiceTest, GenerateID) {
  Extension::ResetGeneratedIdCounter();

  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath no_id_ext = extensions_path.AppendASCII("no_id");
  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  EXPECT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  std::string id1 = loaded_[0]->id();
  EXPECT_EQ(all_zero, id1);
  EXPECT_EQ("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/",
            loaded_[0]->url().spec());
  EXPECT_EQ(loaded_[0]->location(), Extension::LOAD);

  // --load-extension doesn't add entries to prefs
  ValidatePrefKeyCount(0);

  service_->LoadExtension(no_id_ext);
  loop_.RunAllPending();
  std::string id2 = loaded_[1]->id();
  EXPECT_EQ(zero_n_one, id2);
  EXPECT_EQ("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab/",
            loaded_[1]->url().spec());
  EXPECT_EQ(loaded_[1]->location(), Extension::LOAD);

  // --load-extension doesn't add entries to prefs
  ValidatePrefKeyCount(0);
}

// Tests the external installation feature
#if defined(OS_WIN)

TEST_F(ExtensionsServiceTest, ExternalInstallRegistry) {
  // Verify that starting with no providers loads no extensions.
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* reg_provider =
      new MockExtensionProvider(Extension::EXTERNAL_REGISTRY);
  SetMockExternalProvider(Extension::EXTERNAL_REGISTRY, reg_provider);

  // Register a test extension externally using the mock registry provider.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");

  // Add the extension.
  reg_provider->UpdateOrAddExtension(good_crx, "1.0.0.0", source_path);

  // Reloading extensions should find our externally registered extension
  // and install it.
  service_->CheckForUpdates();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::EXTERNAL_REGISTRY, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Reload extensions without changing anything. The extension should be
  // loaded again.
  loaded_.clear();
  service_->ReloadExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  reg_provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  loaded_.clear();
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

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
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::KILLBIT);  // It is an ex-parrot.
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  // Now clear the preference, reinstall, then remove the reg key. The extension
  // should be uninstalled.
  SetPref(good_crx, L"state", Extension::ENABLED);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());
  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_REGISTRY);

  reg_provider->RemoveExtension(good_crx);

  loaded_.clear();
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());
}

#endif

TEST_F(ExtensionsServiceTest, ExternalInstallPref) {
  // Verify that starting with no providers loads no extensions.
  service_->Init();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  // Now add providers. Extension system takes ownership of the objects.
  MockExtensionProvider* pref_provider =
      new MockExtensionProvider(Extension::EXTERNAL_PREF);
  SetMockExternalProvider(Extension::EXTERNAL_PREF, pref_provider);

  // Register a external extension using preinstalled preferences.
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &source_path));
  source_path = source_path.AppendASCII("extensions").AppendASCII("good.crx");

  // Add the extension.
  pref_provider->UpdateOrAddExtension(good_crx, "1.0", source_path);

  // Checking for updates should find our externally registered extension
  // and install it.
  service_->CheckForUpdates();
  loop_.RunAllPending();

  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ(Extension::EXTERNAL_PREF, loaded_[0]->location());
  ASSERT_EQ("1.0.0.0", loaded_[0]->version()->GetString());

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Reload extensions without changing anything. The extension should be
  // loaded again.
  loaded_.clear();
  service_->ReloadExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Now update the extension with a new version. We should get upgraded.
  source_path = source_path.DirName().AppendASCII("good2.crx");
  pref_provider->UpdateOrAddExtension(good_crx, "1.0.0.1", source_path);

  loaded_.clear();
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, GetErrors().size());
  ASSERT_EQ(1u, loaded_.size());
  ASSERT_EQ("1.0.0.1", loaded_[0]->version()->GetString());

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Uninstall the extension and reload. Nothing should happen because the
  // preference should prevent us from reinstalling.
  std::string id = loaded_[0]->id();
  service_->UninstallExtension(id);
  loop_.RunAllPending();

  // The extension should also be gone from the install directory.
  FilePath install_path =
      profile_->GetPath().AppendASCII("Extensions").AppendASCII(id);
  ASSERT_FALSE(file_util::PathExists(install_path));

  loaded_.clear();
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::KILLBIT);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Now clear the preference and reinstall.
  SetPref(good_crx, L"state", Extension::ENABLED);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->CheckForUpdates();
  loop_.RunAllPending();
  ASSERT_EQ(1u, loaded_.size());

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::ENABLED);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // Now set the kill bit and watch the extension go away.
  SetPref(good_crx, L"state", Extension::KILLBIT);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  loaded_.clear();
  service_->ReloadExtensions();
  loop_.RunAllPending();
  ASSERT_EQ(0u, loaded_.size());

  ValidatePrefKeyCount(1);
  ValidatePref(good_crx, L"state", Extension::KILLBIT);
  ValidatePref(good_crx, L"location", Extension::EXTERNAL_PREF);

  // The extension should also be gone from disk.
  FilePath extension_path = install_path.DirName();
  extension_path = extension_path.AppendASCII(good_crx);
  EXPECT_FALSE(file_util::PathExists(extension_path)) <<
      extension_path.ToWStringHack();
}
