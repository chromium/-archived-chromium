// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/custom_home_pages_model.h"
#include "testing/gtest/include/gtest/gtest.h"

// A helper for KVO and NSNotifications. Makes a note that it's been called
// back.
@interface CustomHomePageHelper : NSObject {
 @public
  BOOL sawNotification_;
}
@end

@implementation CustomHomePageHelper
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  sawNotification_ = YES;
}

- (void)entryChanged:(NSNotification*)notify {
  sawNotification_ = YES;
}
@end

class CustomHomePagesModelTest : public testing::Test {
 public:
  CustomHomePagesModelTest() {
    model_.reset([[CustomHomePagesModel alloc]
                    initWithProfile:helper_.profile()]);
   }
  ~CustomHomePagesModelTest() { }

  BrowserTestHelper helper_;
  scoped_nsobject<CustomHomePagesModel> model_;
};

TEST_F(CustomHomePagesModelTest, Init) {
  scoped_nsobject<CustomHomePagesModel> model(
      [[CustomHomePagesModel alloc] initWithProfile:helper_.profile()]);
}

TEST_F(CustomHomePagesModelTest, GetSetURLs) {
  // Basic test.
  std::vector<GURL> urls;
  urls.push_back(GURL("http://www.google.com"));
  [model_ setURLs:urls];
  std::vector<GURL> received_urls = [model_.get() URLs];
  EXPECT_EQ(received_urls.size(), 1U);
  EXPECT_TRUE(urls[0] == received_urls[0]);

  // Set an empty list, make sure we get back an empty list.
  std::vector<GURL> empty;
  [model_ setURLs:empty];
  received_urls = [model_.get() URLs];
  EXPECT_EQ(received_urls.size(), 0U);

  // Give it a list with not well-formed URLs and make sure we get back.
  // only the good ones.
  std::vector<GURL> poorly_formed;
  poorly_formed.push_back(GURL("http://www.google.com"));  // good
  poorly_formed.push_back(GURL("www.google.com"));  // bad
  poorly_formed.push_back(GURL("www.yahoo."));  // bad
  poorly_formed.push_back(GURL("http://www.yahoo.com"));  // good
  [model_ setURLs:poorly_formed];
  received_urls = [model_.get() URLs];
  EXPECT_EQ(received_urls.size(), 2U);
}

// Test that we get a KVO notification when called setURLs.
TEST_F(CustomHomePagesModelTest, KVOObserveWhenListChanges) {
  scoped_nsobject<CustomHomePageHelper> kvo_helper(
      [[CustomHomePageHelper alloc] init]);
  [model_ addObserver:kvo_helper
          forKeyPath:@"customHomePages"
             options:0L
             context:NULL];
  EXPECT_FALSE(kvo_helper.get()->sawNotification_);

  std::vector<GURL> urls;
  urls.push_back(GURL("http://www.google.com"));
  [model_ setURLs:urls];  // Should send kvo change notification.
  EXPECT_TRUE(kvo_helper.get()->sawNotification_);
}

// Test the KVO "to-many" bindings for |customHomePages| and the KVO
// notifiation when items are added to and removed from the list.
TEST_F(CustomHomePagesModelTest, KVO) {
  EXPECT_EQ([model_ countOfCustomHomePages], 0U);

  scoped_nsobject<CustomHomePageHelper> kvo_helper(
      [[CustomHomePageHelper alloc] init]);
  [model_ addObserver:kvo_helper
          forKeyPath:@"customHomePages"
             options:0L
             context:NULL];
  EXPECT_FALSE(kvo_helper.get()->sawNotification_);

  // Cheat and insert NSString objects into the array. As long as we don't
  // call -URLs, we'll be ok.
  [model_ insertObject:@"www.google.com" inCustomHomePagesAtIndex:0];
  EXPECT_TRUE(kvo_helper.get()->sawNotification_);
  [model_ insertObject:@"www.yahoo.com" inCustomHomePagesAtIndex:1];
  [model_ insertObject:@"dev.chromium.org" inCustomHomePagesAtIndex:2];
  EXPECT_EQ([model_ countOfCustomHomePages], 3U);

  EXPECT_TRUE([[model_ objectInCustomHomePagesAtIndex:1]
                  isEqualToString:@"www.yahoo.com"]);

  kvo_helper.get()->sawNotification_ = NO;
  [model_ removeObjectFromCustomHomePagesAtIndex:1];
  EXPECT_TRUE(kvo_helper.get()->sawNotification_);
  EXPECT_EQ([model_ countOfCustomHomePages], 2U);
  EXPECT_TRUE([[model_ objectInCustomHomePagesAtIndex:1]
                  isEqualToString:@"dev.chromium.org"]);
  EXPECT_TRUE([[model_ objectInCustomHomePagesAtIndex:0]
                  isEqualToString:@"www.google.com"]);
}

// Test that when individual items are changed that they broadcast a message.
TEST_F(CustomHomePagesModelTest, ModelChangedNotification) {
  scoped_nsobject<CustomHomePageHelper> kvo_helper(
      [[CustomHomePageHelper alloc] init]);
  [[NSNotificationCenter defaultCenter]
      addObserver:kvo_helper
         selector:@selector(entryChanged:)
             name:kHomepageEntryChangedNotification
            object:nil];

  std::vector<GURL> urls;
  urls.push_back(GURL("http://www.google.com"));
  [model_ setURLs:urls];
  id entry = [model_ objectInCustomHomePagesAtIndex:0];
  [entry setURL:@"http://www.foo.bar"];
  EXPECT_TRUE(kvo_helper.get()->sawNotification_);
}
