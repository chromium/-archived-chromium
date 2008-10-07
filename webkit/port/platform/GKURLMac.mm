// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "KURL.h"
#include "FoundationExtras.h"

// TODO(playmobil): Write unit tests for this file. 

#ifdef USE_GOOGLE_URL_LIBRARY

namespace WebCore {

CFURLRef KURL::createCFURL() const {
  const CString &utf8_url = m_url.utf8String();
  // NOTE: We use UTF-8 here since this encoding is used when computing strings 
  // when returning URL components (e.g calls to NSURL -path). However, this 
  // function is not tolerant of illegal UTF-8 sequences, which could either be
  // a malformed string or bytes in a different encoding, like Shift-JIS, so we 
  // fall back onto using ISO Latin-1 in those cases.
  CFURLRef result = CFURLCreateAbsoluteURLWithBytes(
                        0, 
                        reinterpret_cast<const UInt8*>(utf8_url.data()), 
                        utf8_url.length(),
                        kCFStringEncodingUTF8,
                        0,
                        true);
  if (!result)
    result = CFURLCreateAbsoluteURLWithBytes(0, 
                                             reinterpret_cast<const UInt8*>(
                                                 utf8_url.data()), 
                                             utf8_url.length(), 
                                             kCFStringEncodingISOLatin1, 
                                             0, 
                                             true);
  return result;
}
 
  
// TODO(playmobil): also implement the KURL::KURL(CFURLRef) version.
KURL::KURL(NSURL *url) {
  if (!url) {
    init(KURL(), String(), NULL);
  } else {
    CFIndex bytesLength = CFURLGetBytes(reinterpret_cast<CFURLRef>(url), 0, 0);
    Vector<char, 512> buffer(bytesLength + 1); // 1 for null character to end C string
    char* bytes = &buffer[0];
    CFURLGetBytes(reinterpret_cast<CFURLRef>(url), reinterpret_cast<UInt8*>(bytes), bytesLength);
    bytes[bytesLength] = '\0';
    init(KURL(), String(bytes, bytesLength), 0);
  }
}

KURL::operator NSURL* () const {
  if (!m_isValid)
    return nil;
  
  // CFURL can't hold an empty URL, unlike NSURL.
  if (isEmpty())
    return [NSURL URLWithString:@""];
  
  return HardAutorelease(createCFURL());
}

}  // namespace WebCore

#endif  // USE_GOOGLE_URL_LIBRARY
