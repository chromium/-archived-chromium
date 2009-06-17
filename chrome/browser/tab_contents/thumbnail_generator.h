// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#define CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/renderer_host/render_widget_host_painting_observer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class RenderWidgetHost;
class SkBitmap;

// This class MUST be destroyed after the RenderWidgetHosts, since it installs
// a painting observer that is not removed.
class ThumbnailGenerator : public RenderWidgetHostPaintingObserver,
                           public NotificationObserver {
 public:
  // This class will do nothing until you call StartThumbnailing.
  ThumbnailGenerator();
  ~ThumbnailGenerator();

  // Ensures that we're properly hooked in to generated thumbnails. This can
  // be called repeatedly and with wild abandon to no ill effect.
  void StartThumbnailing();

  SkBitmap GetThumbnailForRenderer(RenderWidgetHost* renderer) const;

#ifdef UNIT_TEST
  // When true, the class will not use a timeout to do the expiration. This
  // will cause expiration to happen on the next run of the message loop.
  // Unit tests case use this to test expiration by choosing when the message
  // loop runs.
  void set_no_timeout(bool no_timeout) { no_timeout_ = no_timeout; }
#endif

 private:
  // RenderWidgetHostPaintingObserver implementation.
  virtual void WidgetWillDestroyBackingStore(RenderWidgetHost* widget,
                                             BackingStore* backing_store);
  virtual void WidgetDidUpdateBackingStore(RenderWidgetHost* widget);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Indicates that the given widget has changed is visibility.
  void WidgetShown(RenderWidgetHost* widget);
  void WidgetHidden(RenderWidgetHost* widget);

  // Called when the given widget is destroyed.
  void WidgetDestroyed(RenderWidgetHost* widget);

  // Timer function called on a delay after a tab has been shown. It will
  // invalidate the thumbnail for hosts with expired thumbnails in shown_hosts_.
  void ShownDelayHandler();

  // Removes the given host from the shown_hosts_ list, if it is there.
  void EraseHostFromShownList(RenderWidgetHost* host);

  NotificationRegistrar registrar_;

  base::OneShotTimer<ThumbnailGenerator> timer_;

  // A list of all RWHs that have been shown and need to have their thumbnail
  // expired at some time in the future with the "slop" time has elapsed. This
  // list will normally have 0 or 1 items in it.
  std::vector<RenderWidgetHost*> shown_hosts_;

  // See the setter above.
  bool no_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailGenerator);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
