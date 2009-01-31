// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

struct ExtensionsOrder {
  bool operator()(const Extension* a, const Extension* b) {
    return a->name() < b->name();
  }
};

}  // namespace

// A mock implementation of ExtensionsServiceFrontendInterface for testing the
// backend.
class ExtensionsServiceTestFrontend
    : public ExtensionsServiceFrontendInterface {
 public:

   ExtensionsServiceTestFrontend() {
     file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("ext_test"),
                                       &install_dir_);
   }

  ~ExtensionsServiceTestFrontend() {
    for (ExtensionList::iterator iter = extensions_.begin();
         iter != extensions_.end(); ++iter) {
      delete *iter;
    }
  }

  std::vector<std::string>* errors() {
    return &errors_;
  }

  ExtensionList* extensions() {
    return &extensions_;
  }

  std::vector<FilePath>* installed() {
    return &installed_;
  }

  FilePath install_dir() {
    return install_dir_;
  }

  // ExtensionsServiceFrontendInterface
  virtual MessageLoop* GetMessageLoop() {
    return &message_loop_;
  }

  virtual void OnExtensionLoadError(const std::string& message) {
    errors_.push_back(message);
  }

  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* new_extensions) {
    extensions_.insert(extensions_.end(), new_extensions->begin(),
                       new_extensions->end());
    delete new_extensions;
    // In the tests we rely on extensions being in particular order,
    // which is not always the case (and is not guaranteed by used APIs).
    std::stable_sort(extensions_.begin(), extensions_.end(), ExtensionsOrder());
  }

  virtual void InstallExtension(const FilePath& extension_path) {
  }

  virtual void OnExtensionInstallError(const std::string& message) {
    errors_.push_back(message);
  }

  virtual void OnExtensionInstalled(FilePath path) {
    installed_.push_back(path);
  }

  void TestInstallExtension(const FilePath& path,
                            ExtensionsServiceBackend* backend,
                            bool should_succeed) {
    ASSERT_TRUE(file_util::PathExists(path));
    EXPECT_EQ(should_succeed,
              backend->InstallExtension(path, install_dir_,
                  scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
    message_loop_.RunAllPending();
    if (should_succeed) {
      EXPECT_EQ(1u, installed_.size());
      EXPECT_EQ(0u, errors_.size());
    } else {
      EXPECT_EQ(0u, installed_.size());
      EXPECT_EQ(1u, errors_.size());
    }
    installed_.clear();
    errors_.clear();
  }


 private:
  MessageLoop message_loop_;
  ExtensionList extensions_;
  std::vector<std::string> errors_;
  std::vector<FilePath> installed_;
  FilePath install_dir_;
};

// make the test a PlatformTest to setup autorelease pools properly on mac
typedef PlatformTest ExtensionsServiceTest;

// Test loading extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectory) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  scoped_refptr<ExtensionsServiceBackend> backend(new ExtensionsServiceBackend);
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  std::vector<Extension*> extensions;
  EXPECT_TRUE(backend->LoadExtensionsFromDirectory(extensions_path,
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get())));
  frontend->GetMessageLoop()->RunAllPending();

  // Note: There can be more errors if there are extra directories, like .svn
  // directories.
  EXPECT_TRUE(frontend->errors()->size() >= 2u);
  ASSERT_EQ(3u, frontend->extensions()->size());

  EXPECT_EQ(std::string("com.google.myextension1"),
            frontend->extensions()->at(0)->id());
  EXPECT_EQ(std::string("My extension 1"),
            frontend->extensions()->at(0)->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            frontend->extensions()->at(0)->description());

  Extension* extension = frontend->extensions()->at(0);
  const UserScriptList& scripts = extension->user_scripts();
  ASSERT_EQ(2u, scripts.size());
  EXPECT_EQ(2u, scripts[0].matches.size());
  EXPECT_EQ("http://*.google.com/*", scripts[0].matches[0]);
  EXPECT_EQ("https://*.google.com/*", scripts[0].matches[1]);
  EXPECT_EQ(extension->path().AppendASCII("script1.js").value(),
            scripts[0].path.value());
  EXPECT_EQ(1u, scripts[1].matches.size());
  EXPECT_EQ("http://*.yahoo.com/*", scripts[1].matches[0]);
  EXPECT_EQ(extension->path().AppendASCII("script2.js").value(),
            scripts[1].path.value());

  EXPECT_EQ(std::string("com.google.myextension2"),
            frontend->extensions()->at(1)->id());
  EXPECT_EQ(std::string("My extension 2"),
            frontend->extensions()->at(1)->name());
  EXPECT_EQ(std::string(""),
            frontend->extensions()->at(1)->description());
  ASSERT_EQ(0u, frontend->extensions()->at(1)->user_scripts().size());

  EXPECT_EQ(std::string("com.google.myextension3"),
            frontend->extensions()->at(2)->id());
  EXPECT_EQ(std::string("My extension 3"),
            frontend->extensions()->at(2)->name());
  EXPECT_EQ(std::string(""),
            frontend->extensions()->at(2)->description());
  ASSERT_EQ(0u, frontend->extensions()->at(2)->user_scripts().size());
};

// Test installing extensions.
TEST_F(ExtensionsServiceTest, InstallExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  scoped_refptr<ExtensionsServiceBackend> backend(new ExtensionsServiceBackend);
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  FilePath path = extensions_path.AppendASCII("good.crx");

  // A simple extension that should install without error.
  frontend->TestInstallExtension(path, backend, true);
  // TODO(erikkay): verify the contents of the installed extension.

  // Installing the same extension twice should fail.
  frontend->TestInstallExtension(path, backend, false);

  // 0-length extension file.
  path = extensions_path.AppendASCII("not_an_extension.crx");
  frontend->TestInstallExtension(path, backend, false);

  // Bad magic number.
  path = extensions_path.AppendASCII("bad_magic.crx");
  frontend->TestInstallExtension(path, backend, false);

  // Poorly formed JSON.
  path = extensions_path.AppendASCII("bad_json.crx");
  frontend->TestInstallExtension(path, backend, false);

  // Incorrect zip hash.
  path = extensions_path.AppendASCII("bad_hash.crx");
  frontend->TestInstallExtension(path, backend, false);

  // TODO(erikkay): add more tests for many of the failure cases.
  // TODO(erikkay): add tests for upgrade cases.
}

