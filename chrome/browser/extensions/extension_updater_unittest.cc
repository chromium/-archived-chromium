// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/test_url_fetcher_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

const char *valid_xml =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";

const char* missing_appid =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' />"
" </app>"
"</gupdate>";

const char* invalid_codebase =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' />"
" </app>"
"</gupdate>";

const char* missing_version =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx' />"
" </app>"
"</gupdate>";

const char* invalid_version =
"<?xml version='1.0'?>"
"<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>"
" <app appid='12345' status='ok'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx' "
"               version='1.2.3.a'/>"
" </app>"
"</gupdate>";

const char *uses_namespace_prefix =
"<?xml version='1.0' encoding='UTF-8'?>"
"<g:gupdate xmlns:g='http://www.google.com/update2/response' protocol='2.0'>"
" <g:app appid='12345'>"
"  <g:updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </g:app>"
"</g:gupdate>";

// Includes unrelated <app> tags from other xml namespaces - this should
// not cause problems.
const char *similar_tagnames =
"<?xml version='1.0' encoding='UTF-8'?>"
"<gupdate xmlns='http://www.google.com/update2/response'"
"         xmlns:a='http://a' protocol='2.0'>"
" <a:app/>"
" <b:app xmlns:b='http://b' />"
" <app appid='12345'>"
"  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
"               version='1.2.3.4' prodversionmin='2.0.143.0' />"
" </app>"
"</gupdate>";


// Do-nothing base class for further specialized test classes.
class MockService : public ExtensionUpdateService {
 public:
  MockService() {}
  virtual ~MockService() {}

  virtual const ExtensionList* extensions() const {
    EXPECT_TRUE(false);
    return NULL;
  }

  virtual void UpdateExtension(const std::string& id,
                               const FilePath& extension_path,
                               bool alert_on_error,
                               ExtensionInstallCallback* callback) {
    EXPECT_TRUE(false);
  }

  virtual Extension* GetExtensionById(const std::string& id) {
    EXPECT_TRUE(false);
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockService);
};


const char* kIdPrefix = "000000000000000000000000000000000000000";

// Creates test extensions and inserts them into list. The name and
// version are all based on their index.
void CreateTestExtensions(int count, ExtensionList *list) {
  for (int i = 1; i <= count; i++) {
    DictionaryValue input;
    Extension* e = new Extension();
    input.SetString(Extension::kVersionKey, StringPrintf("%d.0.0.0", i));
    input.SetString(Extension::kNameKey, StringPrintf("Extension %d", i));
    std::string error;
    EXPECT_TRUE(e->InitFromValue(input, false, &error));
    list->push_back(e);
  }
}

class ServiceForManifestTests : public MockService {
 public:
  ServiceForManifestTests() {}

  virtual ~ServiceForManifestTests() {}

  virtual Extension* GetExtensionById(const std::string& id) {
    for (ExtensionList::iterator iter = extensions_.begin();
        iter != extensions_.end(); ++iter) {
     if ((*iter)->id() == id) {
       return *iter;
     }
    }
    return NULL;
  }

  virtual const ExtensionList* extensions() const { return &extensions_; }

  void set_extensions(ExtensionList extensions) {
    extensions_ = extensions;
  }

 private:
  ExtensionList extensions_;
};

class ServiceForDownloadTests : public MockService {
 public:
  explicit ServiceForDownloadTests() : callback_(NULL) {}
  virtual ~ServiceForDownloadTests() {
    // expect that cleanup happened via FireInstallCallback
    EXPECT_EQ(NULL, callback_);
    EXPECT_TRUE(install_path_.empty());
  }

  virtual void UpdateExtension(const std::string& id,
                               const FilePath& extension_path,
                               bool alert_on_error,
                               ExtensionInstallCallback* callback) {
    // Since this mock only has support for one oustanding update, ensure
    // that the callback doesn't need to be run.
    EXPECT_TRUE(install_path_.empty());
    EXPECT_EQ(NULL, callback_);

    extension_id_ = id;
    install_path_ = extension_path;
    callback_ = callback;
  }

  void FireInstallCallback() {
    EXPECT_TRUE(callback_ != NULL);
    callback_->Run(install_path_, static_cast<Extension*>(NULL));
    delete callback_;
    callback_ = NULL;
    install_path_ = FilePath();
  }

  const std::string& extension_id() { return extension_id_; }
  const FilePath& install_path() { return install_path_; }

 private:
  std::string extension_id_;
  FilePath install_path_;
  ExtensionInstallCallback* callback_;
};

static const int kUpdateFrequencySecs = 15;

// All of our tests that need to use private APIs of ExtensionUpdater live
// inside this class (which is a friend to ExtensionUpdater).
class ExtensionUpdaterTest : public testing::Test {
 public:
  static void expectParseFailure(const char *xml) {
    ScopedVector<ExtensionUpdater::ParseResult> result;
    EXPECT_FALSE(ExtensionUpdater::Parse(xml, &result.get()));
  }

  // Make a test ParseResult
  static ExtensionUpdater::ParseResult* MakeParseResult(std::string id,
                                                        std::string version,
                                                        std::string url) {
    ExtensionUpdater::ParseResult *result = new ExtensionUpdater::ParseResult;
    result->extension_id = id;
    result->version.reset(Version::GetVersionFromString(version));
    result->crx_url = GURL(url);
    return result;
  }

  static void TestXmlParsing() {
    ExtensionErrorReporter::Init(false);

    // Test parsing of a number of invalid xml cases
    expectParseFailure("");
    expectParseFailure(missing_appid);
    expectParseFailure(invalid_codebase);
    expectParseFailure(missing_version);
    expectParseFailure(invalid_version);

    // Parse some valid XML, and check that all params came out as expected
    ScopedVector<ExtensionUpdater::ParseResult> results;
    EXPECT_TRUE(ExtensionUpdater::Parse(valid_xml, &results.get()));
    EXPECT_FALSE(results->empty());
    ExtensionUpdater::ParseResult* firstResult = results->at(0);
    EXPECT_EQ(GURL("http://example.com/extension_1.2.3.4.crx"),
              firstResult->crx_url);
    scoped_ptr<Version> version(Version::GetVersionFromString("1.2.3.4"));
    EXPECT_EQ(0, version->CompareTo(*firstResult->version.get()));
    version.reset(Version::GetVersionFromString("2.0.143.0"));
    EXPECT_EQ(0, version->CompareTo(*firstResult->browser_min_version.get()));

    // Parse some xml that uses namespace prefixes.
    results.reset();
    EXPECT_TRUE(ExtensionUpdater::Parse(uses_namespace_prefix, &results.get()));
    EXPECT_TRUE(ExtensionUpdater::Parse(similar_tagnames, &results.get()));
  }

  static void TestDetermineUpdates() {
    // Create a set of test extensions
    ServiceForManifestTests service;
    ExtensionList tmp;
    CreateTestExtensions(3, &tmp);
    service.set_extensions(tmp);

    MessageLoop message_loop;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, kUpdateFrequencySecs, &message_loop);

    // Check passing an empty list of parse results to DetermineUpdates
    ExtensionUpdater::ParseResultList updates;
    std::vector<int> updateable = updater->DetermineUpdates(updates);
    EXPECT_TRUE(updateable.empty());

    // Create two updates - expect that DetermineUpdates will return the first
    // one (v1.0 installed, v1.1 available) but not the second one (both
    // installed and available at v2.0).
    scoped_ptr<Version> one(Version::GetVersionFromString("1.0"));
    EXPECT_TRUE(tmp[0]->version()->Equals(*one));
    updates.push_back(MakeParseResult(tmp[0]->id(),
        "1.1", "http://localhost/e1_1.1.crx"));
    updates.push_back(MakeParseResult(tmp[1]->id(),
        tmp[1]->VersionString(), "http://localhost/e2_2.0.crx"));
    updateable = updater->DetermineUpdates(updates);
    EXPECT_EQ(1u, updateable.size());
    EXPECT_EQ(0, updateable[0]);
    STLDeleteElements(&updates);
  }

  static void TestMultipleManifestDownloading() {
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    MessageLoop message_loop;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, kUpdateFrequencySecs, &message_loop);

    GURL url1("http://localhost/manifest1");
    GURL url2("http://localhost/manifest2");

    // Request 2 update checks - the first should begin immediately and the
    // second one should be queued up.
    updater->StartUpdateCheck(url1);
    updater->StartUpdateCheck(url2);

    std::string manifest_data("invalid xml");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url1, URLRequestStatus(), 200, ResponseCookies(),
        manifest_data);

    // Now that the first request is complete, make sure the second one has
    // been started.
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kManifestFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url2, URLRequestStatus(), 200, ResponseCookies(),
        manifest_data);
  }

  static void TestSingleExtensionDownloading() {
    MessageLoop message_loop;
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, kUpdateFrequencySecs, &message_loop);

    GURL test_url("http://localhost/extension.crx");

    std::string id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    updater->FetchUpdatedExtension(id, test_url);

    // Call back the ExtensionUpdater with a 200 response and some test data
    std::string extension_data("whatever");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, test_url, URLRequestStatus(), 200, ResponseCookies(),
        extension_data);

    message_loop.RunAllPending();

    // Expect that ExtensionUpdater asked the mock extensions service to install
    // a file with the test data for the right id.
    EXPECT_EQ(id, service.extension_id());
    FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    std::string file_contents;
    EXPECT_TRUE(file_util::ReadFileToString(tmpfile_path, &file_contents));
    EXPECT_TRUE(extension_data == file_contents);

    service.FireInstallCallback();

    message_loop.RunAllPending();

    // Make sure the temp file is cleaned up
    EXPECT_FALSE(file_util::PathExists(tmpfile_path));

    URLFetcher::set_factory(NULL);
  }

  static void TestMultipleExtensionDownloading() {
    MessageLoopForUI message_loop;
    TestURLFetcherFactory factory;
    TestURLFetcher* fetcher = NULL;
    URLFetcher::set_factory(&factory);
    ServiceForDownloadTests service;
    scoped_refptr<ExtensionUpdater> updater =
      new ExtensionUpdater(&service, kUpdateFrequencySecs, &message_loop);

    GURL url1("http://localhost/extension1.crx");
    GURL url2("http://localhost/extension2.crx");

    std::string id1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    std::string id2 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    // Start two fetches
    updater->FetchUpdatedExtension(id1, url1);
    updater->FetchUpdatedExtension(id2, url2);

    // Make the first fetch complete.
    std::string extension_data1("whatever");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url1, URLRequestStatus(), 200, ResponseCookies(),
        extension_data1);
    message_loop.RunAllPending();

    // Expect that the service was asked to do an install with the right data,
    // and fire the callback indicating the install finished.
    FilePath tmpfile_path = service.install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    EXPECT_EQ(id1, service.extension_id());
    service.FireInstallCallback();

    // Make sure the tempfile got cleaned up.
    message_loop.RunAllPending();
    EXPECT_FALSE(tmpfile_path.empty());
    EXPECT_FALSE(file_util::PathExists(tmpfile_path));

    // Make sure the second fetch finished and asked the service to do an
    // update.
    std::string extension_data2("whatever2");
    fetcher = factory.GetFetcherByID(ExtensionUpdater::kExtensionFetcherId);
    EXPECT_TRUE(fetcher != NULL && fetcher->delegate() != NULL);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, url2, URLRequestStatus(), 200, ResponseCookies(),
        extension_data2);
    message_loop.RunAllPending();
    EXPECT_EQ(id2, service.extension_id());
    EXPECT_FALSE(service.install_path().empty());

    // Make sure the correct crx contents were passed for the update call.
    std::string file_contents;
    EXPECT_TRUE(file_util::ReadFileToString(service.install_path(),
                                            &file_contents));
    EXPECT_TRUE(extension_data2 == file_contents);
    service.FireInstallCallback();
    message_loop.RunAllPending();
  }
};

// Because we test some private methods of ExtensionUpdater, it's easer for the
// actual test code to live in ExtenionUpdaterTest methods instead of TEST_F
// subclasses where friendship with ExtenionUpdater is not inherited.

TEST(ExtensionUpdaterTest, TestXmlParsing) {
  ExtensionUpdaterTest::TestXmlParsing();
}

TEST(ExtensionUpdaterTest, TestDetermineUpdates) {
  ExtensionUpdaterTest::TestDetermineUpdates();
}

TEST(ExtensionUpdaterTest, TestMultipleManifestDownloading) {
  ExtensionUpdaterTest::TestMultipleManifestDownloading();
}

TEST(ExtensionUpdaterTest, TestSingleExtensionDownloading) {
  ExtensionUpdaterTest::TestSingleExtensionDownloading();
}

TEST(ExtensionUpdaterTest, TestMultipleExtensionDownloading) {
  ExtensionUpdaterTest::TestMultipleExtensionDownloading();
}


// TODO(asargent) - (http://crbug.com/12780) add tests for:
// -prodversionmin (shouldn't update if browser version too old)
// -manifests & updates arriving out of order / interleaved
// -Profile::GetDefaultRequestContext() returning null
//  (should not crash, but just do check later)
// -malformed update url (empty, file://, has query, has a # fragment, etc.)
// -An extension gets uninstalled while updates are in progress (so it doesn't
//  "come back from the dead")
// -An extension gets manually updated to v3 while we're downloading v2 (ie
//  you don't get downgraded accidentally)
// -An update manifest mentions multiple updates
