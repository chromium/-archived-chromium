// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include "base/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#if defined (OS_WIN)
#include "views/widget/accelerator_handler.h"
#endif
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace ui_test_utils {

namespace {

// Used to block until a navigation completes.
class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver(NavigationController* controller,
                                 int number_of_navigations)
      : navigation_started_(false),
        navigations_completed_(0),
        number_of_navigations_(number_of_navigations) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_START,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   Source<NavigationController>(controller));
    RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::NAV_ENTRY_COMMITTED ||
        type == NotificationType::LOAD_START) {
      navigation_started_ = true;
    } else if (type == NotificationType::LOAD_STOP) {
      if (navigation_started_ &&
          ++navigations_completed_ == number_of_navigations_) {
        navigation_started_ = false;
        MessageLoopForUI::current()->Quit();
      }
    }
  }

 private:
  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

class DOMOperationObserver : public NotificationObserver {
 public:
  explicit DOMOperationObserver(TabContents* tab_contents) {
    registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                   Source<TabContents>(tab_contents));
    RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::DOM_OPERATION_RESPONSE);
    Details<DomOperationNotificationDetails> dom_op_details(details);
    response_ = dom_op_details->json();
    MessageLoopForUI::current()->Quit();
  }

  std::string response() const { return response_; }

 private:
  NotificationRegistrar registrar_;
  std::string response_;

  DISALLOW_COPY_AND_ASSIGN(DOMOperationObserver);
};

// DownloadsCompleteObserver waits for a given number of downloads to complete.
// Example usage:
//
//     ui_test_utils::NavigateToURL(browser(), zip_url);
//     DownloadsCompleteObserver wait_on_download(
//         browser()->profile()->GetDownloadManager(), 1);
//     /* |zip_url| download will be complete by this line. */
//
class DownloadsCompleteObserver : public DownloadManager::Observer,
                                  public DownloadItem::Observer {
 public:
  explicit DownloadsCompleteObserver(DownloadManager* download_manager,
                                    size_t wait_count)
      : download_manager_(download_manager),
        wait_count_(wait_count),
        waiting_(false) {
    download_manager_->AddObserver(this);
  }

  // CheckAllDownloadsComplete will be called when the DownloadManager
  // fires it's ModelChanged() call, and also when incomplete downloads
  // fire their OnDownloadUpdated(). 
  bool CheckAllDownloadsComplete() {
    if (downloads_.size() < wait_count_)
      return false;

    bool still_waiting = false;
    std::vector<DownloadItem*>::iterator it = downloads_.begin();
    for (; it != downloads_.end(); ++it) {
      // We always remove ourselves as an observer, then re-add if the download
      // isn't complete. This is to avoid having to track which downloads we
      // are currently observing. Removing has no effect if we are not currently
      // an observer.
      (*it)->RemoveObserver(this);
      if ((*it)->state() != DownloadItem::COMPLETE) {
        (*it)->AddObserver(this);
        still_waiting = true;
      }
    }

    if (still_waiting)
      return false;

    download_manager_->RemoveObserver(this);
    // waiting_ will have been set if not all downloads were complete on first
    // pass below in SetDownloads().
    if (waiting_)
      MessageLoopForUI::current()->Quit();
    return true;
  }

  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* download) {
    if (download->state() == DownloadItem::COMPLETE) {
      CheckAllDownloadsComplete();
    }
  }

  virtual void OnDownloadOpened(DownloadItem* download) {}

  // DownloadManager::Observer
  virtual void ModelChanged() {
    download_manager_->GetDownloads(this, L"");
  }

  virtual void SetDownloads(std::vector<DownloadItem*>& downloads) {
    downloads_ = downloads;
    if (CheckAllDownloadsComplete())
      return;

    if (!waiting_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
    }
  }


 private:
  // The observed download manager.
  DownloadManager* download_manager_;

  // The current downloads being tracked.
  std::vector<DownloadItem*> downloads_;

  // The number of downloads to wait on completing.
  size_t wait_count_;

  // Whether an internal message loop has been started and must be quit upon
  // all downloads completing.
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsCompleteObserver);
};

}  // namespace

void RunMessageLoop() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  bool did_allow_task_nesting = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);
#if defined (OS_WIN)
  views::AcceleratorHandler handler;
  loop->Run(&handler);
#else
  loop->Run();
#endif
  loop->SetNestableTasksAllowed(did_allow_task_nesting);
}

void WaitForNavigation(NavigationController* controller) {
  WaitForNavigations(controller, 1);
}

void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations) {
  NavigationNotificationObserver observer(controller, number_of_navigations);
}

void NavigateToURL(Browser* browser, const GURL& url) {
  NavigateToURLBlockUntilNavigationsComplete(browser, url, 1);
}

void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations) {
  NavigationController* controller =
      &browser->GetSelectedTabContents()->controller();
  browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  WaitForNavigations(controller, number_of_navigations);
}

Value* ExecuteJavaScript(TabContents* tab_contents,
                         const std::wstring& frame_xpath,
                         const std::wstring& original_script) {
  // TODO(jcampan): we should make the domAutomationController not require an
  //                automation id.
  std::wstring script = L"window.domAutomationController.setAutomationId(0);" +
      original_script;
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(frame_xpath,
                                                                script);
  DOMOperationObserver dom_op_observer(tab_contents);
  std::string json = dom_op_observer.response();
  // Wrap |json| in an array before deserializing because valid JSON has an
  // array or an object as the root.
  json.insert(0, "[");
  json.append("]");

  scoped_ptr<Value> root_val(JSONReader::Read(json, true));
  if (!root_val->IsType(Value::TYPE_LIST))
    return NULL;

  ListValue* list = static_cast<ListValue*>(root_val.get());
  Value* result;
  if (!list || !list->GetSize() ||
      !list->Remove(0, &result))  // Remove gives us ownership of the value.
    return NULL;

  return result;
}

bool ExecuteJavaScriptAndExtractInt(TabContents* tab_contents,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& script,
                                    int* result) {
  DCHECK(result);
  scoped_ptr<Value> value(ExecuteJavaScript(tab_contents, frame_xpath, script));
  if (!value.get())
    return false;

  return value->GetAsInteger(result);
}

bool ExecuteJavaScriptAndExtractBool(TabContents* tab_contents,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result) {
  DCHECK(result);
  scoped_ptr<Value> value(ExecuteJavaScript(tab_contents, frame_xpath, script));
  if (!value.get())
    return false;

  return value->GetAsBoolean(result);
}

bool ExecuteJavaScriptAndExtractString(TabContents* tab_contents,
                                       const std::wstring& frame_xpath,
                                       const std::wstring& script,
                                       std::string* result) {
  DCHECK(result);
  scoped_ptr<Value> value(ExecuteJavaScript(tab_contents, frame_xpath, script));
  if (!value.get())
    return false;

  return value->GetAsString(result);
}

GURL GetTestUrl(const std::wstring& dir, const std::wstring file) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.Append(FilePath::FromWStringHack(dir));
  path = path.Append(FilePath::FromWStringHack(file));
  return net::FilePathToFileURL(path);
}

void WaitForDownloadCount(DownloadManager* download_manager, size_t count) {
  DownloadsCompleteObserver download_observer(download_manager, count);
}

}  // namespace ui_test_utils
