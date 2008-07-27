/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef KURL_h
#define KURL_h

#include "DeprecatedString.h"
#include "PlatformString.h"
#include <wtf/Platform.h>

#ifdef USE_GOOGLE_URL_LIBRARY
#include "CString.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#endif

#if PLATFORM(CF)
typedef const struct __CFURL * CFURLRef;
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURL;
#else
class NSURL;
#endif
#endif
#if PLATFORM(QT)
class QUrl;
#endif

namespace WebCore {

    class KURL;
    class TextEncoding;

    bool operator==(const KURL&, const KURL&);
    inline bool operator!=(const KURL &a, const KURL &b) { return !(a == b); }

    bool equalIgnoringRef(const KURL&, const KURL&);

class KURL {
public:
    KURL();
    KURL(const char*);
    KURL(const KURL&, const DeprecatedString&);
    KURL(const KURL&, const DeprecatedString&, const TextEncoding&);
    KURL(const DeprecatedString&);
#if PLATFORM(MAC)
    KURL(NSURL*);
#endif
#if PLATFORM(CF)
    KURL(CFURLRef);
#endif
#if PLATFORM(QT)
    KURL(const QUrl&);
#endif
#ifdef USE_GOOGLE_URL_LIBRARY
    // For conversions for other structures that have already parsed and
    // canonicalized the URL. The input must be exactly what KURL would have
    // done with the same input.
    KURL(const char* canonical_spec, int canonical_spec_len,
         const url_parse::Parsed& parsed, bool is_valid);
#endif

    bool isValid() const { return m_isValid; }
    bool hasPath() const;

#ifdef USE_GOOGLE_URL_LIBRARY
    bool isNull() const { return m_url.utf8String().isNull(); }
    bool isEmpty() const { return m_url.utf8String().length() == 0; }

    const String& string() const { return m_url.string(); }
    DeprecatedString deprecatedString() const { return m_url.deprecatedString(); }
#else
    bool isNull() const { return urlString.isNull(); }
    bool isEmpty() const { return urlString.isEmpty(); }

    String string() const { return urlString; }
    const DeprecatedString& deprecatedString() const { return urlString; }
#endif

    DeprecatedString protocol() const;
    DeprecatedString host() const;
    unsigned short int port() const;
    DeprecatedString user() const;
    DeprecatedString pass() const;
    DeprecatedString path() const;
    DeprecatedString lastPathComponent() const;
    DeprecatedString query() const;
    DeprecatedString ref() const;
    bool hasRef() const;

    DeprecatedString encodedHtmlRef() const { return ref(); }

    void setProtocol(const DeprecatedString &);
    void setHost(const DeprecatedString &);
    void setPort(unsigned short int);
    void setHostAndPort(const DeprecatedString&);
    void setUser(const DeprecatedString &);
    void setPass(const DeprecatedString &);
    void setPath(const DeprecatedString &);
    void setQuery(const DeprecatedString &);
    void setRef(const DeprecatedString &);

    DeprecatedString prettyURL() const;

#if PLATFORM(CF)
    CFURLRef createCFURL() const;
#endif
#if PLATFORM(MAC)
    NSURL *getNSURL() const;
#endif
#if PLATFORM(QT)
    operator QUrl() const;
#endif

    bool isLocalFile() const;
    String fileSystemPath() const;

    static DeprecatedString decode_string(const DeprecatedString&);
    static DeprecatedString decode_string(const DeprecatedString&, const TextEncoding&);
    static DeprecatedString encode_string(const DeprecatedString&);
    
    friend bool operator==(const KURL &, const KURL &);

#ifdef USE_GOOGLE_URL_LIBRARY
    // Getters for the parsed structure and its corresponding 8-bit string.
    const url_parse::Parsed& parsed() const { return m_parsed; }
    const CString& utf8String() const { return m_url.utf8String(); }
#endif

#ifndef NDEBUG
    void print() const;
#endif

private:
    bool isHierarchical() const;

    bool m_isValid;
#ifdef USE_GOOGLE_URL_LIBRARY
    // Initializes the object. This will call through to one of the backend
    // initializers below depending on whether the string's internal
    // representation is 8 or 16 bit.
    void init(const KURL& base, const DeprecatedString& relative,
              const TextEncoding* query_encoding);

    // Backend initializers. The query encoding parameters are optional and can
    // be NULL (this implies UTF-8). These initializers require that the object
    // has just been created and the strings are NULL. Do not call on an
    // already-constructed object.
    void init(const KURL& base, const char* rel, int rel_len,
              const TextEncoding* query_encoding);
    void init(const KURL& base, const UChar* rel, int rel_len,
              const TextEncoding* query_encoding);

    // Returns the substring of the input identified by the given component.
    DeprecatedString componentString(const url_parse::Component& comp) const;

    // Replaces the given components, modifying the current URL. The current
    // URL must be valid.
    typedef url_canon::Replacements<wchar_t> Replacements;
    void replaceComponents(const Replacements& replacements);

    // Returns true if the scheme matches the given lowercase ASCII scheme.
    bool schemeIs(const char* lower_ascii_scheme) const;

    // We store the URL in UTF-8 (really, ASCII except for the ref which can be
    // UTF-8). WebKit wants Strings and (for now) DeprecatedStrings out of us.
    // We create the conversion of the strings on demand and cache the results
    // to speed things up.
    class URLString {
    public:
        URLString();

        // Setters for the data. Using the ASCII version when you know the
        // data is ASCII will be slightly more efficient. The UTF-8 version
        // will always be correct if the caller is unsure.
        void setUtf8(const char* data, int data_len);
        void setAscii(const char* data, int data_len);

        // TODO(brettw) we can support an additional optimization when KURL
        // class is supported only for Strings. Make this buffer support both
        // optinal Strings and UTF-8 data. This way, we can use the optimization
        // from the original KURL which uses = with the original string when
        // canonicalization did not change it. This allows the strings to share
        // a buffer internally, and saves a malloc.

        // Getters for the data.
        const CString& utf8String() const { return m_utf8; }
        const String& string() const;
        const DeprecatedString deprecatedString() const;

    private:
        CString m_utf8;

        // Set to true when the caller set us using the ASCII setter. We can
        // be more efficient when we know there is no UTF-8 to worry about.
        // This flag is currently always correct, but should be changed to be a
        // hint (see setUtf8).
        bool m_utf8IsASCII;

        mutable bool m_stringIsValid;
        mutable String m_string;

        mutable bool m_deprecatedStringIsValid;
        mutable DeprecatedString m_deprecatedString;
    };

    URLString m_url;
    url_parse::Parsed m_parsed; // Indexes into the UTF-8 version of the string.

#else
    void init(const KURL&, const DeprecatedString&, const TextEncoding&);
    void parse(const char *url, const DeprecatedString *originalString);

    DeprecatedString urlString;

    int schemeEndPos;
    int userStartPos;
    int userEndPos;
    int passwordEndPos;
    int hostEndPos;
    int portEndPos;
    int pathEndPos;
    int queryEndPos;
    int fragmentEndPos;
#endif
    
    friend bool equalIgnoringRef(const KURL& a, const KURL& b);
};

}

#endif // KURL_h
