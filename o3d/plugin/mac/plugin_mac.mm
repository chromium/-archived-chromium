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
#include "plugin/cross/o3d_glue.h"
#include "plugin/cross/main.h"
#include "core/mac/display_window_mac.h"

BreakpadRef gBreakpadRef =  NULL;

using glue::_o3d::PluginObject;
using o3d::DisplayWindowMac;

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


@interface NSWindowController (plugin_hack)
- (id)selectedTab;
@end

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

    // We're visible if (a) we are in fullscreen mode or (b) our cliprect
    // height and width are both a sensible size, ie > 1 pixel.
    // We don't check for 0 as we have to size to 1 x 1 on occasion rather than
    // 0 x 0 to avoid crashing the Apple software renderer, but do not want to
    // actually draw to a 1 x 1 pixel area.
    bool plugin_visible = obj->GetFullscreenMacWindow() ||
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


// Given a WindowRef and an AGLContext, make the context draw in that window.
// Return Value: true if the window is successfully set, false otherwise.
bool SetWindowForAGLContext(AGLContext context, WindowRef window) {
  return (IsMacOSTenFiveOrHigher()) ?
      aglSetWindowRef(context, window) :
      aglSetDrawable(context, GetWindowPort(window));
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

// Returns whether OS is 10.5 (Leopard) or higher.
bool IsMacOSTenFiveOrHigher() {
  static bool isCached = false, result = false;

  if (!isCached) {
    SInt32 major = 0;
    SInt32 minor = 0;
    // These selectors don't exist pre 10.4 but as we check the error
    // the function will correctly return NO which is the right answer.
    result = ((::Gestalt(gestaltSystemVersionMajor,  &major) == noErr) &&
              (::Gestalt(gestaltSystemVersionMinor,  &minor) == noErr) &&
              ((major > 10) || (major == 10 && minor >= 5)));
    isCached = true;
  }
  return result;
}


static Rect CGRect2Rect(const CGRect &inRect) {
  Rect outRect;
  outRect.left = inRect.origin.x;
  outRect.top = inRect.origin.y;
  outRect.right = inRect.origin.x + inRect.size.width;
  outRect.bottom = inRect.origin.y + inRect.size.height;
  return outRect;
}

static CGRect Rect2CGRect(const Rect &inRect) {
  CGRect outRect;
  outRect.origin.x = inRect.left;
  outRect.origin.y = inRect.top;
  outRect.size.width = inRect.right - inRect.left;
  outRect.size.height = inRect.bottom - inRect.top;
  return outRect;
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

static OSStatus HandleOverlayWindow(EventHandlerCallRef inHandlerCallRef,
                                    EventRef inEvent,
                                    void *inUserData) {
  return noErr;
}

static void PaintRoundedCGRect(CGContextRef context,
                               CGRect rect,
                               float radius,
                               bool fill) {
  CGFloat lx = CGRectGetMinX(rect);
  CGFloat cx = CGRectGetMidX(rect);
  CGFloat rx = CGRectGetMaxX(rect);
  CGFloat by = CGRectGetMinY(rect);
  CGFloat cy = CGRectGetMidY(rect);
  CGFloat ty = CGRectGetMaxY(rect);

  CGContextBeginPath(context);
  CGContextMoveToPoint(context, lx, cy);
  CGContextAddArcToPoint(context, lx, by, cx, by, radius);
  CGContextAddArcToPoint(context, rx, by, rx, cy, radius);
  CGContextAddArcToPoint(context, rx, ty, cx, ty, radius);
  CGContextAddArcToPoint(context, lx, ty, lx, cy, radius);
  CGContextClosePath(context);

  if (fill)
    CGContextFillPath(context);
  else
    CGContextStrokePath(context);
}

// Convenience function for fetching SInt32 parameters from Carbon EventRefs.
static SInt32 GetIntEventParam(EventRef inEvent, EventParamName    inName) {
  SInt32    value = 0;
  return (GetEventParameter(inEvent, inName, typeSInt32, NULL, sizeof(value),
                            NULL, &value) == noErr) ? value : 0;
}


#pragma mark ____OVERLAY_WINDOW


static void DrawToOverlayWindow(WindowRef overlayWindow) {
#define kTextWindowHeight 40

  CGContextRef overlayContext = NULL;
  OSStatus     result = noErr;
  // TODO this will need to be a localized string
  char *       display_text = "Press Esc to exit full screen mode.";

  UniChar*     display_text_16 = NULL;
  CFStringRef  display_text_cfstr = NULL;
  int          textLength = 0;

  QDBeginCGContext(GetWindowPort(overlayWindow), &overlayContext);

  display_text_cfstr = CFStringCreateWithCString(kCFAllocatorDefault,
                                                 display_text,
                                                 kCFStringEncodingUTF8);
  textLength = CFStringGetLength(display_text_cfstr);
  display_text_16 = (UniChar*)calloc(textLength + 1, sizeof(*display_text_16));
  CFStringGetCharacters(display_text_cfstr,
                        CFRangeMake(0, textLength),
                        display_text_16);

#define kOverlayWindowFontName "Helvetica"

  CGFloat kWhiteOpaque[]  = {1.0, 1.0, 1.0, 1.0};
  CGFloat kBlackOpaque[]  = {0.0, 0.0, 0.0, 1.0};
  CGFloat kGreyNotOpaque[]  = {0.5, 0.5, 0.5, 0.5};

  Rect bounds = {0,0,100,100};
  GetWindowBounds(overlayWindow, kWindowContentRgn, &bounds);

  CGRect cgTotalRect = Rect2CGRect(bounds);

  CGContextSetShouldSmoothFonts(overlayContext, true);
  CGContextClearRect(overlayContext, cgTotalRect);

  CGColorSpaceRef myColorSpace = CGColorSpaceCreateDeviceRGB();
  CGColorRef textShadow = CGColorCreate(myColorSpace, kBlackOpaque);
  CGColorRef roundRectBackColor = CGColorCreate(myColorSpace, kGreyNotOpaque);
  CGSize shadowOffset = {0.0,0.0};

  CGContextSetFillColor(overlayContext, kWhiteOpaque);
  CGContextSetStrokeColor(overlayContext, kWhiteOpaque);

  if (strlen(display_text)) {
    ATSUStyle         style;
    ATSUTextLayout    layout;
    ATSUFontID        font;
    Fixed             pointSize = Long2Fix(36);

    ATSUCreateStyle(&style);
    ATSUFindFontFromName(kOverlayWindowFontName, strlen(kOverlayWindowFontName),
                         kFontFullName, kFontNoPlatformCode, kFontNoScriptCode,
                         kFontNoLanguageCode, &font);

    MySetAttribute(style, kATSUFontTag, sizeof(font), &font);
    MySetAttribute(style, kATSUSizeTag, sizeof(pointSize), &pointSize);

    ATSUCreateTextLayout(&layout);
    ATSUSetTextPointerLocation(layout, display_text_16,
                               kATSUFromTextBeginning, kATSUToTextEnd,
                               textLength);
    ATSUSetRunStyle(layout, style, kATSUFromTextBeginning, kATSUToTextEnd);

    MySetLayoutControl(layout, kATSUCGContextTag,
                       sizeof(CGContextRef),  &overlayContext);

    // Need to enable this for languages like Japanese to draw as something
    // other than a series of squares.
    ATSUSetTransientFontMatching(layout, true);

    CGContextSetFillColorWithColor(overlayContext, roundRectBackColor);
    CGRect mine = CGRectMake((bounds.right/2.0) - 400.0,
                             (bounds.bottom/2.0)-30.0, 800.0, 80.0);
    PaintRoundedCGRect(overlayContext, mine, 16.0, true);

    CGContextSetFillColor(overlayContext, kWhiteOpaque);

    CGContextSaveGState(overlayContext);
    CGContextSetShadowWithColor(overlayContext, shadowOffset, 4.0, textShadow);
    ATSUDrawText(layout, kATSUFromTextBeginning, kATSUToTextEnd,
                 X2Fix((bounds.right/2.0) - 300.0), X2Fix(bounds.bottom/2.0));
    CGContextRestoreGState(overlayContext);

    ATSUDisposeStyle(style);
    ATSUDisposeTextLayout(layout);
  }


  CGColorRelease(roundRectBackColor);
  CGColorRelease (textShadow);
  CGColorSpaceRelease (myColorSpace);
  CFReleaseIfNotNull(display_text_cfstr);

  QDEndCGContext(GetWindowPort(overlayWindow), &overlayContext);
}


static WindowRef CreateOverlayWindow(void) {
  Rect        bounds = CGRect2Rect(CGDisplayBounds(CGMainDisplayID()));
  WindowClass wClass = kOverlayWindowClass;
  WindowRef   window = NULL;
  OSStatus    err = noErr;
  WindowAttributes  overlayAttributes = kWindowNoShadowAttribute |
                                        kWindowIgnoreClicksAttribute |
                                        kWindowNoActivatesAttribute |
                                        kWindowStandardHandlerAttribute;
  EventTypeSpec  eventTypes[] = {
    kEventClassWindow,  kEventWindowDrawContent,
    kEventClassWindow, kEventWindowShown
  };


  err = CreateNewWindow(wClass,
                        overlayAttributes,
                        &bounds,
                        &window);
  if (err)
    return NULL;

  ShowWindow(window);
  InstallEventHandler(GetWindowEventTarget(window), HandleOverlayWindow,
                      sizeof(eventTypes)/sizeof(eventTypes[0]), eventTypes,
                      NULL, NULL);
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

static WindowRef CreateFullscreenWindow(PluginObject *obj,
                                        int mode_id) {
  Rect        bounds = CGRect2Rect(CGDisplayBounds(CGMainDisplayID()));
  WindowRef   window = NULL;
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

  err = CreateNewWindow(kSimpleWindowClass,
                        kWindowStandardHandlerAttribute,
                        &bounds,
                        &window);
  if (err)
    return NULL;

  InstallEventHandler(GetWindowEventTarget(window), HandleFullscreenWindow,
                      sizeof(eventTypes)/sizeof(eventTypes[0]), eventTypes,
                      obj->npp(), NULL);
  ShowWindow(window);
  return window;
}

void CleanupFullscreenWindow(PluginObject *obj) {
  if (obj->GetFullscreenMacWindow())   {
    DisposeWindow(obj->GetFullscreenMacWindow());
    obj->SetFullscreenMacWindow(NULL);
  }

  if (obj->GetFullscreenOverlayMacWindow()) {
    DisposeWindow(obj->GetFullscreenOverlayMacWindow());
    obj->SetFullscreenOverlayMacWindow(NULL);
  }
}


bool PluginObject::RequestFullscreenDisplay() {
#ifndef NDEBUG  // TODO: Remove after all security is in.
  // If already in fullscreen mode, do nothing.
  if (GetFullscreenMacWindow())
    return false;

  SetSystemUIMode(kUIModeAllSuppressed, kUIOptionAutoShowMenuBar);
  SetFullscreenMacWindow(
      CreateFullscreenWindow(this, fullscreen_region_mode_id_));

  Rect bounds = {0,0,0,0};
  GetWindowBounds(GetFullscreenMacWindow(), kWindowContentRgn, &bounds);

  SetWindowForAGLContext(mac_agl_context_, GetFullscreenMacWindow());
  aglDisable(mac_agl_context_, AGL_BUFFER_RECT);
  renderer()->SetClientOriginOffset(0, 0);
  renderer_->Resize(bounds.right - bounds.left, bounds.bottom - bounds.top);

  const double kFadeOutTime = 3.0;
  SetFullscreenOverlayMacWindow(CreateOverlayWindow());
  DrawToOverlayWindow(GetFullscreenOverlayMacWindow());
  TransitionWindowOptions options = {0, kFadeOutTime, NULL, NULL};
  TransitionWindowWithOptions(GetFullscreenOverlayMacWindow(),
                              kWindowFadeTransitionEffect,
                              kWindowHideTransitionAction,
                              NULL, true, &options);
  return true;
#else
  return false;
#endif
}

void PluginObject::CancelFullscreenDisplay() {
#ifndef NDEBUG  // TODO: Remove after user-prompt feature goes in.

  // if not in fullscreen mode, do nothing
  if (!GetFullscreenMacWindow())
    return;

  //fullscreen_ = false;
  SetWindowForAGLContext(mac_agl_context_, mac_window_);
  aglSetInteger(mac_agl_context_, AGL_BUFFER_RECT, last_buffer_rect_);
  aglEnable(mac_agl_context_, AGL_BUFFER_RECT);
  renderer_->Resize(prev_width_, prev_height_);
  CleanupFullscreenWindow(this);

  // Somehow the browser window does not automatically activate again
  // when we close the fullscreen window, so explicitly reactivate it.
  if (mac_cocoa_window_) {
    NSWindow* browser_window = (NSWindow*) mac_cocoa_window_;
    [browser_window makeKeyAndOrderFront:browser_window];
  } else {
    SelectWindow(mac_window_);
  }

  SetSystemUIMode(kUIModeNormal, 0);
#endif
}

