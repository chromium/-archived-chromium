// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_view_host.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SiteInstanceTest : public testing::Test {
 private:
  MessageLoopForUI message_loop_;
};

class TestBrowsingInstance : public BrowsingInstance {
 public:
  TestBrowsingInstance(Profile* profile, int* deleteCounter)
      : BrowsingInstance(profile),
        deleteCounter_(deleteCounter),
        use_process_per_site(false) {}
  ~TestBrowsingInstance() {
    (*deleteCounter_)++;
  }

  // Overrides BrowsingInstance::ShouldUseProcessPerSite so that we can test
  // both alternatives without using command-line switches.
  bool ShouldUseProcessPerSite(const GURL& url) {
    return use_process_per_site;
  }

  // Set by individual tests.
  bool use_process_per_site;

 private:
  int* deleteCounter_;
};


class TestSiteInstance : public SiteInstance {
 public:
  static TestSiteInstance* CreateTestSiteInstance(Profile* profile,
                                                  int* siteDeleteCounter,
                                                  int* browsingDeleteCounter) {
    TestBrowsingInstance* browsing_instance =
      new TestBrowsingInstance(profile, browsingDeleteCounter);
    return new TestSiteInstance(browsing_instance, siteDeleteCounter);
  }

 private:
  TestSiteInstance(BrowsingInstance* browsing_instance, int* deleteCounter)
    : SiteInstance(browsing_instance), deleteCounter_(deleteCounter) {}
  ~TestSiteInstance() {
    (*deleteCounter_)++;
  }

  int* deleteCounter_;
};

}  // namespace

// Test to ensure no memory leaks for SiteInstance objects.
TEST_F(SiteInstanceTest, SiteInstanceDestructor) {
  int siteDeleteCounter = 0;
  int browsingDeleteCounter = 0;
  const GURL url("test:foo");

  // Ensure that instances are deleted when their NavigationEntries are gone.
  TestSiteInstance* instance =
      TestSiteInstance::CreateTestSiteInstance(NULL, &siteDeleteCounter,
                                               &browsingDeleteCounter);
  EXPECT_EQ(0, siteDeleteCounter);

  NavigationEntry* e1 = new NavigationEntry(TAB_CONTENTS_WEB, instance, 0, url,
                                            GURL(),
                                            std::wstring(),
                                            PageTransition::LINK);

  // Redundantly setting e1's SiteInstance shouldn't affect the ref count.
  e1->set_site_instance(instance);
  EXPECT_EQ(0, siteDeleteCounter);

  // Add a second reference
  NavigationEntry* e2 = new NavigationEntry(TAB_CONTENTS_WEB, instance, 0, url,
                                            GURL(), std::wstring(), 
                                            PageTransition::LINK);

  // Now delete both entries and be sure the SiteInstance goes away.
  delete e1;
  EXPECT_EQ(0, siteDeleteCounter);
  EXPECT_EQ(0, browsingDeleteCounter);
  delete e2;
  EXPECT_EQ(1, siteDeleteCounter);
  // instance is now deleted
  EXPECT_EQ(1, browsingDeleteCounter);
  // browsing_instance is now deleted

  // Ensure that instances are deleted when their RenderViewHosts are gone.
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  instance =
      TestSiteInstance::CreateTestSiteInstance(profile.get(),
                                               &siteDeleteCounter,
                                               &browsingDeleteCounter);
  WebContents* contents = new WebContents(
      profile.get(), instance, NULL, MSG_ROUTING_NONE, NULL);
  contents->SetupController(profile.get());
  EXPECT_EQ(1, siteDeleteCounter);
  EXPECT_EQ(1, browsingDeleteCounter);

  contents->CloseContents();
  // Make sure that we flush any messages related to WebContents destruction.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(2, siteDeleteCounter);
  EXPECT_EQ(2, browsingDeleteCounter);
  // contents is now deleted, along with instance and browsing_instance
}

// Test that NavigationEntries with SiteInstances can be cloned, but that their
// SiteInstances can be changed afterwards.  Also tests that the ref counts are
// updated properly after the change.
TEST_F(SiteInstanceTest, CloneNavigationEntry) {
  int siteDeleteCounter1 = 0;
  int siteDeleteCounter2 = 0;
  int browsingDeleteCounter = 0;
  const GURL url("test:foo");

  SiteInstance* instance1 =
      TestSiteInstance::CreateTestSiteInstance(NULL, &siteDeleteCounter1,
                                               &browsingDeleteCounter);
  SiteInstance* instance2 =
      TestSiteInstance::CreateTestSiteInstance(NULL, &siteDeleteCounter2,
                                               &browsingDeleteCounter);

  NavigationEntry* e1 = new NavigationEntry(TAB_CONTENTS_WEB, instance1, 0,
                                            url, GURL(),
                                            std::wstring(),
                                            PageTransition::LINK);
  // Clone the entry
  NavigationEntry* e2 = new NavigationEntry(*e1);

  // Should be able to change the SiteInstance of the cloned entry.
  e2->set_site_instance(instance2);

  // The first SiteInstance should go away after deleting e1, since e2 should
  // no longer be referencing it.
  delete e1;
  EXPECT_EQ(1, siteDeleteCounter1);
  EXPECT_EQ(0, siteDeleteCounter2);

  // The second SiteInstance should go away after deleting e2.
  delete e2;
  EXPECT_EQ(1, siteDeleteCounter1);
  EXPECT_EQ(1, siteDeleteCounter2);

  // Both BrowsingInstances are also now deleted
  EXPECT_EQ(2, browsingDeleteCounter);
}

// Test to ensure UpdateMaxPageID is working properly.
TEST_F(SiteInstanceTest, UpdateMaxPageID) {
  scoped_refptr<SiteInstance> instance(SiteInstance::CreateSiteInstance(NULL));
  EXPECT_EQ(-1, instance.get()->max_page_id());

  // Make sure max_page_id_ is monotonically increasing.
  instance.get()->UpdateMaxPageID(3);
  instance.get()->UpdateMaxPageID(1);
  EXPECT_EQ(3, instance.get()->max_page_id());
}

// Test to ensure GetProcess returns and creates processes correctly.
TEST_F(SiteInstanceTest, GetProcess) {
  // Ensure that GetProcess returns the process based on its host id.
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  scoped_ptr<BrowserRenderProcessHost> host1(
      new BrowserRenderProcessHost(profile.get()));
  scoped_refptr<SiteInstance> instance(
      SiteInstance::CreateSiteInstance(profile.get()));
  instance.get()->set_process_host_id(host1.get()->host_id());
  EXPECT_EQ(host1.get(), instance.get()->GetProcess());

  // Ensure that GetProcess creates a new process if no host id is set.
  scoped_refptr<SiteInstance> instance2(
      SiteInstance::CreateSiteInstance(profile.get()));
  scoped_ptr<RenderProcessHost> host2(instance2.get()->GetProcess());
  EXPECT_TRUE(host2.get() != NULL);
  EXPECT_NE(host1.get(), host2.get());
}

// Test to ensure SetSite and site() work properly.
TEST_F(SiteInstanceTest, SetSite) {
  scoped_refptr<SiteInstance> instance(SiteInstance::CreateSiteInstance(NULL));
  EXPECT_FALSE(instance->has_site());
  EXPECT_TRUE(instance.get()->site().is_empty());

  instance.get()->SetSite(GURL("http://www.google.com/index.html"));
  EXPECT_EQ(GURL("http://google.com"), instance.get()->site());

  EXPECT_TRUE(instance->has_site());
}

// Test to ensure GetSiteForURL properly returns sites for URLs.
TEST_F(SiteInstanceTest, GetSiteForURL) {
  // Pages are irrelevant.
  GURL test_url = GURL("http://www.google.com/index.html");
  EXPECT_EQ(GURL("http://google.com"), SiteInstance::GetSiteForURL(test_url));

  // Ports are irrlevant.
  test_url = GURL("https://www.google.com:8080");
  EXPECT_EQ(GURL("https://google.com"), SiteInstance::GetSiteForURL(test_url));

  // Javascript URLs have no site.
  test_url = GURL("javascript:foo();");
  EXPECT_EQ(GURL::EmptyGURL(), SiteInstance::GetSiteForURL(test_url));

  test_url = GURL("http://foo/a.html");
  EXPECT_EQ(GURL("http://foo"), SiteInstance::GetSiteForURL(test_url));

  test_url = GURL("file:///C:/Downloads/");
  EXPECT_EQ(GURL::EmptyGURL(), SiteInstance::GetSiteForURL(test_url));

  // TODO(creis): Do we want to special case file URLs to ensure they have
  // either no site or a special "file://" site?  We currently return
  // "file://home/" as the site, which seems broken.
  // test_url = GURL("file://home/");
  // EXPECT_EQ(GURL::EmptyGURL(), SiteInstance::GetSiteForURL(test_url));
}

// Test of distinguishing URLs from different sites.  Most of this logic is
// tested in RegistryControlledDomainTest.  This test focuses on URLs with
// different schemes or ports.
TEST_F(SiteInstanceTest, IsSameWebSite) {
  GURL url_foo = GURL("http://foo/a.html");
  GURL url_foo2 = GURL("http://foo/b.html");
  GURL url_foo_https = GURL("https://foo/a.html");
  GURL url_foo_port = GURL("http://foo:8080/a.html");
  GURL url_javascript = GURL("javascript:alert(1);");
  GURL url_crash = GURL("about:crash");
  GURL url_hang = GURL("about:hang");
  GURL url_shorthang = GURL("about:shorthang");

  // Same scheme and port -> same site.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_foo, url_foo2));

  // Different scheme -> different site.
  EXPECT_FALSE(SiteInstance::IsSameWebSite(url_foo, url_foo_https));

  // Different port -> same site.
  // (Changes to document.domain make renderer ignore the port.)
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_foo, url_foo_port));

  // JavaScript links should be considered same site for anything.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_javascript, url_foo));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_javascript, url_foo_https));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_javascript, url_foo_port));

  // The crash/hang URLs should also be treated as same site.  (Bug 1143809.)
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_crash, url_foo));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_hang, url_foo));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(url_shorthang, url_foo));
}

// Test to ensure that there is only one SiteInstance per site in a given
// BrowsingInstance, when process-per-site is not in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSite) {
  int deleteCounter = 0;
  TestBrowsingInstance* browsing_instance =
      new TestBrowsingInstance(NULL, &deleteCounter);
  browsing_instance->use_process_per_site = false;

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstance> site_instance_a1(
      browsing_instance->GetSiteInstanceForURL(url_a1));
  EXPECT_TRUE(site_instance_a1.get() != NULL);

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstance> site_instance_b1(
      browsing_instance->GetSiteInstanceForURL(url_b1));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1.get()->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(url_a2));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1.get()->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same or different
  // profile) should return a different SiteInstance.
  TestBrowsingInstance* browsing_instance2 =
      new TestBrowsingInstance(NULL, &deleteCounter);
  browsing_instance2->use_process_per_site = false;
  // Ensure the new SiteInstance is ref counted so that it gets deleted.
  scoped_refptr<SiteInstance> site_instance_a2_2(
      browsing_instance2->GetSiteInstanceForURL(url_a2));
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_2.get());

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.yahoo.com")));

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GURL("https://www.google.com")));
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(
      GURL("http://www.yahoo.com")));

  // browsing_instances will be deleted when their SiteInstances are deleted
}

// Test to ensure that there is only one SiteInstance per site for an entire
// Profile, if process-per-site is in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSiteInProfile) {
  int deleteCounter = 0;
  TestBrowsingInstance* browsing_instance =
      new TestBrowsingInstance(NULL, &deleteCounter);
  browsing_instance->use_process_per_site = true;

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstance> site_instance_a1(
      browsing_instance->GetSiteInstanceForURL(url_a1));
  EXPECT_TRUE(site_instance_a1.get() != NULL);

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstance> site_instance_b1(
      browsing_instance->GetSiteInstanceForURL(url_b1));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1.get()->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(url_a2));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1.get()->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same profile)
  // should also return the same SiteInstance.
  // This BrowsingInstance doesn't get its own SiteInstance within the test, so
  // it won't be deleted by its children.  Thus, we'll keep a ref count to it
  // to make sure it gets deleted.
  scoped_refptr<TestBrowsingInstance> browsing_instance2(
      new TestBrowsingInstance(NULL, &deleteCounter));
  browsing_instance2->use_process_per_site = true;
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance2->GetSiteInstanceForURL(url_a2));

  // A visit to the original site in a new BrowsingInstance (different profile)
  // should return a different SiteInstance.
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  TestBrowsingInstance* browsing_instance3 =
      new TestBrowsingInstance(profile.get(), &deleteCounter);
  browsing_instance3->use_process_per_site = true;
  // Ensure the new SiteInstance is ref counted so that it gets deleted.
  scoped_refptr<SiteInstance> site_instance_a2_3(
      browsing_instance3->GetSiteInstanceForURL(url_a2));
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_3.get());

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.yahoo.com")));  // visited before
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://www.yahoo.com")));  // different BI, but same profile

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GURL("https://www.google.com")));  // not visited before
  EXPECT_FALSE(browsing_instance3->HasSiteInstance(
      GURL("http://www.yahoo.com")));  // different BI, different profile

  // browsing_instances will be deleted when their SiteInstances are deleted
}

