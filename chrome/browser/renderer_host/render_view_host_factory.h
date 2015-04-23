// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_FACTORY_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_FACTORY_H_

#include "base/basictypes.h"

class RenderViewHost;
class RenderViewHostDelegate;
class SiteInstance;

namespace base {
class WaitableEvent;
}  // namespace base

// A factory for creating RenderViewHosts. There is a global factory function
// that can be installed for the purposes of testing to provide a specialized
// RenderViewHost class.
class RenderViewHostFactory {
 public:
  // Creates a RenderViewHost using the currently registered factory, or the
  // default one if no factory is registered. Ownership of the returned
  // pointer will be passed to the caller.
  static RenderViewHost* Create(SiteInstance* instance,
                                RenderViewHostDelegate* delegate,
                                int routing_id,
                                base::WaitableEvent* modal_dialog_event);

  // Returns true if there is currently a globally-registered factory.
  static bool has_factory() {
    return !!factory_;
  }

 protected:
  RenderViewHostFactory() {}
  virtual ~RenderViewHostFactory() {}

  // You can derive from this class and specify an implementation for this
  // function to create a different kind of RenderViewHost for testing.
  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      base::WaitableEvent* modal_dialog_event) = 0;

  // Registers your factory to be called when new RenderViewHosts are created.
  // We have only one global factory, so there must be no factory registered
  // before the call. This class does NOT take ownership of the pointer.
  static void RegisterFactory(RenderViewHostFactory* factory);

  // Unregister the previously registered factory. With no factory registered,
  // the default RenderViewHosts will be created.
  static void UnregisterFactory();

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default RenderViewHosts.
  static RenderViewHostFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostFactory);
};


#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_FACTORY_H_
