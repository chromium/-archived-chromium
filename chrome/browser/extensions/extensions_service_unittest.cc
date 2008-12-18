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

 private:
  MessageLoop message_loop_;
  ExtensionList extensions_;
  std::vector<std::string> errors_;
};

// make the test a PlatformTest to setup autorelease pools properly on mac
typedef PlatformTest ExtensionsServiceTest;

// Test loading extensions from the profile directory.
TEST_F(ExtensionsServiceTest, LoadAllExtensionsFromDirectory) {
  std::wstring extensions_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_dir));
  FilePath manifest_path = FilePath::FromWStringHack(extensions_dir).Append(
      FILE_PATH_LITERAL("extensions"));

  scoped_refptr<ExtensionsServiceBackend> backend(new ExtensionsServiceBackend);
  scoped_refptr<ExtensionsServiceTestFrontend> frontend(
      new ExtensionsServiceTestFrontend);

  std::vector<Extension*> extensions;
  EXPECT_TRUE(backend->LoadExtensionsFromDirectory(manifest_path,
      scoped_refptr<ExtensionsServiceFrontendInterface>(frontend.get())));
  frontend->GetMessageLoop()->RunAllPending();

  // Note: There can be more errors if there are extra directories, like .svn
  // directories.
  EXPECT_TRUE(frontend->errors()->size() >= 2u);
  ASSERT_EQ(2u, frontend->extensions()->size());

  EXPECT_EQ(std::string("com.google.myextension1"),
            frontend->extensions()->at(0)->id());
  EXPECT_EQ(std::string("My extension 1"),
            frontend->extensions()->at(0)->name());
  EXPECT_EQ(std::string("The first extension that I made."),
            frontend->extensions()->at(0)->description());
  ASSERT_EQ(2u, frontend->extensions()->at(0)->content_scripts().size());
  EXPECT_EQ(std::string("script1.user.js"),
            frontend->extensions()->at(0)->content_scripts().at(0));
  EXPECT_EQ(std::string("script2.user.js"),
            frontend->extensions()->at(0)->content_scripts().at(1));

  EXPECT_EQ(std::string("com.google.myextension2"),
            frontend->extensions()->at(1)->id());
  EXPECT_EQ(std::string("My extension 2"),
            frontend->extensions()->at(1)->name());
  EXPECT_EQ(std::string(""),
            frontend->extensions()->at(1)->description());
  ASSERT_EQ(0u, frontend->extensions()->at(1)->content_scripts().size());
};
