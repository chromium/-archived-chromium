// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"


// A mock implementation of ExtensionsServiceFrontendInterface for testing the
// backend.
class ExtensionsServiceTestFrontend
    : public ExtensionsServiceFrontendInterface {
 public:
  std::vector<std::wstring>* errors() {
    return &errors_;
  }

  ExtensionList* extensions() {
    return extensions_.get();
  }

  // ExtensionsServiceFrontendInterface
  virtual MessageLoop* GetMessageLoop() {
    return &message_loop_;
  }

  virtual void OnExtensionLoadError(const std::wstring& message) {
    errors_.push_back(message);
  }

  virtual void OnExtensionsLoadedFromDirectory(ExtensionList* extensions) {
    extensions_.reset(extensions);
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<ExtensionList> extensions_;
  std::vector<std::wstring> errors_;
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
  EXPECT_EQ(Extension::kInvalidManifestError, frontend->errors()->at(0));
  EXPECT_EQ(Extension::kInvalidManifestError, frontend->errors()->at(1));
  EXPECT_EQ(2u, frontend->extensions()->size());

  EXPECT_EQ(std::wstring(L"com.google.myextension1"),
            frontend->extensions()->at(0)->id());
  EXPECT_EQ(std::wstring(L"My extension 1"),
            frontend->extensions()->at(0)->name());
  EXPECT_EQ(std::wstring(L"The first extension that I made."),
            frontend->extensions()->at(0)->description());
  EXPECT_EQ(2u, frontend->extensions()->at(0)->content_scripts().size());
  EXPECT_EQ(std::wstring(L"script1.user.js"),
            frontend->extensions()->at(0)->content_scripts().at(0));
  EXPECT_EQ(std::wstring(L"script2.user.js"),
            frontend->extensions()->at(0)->content_scripts().at(1));

  EXPECT_EQ(std::wstring(L"com.google.myextension2"),
            frontend->extensions()->at(1)->id());
  EXPECT_EQ(std::wstring(L"My extension 2"),
            frontend->extensions()->at(1)->name());
  EXPECT_EQ(std::wstring(L""),
            frontend->extensions()->at(1)->description());
  EXPECT_EQ(0u, frontend->extensions()->at(1)->content_scripts().size());
};
