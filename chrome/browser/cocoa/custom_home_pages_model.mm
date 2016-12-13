// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/custom_home_pages_model.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"

NSString* const kHomepageEntryChangedNotification =
    @"kHomepageEntryChangedNotification";

// An entry representing a single item in the custom home page model. Stores
// a url and a favicon.
@interface CustomHomePageEntry : NSObject {
 @private
  scoped_nsobject<NSString> url_;
  scoped_nsobject<NSImage> icon_;

  // If non-zero, indicates we're loading the favicon for the page.
  HistoryService::Handle icon_handle_;
}
@property(nonatomic, copy) NSString* URL;
@property(nonatomic, retain) NSImage* image;
@end

//----------------------------------------------------------------------------


@implementation CustomHomePagesModel

- (id)initWithProfile:(Profile*)profile {
  if ((self = [super init])) {
    profile_ = profile;
    entries_.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (NSUInteger)countOfCustomHomePages {
  return [entries_ count];
}

- (id)objectInCustomHomePagesAtIndex:(NSUInteger)index {
  return [entries_ objectAtIndex:index];
}

- (void)insertObject:(id)object inCustomHomePagesAtIndex:(NSUInteger)index {
  [entries_ insertObject:object atIndex:index];
}

- (void)removeObjectFromCustomHomePagesAtIndex:(NSUInteger)index {
  [entries_ removeObjectAtIndex:index];
}

// Get/set the urls the model currently contains as a group. These will weed
// out any URLs that are empty and not add them to the model. As a result,
// the next time they're persisted to the prefs backend, they'll disappear.

- (std::vector<GURL>)URLs {
  std::vector<GURL> urls;
  for (CustomHomePageEntry* entry in entries_.get()) {
    const char* urlString = [[entry URL] UTF8String];
    if (urlString && std::strlen(urlString)) {
      urls.push_back(GURL(std::string(urlString)));
    }
  }
  return urls;
}

- (void)setURLs:(const std::vector<GURL>&)urls {
  [self willChangeValueForKey:@"customHomePages"];
  [entries_ removeAllObjects];
  for (size_t i = 0; i < urls.size(); ++i) {
    scoped_nsobject<CustomHomePageEntry> entry(
        [[CustomHomePageEntry alloc] init]);
    const char* urlString = urls[i].spec().c_str();
    if (urlString && std::strlen(urlString)) {
      [entry setURL:[NSString stringWithCString:urlString
                                       encoding:NSUTF8StringEncoding]];
      [entries_ addObject:entry];
    }
  }
  [self didChangeValueForKey:@"customHomePages"];
}

@end

//---------------------------------------------------------------------------

@implementation CustomHomePageEntry

- (void)setURL:(NSString*)url {
  // Make sure the url is valid before setting it by fixing it up.
  std::string urlToFix(base::SysNSStringToUTF8(url));
  urlToFix = URLFixerUpper::FixupURL(urlToFix, "");
  url_.reset([base::SysUTF8ToNSString(urlToFix) retain]);

  // Broadcast that an individual item has changed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHomepageEntryChangedNotification object:nil];

  // TODO(pinkerton): fetch favicon, convert to NSImage
}

- (NSString*)URL {
  return url_.get();
}

- (void)setImage:(NSImage*)image {
  icon_.reset(image);
}

- (NSImage*)image {
  return icon_.get();
}

@end
