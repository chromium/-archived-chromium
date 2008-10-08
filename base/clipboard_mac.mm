// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/string_util.h"

namespace {

// Would be nice if this were in UTCoreTypes.h, but it isn't
const NSString* kUTTypeURLName = @"public.url-name";

NSString* nsStringForWString(const std::wstring& string) {
  string16 text16 = WideToUTF16(string);
  return [NSString stringWithCharacters:text16.c_str() length:text16.length()];
}

}  // namespace

Clipboard::Clipboard() {
}

Clipboard::~Clipboard() {
}

void Clipboard::Clear() {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb declareTypes:[NSArray array] owner:nil];
}

void Clipboard::WriteText(const std::wstring& text) {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
NSLog(@"pasteboard: %@", pb);
  [pb addTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
NSString* f = nsStringForWString(text);
NSLog(@"%@", f);
  [pb setString:nsStringForWString(text) forType:NSStringPboardType];
NSArray* types = [pb types];
NSLog(@"%@", [types description]);
}

void Clipboard::WriteHTML(const std::wstring& markup,
                          const std::string& src_url) {
  // TODO(avi): src_url?
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb addTypes:[NSArray arrayWithObject:NSHTMLPboardType] owner:nil];
  [pb setString:nsStringForWString(markup) forType:NSHTMLPboardType];
}

void Clipboard::WriteBookmark(const std::wstring& title,
                              const std::string& url) {
  WriteHyperlink(title, url);
}

void Clipboard::WriteHyperlink(const std::wstring& title,
                               const std::string& url) {
  // TODO(playmobil): In the Windows version of this function, an HTML
  // representation of the bookmark is also added to the clipboard, to support
  // drag and drop of web shortcuts.  I don't think we need to do this on the
  // Mac, but we should double check later on.
  NSURL* nsurl = [NSURL URLWithString:
                  [NSString stringWithUTF8String:url.c_str()]];
  NSString* nstitle = nsStringForWString(title);

  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  // passing UTIs into the pasteboard methods is valid >= 10.5
  [pb addTypes:[NSArray arrayWithObjects:NSURLPboardType,
                    kUTTypeURLName, nil]
             owner:nil];
  [nsurl writeToPasteboard:pb];
  [pb setString:nstitle forType:kUTTypeURLName];
}

void Clipboard::WriteFile(const std::wstring& file) {
  std::vector<std::wstring> files;
  files.push_back(file);
  WriteFiles(files);
}

void Clipboard::WriteFiles(const std::vector<std::wstring>& files) {
  NSMutableArray* fileList = [NSMutableArray arrayWithCapacity:files.size()];
  for (unsigned int i = 0; i < files.size(); ++i) {
    [fileList addObject:nsStringForWString(files[i])];
  }

  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb addTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
  [pb setPropertyList:fileList forType:NSFilenamesPboardType];
}

bool Clipboard::IsFormatAvailable(NSString* format) const {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSArray* types = [pb types];

  return [types containsObject:format];
}

void Clipboard::ReadText(std::wstring* result) const {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSString* contents = [pb stringForType:NSStringPboardType];

  UTF8ToWide([contents UTF8String],
             [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
             result);
}

void Clipboard::ReadAsciiText(std::string* result) const {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSString* contents = [pb stringForType:NSStringPboardType];

  *result = std::string([contents UTF8String]);
}

void Clipboard::ReadHTML(std::wstring* markup, std::string* src_url) const {
  if (markup) {
    markup->clear();

    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    NSArray *supportedTypes = [NSArray arrayWithObjects:NSHTMLPboardType,
                                                        NSStringPboardType,
                                                        nil];
    NSString *bestType = [pb availableTypeFromArray:supportedTypes];
    NSString* contents = [pb stringForType:bestType];
    UTF8ToWide([contents UTF8String],
               [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
               markup);
  }

  // TODO(avi): src_url?
  if (src_url)
    src_url->clear();
}

void Clipboard::ReadBookmark(std::wstring* title, std::string* url) const {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];

  if (title) {
    title->clear();

    NSString* contents = [pb stringForType:kUTTypeURLName];
    UTF8ToWide([contents UTF8String],
               [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
               title);
  }

  if (url) {
    url->clear();

    NSURL* nsurl = [NSURL URLFromPasteboard:pb];
    *url = std::string([[nsurl absoluteString] UTF8String]);
  }
}

void Clipboard::ReadFile(std::wstring* file) const {
  if (!file) {
    NOTREACHED();
    return;
  }

  file->clear();
  std::vector<std::wstring> files;
  ReadFiles(&files);

  // Take the first file, if available.
  if (!files.empty())
    file->assign(files[0]);
}

void Clipboard::ReadFiles(std::vector<std::wstring>* files) const {
  if (!files) {
    NOTREACHED();
    return;
  }

  files->clear();

  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSArray* fileList = [pb propertyListForType:NSFilenamesPboardType];

  for (unsigned int i = 0; i < [fileList count]; ++i) {
    std::wstring file = UTF8ToWide([[fileList objectAtIndex:i] UTF8String]);
    files->push_back(file);
  }
}

// static
Clipboard::FormatType Clipboard::GetUrlFormatType() {
  return NSURLPboardType;
}

// static
Clipboard::FormatType Clipboard::GetUrlWFormatType() {
  return NSURLPboardType;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  return NSStringPboardType;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  return NSStringPboardType;
}

// static
Clipboard::FormatType Clipboard::GetFilenameFormatType() {
  return NSFilenamesPboardType;
}

// static
Clipboard::FormatType Clipboard::GetFilenameWFormatType() {
  return NSFilenamesPboardType;
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  return NSHTMLPboardType;
}
