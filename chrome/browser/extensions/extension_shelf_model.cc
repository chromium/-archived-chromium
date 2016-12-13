// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_shelf_model.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"

/////////////////////////////

ExtensionShelfModel::ExtensionShelfModel(Browser* browser)
    : browser_(browser), ready_(false) {
  // Watch extensions loaded and unloaded notifications.
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 NotificationService::AllSources());

  // Add any already-loaded extensions now, since we missed the notification for
  // those.
  ExtensionsService* service = browser_->profile()->GetExtensionsService();
  if (service) {  // This can be null in unit tests.
    prefs_ = browser_->profile()->GetExtensionsService()->extension_prefs();
    registrar_.Add(this, NotificationType::EXTENSION_SHELF_MODEL_CHANGED,
                   Source<ExtensionPrefs>(prefs_));
    ready_ = service->is_ready();
    if (ready_) {
      AddExtensions(service->extensions());
      SortToolstrips();
    }
  }
}

ExtensionShelfModel::~ExtensionShelfModel() {
  int count = toolstrips_.size();
  while (count) {
    count--;
    RemoveToolstripAt(count);
  }
  DCHECK(observers_.size() == 0);
}

void ExtensionShelfModel::AddObserver(ExtensionShelfModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ExtensionShelfModel::RemoveObserver(
    ExtensionShelfModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionShelfModel::AppendToolstrip(ExtensionHost* toolstrip) {
  InsertToolstripAt(count(), toolstrip);
}

void ExtensionShelfModel::InsertToolstripAt(int index,
                                            ExtensionHost* toolstrip) {
  toolstrips_.insert(toolstrips_.begin() + index, toolstrip);
  if (ready_) {
    FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                      ToolstripInsertedAt(toolstrip, index));
  }
}

void ExtensionShelfModel::RemoveToolstripAt(int index) {
  ExtensionHost* toolstrip = ToolstripAt(index);
  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripRemovingAt(toolstrip, index));
  toolstrips_.erase(toolstrips_.begin() + index);
  delete toolstrip;
}

void ExtensionShelfModel::MoveToolstripAt(int index, int to_index) {
  if (index == to_index)
    return;

  ExtensionHost* toolstrip = toolstrips_.at(index);
  toolstrips_.erase(toolstrips_.begin() + index);
  toolstrips_.insert(toolstrips_.begin() + to_index, toolstrip);

  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripMoved(toolstrip, index, to_index));

  UpdatePrefs();
}

int ExtensionShelfModel::IndexOfToolstrip(ExtensionHost* toolstrip) {
  ExtensionToolstrips::iterator i =
    std::find(toolstrips_.begin(), toolstrips_.end(), toolstrip);
  if (i == toolstrips_.end())
    return -1;
  return i - toolstrips_.begin();
}

ExtensionHost* ExtensionShelfModel::ToolstripAt(int index) {
  return toolstrips_[index];
}

void ExtensionShelfModel::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_LOADED:
      if (ready_)
        AddExtensions(Details<ExtensionList>(details).ptr());
      break;
    case NotificationType::EXTENSION_UNLOADED:
      RemoveExtension(Details<Extension>(details).ptr());
      break;
    case NotificationType::EXTENSIONS_READY:
      if (browser_->profile()->GetExtensionsService()) {
        AddExtensions(browser_->profile()->GetExtensionsService()->extensions());
        SortToolstrips();
      }
      ready_ = true;
      break;
    case NotificationType::EXTENSION_SHELF_MODEL_CHANGED:
      // Ignore changes that this model originated.
      if (Details<ExtensionShelfModel>(details).ptr() != this)
        SortToolstrips();
      break;
    default:
      DCHECK(false) << "Unhandled notification of type: " << type.value;
      break;
  }
}

void ExtensionShelfModel::AddExtension(Extension* extension) {
  ExtensionProcessManager* manager =
      browser_->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return;

  for (std::vector<std::string>::const_iterator toolstrip_path =
       extension->toolstrips().begin();
       toolstrip_path != extension->toolstrips().end(); ++toolstrip_path) {
    GURL url = extension->GetResourceURL(*toolstrip_path);
    ExtensionHost* host = manager->CreateView(extension, url, browser_);
    AppendToolstrip(host);
  }
}

void ExtensionShelfModel::AddExtensions(const ExtensionList* extensions) {
  if (extensions->size()) {
    ExtensionList::const_iterator extension = extensions->begin();
    for (; extension != extensions->end(); ++extension)
      AddExtension(*extension);
  }
}

void ExtensionShelfModel::RemoveExtension(Extension* extension) {
  bool changed = false;
  for (int i = count() - 1; i >= 0; --i) {
    ExtensionHost* t = ToolstripAt(i);
    if (t->extension()->id() == extension->id()) {
      changed = true;
      RemoveToolstripAt(i);

      // There can be more than one toolstrip per extension, so we have to keep
      // looping even after finding a match.
    }
  }
  if (changed)
    UpdatePrefs();
}

void ExtensionShelfModel::UpdatePrefs() {
  if (!prefs_)
    return;

  // It's easiest to just rebuild the list each time.
  ExtensionPrefs::URLList urls;
  for (int i = 0; i < count(); ++i)
    urls.push_back(ToolstripAt(i)->GetURL());
  prefs_->SetShelfToolstripOrder(urls);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_SHELF_MODEL_CHANGED,
      Source<ExtensionPrefs>(prefs_),
      Details<ExtensionShelfModel>(this));
}

void ExtensionShelfModel::SortToolstrips() {
  ExtensionPrefs::URLList urls = prefs_->GetShelfToolstripOrder();
  ExtensionToolstrips copy =
      ExtensionToolstrips(toolstrips_.begin(), toolstrips_.end());
  toolstrips_.clear();

  // Go through the urls and find the matching toolstrip, re-adding it to the
  // new list in the proper order.
  for (size_t i = 0; i < urls.size(); ++i) {
    GURL& url = urls[i];
    for (ExtensionToolstrips::iterator toolstrip = copy.begin();
        toolstrip != copy.end(); ++toolstrip) {
      if (url == (*toolstrip)->GetURL()) {
        // Note that it's technically possible for the same URL to appear in
        // multiple toolstrips, so we don't do any testing for uniqueness.
        toolstrips_.push_back(*toolstrip);

        // Remove the toolstrip from the list so we don't have to iterate over
        // it next time.
        copy.erase(toolstrip);
        break;
      }
    }
  }

  // Any toolstrips remaining in |copy| were somehow missing from the prefs,
  // so just append them to the end.
  for (ExtensionToolstrips::iterator toolstrip = copy.begin();
      toolstrip != copy.end(); ++toolstrip) {
    toolstrips_.push_back(*toolstrip);
  }

  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ShelfModelReloaded());
}
