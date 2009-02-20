// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include <xcb/xcb.h>

#include "base/logging.h"
#include "chrome/common/transport_dib.h"

#ifdef NDEBUG
#define XCB_CALL(func, ...) func(__VA_ARGS__)
#else
#define XCB_CALL(func, ...) do { \
  xcb_void_cookie_t cookie = func##_checked(__VA_ARGS__); \
  xcb_generic_error_t* error = xcb_request_check(connection_, cookie); \
  if (error) { \
    CHECK(false) << "XCB error" \
                 << " code:" << error->error_code \
                 << " type:" << error->response_type \
                 << " sequence:" << error->sequence; \
  } \
} while(false);
#endif

BackingStore::BackingStore(const gfx::Size& size,
                           xcb_connection_t* connection,
                           xcb_window_t window,
                           bool use_shared_memory)
    : connection_(connection),
      use_shared_memory_(use_shared_memory),
      pixmap_(xcb_generate_id(connection)) {
  XCB_CALL(xcb_create_pixmap, connection_, 32, pixmap, window, size.width(),
           size.height());
}

BackingStore::~BackingStore() {
  XCB_CALL(xcb_free_pixmap, pixmap_);
}
