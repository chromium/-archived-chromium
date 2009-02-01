// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printed_pages_source.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestSource : public printing::PrintedPagesSource {
 public:
  virtual void RenderOnePrintedPage(printing::PrintedDocument* document,
                                    int page_number) {
  }
  virtual std::wstring RenderSourceName() {
    return L"";
  }
  virtual GURL RenderSourceUrl() {
    return GURL();
  }
};

class TestPrintJob : public printing::PrintJob {
 public:
  TestPrintJob(printing::PrintedPagesSource* source, volatile bool* check)
      : printing::PrintJob(source), check_(check) {
  }
  TestPrintJob(volatile bool* check) : check_(check) {
  }
  ~TestPrintJob() {
    *check_ = true;
  }
 private:
  volatile bool* check_;
};

class TestPrintNotifObserv : public NotificationObserver {
 public:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ASSERT_EQ(NotificationType::PRINT_JOB_EVENT, type.value);
    printing::JobEventDetails::Type event_type =
        Details<printing::JobEventDetails>(details)->type();
    EXPECT_NE(printing::JobEventDetails::NEW_DOC, event_type);
    EXPECT_NE(printing::JobEventDetails::NEW_PAGE, event_type);
    EXPECT_NE(printing::JobEventDetails::PAGE_DONE, event_type);
    EXPECT_NE(printing::JobEventDetails::DOC_DONE, event_type);
    EXPECT_NE(printing::JobEventDetails::JOB_DONE, event_type);
    EXPECT_NE(printing::JobEventDetails::ALL_PAGES_REQUESTED, event_type);
    if (event_type == printing::JobEventDetails::USER_INIT_DONE ||
        event_type == printing::JobEventDetails::USER_INIT_CANCELED ||
        event_type == printing::JobEventDetails::DEFAULT_INIT_DONE ||
        event_type == printing::JobEventDetails::FAILED) {
      MessageLoop::current()->Quit();
      return;
    }
  }
};

}  // namespace

TEST(PrintJobTest, SimplePrint) {
  // Test the multithreaded nature of PrintJob to make sure we can use it with
  // known livetime.
  TestPrintNotifObserv observ;
  MessageLoop current;
  NotificationService::current()->AddObserver(
      &observ, NotificationType::ALL,
      NotificationService::AllSources());
  TestSource source;
  volatile bool check = false;
  scoped_refptr<printing::PrintJob> job(new TestPrintJob(&source, &check));
  job->GetSettings(printing::PrintJob::DEFAULTS, NULL);
  EXPECT_EQ(MessageLoop::current(), job->message_loop());
  current.Run();
  job->Stop();
  job = NULL;
  EXPECT_TRUE(check);
  NotificationService::current()->RemoveObserver(
      &observ, NotificationType::ALL,
      NotificationService::AllSources());
}

TEST(PrintJobTest, SimplePrintLateInit) {
  volatile bool check = false;
  MessageLoop current;
  scoped_refptr<printing::PrintJob> job(new TestPrintJob(&check));
  job = NULL;
  EXPECT_TRUE(check);
  /* TODO(maruel): Test these.
  job->Initialize()
  job->Observe();
  job->GetSettingsDone();
  job->DetachWorker();
  job->message_loop();
  job->settings();
  job->cookie();
  job->GetSettings(printing::DEFAULTS, printing::ASK_USER, NULL);
  job->StartPrinting();
  job->Stop();
  job->Cancel();
  job->RequestMissingPages();
  job->FlushJob(timeout_ms);
  job->DisconnectSource();
  job->is_job_pending();
  job->is_print_dialog_box_shown();
  job->document();
  // Private
  job->UpdatePrintedDocument(NULL);
  scoped_refptr<printing::JobEventDetails> event_details;
  job->OnNotifyPrintJobEvent(event_details);
  job->OnDocumentDone();
  job->ControlledWorkerShutdown();
  */
}
