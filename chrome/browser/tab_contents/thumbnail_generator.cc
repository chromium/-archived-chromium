// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/thumbnail_generator.h"

#include <algorithm>

#include "base/histogram.h"
#include "base/time.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/property_bag.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Overview
// --------
// This class provides current thumbnails for tabs. The simplest operation is
// when a request for a thumbnail comes in, to grab the backing store and make
// a smaller version of that.
//
// A complication happens because we don't always have nice backing stores for
// all tabs (there is a cache of several tabs we'll keep backing stores for).
// To get thumbnails for tabs with expired backing stores, we listen for
// backing stores that are being thrown out, and generate thumbnails before
// that happens. We attach them to the RenderWidgetHost via the property bag
// so we can retrieve them later. When a tab has a live backing store again,
// we throw away the thumbnail since it's now out-of-date.
//
// Another complication is performance. If the user brings up a tab switcher, we
// don't want to get all 5 cached backing stores since it is a very large amount
// of data. As a result, we generate thumbnails for tabs that are hidden even
// if the backing store is still valid. This means we'll have to do a maximum
// of generating thumbnails for the visible tabs at any point.
//
// The last performance consideration is when the user switches tabs quickly.
// This can happen by doing Control-PageUp/Down or juct clicking quickly on
// many different tabs (like when you're looking for one). We don't want to
// slow this down by making thumbnails for each tab as it's hidden. Therefore,
// we have a timer so that we don't invalidate thumbnails for tabs that are
// only shown briefly (which would cause the thumbnail to be regenerated when
// the tab is hidden).

namespace {

static const int kThumbnailWidth = 294;
static const int kThumbnailHeight = 204;

// Indicates the time that the RWH must be visible for us to update the
// thumbnail on it. If the user holds down control enter, there will be a lot
// of backing stores created and destroyed. WE don't want to interfere with
// that.
//
// Any operation that happens within this time of being shown is ignored.
// This means we won't throw the thumbnail away when the backing store is
// painted in this time.
static const int kVisibilitySlopMS = 3000;

static const char kThumbnailHistogramName[] = "Thumbnail.ComputeMS";

struct WidgetThumbnail {
  SkBitmap thumbnail;

  // Indicates the last time the RWH was shown and hidden.
  base::TimeTicks last_shown;
  base::TimeTicks last_hidden;
};

PropertyAccessor<WidgetThumbnail>* GetThumbnailAccessor() {
  static PropertyAccessor<WidgetThumbnail> accessor;
  return &accessor;
}

// Returns the existing WidgetThumbnail for a RVH, or creates a new one and
// returns that if none exists.
WidgetThumbnail* GetDataForHost(RenderWidgetHost* host) {
  WidgetThumbnail* wt = GetThumbnailAccessor()->GetProperty(
      host->property_bag());
  if (wt)
    return wt;

  GetThumbnailAccessor()->SetProperty(host->property_bag(),
                                      WidgetThumbnail());
  return GetThumbnailAccessor()->GetProperty(host->property_bag());
}

#if defined(OS_WIN)

// PlatformDevices/Canvases can't be copied like a regular SkBitmap (at least
// on Windows). So the second parameter is the canvas to draw into. It should
// be sized to the size of the backing store.
void GetBitmapForBackingStore(BackingStore* backing_store,
                              skia::PlatformCanvas* canvas) {
  HDC dc = canvas->beginPlatformPaint();
  BitBlt(dc, 0, 0,
         backing_store->size().width(), backing_store->size().height(),
         backing_store->hdc(), 0, 0, SRCCOPY);
  canvas->endPlatformPaint();
}

#endif

// Creates a downsampled thumbnail for the given backing store. The returned
// bitmap will be isNull if there was an error creating it.
SkBitmap GetThumbnailForBackingStore(BackingStore* backing_store) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();

  SkBitmap result;

  // TODO(brettw) write this for other platforms. If you enable this, be sure
  // to also enable the unit tests for the same platform in
  // thumbnail_generator_unittest.cc
#if defined(OS_WIN)
  // Get the bitmap as a Skia object so we can resample it. This is a large
  // allocation and we can tolerate failure here, so give up if the allocation
  // fails.
  skia::PlatformCanvas temp_canvas;
  if (!temp_canvas.initialize(backing_store->size().width(),
                              backing_store->size().height(), true))
    return result;
  GetBitmapForBackingStore(backing_store, &temp_canvas);

  // Get the bitmap out of the canvas and resample it. It would be nice if this
  // whole Windows-specific block could be put into a function, but the memory
  // management wouldn't work out because the bitmap is a PlatformDevice which
  // can't actually be copied.
  const SkBitmap& bmp = temp_canvas.getTopPlatformDevice().accessBitmap(false);

#elif defined(OS_LINUX)
  SkBitmap bmp = backing_store->PaintRectToBitmap(
      gfx::Rect(0, 0,
                backing_store->size().width(), backing_store->size().height()));

#elif defined(OS_MACOSX)
  SkBitmap bmp;
  NOTIMPLEMENTED();
#endif

  result = skia::ImageOperations::DownsampleByTwoUntilSize(
      bmp,
      kThumbnailWidth, kThumbnailHeight);

#if defined(OS_WIN)
  // This is a bit subtle. SkBitmaps are refcounted, but the magic ones in
  // PlatformCanvas on Windows can't be ssigned to SkBitmap with proper
  // refcounting.  If the bitmap doesn't change, then the downsampler will
  // return the input bitmap, which will be the reference to the weird
  // PlatformCanvas one insetad of a regular one. To get a regular refcounted
  // bitmap, we need to copy it.
  if (bmp.width() == result.width() && bmp.height() == result.height())
    bmp.copyTo(&result, SkBitmap::kARGB_8888_Config);
#endif

  HISTOGRAM_TIMES(kThumbnailHistogramName,
                  base::TimeTicks::Now() - begin_compute_thumbnail);
  return result;
}

}  // namespace

ThumbnailGenerator::ThumbnailGenerator()
    : no_timeout_(false) {
  // The BrowserProcessImpl creates this non-lazily. If you add nontrivial
  // stuff here, be sure to convert it to being lazily created.
  //
  // We don't register for notifications here since BrowserProcessImpl creates
  // us before the NotificationService is.
}

ThumbnailGenerator::~ThumbnailGenerator() {
}

void ThumbnailGenerator::StartThumbnailing() {
  if (registrar_.IsEmpty()) {
    // Even though we deal in RenderWidgetHosts, we only care about its
    // subclass, RenderViewHost when it is in a tab. We don't make thumbnails
    // for RenderViewHosts that aren't in tabs, or RenderWidgetHosts that
    // aren't views like select popups.
    registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                   NotificationService::AllSources());

    registrar_.Add(this, NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::RENDER_WIDGET_HOST_DESTROYED,
                   NotificationService::AllSources());
  }
}

SkBitmap ThumbnailGenerator::GetThumbnailForRenderer(
    RenderWidgetHost* renderer) const {
  WidgetThumbnail* wt = GetDataForHost(renderer);

  BackingStore* backing_store = renderer->GetBackingStore(false);
  if (!backing_store) {
    // When we have no backing store, there's no choice in what to use. We
    // have to return either the existing thumbnail or the empty one if there
    // isn't a saved one.
    return wt->thumbnail;
  }

  // Now that we have a backing store, we have a choice to use it to make
  // a new thumbnail, or use a previously stashed one if we have it.
  //
  // Return the previously-computed one if we have it and it hasn't expired.
  if (!wt->thumbnail.isNull() &&
      (no_timeout_ ||
       base::TimeTicks::Now() -
       base::TimeDelta::FromMilliseconds(kVisibilitySlopMS) < wt->last_shown))
    return wt->thumbnail;

  // Save this thumbnail in case we need to use it again soon. It will be
  // invalidated on the next paint.
  wt->thumbnail = GetThumbnailForBackingStore(backing_store);
  return wt->thumbnail;
}

void ThumbnailGenerator::WidgetWillDestroyBackingStore(
    RenderWidgetHost* widget,
    BackingStore* backing_store) {
  // Since the backing store is going away, we need to save it as a thumbnail.
  WidgetThumbnail* wt = GetDataForHost(widget);

  // If there is already a thumbnail on the RWH that's visible, it means that
  // not enough time has elapsed since being shown, and we can ignore generating
  // a new one.
  if (!wt->thumbnail.isNull())
    return;

  // Save a scaled-down image of the page in case we're asked for the thumbnail
  // when there is no RenderViewHost. If this fails, we don't want to overwrite
  // an existing thumbnail.
  SkBitmap new_thumbnail = GetThumbnailForBackingStore(backing_store);
  if (!new_thumbnail.isNull())
    wt->thumbnail = new_thumbnail;
}

void ThumbnailGenerator::WidgetDidUpdateBackingStore(
    RenderWidgetHost* widget) {
  // Clear the current thumbnail since it's no longer valid.
  WidgetThumbnail* wt = GetThumbnailAccessor()->GetProperty(
      widget->property_bag());
  if (!wt)
    return;  // Nothing to do.

  // If this operation is within the time slop after being shown, keep the
  // existing thumbnail.
  if (no_timeout_ ||
      base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(kVisibilitySlopMS) < wt->last_shown)
    return;  // TODO(brettw) schedule thumbnail generation for this renderer in
             // case we don't get a paint for it after the time slop, but it's
             // still visible.

  // Clear the thumbnail, since it's now out of date.
  wt->thumbnail = SkBitmap();
}

void ThumbnailGenerator::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      // Install our observer for all new RVHs.
      RenderViewHost* renderer = Details<RenderViewHost>(details).ptr();
      renderer->set_painting_observer(this);
      break;
    }

    case NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED:
      if (*Details<bool>(details).ptr())
        WidgetShown(Source<RenderWidgetHost>(source).ptr());
      else
        WidgetHidden(Source<RenderWidgetHost>(source).ptr());
      break;

    case NotificationType::RENDER_WIDGET_HOST_DESTROYED:
      WidgetDestroyed(Source<RenderWidgetHost>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}

void ThumbnailGenerator::WidgetShown(RenderWidgetHost* widget) {
  WidgetThumbnail* wt = GetDataForHost(widget);
  wt->last_shown = base::TimeTicks::Now();

  // If there is no thumbnail (like we're displaying a background tab for the
  // first time), then we don't have do to invalidate the existing one.
  if (wt->thumbnail.isNull())
    return;

  std::vector<RenderWidgetHost*>::iterator found =
      std::find(shown_hosts_.begin(), shown_hosts_.end(), widget);
  if (found != shown_hosts_.end()) {
    NOTREACHED() << "Showing a RWH we already think is shown";
    shown_hosts_.erase(found);
  }
  shown_hosts_.push_back(widget);

  // Keep the old thumbnail for a small amount of time after the tab has been
  // shown. This is so in case it's hidden quickly again, we don't waste any
  // work regenerating it.
  if (timer_.IsRunning())
    return;
  timer_.Start(base::TimeDelta::FromMilliseconds(
                   no_timeout_ ? 0 : kVisibilitySlopMS),
               this, &ThumbnailGenerator::ShownDelayHandler);
}

void ThumbnailGenerator::WidgetHidden(RenderWidgetHost* widget) {
  WidgetThumbnail* wt = GetDataForHost(widget);
  wt->last_hidden = base::TimeTicks::Now();

  // If the tab is on the list of ones to invalidate the thumbnail, we need to
  // remove it.
  EraseHostFromShownList(widget);

  // There may still be a valid cached thumbnail on the RWH, so we don't need to
  // make a new one.
  if (!wt->thumbnail.isNull())
    return;
  wt->thumbnail = GetThumbnailForRenderer(widget);
}

void ThumbnailGenerator::WidgetDestroyed(RenderWidgetHost* widget) {
  EraseHostFromShownList(widget);
}

void ThumbnailGenerator::ShownDelayHandler() {
  base::TimeTicks threshold = base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(kVisibilitySlopMS);

  // Check the list of all pending RWHs (normally only one) to see if any of
  // their times have expired.
  for (size_t i = 0; i < shown_hosts_.size(); i++) {
    WidgetThumbnail* wt = GetDataForHost(shown_hosts_[i]);
    if (no_timeout_ || wt->last_shown <= threshold) {
      // This thumbnail has expired, delete it.
      wt->thumbnail = SkBitmap();
      shown_hosts_.erase(shown_hosts_.begin() + i);
      i--;
    }
  }

  // We need to schedule another run if there are still items in the list to
  // process. We use half the timeout for these re-runs to catch the items
  // that were added since the timer was run the first time.
  if (!shown_hosts_.empty()) {
    DCHECK(!no_timeout_);
    timer_.Start(base::TimeDelta::FromMilliseconds(kVisibilitySlopMS) / 2, this,
                 &ThumbnailGenerator::ShownDelayHandler);
  }
}

void ThumbnailGenerator::EraseHostFromShownList(RenderWidgetHost* widget) {
  std::vector<RenderWidgetHost*>::iterator found =
      std::find(shown_hosts_.begin(), shown_hosts_.end(), widget);
  if (found != shown_hosts_.end())
    shown_hosts_.erase(found);
}
