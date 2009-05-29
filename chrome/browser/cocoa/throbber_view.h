// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_THROBBER_VIEW_H_
#define CHROME_BROWSER_COCOA_THROBBER_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

@class TimerTarget;

// A class that knows how to draw an animated state to indicate progress.
// Currently draws via a sequence of frames in an image, but may ultimately be
// written to render paths that are rotated around a center origin. Creating
// the class starts the animation, destroying it stops it. There is no state
// where the class is frozen on an image and not animating.

@interface ThrobberView : NSView {
 @private
  scoped_nsobject<CIImage> image_;
  scoped_nsobject<TimerTarget> target_;  // Target of animation timer.
  NSTimer* timer_;  // Animation timer. Weak, owned by runloop.
  unsigned int numFrames_;  // Number of frames in this animation.
  unsigned int animationFrame_;  // Current frame of the animation,
                                 // [0..numFrames_)
}

// Creates the view with |frame| and the image strip desginated by |image|. The
// image needs to be made of squares such that the height divides evently into
// the width. Takes ownership of |image|.
- (id)initWithFrame:(NSRect)frame image:(NSImage*)image;

// Allows changing the image once the view has been created, such as when the
// view is loaded from a nib. The same restrictions as above apply.
- (void)setImage:(NSImage*)image;

@end

#endif  // CHROME_BROWSER_COCOA_THROBBER_VIEW_H_
