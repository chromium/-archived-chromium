// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "test_shell_webview.h"

#import <Cocoa/Cocoa.h>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/webwidget_host.h"

@implementation TestShellWebView

@synthesize shell = shell_;

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    trackingArea_ =
    [[NSTrackingArea alloc] initWithRect:frame
                                 options:NSTrackingMouseMoved |
                                         NSTrackingActiveInActiveApp |
                                         NSTrackingInVisibleRect
                                   owner:self
                                userInfo:nil];
    [self addTrackingArea:trackingArea_];
  }
  return self;
}

- (void) dealloc {
  [self removeTrackingArea:trackingArea_];
  [trackingArea_ release];

  [super dealloc];
}

- (void)drawRect:(NSRect)rect {
  CGContextRef context =
      reinterpret_cast<CGContextRef>([[NSGraphicsContext currentContext]
                                      graphicsPort]);

  // start by filling the rect with magenta, so that we can see what's drawn
  CGContextSetRGBFillColor (context, 1, 0, 1, 1);
  CGContextFillRect(context, NSRectToCGRect(rect));

  if (shell_ && shell_->webView()) {
    gfx::Rect client_rect(NSRectToCGRect(rect));
    // flip from cocoa coordinates
    client_rect.set_y([self frame].size.height -
                      client_rect.height() - client_rect.y());

    shell_->webViewHost()->UpdatePaintRect(client_rect);
    shell_->webViewHost()->Paint();
  }
}

- (IBAction)goBack:(id)sender {
  if (shell_)
    shell_->GoBackOrForward(-1);
}

- (IBAction)goForward:(id)sender {
  if (shell_)
    shell_->GoBackOrForward(1);
}

- (IBAction)reload:(id)sender {
  if (shell_)
    shell_->Reload();
}

- (IBAction)stopLoading:(id)sender {
  if (shell_ && shell_->webView())
    shell_->webView()->StopLoading();
}

- (IBAction)takeURLStringValueFrom:(NSTextField *)sender {
  NSString *url = [sender stringValue];

  // if it doesn't already have a prefix, add http. If we can't parse it,
  // just don't bother rather than making things worse.
  NSURL* tempUrl = [NSURL URLWithString:url];
  if (tempUrl && ![tempUrl scheme])
    url = [@"http://" stringByAppendingString:url];
  shell_->LoadURL(UTF8ToWide([url UTF8String]).c_str());
}

- (void)mouseDown:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)rightMouseDown:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)otherMouseDown:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)mouseUp:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)rightMouseUp:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)otherMouseUp:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)mouseMoved:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)mouseDragged:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)scrollWheel:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->WheelEvent(theEvent);
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)mouseEntered:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)mouseExited:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->MouseEvent(theEvent);
}

- (void)keyDown:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->KeyEvent(theEvent);
}

- (void)keyUp:(NSEvent *)theEvent {
  if (shell_ && shell_->webView())
    shell_->webViewHost()->KeyEvent(theEvent);
}

- (BOOL)isOpaque {
  return YES;
}

- (BOOL)canBecomeKeyView {
  return shell_ && shell_->webView();
}

- (BOOL)acceptsFirstResponder {
  return shell_ && shell_->webView();
}

- (BOOL)becomeFirstResponder {
  if (shell_ && shell_->webView()) {
    shell_->webViewHost()->SetFocus(YES);
    return YES;
  }

  return NO;
}

- (BOOL)resignFirstResponder {
  if (shell_ && shell_->webView()) {
    shell_->webViewHost()->SetFocus(NO);
    return YES;
  }

  return NO;
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (shell_ && shell_->webView())
    shell_->webViewHost()->Resize(gfx::Rect(NSRectToCGRect(frameRect)));
  [self setNeedsDisplay:YES];
}

@end
