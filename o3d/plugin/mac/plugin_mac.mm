/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include "plugin_mac.h"
#include <Breakpad/Breakpad.h>
#include <Cocoa/Cocoa.h>
#include <QuickTime/QuickTime.h>
#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/main.h"
#include "core/mac/display_window_mac.h"
#include "plugin/mac/graphics_utils_mac.h"

#if !defined(O3D_INTERNAL_PLUGIN)
BreakpadRef gBreakpadRef =  NULL;
#endif

using glue::_o3d::PluginObject;
using o3d::DisplayWindowMac;

@interface NSWindowController (plugin_hack)
- (id)selectedTab;
@end

namespace o3d {

// Returns the version number of the running Mac browser, as parsed from
// the short version string in the plist of the app's bundle.
bool GetBrowserVersionInfo(int *returned_major,
                           int *returned_minor,
                           int *returned_bugfix) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int major = 0;
  int minor = 0;
  int bugfix = 0;
  NSBundle *app_bundle = [NSBundle mainBundle];
  NSString *versionString =
      [app_bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

  if ([versionString length]) {
    NSScanner *versionScanner = [NSScanner scannerWithString:versionString];
    NSCharacterSet *dotSet =
        [NSCharacterSet characterSetWithCharactersInString:@"."];
    NSCharacterSet *numSet =
        [NSCharacterSet characterSetWithCharactersInString:@"0123456789"];
    NSCharacterSet *notNumSet = [numSet invertedSet];

    // Skip anything at start that is not a number.
    [versionScanner scanCharactersFromSet:notNumSet intoString:NULL];

    // Keep reading values until we get all 3 numbers, or something isn't found.
    if ([versionScanner scanInt:&major])
      if ([versionScanner scanCharactersFromSet:dotSet intoString:NULL])
        if ([versionScanner scanInt:&minor])
          if ([versionScanner scanCharactersFromSet:dotSet intoString:NULL])
            [versionScanner scanInt:&bugfix];
  }

  *returned_major = major;
  *returned_minor = minor;
  *returned_bugfix = bugfix;

  [pool release];

  // If we read any version numbers, then we succeeded.
  return (major || minor || bugfix);
}


// Code for peeking at the selected tab object for a WindowRef in Safari.  This
// is purely so we can detect changes in active tab in Safari, since it does not
// notify us when our tab is hidden or revealed.
//
// If the browser is not Safari these functions should harmlessly be NULL since
// the walk from windowref to cocoa window (of type "BrowserWindow") to cocoa
// windowcontroller to selectedTab should harmlessly break down somewhere along
// the way.
//
// This is useful merely for peeking at Safari's internal state for a tab change
// event, as Safari does not notify us when this happens.  It coerces the value
// to a void*, as the plain .cc files don't like the type id, and we actually
// only care whether the value changes or not.


void ReleaseSafariBrowserWindow(void* browserWindow) {
  NSWindow* cocoaWindow = (NSWindow*) browserWindow;
  [cocoaWindow release];
}

void* SafariBrowserWindowForWindowRef(WindowRef theWindow) {
  if (theWindow != NULL) {
    NSWindow* cocoaWindow = [[NSWindow alloc] initWithWindowRef:theWindow];
    if (cocoaWindow) {
      if (strcmp(object_getClassName(cocoaWindow), "BrowserWindow") == 0) {
        return cocoaWindow;
      } else {
        [cocoaWindow release];
      }
    }
  }
  return NULL;
}

void* SelectedTabForSafariBrowserWindow(void* browserWindow) {
  id selectedTab = nil;
  NSWindow* cocoaWindow = (NSWindow*) browserWindow;
  if (cocoaWindow) {
    @try {
      selectedTab = [[cocoaWindow windowController] selectedTab];
    } @catch(NSException* exc) {
    }
  }
  return (void*) selectedTab;
}

// Detects when Safari has hidden or revealed our tab.
// If a hide event is detected, it sets the mac_surface_hidden_ flag and hides
// the surface.
// Later, if the mac_surface_hidden_ flag is set but we are no longer hidden,
// ie DetectTabHiding is now returning false, it restores the surface to the
// previous state.
void ManageSafariTabSwitching(PluginObject* obj) {
  if (obj->DetectTabHiding()) {
    if (!obj->mac_surface_hidden_) {
      obj->mac_surface_hidden_ = true;
      GLint rect[4] = {0,0,0,0};
      aglSetInteger(obj->mac_agl_context_, AGL_BUFFER_RECT, rect);
      aglEnable(obj->mac_agl_context_, AGL_BUFFER_RECT);
    }
  } else if (obj->mac_surface_hidden_) {
    obj->mac_surface_hidden_ = false;
    aglSetInteger(obj->mac_agl_context_, AGL_BUFFER_RECT,
                  obj->last_buffer_rect_);
    aglEnable(obj->mac_agl_context_, AGL_BUFFER_RECT);
  }
}


#pragma mark ____RenderTimer

RenderTimer gRenderTimer;
std::vector<NPP> RenderTimer::instances_;

void RenderTimer::Start() {
  CFRunLoopTimerContext timerContext;
  memset(&timerContext, 0, sizeof(timerContext));
  timerContext.info = (void*)NULL;

  if (!timerRef_) {
    timerRef_ = CFRunLoopTimerCreate(NULL,
                                     1.0,
                                     1.0 / 60.0,
                                     0,
                                     0,
                                     TimerCallback,
                                     &timerContext);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef_, kCFRunLoopCommonModes);
  }
}

void RenderTimer::Stop() {
  if (timerRef_) {
    CFRunLoopTimerInvalidate(timerRef_);
    timerRef_ = NULL;
  }
  instances_.clear();   // this should already be empty, but make sure
}

void RenderTimer::AddInstance(NPP instance) {
  // avoid adding the same instance twice!
  InstanceIterator i = find(instances_.begin(), instances_.end(), instance);
  if (i == instances_.end()) {
    instances_.push_back(instance);
  }
}

void RenderTimer::RemoveInstance(NPP instance) {
  InstanceIterator i = find(instances_.begin(), instances_.end(), instance);
  if (i != instances_.end()) {
    instances_.erase(i);
  }
}

void RenderTimer::TimerCallback(CFRunLoopTimerRef timer, void* info) {
  HANDLE_CRASHES;
  for (int i = 0; i < instances_.size(); ++i) {
    NPP instance = instances_[i];
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);

    ManageSafariTabSwitching(obj);
    obj->client()->Tick();

    bool in_fullscreen = obj->GetFullscreenMacWindow();

    if (in_fullscreen) {
      obj->FullscreenIdle();
    }

    // We're visible if (a) we are in fullscreen mode or (b) our cliprect
    // height and width are both a sensible size, ie > 1 pixel.
    // We don't check for 0 as we have to size to 1 x 1 on occasion rather than
    // 0 x 0 to avoid crashing the Apple software renderer, but do not want to
    // actually draw to a 1 x 1 pixel area.
    bool plugin_visible = in_fullscreen ||
        (obj->last_buffer_rect_[2] > 1 && obj->last_buffer_rect_[3] > 1);

    if (plugin_visible && obj->WantsRedraw()) {
      obj->SetWantsRedraw(false);  // for on-demand drawing

      // Force a sync to the VBL (once per timer callback)
      // to avoid tearing
      GLint sync = (i == 0);
      if (obj->mac_cgl_context_) {
        CGLSetParameter(obj->mac_cgl_context_, kCGLCPSwapInterval, &sync);
      } else if (obj->mac_agl_context_) {
        aglSetInteger(obj->mac_agl_context_, AGL_SWAP_INTERVAL, &sync);
      }

      obj->client()->RenderClient();
    }
  }
}

#pragma mark ____BREAKPAD

bool ExceptionCallback(int exception_type,
                       int exception_code,
                       mach_port_t crashing_thread) {
  return BreakpadEnabler::IsEnabled();
}

void InitializeBreakpad() {
  if (!gBreakpadRef) {
    NSBundle* bundle = [NSBundle bundleWithIdentifier:@"com.google.o3d"];
    NSDictionary* info = [bundle infoDictionary];

    gBreakpadRef = BreakpadCreate(info);
    BreakpadSetFilterCallback(gBreakpadRef, ExceptionCallback);
  }
}

void ShutdownBreakpad() {
  BreakpadRelease(gBreakpadRef);
  gBreakpadRef = NULL;
}

#pragma mark ____MISCELLANEOUS_HELPER

void CFReleaseIfNotNull(CFTypeRef cf) {
  if (cf != NULL)
    CFRelease(cf);
}



// Converts an old style Mac HFS path eg "HD:Users:xxx:file.zip" into
// a standard Posix path eg "/Users/xxx/file.zip"
// Assumes UTF8 in and out, returns a block of memory allocated with new,
// so you'll want to delete this at some point.
// Returns NULL in the event of an error.
char* CreatePosixFilePathFromHFSFilePath(const char* hfsPath) {
  CFStringRef cfHFSPath = NULL;
  CFStringRef cfPosixPath = NULL;
  CFURLRef cfHFSURL = NULL;
  char* posix_path = NULL;

  if (hfsPath && hfsPath[0]) {
    cfHFSPath = CFStringCreateWithCString(kCFAllocatorDefault,
                                          hfsPath,
                                          kCFStringEncodingUTF8);
    if (cfHFSPath) {
      cfHFSURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                               cfHFSPath,
                                               kCFURLHFSPathStyle,
                                               false);
      if (cfHFSURL) {
        cfPosixPath = CFURLCopyFileSystemPath(cfHFSURL, kCFURLPOSIXPathStyle);
        if (cfPosixPath) {
          // returned value includes space for 0 terminator byte
          int maxSize =
              CFStringGetMaximumSizeOfFileSystemRepresentation(cfPosixPath);
          posix_path = new char [maxSize];
          CFStringGetFileSystemRepresentation(cfPosixPath,
                                              posix_path,
                                              maxSize);
        }
      }
    }
  }
  CFReleaseIfNotNull(cfHFSPath);
  CFReleaseIfNotNull(cfPosixPath);
  CFReleaseIfNotNull(cfHFSURL);
  return posix_path;
}



// A little wrapper for ATSUSetAttributes to make calling it with one attribute
// less annoying.
static void MySetAttribute(ATSUStyle style,
                           ATSUAttributeTag tag,
                           ByteCount size,
                           ATSUAttributeValuePtr value) {

  ATSUAttributeTag      tags[2]   = {tag, 0};
  ByteCount             sizes[2]  = {size, 0};
  ATSUAttributeValuePtr values[2] = {value, 0};

  ATSUSetAttributes(style, 1, tags, sizes, values);
}

// A little wrapper for ATSUSetLayoutControls to make calling it with one
// attribute less annoying.
static void MySetLayoutControl(ATSUTextLayout layout,
                               ATSUAttributeTag tag,
                               ByteCount size,
                               ATSUAttributeValuePtr value) {

  ATSUAttributeTag      tags[2]   = {tag, 0};
  ByteCount             sizes[2]  = {size, 0};
  ATSUAttributeValuePtr values[2] = {value, 0};

  ATSUSetLayoutControls(layout, 1, tags, sizes, values);
}



// Convenience function for fetching SInt32 parameters from Carbon EventRefs.
static SInt32 GetIntEventParam(EventRef inEvent, EventParamName    inName) {
  SInt32    value = 0;
  return (GetEventParameter(inEvent, inName, typeSInt32, NULL, sizeof(value),
                            NULL, &value) == noErr) ? value : 0;
}


#pragma mark ____OVERLAY_WINDOW

// Returns the unicode 16 chars that we need to display as the fullscreen
// message. Should be disposed with free() after use.
static UniChar * GetFullscreenDisplayText(int *returned_length) {
  // TODO this will need to be a localized string.
  NSString* ns_display_text = @"Press ESC to exit fullscreen";
  int count = [ns_display_text length];
  UniChar* display_text_16 = (UniChar*) calloc(count, sizeof(UniChar));

  [ns_display_text getCharacters:display_text_16];
  *returned_length = count;
  return display_text_16;
}


static void DrawToOverlayWindow(WindowRef overlayWindow) {
  CGContextRef overlayContext = NULL;
  OSStatus     result = noErr;
  CGFloat kWhiteOpaque[]  = {1.0, 1.0, 1.0, 1.0};
  CGFloat kBlackOpaque[]  = {0.0, 0.0, 0.0, 1.0};
  CGFloat kGreyNotOpaque[]  = {0.5, 0.5, 0.5, 0.5};
  CGFloat kBlackNotOpaque[]  = {0.0, 0.0, 0.0, 0.5};
  Rect bounds = {0, 0, 0, 0};
  const char* kOverlayWindowFontName = "Arial";
  const int kPointSize  = 22;
  const float kShadowRadius = 5.0;
  const float kRoundRectRadius = 9.0;
  const float kTextLeftMargin = 15.0;
  const float kTextBottomMargin = 22.0;

  QDBeginCGContext(GetWindowPort(overlayWindow), &overlayContext);
  GetWindowBounds(overlayWindow, kWindowContentRgn, &bounds);

  // Make the global rect local.
  bounds.right -= bounds.left;
  bounds.left = 0;
  bounds.bottom -= bounds.top;
  bounds.top = 0;

  CGRect cgTotalRect = Rect2CGRect(bounds);
  CGContextSetShouldSmoothFonts(overlayContext, true);
  CGContextClearRect(overlayContext, cgTotalRect);

  CGColorSpaceRef myColorSpace = CGColorSpaceCreateDeviceRGB();
  CGColorRef shadow = CGColorCreate(myColorSpace, kBlackNotOpaque);
  CGColorRef roundRectBackColor = CGColorCreate(myColorSpace, kBlackNotOpaque);
  CGSize shadowOffset = {0.0,0.0};

  CGContextSetFillColor(overlayContext, kWhiteOpaque);
  CGContextSetStrokeColor(overlayContext, kWhiteOpaque);

    // Draw the round rect background.
  CGContextSaveGState(overlayContext);
  CGContextSetFillColorWithColor(overlayContext, roundRectBackColor);
  CGRect cg_rounded_area =
      CGRectMake(// Offset from left and bottom to give shadow its space.
                 kShadowRadius, kShadowRadius,
                 // Increase width and height so rounded corners
                 // will be clipped out, except at bottom left.
                 (bounds.right - bounds.left) + 30,
                 (bounds.bottom - bounds.top) + 30);
  // Save state before applying shadow.
  CGContextSetShadowWithColor(overlayContext, shadowOffset,
                              kShadowRadius, shadow);
  PaintRoundedCGRect(overlayContext, cg_rounded_area, kRoundRectRadius, true);
  // Restore graphics state to remove shadow.
  CGContextRestoreGState(overlayContext);

  // Draw the text.
  int text_length = 0;
  UniChar* display_text = GetFullscreenDisplayText(&text_length);

  if ((text_length > 0) && (display_text != NULL)) {
    ATSUStyle         style;
    ATSUTextLayout    layout;
    ATSUFontID        font;
    Fixed             pointSize = Long2Fix(kPointSize);
    Boolean           is_bold = true;

    ATSUCreateStyle(&style);
    ATSUFindFontFromName(kOverlayWindowFontName, strlen(kOverlayWindowFontName),
                         kFontFullName, kFontNoPlatformCode, kFontNoScriptCode,
                         kFontNoLanguageCode, &font);

    MySetAttribute(style, kATSUFontTag, sizeof(font), &font);
    MySetAttribute(style, kATSUSizeTag, sizeof(pointSize), &pointSize);
    MySetAttribute(style, kATSUQDBoldfaceTag, sizeof(Boolean), &is_bold);


    ATSUCreateTextLayout(&layout);
    ATSUSetTextPointerLocation(layout, display_text,
                               kATSUFromTextBeginning, kATSUToTextEnd,
                               text_length);
    ATSUSetRunStyle(layout, style, kATSUFromTextBeginning, kATSUToTextEnd);

    MySetLayoutControl(layout, kATSUCGContextTag,
                       sizeof(CGContextRef),  &overlayContext);

    // Need to enable this for languages like Japanese to draw as something
    // other than a series of squares.
    ATSUSetTransientFontMatching(layout, true);


    CGContextSetFillColor(overlayContext, kWhiteOpaque);
    ATSUDrawText(layout, kATSUFromTextBeginning, kATSUToTextEnd,
                 X2Fix(kShadowRadius + kTextLeftMargin),
                 X2Fix(kShadowRadius + kTextBottomMargin));
    ATSUDisposeStyle(style);
    ATSUDisposeTextLayout(layout);
    free(display_text);
  }

  CGColorRelease(roundRectBackColor);
  CGColorRelease(shadow);
  CGColorSpaceRelease (myColorSpace);

  QDEndCGContext(GetWindowPort(overlayWindow), &overlayContext);
}

static OSStatus HandleOverlayWindow(EventHandlerCallRef inHandlerCallRef,
                                    EventRef inEvent,
                                    void *inUserData) {
  OSType event_class = GetEventClass(inEvent);
  OSType event_kind = GetEventKind(inEvent);

  if (event_class == kEventClassWindow &&
      event_kind == kEventWindowPaint) {
      WindowRef theWindow = NULL;
    GetEventParameter(inEvent, kEventParamDirectObject,
                      typeWindowRef, NULL,
                      sizeof(theWindow), NULL,
                      &theWindow);
    if (theWindow) {
      CallNextEventHandler(inHandlerCallRef, inEvent);
      DrawToOverlayWindow(theWindow);
    }
  }

  return noErr;
}



static Rect GetOverlayWindowRect(bool visible) {
#define kOverlayHeight 60
#define kOverlayWidth 340
  Rect screen_bounds = CGRect2Rect(CGDisplayBounds(CGMainDisplayID()));
  Rect hidden_window_bounds = {screen_bounds.top - kOverlayHeight,
                               screen_bounds.right - kOverlayWidth,
                               screen_bounds.top,
                               screen_bounds.right};
  Rect visible_window_bounds = {screen_bounds.top,
                                screen_bounds.right - kOverlayWidth,
                                screen_bounds.top + kOverlayHeight,
                                screen_bounds.right};

  return (visible) ? visible_window_bounds : hidden_window_bounds;
}


static WindowRef CreateOverlayWindow(void) {
  Rect        window_bounds = GetOverlayWindowRect(false);
  WindowClass wClass = kOverlayWindowClass;
  WindowRef   window = NULL;
  OSStatus    err = noErr;
  WindowAttributes  overlayAttributes = kWindowNoShadowAttribute |
                                        kWindowIgnoreClicksAttribute |
                                        kWindowNoActivatesAttribute |
                                        kWindowStandardHandlerAttribute;
  EventTypeSpec  eventTypes[] = {
    kEventClassWindow, kEventWindowPaint,
    kEventClassWindow, kEventWindowShown
  };

  err = CreateNewWindow(wClass,
                        overlayAttributes,
                        &window_bounds,
                        &window);
  if (err)
    return NULL;

  SetWindowLevel(window, CGShieldingWindowLevel() + 1);
  InstallEventHandler(GetWindowEventTarget(window), HandleOverlayWindow,
                      sizeof(eventTypes)/sizeof(eventTypes[0]), eventTypes,
                      NULL, NULL);
  ShowWindow(window);

  return window;
}


// Maps the MacOS button numbers to the constants used by our
// event mechanism.  Not quite as obvious as you might think, as the Mac
// thinks the numbering should go left, right, middle and our W3C-influenced
// system goes left, middle, right.
// Defaults to left-button if passed a strange value.  Pass Cocoa mouse button
// codes as-is (they start at 0), pass Carbon button codes - 1.
o3d::Event::Button MacOSMouseButtonNumberToO3DButton(int inButton) {

  switch(inButton) {
    case 0:
      return o3d::Event::BUTTON_LEFT;
    case 1:
      return o3d::Event::BUTTON_RIGHT;
    case 2:
      return o3d::Event::BUTTON_MIDDLE;
    case 3:
      return o3d::Event::BUTTON_4;
    case 4:
      return o3d::Event::BUTTON_5;
  }

  return o3d::Event::BUTTON_LEFT;
}


#pragma mark ____FULLSCREEN_WINDOW


// Handles the CarbonEvents that we get sent for the fullscreen mode window.
// Most of these can be converted to EventRecord events and handled by the
// HandleMacEvent() function in main_mac.mm, but some have no equivalent in
// that space, scroll-wheel events for example, and so must be handled here.
static OSStatus HandleFullscreenWindow(EventHandlerCallRef inHandlerCallRef,
                                       EventRef inEvent,
                                       void *inUserData) {
  OSStatus err = noErr;
  OSType event_class = GetEventClass(inEvent);
  OSType event_kind = GetEventKind(inEvent);
  NPP instance = (NPP)inUserData;
  PluginObject* obj = (PluginObject*)(instance->pdata);
  HIPoint mouse_loc = { 0.0, 0.0 };
  bool is_scroll_event = event_class == kEventClassMouse &&
                         (event_kind == kEventMouseScroll ||
                          event_kind == kEventMouseWheelMoved);

  // If it's any kind of mouse event, get the global mouse loc.
  if (event_class == kEventClassMouse) {
    GetEventParameter(inEvent, kEventParamMouseLocation,
                      typeHIPoint, NULL,
                      sizeof(mouse_loc), NULL,
                      &mouse_loc);
  }

  // Handle the two kinds of scroll message we understand.
  if (is_scroll_event) {
    SInt32 x_scroll = 0;
    SInt32 y_scroll = 0;
    EventMouseWheelAxis axis = kEventMouseWheelAxisY;

    switch (event_kind) {
      // The newer kind of scroll event, as sent when two-finger
      // dragging on a touchpad.
      case kEventMouseScroll:
        x_scroll = GetIntEventParam(inEvent,
                                    kEventParamMouseWheelSmoothHorizontalDelta);
        y_scroll = GetIntEventParam(inEvent,
                                    kEventParamMouseWheelSmoothVerticalDelta);

        // only pass x or y value - pass whichever is larger
        if (x_scroll && y_scroll) {
          if (abs(x_scroll) > abs(y_scroll))
            y_scroll = 0;
          else
            x_scroll = 0;
        }
        break;
        // The older kind of scroll event, as sent when using the wheel on
        // a third-party mouse.
      case kEventMouseWheelMoved:
        GetEventParameter(inEvent,  kEventParamMouseWheelAxis,
                          typeMouseWheelAxis, NULL,
                          sizeof(axis), NULL,
                          &axis);

        if (axis == kEventMouseWheelAxisY) {
          y_scroll = GetIntEventParam(inEvent,
                                      kEventParamMouseWheelDelta);
        } else {
          x_scroll = GetIntEventParam(inEvent,
                                      kEventParamMouseWheelDelta);
        }
        break;
    }

    // Dispatch the event now that we have all the data.
    if (x_scroll || y_scroll) {
      o3d::Event event(o3d::Event::TYPE_WHEEL);
      event.set_delta(x_scroll, y_scroll);
      // Global and local locs are the same, in this case,
      // as we have a fullscreen window at 0,0.
      event.set_position(mouse_loc.x, mouse_loc.y,
                         mouse_loc.x, mouse_loc.y, true);
      obj->client()->AddEventToQueue(event);
    }
    return noErr;
  } else if (event_class == kEventClassMouse &&
             (event_kind == kEventMouseDown || event_kind == kEventMouseUp)) {

    o3d::Event::Type type = (event_kind == kEventMouseDown) ?
                            o3d::Event::TYPE_MOUSEDOWN :
                            o3d::Event::TYPE_MOUSEUP;
    o3d::Event event(type);
    event.set_position(mouse_loc.x, mouse_loc.y,
                       mouse_loc.x, mouse_loc.y, true);

    EventMouseButton button = 0;
    GetEventParameter(inEvent,  kEventParamMouseButton,
                      typeMouseButton, NULL,
                      sizeof(button), NULL,
                      &button);
    // Carbon mouse button numbers start at 1, so subtract 1 here -
    // Cocoa mouse buttons, by contrast,  start at 0).
    event.set_button(MacOSMouseButtonNumberToO3DButton(button - 1));

    // add the modifiers to the event, if any
    UInt32 carbonMods = GetIntEventParam(inEvent,
                                         kEventParamKeyModifiers);
    if (carbonMods) {
      int modifier_state = 0;
      if (carbonMods & controlKey) {
        modifier_state |= o3d::Event::MODIFIER_CTRL;
      }
      if (carbonMods & shiftKey) {
        modifier_state |= o3d::Event::MODIFIER_SHIFT;
      }
      if (carbonMods & optionKey) {
        modifier_state |= o3d::Event::MODIFIER_ALT;
      }
      if (carbonMods & cmdKey) {
        modifier_state |= o3d::Event::MODIFIER_META;
      }

      event.set_modifier_state(modifier_state);
    }

    obj->client()->AddEventToQueue(event);
  } else {  // not a scroll event or a click

    // All other events are currently handled by being converted to an
    // old-style EventRecord as passed by the classic NPAPI interface
    // and dispatched through our common routine.
    EventRecord eventRecord;

    if (ConvertEventRefToEventRecord(inEvent, &eventRecord)) {
      HandleMacEvent(&eventRecord, (NPP)inUserData);
      return noErr;
    } else {
      return eventNotHandledErr;
    }
  }
}


static WindowRef CreateFullscreenWindow(WindowRef window,
                                        PluginObject *obj,
                                        int mode_id) {
  Rect        bounds = CGRect2Rect(CGDisplayBounds(CGMainDisplayID()));
  OSStatus    err = noErr;
  EventTypeSpec  eventTypes[] = {
    kEventClassKeyboard, kEventRawKeyDown,
    kEventClassKeyboard, kEventRawKeyRepeat,
    kEventClassKeyboard, kEventRawKeyUp,
    kEventClassMouse,    kEventMouseDown,
    kEventClassMouse,    kEventMouseUp,
    kEventClassMouse,    kEventMouseMoved,
    kEventClassMouse,    kEventMouseDragged,
    kEventClassMouse,    kEventMouseScroll,
    kEventClassMouse,    kEventMouseWheelMoved
  };

  if (window == NULL)
    err = CreateNewWindow(kSimpleWindowClass,
                          kWindowStandardHandlerAttribute,
                          &bounds,
                          &window);
  if (err)
    return NULL;

  SetWindowLevel(window, CGShieldingWindowLevel() + 1);

  InstallEventHandler(GetWindowEventTarget(window), HandleFullscreenWindow,
                      sizeof(eventTypes)/sizeof(eventTypes[0]), eventTypes,
                      obj->npp(), NULL);
  ShowWindow(window);
  return window;
}

void CleanupFullscreenWindow(PluginObject *obj) {
  WindowRef fs_window = obj->GetFullscreenMacWindow();
  WindowRef fs_o_window = obj->GetFullscreenOverlayMacWindow();

  obj->SetFullscreenMacWindow(NULL);
  obj->SetFullscreenOverlayMacWindow(NULL);

  if (fs_window) {
    HideWindow(fs_window);
    ReleaseWindowGroup(GetWindowGroup(fs_window));
    DisposeWindow(fs_window);
  }

  if(fs_o_window) {
    HideWindow(fs_o_window);
    ReleaseWindowGroup(GetWindowGroup(fs_o_window));
    DisposeWindow(fs_o_window);
  }
}

#pragma mark ____SCREEN_RESOLUTION_MANAGEMENT


// Constant kO3D_MODE_OFFSET is added to the position in the array returned by
// CGDisplayAvailableModes to make it an ID. This makes IDs distinguishable from
// array positions when debugging, and also means that ID 0 can have a special
// meaning (current mode) rather than meaning the first resolution in the list.
const int kO3D_MODE_OFFSET = 100;

// Extracts data from the Core Graphics screen mode data passed in.
// Returns false if the mode is an undesirable one, ie if it is not safe for
// the current screen hardware, is stretched, or is interlaced.
// Returns various information about the mode in the var parameters passed.
static bool ExtractDisplayModeData(NSDictionary *mode_dict,
                                   int *width,
                                   int *height,
                                   int *refresh_rate,
                                   int *bits_per_pixel) {

  *width = [[mode_dict objectForKey:(id)kCGDisplayWidth] intValue];
  *height = [[mode_dict objectForKey:(id)kCGDisplayHeight] intValue];
  *refresh_rate = [[mode_dict objectForKey:(id)kCGDisplayRefreshRate] intValue];
  *bits_per_pixel =
      [[mode_dict objectForKey:(id)kCGDisplayBitsPerPixel] intValue];

  if (![mode_dict objectForKey:(id)kCGDisplayModeIsSafeForHardware])
    return false;

  if ([mode_dict objectForKey:(id)kCGDisplayModeIsStretched])
    return false;

  if ([mode_dict objectForKey:(id)kCGDisplayModeIsInterlaced])
    return false;

  return true;
}


// Returns information on a display mode, which is mode n - kO3D_MODE_OFFSET
// in the raw list returned by CGDisplayAvailableModes on the main screen,
// with kO3D_MODE_OFFSET + 0 being the first entry in the array.
static bool GetOtherDisplayMode(int id, o3d::DisplayMode *mode) {
  NSArray *mac_modes = (NSArray*) CGDisplayAvailableModes(CGMainDisplayID());
  int num_modes = [mac_modes count];
  int array_offset = id - kO3D_MODE_OFFSET;

  if (array_offset >= 0 && array_offset < num_modes) {
    int width = 0;
    int height = 0;
    int refresh_rate = 0;
    int bpp = 0;

    ExtractDisplayModeData([mac_modes objectAtIndex:array_offset],
                           &width, &height, &refresh_rate, &bpp);
    mode->Set(width, height, refresh_rate, id);
    return true;
  }

  return false;
}


static int GetCGDisplayModeID(NSDictionary* mode_dict) {
  return [[mode_dict valueForKey:@"Mode"] intValue];
}

// Returns DisplayMode data for the current state of the main display.
static void GetCurrentDisplayMode(o3d::DisplayMode *mode) {
  int width = 0;
  int height = 0;
  int refresh_rate = 0;
  int bpp = 0;
  int mode_id = 0;

  NSDictionary* current_mode =
      (NSDictionary*)CGDisplayCurrentMode(CGMainDisplayID());

  // To get the O3D mode id of the current mode, we need to find it in the list
  // of all modes, since the id we use is it's index + kO3D_MODE_OFFSET.

  // Get the CG id of current mode so that we will recognize it.
  int current_cg_id = GetCGDisplayModeID(current_mode);

  // Get list of all modes.
  NSArray *modes = (NSArray*)CGDisplayAvailableModes(CGMainDisplayID());
  int num_modes = [modes count];

  // Find current mode in that list, and compute the O3D id for it.
  for (int x = 0 ; x < num_modes ; x++) {
    if (GetCGDisplayModeID([modes objectAtIndex:x]) == current_cg_id) {
      mode_id = x + kO3D_MODE_OFFSET;
      break;
    }
  }

  ExtractDisplayModeData(current_mode, &width, &height, &refresh_rate, &bpp);
  mode->Set(width, height, refresh_rate, mode_id);
}


bool PluginObject::GetDisplayMode(int id, o3d::DisplayMode *mode) {
  if (id == o3d::Renderer::DISPLAY_MODE_DEFAULT) {
    GetCurrentDisplayMode(mode);
    return true;
  } else {
    return GetOtherDisplayMode(id, mode);
  }
}


void PluginObject::GetDisplayModes(std::vector<o3d::DisplayMode> *modes) {
  NSArray* mac_modes = (NSArray*)CGDisplayAvailableModes(CGMainDisplayID());
  int num_modes = [mac_modes count];
  std::vector<o3d::DisplayMode> modes_found;

  for (int i = 0; i < num_modes; ++i) {
    int width = 0;
    int height = 0;
    int refresh_rate = 0;
    int bpp = 0;

    if (o3d::ExtractDisplayModeData([mac_modes objectAtIndex:i],
        &width,
        &height,
        &refresh_rate,
        &bpp) && bpp == 32)
      modes_found.push_back(o3d::DisplayMode(width, height, refresh_rate,
                                             i + o3d::kO3D_MODE_OFFSET));
  }

  modes->swap(modes_found);
}


#pragma mark ____FULLSCREEN_SWITCHING

#define kTransitionTime 1.0

bool PluginObject::RequestFullscreenDisplay() {
  // If already in fullscreen mode, do nothing.
  if (GetFullscreenMacWindow())
    return false;

  int target_width = 0;
  int target_height = 0;

  if (fullscreen_region_valid_ &&
      fullscreen_region_mode_id_ != Renderer::DISPLAY_MODE_DEFAULT) {
    o3d::DisplayMode the_mode;
    if (GetDisplayMode(fullscreen_region_mode_id_, &the_mode)) {
      target_width = the_mode.width();
      target_height = the_mode.height();
    }
  }

  // check which mode we are in now
  o3d::DisplayMode current_mode;
  GetCurrentDisplayMode(&current_mode);

  WindowRef fullscreen_window = NULL;

  // Determine if screen mode switching is actually required.
  if (target_width != 0 &&
      target_height != 0 &&
      target_width != current_mode.width() &&
      target_height != current_mode.height()) {
    short short_target_width = target_width;
    short short_target_height = target_height;
    BeginFullScreen(&mac_fullscreen_state_,
                    GetMainDevice(),
                    &short_target_width,
                    &short_target_height,
                    &fullscreen_window,
                    NULL,
                    fullScreenCaptureAllDisplays);
  } else {
    SetSystemUIMode(kUIModeAllSuppressed, kUIOptionAutoShowMenuBar);
    mac_fullscreen_state_ = NULL;
  }

  SetFullscreenMacWindow(o3d::CreateFullscreenWindow(
      NULL,
      this,
      fullscreen_region_mode_id_));
  Rect bounds = {0,0,0,0};
  GetWindowBounds(GetFullscreenMacWindow(), kWindowContentRgn, &bounds);

  o3d::SetWindowForAGLContext(mac_agl_context_, GetFullscreenMacWindow());
  aglDisable(mac_agl_context_, AGL_BUFFER_RECT);
  renderer()->SetClientOriginOffset(0, 0);
  renderer_->Resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

  fullscreen_ = true;
  client()->SendResizeEvent(renderer_->width(), renderer_->height(), true);

  SetFullscreenOverlayMacWindow(o3d::CreateOverlayWindow());
  ShowWindow(mac_fullscreen_overlay_window_);
  o3d::SlideWindowToRect(mac_fullscreen_overlay_window_,
                         o3d::Rect2CGRect(o3d::GetOverlayWindowRect(true)),
                         kTransitionTime);

  // Hide the overlay text 4 seconds from now.
  time_to_hide_overlay_ = [NSDate timeIntervalSinceReferenceDate] + 4.0;

  return true;
}

void PluginObject::CancelFullscreenDisplay() {
  // if not in fullscreen mode, do nothing
  if (!GetFullscreenMacWindow())
    return;

  o3d::SetWindowForAGLContext(mac_agl_context_, mac_window_);

  o3d::CleanupFullscreenWindow(this);

  renderer_->Resize(prev_width_, prev_height_);
  aglSetInteger(mac_agl_context_, AGL_BUFFER_RECT, last_buffer_rect_);
  aglEnable(mac_agl_context_, AGL_BUFFER_RECT);

  if (mac_fullscreen_state_) {
    EndFullScreen(mac_fullscreen_state_, 0);
    mac_fullscreen_state_ = NULL;
  } else {
    SetSystemUIMode(kUIModeNormal, 0);
  }
  fullscreen_ = false;
  client()->SendResizeEvent(prev_width_, prev_height_, false);


  // Somehow the browser window does not automatically activate again
  // when we close the fullscreen window, so explicitly reactivate it.
  if (mac_cocoa_window_) {
    NSWindow* browser_window = (NSWindow*) mac_cocoa_window_;
    [browser_window makeKeyAndOrderFront:browser_window];
  } else if (mac_window_) {
    SelectWindow(mac_window_);
  }
}

void PluginObject::FullscreenIdle() {
  if ((mac_fullscreen_overlay_window_ != NULL) &&
      (time_to_hide_overlay_ != 0.0) &&
      (time_to_hide_overlay_ < [NSDate timeIntervalSinceReferenceDate])) {
    time_to_hide_overlay_ = 0.0;
    o3d::SlideWindowToRect(mac_fullscreen_overlay_window_,
                           o3d::Rect2CGRect(o3d::GetOverlayWindowRect(false)),
                           kTransitionTime);
  }
}

}  // namespace o3d
