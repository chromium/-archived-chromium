// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/transport_dib.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"

static const int kBitmapWidth = 100;
static const int kBitmapHeight = 100;

// TODO(brettw) enable this when GetThumbnailForBackingStore is implemented
// for other platforms in thumbnail_generator.cc
//#if defined(OS_WIN)
// TODO(brettw) enable this on Windows after we clobber a build to see if the
// failures of this on the buildbot can be resolved.
#if 0

class ThumbnailGeneratorTest : public testing::Test {
 public:
  ThumbnailGeneratorTest()
      : profile_(),
        process_(new MockRenderProcessHost(&profile_)),
        widget_(process_, 1),
        view_(&widget_) {
    // Paiting will be skipped if there's no view.
    widget_.set_view(&view_);

    // Need to send out a create notification for the RWH to get hooked. This is
    // a little scary in that we don't have a RenderView, but the only listener
    // will want a RenderWidget, so it works out OK.
    NotificationService::current()->Notify(
        NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
        Source<RenderViewHostManager>(NULL),
        Details<RenderViewHost>(reinterpret_cast<RenderViewHost*>(&widget_)));

    transport_dib_.reset(TransportDIB::Create(kBitmapWidth * kBitmapHeight * 4,
                                              1));

    // We don't want to be sensitive to timing.
    generator_.StartThumbnailing();
    generator_.set_no_timeout(true);
  }

 protected:
  // Indicates what bitmap should be sent with the paint message. _OTHER will
  // only be retrned by CheckFirstPixel if the pixel is none of the others.
  enum TransportType { TRANSPORT_BLACK, TRANSPORT_WHITE, TRANSPORT_OTHER };

  void SendPaint(TransportType type) {
    ViewHostMsg_PaintRect_Params params;
    params.bitmap_rect = gfx::Rect(0, 0, kBitmapWidth, kBitmapHeight);
    params.view_size = params.bitmap_rect.size();
    params.flags = 0;

    scoped_ptr<skia::PlatformCanvas> canvas(
        transport_dib_->GetPlatformCanvas(kBitmapWidth, kBitmapHeight));
    switch (type) {
      case TRANSPORT_BLACK:
        canvas->getTopPlatformDevice().accessBitmap(true).eraseARGB(
            0xFF, 0, 0, 0);
        break;
      case TRANSPORT_WHITE:
        canvas->getTopPlatformDevice().accessBitmap(true).eraseARGB(
            0xFF, 0xFF, 0xFF, 0xFF);
        break;
      case TRANSPORT_OTHER:
      default:
        NOTREACHED();
        break;
    }

    params.bitmap = transport_dib_->id();

    ViewHostMsg_PaintRect msg(1, params);
    widget_.OnMessageReceived(msg);
  }

  TransportType ClassifyFirstPixel(const SkBitmap& bitmap) {
    // Returns the color of the first pixel of the bitmap. The bitmap must be
    // non-empty.
    SkAutoLockPixels lock(bitmap);
    uint32 pixel = *bitmap.getAddr32(0, 0);

    if (SkGetPackedA32(pixel) != 0xFF)
      return TRANSPORT_OTHER;  // All values expect an opqaue alpha channel

    if (SkGetPackedR32(pixel) == 0 &&
        SkGetPackedG32(pixel) == 0 &&
        SkGetPackedB32(pixel) == 0)
      return TRANSPORT_BLACK;

    if (SkGetPackedR32(pixel) == 0xFF &&
        SkGetPackedG32(pixel) == 0xFF &&
        SkGetPackedB32(pixel) == 0xFF)
      return TRANSPORT_WHITE;

    EXPECT_TRUE(false) << "Got weird color: " << pixel;
    return TRANSPORT_OTHER;
  }

  MessageLoopForUI message_loop_;

  TestingProfile profile_;

  // This will get deleted when the last RHWH associated with it is destroyed.
  MockRenderProcessHost* process_;

  RenderWidgetHost widget_;
  TestRenderWidgetHostView view_;
  ThumbnailGenerator generator_;

  scoped_ptr<TransportDIB> transport_dib_;

 private:
  // testing::Test implementation.
  void SetUp() {
  }
  void TearDown() {
  }
};

TEST_F(ThumbnailGeneratorTest, NoThumbnail) {
  // This is the case where there is no thumbnail available on the tab and
  // there is no backing store. There should be no image returned.
  SkBitmap result = generator_.GetThumbnailForRenderer(&widget_);
  EXPECT_TRUE(result.isNull());
}

// Tests basic thumbnail generation when a backing store is discarded.
TEST_F(ThumbnailGeneratorTest, DiscardBackingStore) {
  // First set up a backing store and then discard it.
  SendPaint(TRANSPORT_BLACK);
  widget_.WasHidden();
  ASSERT_TRUE(BackingStoreManager::ExpireBackingStoreForTest(&widget_));
  ASSERT_FALSE(widget_.GetBackingStore(false));

  // The thumbnail generator should have stashed a thumbnail of the page.
  SkBitmap result = generator_.GetThumbnailForRenderer(&widget_);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(TRANSPORT_BLACK, ClassifyFirstPixel(result));
}

TEST_F(ThumbnailGeneratorTest, QuickShow) {
  // Set up a hidden widget with a black cached thumbnail and an expired
  // backing store.
  SendPaint(TRANSPORT_BLACK);
  widget_.WasHidden();
  ASSERT_TRUE(BackingStoreManager::ExpireBackingStoreForTest(&widget_));
  ASSERT_FALSE(widget_.GetBackingStore(false));

  // Now show the widget and paint white.
  widget_.WasRestored();
  SendPaint(TRANSPORT_WHITE);

  // The black thumbnail should still be cached because it hasn't processed the
  // timer message yet.
  SkBitmap result = generator_.GetThumbnailForRenderer(&widget_);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(TRANSPORT_BLACK, ClassifyFirstPixel(result));

  // Running the message loop will process the timer, which should expire the
  // cached thumbnail. Asking again should give us a new one computed from the
  // backing store.
  message_loop_.RunAllPending();
  result = generator_.GetThumbnailForRenderer(&widget_);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(TRANSPORT_WHITE, ClassifyFirstPixel(result));
}

#endif
