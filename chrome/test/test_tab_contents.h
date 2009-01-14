// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TEST_TEST_TAB_CONTENTS_H_
#define CHROME_BROWSER_TEST_TEST_TAB_CONTENTS_H_

#include "base/string_util.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_factory.h"

// TabContents typed created by TestTabContentsFactory.
class TestTabContents : public TabContents {
 public:
  BEGIN_MSG_MAP(TestTabContents)
  END_MSG_MAP()

  explicit TestTabContents(TabContentsType type);

  // Sets the site instance used by *ALL* TestTabContents.
  static void set_site_instance(SiteInstance* site_instance) {
    site_instance_ = site_instance;
  }

  // Sets whether we commit the load on NavigateToPendingEntry. The default is
  // false.
  void set_commit_on_navigate(bool commit_on_navigate) {
    commit_on_navigate_ = commit_on_navigate;
  }

  // Overridden from TabContents so we can provide a non-NULL site instance in
  // some cases. To use, the test will have to set the site_instance_ member
  // variable to some site instance it creates.
  virtual SiteInstance* GetSiteInstance() const {
    return site_instance_;
  }

  // Does one of two things. If |commit_on_navigate| is true the navigation is
  // committed immediately. Otherwise this returns true and you must invoke
  // CompleteNavigationAsRenderer when you want to commit the load.
  virtual bool NavigateToPendingEntry(bool reload);

  // Sets up a call to RendererDidNavigate pretending to be a main frame
  // navigation to the given URL.
  void CompleteNavigationAsRenderer(int page_id, const GURL& url);

 private:
  static SiteInstance* site_instance_;

  bool commit_on_navigate_;
  int next_page_id_;

  DISALLOW_COPY_AND_ASSIGN(TestTabContents);
};

// TestTabContentsFactory is a TabContentsFactory that can be used for tests.
//
// Use the CreateAndRegisterFactory method to create and register a new
// TestTabContentsFactory. You can use scheme() to determine the resulting
// scheme and type() for the resulting TabContentsType.
//
// TestTabContentsFactory unregisters itself from the TabContentsFactory in its
// destructor.
class TestTabContentsFactory : public TabContentsFactory {
 public:
  // Creates a new TestTabContentsFactory and registers it for the next
  // free TabContentsType. The destructor unregisters the factory.
  static TestTabContentsFactory* CreateAndRegisterFactory() {
    TabContentsType new_type = TabContentsFactory::NextUnusedType();
    TestTabContentsFactory* new_factory =
        new TestTabContentsFactory(
            new_type, "test" + IntToString(static_cast<int>(new_type)));
    TabContents::RegisterFactory(new_type, new_factory);
    return new_factory;
  }

  TestTabContentsFactory(TabContentsType type, const std::string& scheme)
      : type_(type),
        scheme_(scheme) {
  }

  ~TestTabContentsFactory() {
    TabContents::RegisterFactory(type_, NULL);
  }

  virtual TabContents* CreateInstance() {
    return CreateInstanceImpl();
  }

  TestTabContents* CreateInstanceImpl() {
    return new TestTabContents(type_);
  }

  virtual bool CanHandleURL(const GURL& url) {
    return url.SchemeIs(scheme_.c_str());
  }

  const std::string& scheme() const { return scheme_; }

  GURL test_url_with_path(const std::string& path) const {
    return GURL(scheme() + ":" + path);
  }

  TabContentsType type() const { return type_; }

 private:
  TabContentsType type_;

  const std::string scheme_;

  DISALLOW_COPY_AND_ASSIGN(TestTabContentsFactory);
};

#endif  // CHROME_BROWSER_TEST_TEST_TAB_CONTENTS_H_
