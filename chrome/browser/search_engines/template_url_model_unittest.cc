// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

// A Task used to coordinate when the database has finished processing
// requests. See note in BlockTillServiceProcessesRequests for details.
//
// When Run() schedules a QuitTask on the message loop it was created with.
class QuitTask2 : public Task {
 public:
  QuitTask2() : main_loop_(MessageLoop::current()) {}

  virtual void Run() {
    main_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  MessageLoop* main_loop_;
};

// Subclass the TestingProfile so that it can return a WebDataService.
class TemplateURLModelTestingProfile : public TestingProfile {
 public:
  TemplateURLModelTestingProfile() : TestingProfile() { }

  void SetUp() {
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.AppendASCII("TemplateURLModelTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);

    FilePath path = test_dir_.AppendASCII("TestDataService.db");
    service_ = new WebDataService;
    EXPECT_TRUE(service_->InitWithPath(path));
  }

  void TearDown() {
    // Clean up the test directory.
    service_->Shutdown();
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  virtual WebDataService* GetWebDataService(ServiceAccessType access) {
    return service_.get();
  }

 private:
  scoped_refptr<WebDataService> service_;
  FilePath test_dir_;
};

// Trivial subclass of TemplateURLModel that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLModel : public TemplateURLModel {
 public:
  explicit TestingTemplateURLModel(Profile* profile)
      : TemplateURLModel(profile) {
  }

  std::wstring GetAndClearSearchTerm() {
    std::wstring search_term;
    search_term.swap(search_term_);
    return search_term;
  }

 protected:
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const std::wstring& term) {
    search_term_ = term;
  }

 private:
  std::wstring search_term_;

  DISALLOW_EVIL_CONSTRUCTORS(TestingTemplateURLModel);
};

class TemplateURLModelTest : public testing::Test,
                             public TemplateURLModelObserver {
 public:
  TemplateURLModelTest() : changed_count_(0) {
  }

  virtual void SetUp() {
    profile_.reset(new TemplateURLModelTestingProfile());
    profile_->SetUp();
    model_.reset(new TestingTemplateURLModel(profile_.get()));
    model_->AddObserver(this);
  }

  virtual void TearDown() {
    profile_->TearDown();
    delete TemplateURLRef::google_base_url_;
    TemplateURLRef::google_base_url_ = NULL;

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }

  TemplateURL* AddKeywordWithDate(const std::wstring& keyword,
                                  bool autogenerate_keyword,
                                  const std::wstring& url,
                                  const std::wstring& short_name,
                                  bool safe_for_autoreplace,
                                  Time created_date) {
    TemplateURL* template_url = new TemplateURL();
    template_url->SetURL(url, 0, 0);
    template_url->set_keyword(keyword);
    template_url->set_autogenerate_keyword(autogenerate_keyword);
    template_url->set_short_name(short_name);
    template_url->set_date_created(created_date);
    template_url->set_safe_for_autoreplace(safe_for_autoreplace);
    model_->Add(template_url);
    EXPECT_NE(0, template_url->id());
    return template_url;
  }

  virtual void OnTemplateURLModelChanged() {
    changed_count_++;
  }

  void VerifyObserverCount(int expected_changed_count) {
    ASSERT_EQ(expected_changed_count, changed_count_);
    changed_count_ = 0;
  }

  // Blocks the caller until the service has finished servicing all pending
  // requests.
  void BlockTillServiceProcessesRequests() {
    // Schedule a task on the background thread that is processed after all
    // pending requests on the background thread.
    profile_->GetWebDataService(Profile::EXPLICIT_ACCESS)->thread()->
        message_loop()->PostTask(FROM_HERE, new QuitTask2());
    // Run the current message loop. QuitTask2, when run, invokes Quit,
    // which unblocks this.
    MessageLoop::current()->Run();
  }

  // Makes sure the load was successful and sent the correct notification.
  void VerifyLoad() {
    ASSERT_FALSE(model_->loaded());
    model_->Load();
    BlockTillServiceProcessesRequests();
    VerifyObserverCount(1);
    changed_count_ = 0;
  }

  // Creates a new TemplateURLModel.
  void ResetModel(bool verify_load) {
    model_.reset(new TestingTemplateURLModel(profile_.get()));
    model_->AddObserver(this);
    changed_count_ = 0;
    if (verify_load)
      VerifyLoad();
  }

  // Verifies the two TemplateURLs are equal.
  void AssertEquals(const TemplateURL& expected, const TemplateURL& actual) {
    ASSERT_EQ(expected.url()->url(), actual.url()->url());
    ASSERT_EQ(expected.keyword(), actual.keyword());
    ASSERT_EQ(expected.short_name(), actual.short_name());
    ASSERT_TRUE(expected.GetFavIconURL() == actual.GetFavIconURL());
    ASSERT_EQ(expected.id(), actual.id());
    ASSERT_EQ(expected.safe_for_autoreplace(), actual.safe_for_autoreplace());
    ASSERT_EQ(expected.show_in_default_list(), actual.show_in_default_list());
    ASSERT_TRUE(expected.date_created() == actual.date_created());
  }

  std::wstring GetAndClearSearchTerm() {
    return model_->GetAndClearSearchTerm();
  }

  void SetGoogleBaseURL(const std::wstring& base_url) const {
    delete TemplateURLRef::google_base_url_;
    TemplateURLRef::google_base_url_ = new std::wstring(base_url);
  }

  MessageLoopForUI message_loop_;
  scoped_ptr<TemplateURLModelTestingProfile> profile_;
  scoped_ptr<TestingTemplateURLModel> model_;
  int changed_count_;
};

TEST_F(TemplateURLModelTest, Load) {
  VerifyLoad();
}

TEST_F(TemplateURLModelTest, AddUpdateRemove) {
  // Add a new TemplateURL.
  VerifyLoad();
  const size_t initial_count = model_->GetTemplateURLs().size();

  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL(L"http://www.google.com/foo/bar", 0, 0);
  t_url->set_keyword(L"keyword");
  t_url->set_short_name(L"google");
  GURL favicon_url("http://favicon.url");
  t_url->SetFavIconURL(favicon_url);
  t_url->set_date_created(Time::FromTimeT(100));
  t_url->set_safe_for_autoreplace(true);
  model_->Add(t_url);
  ASSERT_TRUE(model_->CanReplaceKeyword(L"keyword", std::wstring(), NULL));
  VerifyObserverCount(1);
  BlockTillServiceProcessesRequests();
  // We need to clone as model takes ownership of TemplateURL and will
  // delete it.
  TemplateURL cloned_url(*t_url);
  ASSERT_EQ(1 + initial_count, model_->GetTemplateURLs().size());
  ASSERT_TRUE(model_->GetTemplateURLForKeyword(t_url->keyword()) == t_url);
  ASSERT_TRUE(t_url->date_created() == cloned_url.date_created());

  // Reload the model to verify it was actually saved to the database.
  ResetModel(true);
  ASSERT_EQ(1 + initial_count, model_->GetTemplateURLs().size());
  const TemplateURL* loaded_url = model_->GetTemplateURLForKeyword(L"keyword");
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(cloned_url, *loaded_url);
  ASSERT_TRUE(model_->CanReplaceKeyword(L"keyword", std::wstring(), NULL));

  // Mutate an element and verify it succeeded.
  model_->ResetTemplateURL(loaded_url, L"a", L"b", L"c");
  ASSERT_EQ(L"a", loaded_url->short_name());
  ASSERT_EQ(L"b", loaded_url->keyword());
  ASSERT_EQ(L"c", loaded_url->url()->url());
  ASSERT_FALSE(loaded_url->safe_for_autoreplace());
  ASSERT_TRUE(model_->CanReplaceKeyword(L"keyword", std::wstring(), NULL));
  ASSERT_FALSE(model_->CanReplaceKeyword(L"b", std::wstring(), NULL));
  cloned_url = *loaded_url;
  BlockTillServiceProcessesRequests();
  ResetModel(true);
  ASSERT_EQ(1 + initial_count, model_->GetTemplateURLs().size());
  loaded_url = model_->GetTemplateURLForKeyword(L"b");
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(cloned_url, *loaded_url);

  // Remove an element and verify it succeeded.
  model_->Remove(loaded_url);
  VerifyObserverCount(1);
  ResetModel(true);
  ASSERT_EQ(initial_count, model_->GetTemplateURLs().size());
  EXPECT_TRUE(model_->GetTemplateURLForKeyword(L"b") == NULL);
}

TEST_F(TemplateURLModelTest, GenerateKeyword) {
  ASSERT_EQ(L"", TemplateURLModel::GenerateKeyword(GURL(), true));
  // Shouldn't generate keywords for https.
  ASSERT_EQ(L"", TemplateURLModel::GenerateKeyword(GURL("https://blah"), true));
  ASSERT_EQ(L"foo", TemplateURLModel::GenerateKeyword(GURL("http://foo"),
                                                      true));
  // www. should be stripped.
  ASSERT_EQ(L"foo", TemplateURLModel::GenerateKeyword(GURL("http://www.foo"),
                                                      true));
  // Shouldn't generate keywords with paths, if autodetected.
  ASSERT_EQ(L"", TemplateURLModel::GenerateKeyword(GURL("http://blah/foo"),
                                                   true));
  ASSERT_EQ(L"blah", TemplateURLModel::GenerateKeyword(GURL("http://blah/foo"),
                                                       false));
  // FTP shouldn't generate a keyword.
  ASSERT_EQ(L"", TemplateURLModel::GenerateKeyword(GURL("ftp://blah/"), true));
  // Make sure we don't get a trailing /
  ASSERT_EQ(L"blah", TemplateURLModel::GenerateKeyword(GURL("http://blah/"),
                                                       true));
}

TEST_F(TemplateURLModelTest, ClearBrowsingData_Keywords) {
  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

  // Nothing has been added.
  EXPECT_EQ(0U, model_->GetTemplateURLs().size());

  // Create one with a 0 time.
  AddKeywordWithDate(L"key1", false, L"http://foo1", L"name1", true, Time());
  // Create one for now and +/- 1 day.
  AddKeywordWithDate(L"key2", false, L"http://foo2", L"name2", true,
                     now - one_day);
  AddKeywordWithDate(L"key3", false, L"http://foo3", L"name3", true, now);
  AddKeywordWithDate(L"key4", false, L"http://foo4", L"name4", true,
                     now + one_day);
  // Try the other three states.
  AddKeywordWithDate(L"key5", false, L"http://foo5", L"name5", false, now);
  AddKeywordWithDate(L"key6", false, L"http://foo6", L"name6", false, month_ago);

  // We just added a few items, validate them.
  EXPECT_EQ(6U, model_->GetTemplateURLs().size());

  // Try removing from current timestamp. This should delete the one in the
  // future and one very recent one.
  model_->RemoveAutoGeneratedSince(now);
  EXPECT_EQ(4U, model_->GetTemplateURLs().size());

  // Try removing from two months ago. This should only delete items that are
  // auto-generated.
  model_->RemoveAutoGeneratedSince(now - TimeDelta::FromDays(60));
  EXPECT_EQ(3U, model_->GetTemplateURLs().size());

  // Make sure the right values remain.
  EXPECT_EQ(L"key1", model_->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model_->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(0U, model_->GetTemplateURLs()[0]->date_created().ToInternalValue());

  EXPECT_EQ(L"key5", model_->GetTemplateURLs()[1]->keyword());
  EXPECT_FALSE(model_->GetTemplateURLs()[1]->safe_for_autoreplace());
  EXPECT_EQ(now.ToInternalValue(),
            model_->GetTemplateURLs()[1]->date_created().ToInternalValue());

  EXPECT_EQ(L"key6", model_->GetTemplateURLs()[2]->keyword());
  EXPECT_FALSE(model_->GetTemplateURLs()[2]->safe_for_autoreplace());
  EXPECT_EQ(month_ago.ToInternalValue(),
            model_->GetTemplateURLs()[2]->date_created().ToInternalValue());

  // Try removing from Time=0. This should delete one more.
  model_->RemoveAutoGeneratedSince(Time());
  EXPECT_EQ(2U, model_->GetTemplateURLs().size());
}

TEST_F(TemplateURLModelTest, Reset) {
  // Add a new TemplateURL.
  VerifyLoad();
  const size_t initial_count = model_->GetTemplateURLs().size();
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL(L"http://www.google.com/foo/bar", 0, 0);
  t_url->set_keyword(L"keyword");
  t_url->set_short_name(L"google");
  GURL favicon_url("http://favicon.url");
  t_url->SetFavIconURL(favicon_url);
  t_url->set_date_created(Time::FromTimeT(100));
  model_->Add(t_url);

  VerifyObserverCount(1);
  BlockTillServiceProcessesRequests();

  // Reset the short name, keyword, url and make sure it takes.
  const std::wstring new_short_name(L"a");
  const std::wstring new_keyword(L"b");
  const std::wstring new_url(L"c");
  model_->ResetTemplateURL(t_url, new_short_name, new_keyword, new_url);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(new_keyword, t_url->keyword());
  ASSERT_EQ(new_url, t_url->url()->url());

  // Make sure the mappings in the model were updated.
  ASSERT_TRUE(model_->GetTemplateURLForKeyword(new_keyword) == t_url);
  ASSERT_TRUE(model_->GetTemplateURLForKeyword(L"keyword") == NULL);

  TemplateURL last_url = *t_url;

  // Reload the model from the database and make sure the change took.
  ResetModel(true);
  t_url = NULL;
  EXPECT_EQ(initial_count + 1, model_->GetTemplateURLs().size());
  const TemplateURL* read_url = model_->GetTemplateURLForKeyword(new_keyword);
  ASSERT_TRUE(read_url);
  AssertEquals(last_url, *read_url);
}

TEST_F(TemplateURLModelTest, DefaultSearchProvider) {
  // Add a new TemplateURL.
  VerifyLoad();
  const size_t initial_count = model_->GetTemplateURLs().size();
  TemplateURL* t_url = AddKeywordWithDate(L"key1", false, L"http://foo1",
                                          L"name1", true, Time());

  changed_count_ = 0;
  model_->SetDefaultSearchProvider(t_url);

  ASSERT_EQ(t_url, model_->GetDefaultSearchProvider());

  ASSERT_TRUE(t_url->safe_for_autoreplace());
  ASSERT_TRUE(t_url->show_in_default_list());

  // Setting the default search provider should have caused notification.
  VerifyObserverCount(1);

  BlockTillServiceProcessesRequests();

  TemplateURL cloned_url = *t_url;

  ResetModel(true);
  t_url = NULL;

  // Make sure when we reload we get a default search provider.
  EXPECT_EQ(1 + initial_count, model_->GetTemplateURLs().size());
  ASSERT_TRUE(model_->GetDefaultSearchProvider());
  AssertEquals(cloned_url, *model_->GetDefaultSearchProvider());
}

TEST_F(TemplateURLModelTest, TemplateURLWithNoKeyword) {
  VerifyLoad();

  const size_t initial_count = model_->GetTemplateURLs().size();

  AddKeywordWithDate(std::wstring(), false, L"http://foo1", L"name1", true,
                     Time());

  // We just added a few items, validate them.
  ASSERT_EQ(initial_count + 1, model_->GetTemplateURLs().size());

  // Reload the model from the database and make sure we get the url back.
  ResetModel(true);

  ASSERT_EQ(1 + initial_count, model_->GetTemplateURLs().size());

  bool found_keyword = false;
  for (size_t i = 0; i < initial_count + 1; ++i) {
    if (model_->GetTemplateURLs()[i]->keyword().empty()) {
      found_keyword = true;
      break;
    }
  }
  ASSERT_TRUE(found_keyword);
}

TEST_F(TemplateURLModelTest, CantReplaceWithSameKeyword) {
  ASSERT_TRUE(model_->CanReplaceKeyword(L"foo", std::wstring(), NULL));
  TemplateURL* t_url = AddKeywordWithDate(L"foo", false, L"http://foo1",
                                          L"name1", true, Time());

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model_->CanReplaceKeyword(L"foo", L"http://foo2", NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model_->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                           t_url->url()->url());

  ASSERT_FALSE(model_->CanReplaceKeyword(L"foo", L"http://foo2", NULL));
}

TEST_F(TemplateURLModelTest, CantReplaceWithSameHosts) {
  ASSERT_TRUE(model_->CanReplaceKeyword(L"foo", L"http://foo.com", NULL));
  TemplateURL* t_url = AddKeywordWithDate(L"foo", false, L"http://foo.com",
                                          L"name1", true, Time());

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model_->CanReplaceKeyword(L"bar", L"http://foo.com", NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model_->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                           t_url->url()->url());

  ASSERT_FALSE(model_->CanReplaceKeyword(L"bar", L"http://foo.com", NULL));
}

TEST_F(TemplateURLModelTest, HasDefaultSearchProvider) {
  // We should have a default search provider even if we haven't loaded.
  ASSERT_TRUE(model_->GetDefaultSearchProvider());

  // Now force the model to load and make sure we still have a default.
  VerifyLoad();

  ASSERT_TRUE(model_->GetDefaultSearchProvider());
}

TEST_F(TemplateURLModelTest, DefaultSearchProviderLoadedFromPrefs) {
  VerifyLoad();

  TemplateURL* template_url = new TemplateURL();
  template_url->SetURL(L"http://url", 0, 0);
  template_url->SetSuggestionsURL(L"http://url2", 0, 0);
  template_url->set_short_name(L"a");
  template_url->set_safe_for_autoreplace(true);
  template_url->set_date_created(Time::FromTimeT(100));

  model_->Add(template_url);

  const TemplateURL::IDType id = template_url->id();

  model_->SetDefaultSearchProvider(template_url);

  BlockTillServiceProcessesRequests();

  TemplateURL first_default_search_provider = *template_url;

  template_url = NULL;

  // Reset the model and don't load it. The template url we set as the default
  // should be pulled from prefs now.
  ResetModel(false);

  // NOTE: This doesn't use AssertEquals as only a subset of the TemplateURLs
  // value are persisted to prefs.
  const TemplateURL* default_turl = model_->GetDefaultSearchProvider();
  ASSERT_TRUE(default_turl);
  ASSERT_TRUE(default_turl->url());
  ASSERT_EQ(L"http://url", default_turl->url()->url());
  ASSERT_TRUE(default_turl->suggestions_url());
  ASSERT_EQ(L"http://url2", default_turl->suggestions_url()->url());
  ASSERT_EQ(L"a", default_turl->short_name());
  ASSERT_EQ(id, default_turl->id());

  // Now do a load and make sure the default search provider really takes.
  VerifyLoad();

  ASSERT_TRUE(model_->GetDefaultSearchProvider());
  AssertEquals(first_default_search_provider,
               *model_->GetDefaultSearchProvider());
}

TEST_F(TemplateURLModelTest, BuildQueryTerms) {
  struct TestData {
    const std::string url;
    const bool result;
    // Keys and values are a semicolon separated list of expected values in the
    // map.
    const std::string keys;
    const std::string values;
  } data[] = {
    // No query should return false.
    { "http://blah/", false, "", "" },

    // Query with empty key should return false.
    { "http://blah/foo?=y", false, "", "" },

    // Query with key occurring multiple times should return false.
    { "http://blah/foo?x=y&x=z", false, "", "" },

    { "http://blah/foo?x=y", true, "x", "y" },
    { "http://blah/foo?x=y&y=z", true, "x;y", "y;z" },

    // Key occurring multiple times should get an empty string.
    { "http://blah/foo?x=y&x=z&y=z", true, "x;y", ";z" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    TemplateURLModel::QueryTerms terms;
    ASSERT_EQ(data[i].result,
              TemplateURLModel::BuildQueryTerms(GURL(data[i].url), &terms));
    if (data[i].result) {
      std::vector<std::string> keys;
      std::vector<std::string> values;
      SplitString(data[i].keys, ';', &keys);
      SplitString(data[i].values, ';', &values);
      ASSERT_TRUE(keys.size() == values.size());
      ASSERT_EQ(keys.size(), terms.size());
      for (size_t j = 0; j < keys.size(); ++j) {
        TemplateURLModel::QueryTerms::iterator term_iterator =
            terms.find(keys[j]);
        ASSERT_TRUE(term_iterator != terms.end());
        ASSERT_EQ(values[j], term_iterator->second);
      }
    }
  }
}

TEST_F(TemplateURLModelTest, UpdateKeywordSearchTermsForURL) {
  struct TestData {
    const std::string url;
    const std::wstring term;
  } data[] = {
    { "http://foo/", L"" },
    { "http://foo/foo?q=xx", L"" },
    { "http://x/bar?q=xx", L"" },
    { "http://x/foo?y=xx", L"" },
    { "http://x/foo?q=xx", L"xx" },
    { "http://x/foo?a=b&q=xx", L"xx" },
    { "http://x/foo?q=b&q=xx", L"" },
  };

  AddKeywordWithDate(L"x", false, L"http://x/foo?q={searchTerms}", L"name",
                     false, Time());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLVisitedDetails details;
    details.row = history::URLRow(GURL(data[i].url));
    details.transition = 0;
    model_->UpdateKeywordSearchTermsForURL(details);
    EXPECT_EQ(data[i].term, GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLModelTest, DontUpdateKeywordSearchForNonReplaceable) {
  struct TestData {
    const std::string url;
  } data[] = {
    { "http://foo/" },
    { "http://x/bar?q=xx" },
    { "http://x/foo?y=xx" },
  };

  AddKeywordWithDate(L"x", false, L"http://x/foo", L"name", false, Time());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLVisitedDetails details;
    details.row = history::URLRow(GURL(data[i].url));
    details.transition = 0;
    model_->UpdateKeywordSearchTermsForURL(details);
    ASSERT_EQ(std::wstring(), GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLModelTest, ChangeGoogleBaseValue) {
  // NOTE: Do not do a VerifyLoad() here as it will load the prepopulate data,
  // which also has a {google:baseURL} keyword in it, which will confuse this
  // test.
  SetGoogleBaseURL(L"http://google.com/");
  const TemplateURL* t_url = AddKeywordWithDate(std::wstring(), true,
      L"{google:baseURL}?q={searchTerms}", L"name", false, Time());
  ASSERT_EQ(t_url, model_->GetTemplateURLForHost("google.com"));
  EXPECT_EQ("google.com", t_url->url()->GetHost());
  EXPECT_EQ(L"google.com", t_url->keyword());

  // Change the Google base url.
  model_->loaded_ = true;  // Hack to make sure we get notified of the base URL
                           // changing.
  SetGoogleBaseURL(L"http://foo.com/");
  model_->GoogleBaseURLChanged();
  VerifyObserverCount(1);

  // Make sure the host->TemplateURL map was updated appropriately.
  ASSERT_EQ(t_url, model_->GetTemplateURLForHost("foo.com"));
  EXPECT_TRUE(model_->GetTemplateURLForHost("google.com") == NULL);
  EXPECT_EQ("foo.com", t_url->url()->GetHost());
  EXPECT_EQ(L"foo.com", t_url->keyword());
  EXPECT_EQ(L"http://foo.com/?q=x", t_url->url()->ReplaceSearchTerms(*t_url,
      L"x", TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
}

struct QueryHistoryCallbackImpl {
  QueryHistoryCallbackImpl() : success(false) {}

  void Callback(HistoryService::Handle handle,
                bool success, const history::URLRow* row,
                history::VisitVector* visits) {
    this->success = success;
    if (row)
      this->row = *row;
    if (visits)
      this->visits = *visits;
  }

  bool success;
  history::URLRow row;
  history::VisitVector visits;
};

// Make sure TemplateURLModel generates a KEYWORD_GENERATED visit for
// KEYWORD visits.
TEST_F(TemplateURLModelTest, GenerateVisitOnKeyword) {
  VerifyLoad();
  profile_->CreateHistoryService(true);

  // Create a keyword.
  TemplateURL* t_url = AddKeywordWithDate(
      L"keyword", false, L"http://foo.com/foo?query={searchTerms}",
      L"keyword", true, base::Time::Now());

  // Add a visit that matches the url of the keyword.
  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history->AddPage(
      GURL(WideToUTF8(t_url->url()->ReplaceSearchTerms(*t_url, L"blah", 0,
                                                       std::wstring()))),
      NULL, 0, GURL(), PageTransition::KEYWORD, history::RedirectList(),
      false);

  // Wait for history to finish processing the request.
  profile_->BlockUntilHistoryProcessesPendingRequests();

  // Query history for the generated url.
  CancelableRequestConsumer consumer;
  QueryHistoryCallbackImpl callback;
  history->QueryURL(GURL("http://keyword"), true, &consumer,
      NewCallback(&callback, &QueryHistoryCallbackImpl::Callback));

  // Wait for the request to be processed.
  profile_->BlockUntilHistoryProcessesPendingRequests();

  // And make sure the url and visit were added.
  EXPECT_TRUE(callback.success);
  EXPECT_NE(0, callback.row.id());
  ASSERT_EQ(1U, callback.visits.size());
  EXPECT_EQ(PageTransition::KEYWORD_GENERATED,
            PageTransition::StripQualifier(callback.visits[0].transition));
}
