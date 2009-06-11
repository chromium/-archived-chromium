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

ExtensionShelfModel::ExtensionShelfModel(Browser* browser) : browser_(browser) {
  // Watch extensions loaded and unloaded notifications.
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());

  // Add any already-loaded extensions now, since we missed the notification for
  // those.
  ExtensionsService* service = browser_->profile()->GetExtensionsService();
  if (service)  // This can be null in unit tests.
    AddExtensions(service->extensions());
}

ExtensionShelfModel::~ExtensionShelfModel() {
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
  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripInsertedAt(toolstrip, index));
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
}

int ExtensionShelfModel::IndexOfToolstrip(ExtensionHost* toolstrip) {
  ExtensionToolstripList::iterator i =
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
    case NotificationType::EXTENSIONS_LOADED: {
      const ExtensionList* extensions = Details<ExtensionList>(details).ptr();
      AddExtensions(extensions);
      break;
    }
    case NotificationType::EXTENSION_UNLOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      RemoveExtension(extension);
      break;
    }
    default:
      DCHECK(false) << "Unhandled notification of type: " << type.value;
      break;
  }
}

void ExtensionShelfModel::AddExtensions(const ExtensionList* extensions) {
  ExtensionProcessManager* manager =
      browser_->profile()->GetExtensionProcessManager();
  if (!manager)
    return;

  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    for (std::vector<std::string>::const_iterator toolstrip_path =
         (*extension)->toolstrips().begin();
         toolstrip_path != (*extension)->toolstrips().end(); ++toolstrip_path) {
      ExtensionHost* host =
          manager->CreateView(*extension,
                              (*extension)->GetResourceURL(*toolstrip_path),
                              browser_);
      AppendToolstrip(host);
    }
  }
}

void ExtensionShelfModel::RemoveExtension(const Extension* extension) {
  for (int i = count() - 1; i >= 0; --i) {
    ExtensionHost* t = ToolstripAt(i);
    if (t->extension()->id() == extension->id()) {
      RemoveToolstripAt(i);
      // There can be more than one toolstrip per extension, so we have to keep
      // looping even after finding a match.
    }
  }
}
