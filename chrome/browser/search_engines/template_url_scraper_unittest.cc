// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_util.h"

namespace {
class TemplateURLScraperTest : public InProcessBrowserTest {
 public:
  TemplateURLScraperTest() {
  }

 protected:
  virtual void ConfigureHostMapper(net::RuleBasedHostMapper* host_mapper) {
    InProcessBrowserTest::ConfigureHostMapper(host_mapper);
    // We use foo.com in our tests.
    host_mapper->AddRule("*.foo.com", "localhost");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLScraperTest);
};

class TemplateURLModelLoader : public NotificationObserver {
 public:
  explicit TemplateURLModelLoader(TemplateURLModel* model) : model_(model) {
    registrar_.Add(this, NotificationType::TEMPLATE_URL_MODEL_LOADED,
                   Source<TemplateURLModel>(model));
    model_->Load();
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::TEMPLATE_URL_MODEL_LOADED &&
        Source<TemplateURLModel>(source).ptr() == model_) {
      MessageLoop::current()->Quit();
    }
  }

 private:
  NotificationRegistrar registrar_;

  TemplateURLModel* model_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLModelLoader);
};

}  // namespace

/*
IN_PROC_BROWSER_TEST_F(TemplateURLScraperTest, ScrapeWithOnSubmit) {
  TemplateURLModel* template_urls = browser()->profile()->GetTemplateURLModel();
  TemplateURLModelLoader loader(template_urls);

  std::vector<const TemplateURL*> all_urls = template_urls->GetTemplateURLs();

  // We need to substract the default pre-populated engines that the profile is
  // set up with.
  size_t default_index = 0;
  std::vector<TemplateURL*> prepopulate_urls;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(
      browser()->profile()->GetPrefs(),
      &prepopulate_urls,
      &default_index);

  EXPECT_EQ(prepopulate_urls.size(), all_urls.size());

  scoped_refptr<HTTPTestServer> server(
      HTTPTestServer::CreateServerWithFileRootURL(
          L"chrome/test/data/template_url_scraper/submit_handler", L"/",
          g_browser_process->io_thread()->message_loop()));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.foo.com:1337/"), 2);

  all_urls = template_urls->GetTemplateURLs();
  EXPECT_EQ(1, all_urls.size() - prepopulate_urls.size());
}
*/
