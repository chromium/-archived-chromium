#include "chrome/browser/extensions/extension_browsertest.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/ui_test_utils.h"

namespace {
// Amount of time to wait to load an extension. This is purposely obscenely
// long because it will only get used in the case of failure and we want to
// minimize false positives.
static const int kTimeoutMs = 60 * 1000;  // 1 minute
};

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
void ExtensionBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // This enables DOM automation for tab contentses.
  EnableDOMAutomation();

  // This enables it for extension hosts.
  ExtensionHost::EnableDOMAutomation();

  command_line->AppendSwitch(switches::kEnableExtensions);

  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");
}

bool ExtensionBrowserTest::LoadExtension(const FilePath& path) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  size_t num_before = service->extensions()->size();
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                 NotificationService::AllSources());
  service->LoadExtension(path);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
      kTimeoutMs);
  ui_test_utils::RunMessageLoop();
  registrar_.Remove(this, NotificationType::EXTENSIONS_LOADED,
                    NotificationService::AllSources());
  size_t num_after = service->extensions()->size();
  if (num_after != (num_before + 1))
    return false;

  return WaitForExtensionHostsToLoad();
}

bool ExtensionBrowserTest::InstallExtension(const FilePath& path) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->set_show_extensions_prompts(false);
  size_t num_before = service->extensions()->size();

  registrar_.Add(this, NotificationType::EXTENSION_INSTALLED,
                 NotificationService::AllSources());
  service->InstallExtension(path);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
      kTimeoutMs);
  ui_test_utils::RunMessageLoop();
  registrar_.Remove(this, NotificationType::EXTENSION_INSTALLED,
                    NotificationService::AllSources());
  size_t num_after = service->extensions()->size();
  if (num_after != (num_before + 1)) {
    std::cout << "Num extensions before: " << IntToString(num_before)
              << "num after: " << IntToString(num_after)
              << "Installed extensions are:\n";
    for (size_t i = 0; i < service->extensions()->size(); ++i)
      std::cout << "  " << service->extensions()->at(i)->id() << "\n";
    return false;
  }

  return WaitForExtensionHostsToLoad();
}

void ExtensionBrowserTest::UninstallExtension(const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->UninstallExtension(extension_id, false);
}

bool ExtensionBrowserTest::WaitForExtensionHostsToLoad() {
  // Wait for all the extension hosts that exist to finish loading.
  // NOTE: This assumes that the extension host list is not changing while
  // this method is running.
  ExtensionProcessManager* manager =
        browser()->profile()->GetExtensionProcessManager();
  base::Time start_time = base::Time::Now();
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
     iter != manager->end(); ++iter) {
    while (!(*iter)->did_stop_loading()) {
      if ((base::Time::Now() - start_time).InMilliseconds() > kTimeoutMs) {
        std::cout << "Extension host did not load for URL: "
                  << (*iter)->GetURL().spec();
        return false;
      }

      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          new MessageLoop::QuitTask, 200);
      ui_test_utils::RunMessageLoop();
    }
  }

  return true;
}

void ExtensionBrowserTest::Observe(NotificationType type,
                     const NotificationSource& source,
                     const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_INSTALLED:
    case NotificationType::EXTENSIONS_LOADED:
      MessageLoopForUI::current()->Quit();
      break;
    default:
      NOTREACHED();
      break;
  }
}
