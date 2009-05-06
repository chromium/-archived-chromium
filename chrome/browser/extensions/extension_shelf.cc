// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_shelf.h"

#include "base/logging.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "skia/ext/skia_utils.h"

namespace {

// Margins around the content.
static const int kTopMargin = 2;
static const int kBottomMargin = 2;
static const int kLeftMargin = 0;
static const int kRightMargin = 0;

// Padding on left and right side of an extension toolstrip.
static const int kToolstripPadding = 2;

// Width of the toolstrip divider.
static const int kToolstripDividerWidth = 2;

// Preferred height of the ExtensionShelf.
static const int kShelfHeight = 29;

// Colors for the ExtensionShelf.
static const SkColor kBackgroundColor = SkColorSetRGB(230, 237, 244);
static const SkColor kBorderColor = SkColorSetRGB(201, 212, 225);
static const SkColor kDividerHighlightColor = SkColorSetRGB(247, 250, 253);

// TODO(erikkay) convert back to a gradient when Glen figures out the
// specs.
// static const SkColor kBackgroundColor = SkColorSetRGB(237, 244, 252);
// static const SkColor kTopGradientColor = SkColorSetRGB(222, 234, 248);

}  // namespace


ExtensionShelf::ExtensionShelf(Browser* browser) : browser_(browser) {
  // Watch extensions loaded notification.
  NotificationService* ns = NotificationService::current();
  Source<Profile> ns_source(browser->profile()->GetOriginalProfile());
  ns->AddObserver(this, NotificationType::EXTENSIONS_LOADED,
                  NotificationService::AllSources());

  // Add any already-loaded extensions now, since we missed the notification for
  // those.
  ExtensionsService* service = browser_->profile()->GetExtensionsService();
  if (service) {  // This can be null in unit tests.
    if (AddExtensionViews(service->extensions())) {
      Layout();
      SchedulePaint();
    }
  }
}

ExtensionShelf::~ExtensionShelf() {
  NotificationService* ns = NotificationService::current();
  ns->RemoveObserver(this, NotificationType::EXTENSIONS_LOADED,
                     NotificationService::AllSources());
}

void ExtensionShelf::Paint(ChromeCanvas* canvas) {
#if 0
  // TODO(erikkay) re-enable this when Glen has the gradient values worked out.
  SkPaint paint;
  paint.setShader(skia::CreateGradientShader(0,
                                             height(),
                                             kTopGradientColor,
                                             kBackgroundColor))->safeUnref();
  canvas->FillRectInt(0, 0, width(), height(), paint);
#else
  canvas->FillRectInt(kBackgroundColor, 0, 0, width(), height());
#endif

  canvas->FillRectInt(kBorderColor, 0, 0, width(), 1);
  canvas->FillRectInt(kBorderColor, 0, height() - 1, width(), 1);

  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i) {
    int right = GetChildViewAt(i)->bounds().right() + kToolstripPadding;
    int h = height() - 2;
    canvas->FillRectInt(kBorderColor, right, 1, 1, h);
    canvas->FillRectInt(kDividerHighlightColor, right + 1, 1, 1, h);
  }

  SkRect background_rect = {
      SkIntToScalar(0),
      SkIntToScalar(1),
      SkIntToScalar(1),
      SkIntToScalar(height() - 2)};
  InitBackground(canvas, background_rect);
}

gfx::Size ExtensionShelf::GetPreferredSize() {
  if (HasExtensionViews())
    return gfx::Size(0, kShelfHeight);
  return gfx::Size(0, 0);
}

void ExtensionShelf::Layout() {
  if (!GetParent())
    return;

  int x = kLeftMargin;
  int y = kTopMargin;
  int content_height = height() - kTopMargin - kBottomMargin;
  int max_x = width() - kRightMargin;

  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i) {
    x += kToolstripPadding;  // left padding
    views::View* child = GetChildViewAt(i);
    gfx::Size pref = child->GetPreferredSize();
    int next_x = x + pref.width() + kToolstripPadding;  // right padding
    child->SetVisible(next_x < max_x);
    child->SetBounds(x, y, pref.width(), content_height);
    child->Layout();
    x = next_x + kToolstripDividerWidth;
  }
}

void ExtensionShelf::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_LOADED: {
      const ExtensionList* extensions = Details<ExtensionList>(details).ptr();
      if (AddExtensionViews(extensions)) {
        Layout();
        SchedulePaint();
        // TODO(erikkay) come up with a better way to notify parents
        if (GetParent())
          GetParent()->Layout();
      }
      break;
    }
    default:
      DCHECK(false) << "Unhandled notification of type: " << type.value;
      break;
  }
}

bool ExtensionShelf::AddExtensionViews(const ExtensionList* extensions) {
  bool added_toolstrip = false;
  ExtensionProcessManager* manager =
      browser_->profile()->GetExtensionProcessManager();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    for (std::vector<std::string>::const_iterator toolstrip_path =
         (*extension)->toolstrips().begin();
         toolstrip_path != (*extension)->toolstrips().end(); ++toolstrip_path) {
      ExtensionView* toolstrip =
          manager->CreateView(*extension,
                              (*extension)->GetResourceURL(*toolstrip_path),
                              browser_);
      if (!background_.empty())
        toolstrip->SetBackground(background_);
      AddChildView(toolstrip);
      added_toolstrip = true;
    }
  }
  return added_toolstrip;
}

bool ExtensionShelf::HasExtensionViews() {
  return GetChildViewCount() > 0;
}

void ExtensionShelf::InitBackground(ChromeCanvas* canvas,
                                    const SkRect& subset) {
  if (!background_.empty())
    return;

  const SkBitmap& background = canvas->getDevice()->accessBitmap(false);

  // Extract the correct subset of the toolstrip background into a bitmap. We
  // must use a temporary here because extractSubset() returns a bitmap that
  // references pixels in the original one and we want to actually make a copy
  // that will have a long lifetime.
  SkBitmap temp;
  temp.setConfig(background.config(),
                 static_cast<int>(subset.width()),
                 static_cast<int>(subset.height()));

  SkRect mapped_subset = subset;
  bool result = canvas->getTotalMatrix().mapRect(&mapped_subset);
  DCHECK(result);

  SkIRect isubset;
  mapped_subset.round(&isubset);
  result = background.extractSubset(&temp, isubset);
  if (!result)
    return;

  temp.copyTo(&background_, temp.config());
  DCHECK(background_.readyToDraw());

  // Tell all extension views about the new background
  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i)
    static_cast<ExtensionView*>(GetChildViewAt(i))->SetBackground(background_);
}
