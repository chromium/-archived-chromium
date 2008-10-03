/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "PlatformString.h"

#ifdef USE_GOOGLE_URL_LIBRARY
#include "CString.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#endif

#if PLATFORM(CF)
typedef const struct __CFURL* CFURLRef;
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURL;
#else
class NSURL;
#endif
#endif

#if PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE
#endif

namespace WebCore {

class TextEncoding;
struct KURLHash;

// FIXME: Our terminology here is a bit inconsistent. We refer to the part
// after the "#" as the "fragment" in some places and the "ref" in others.
// We should fix the terminology to match the URL and URI RFCs as closely
// as possible to resolve this.

class KURL {
public:
    // Generates a URL which contains a null string.
    KURL() { invalidate(); }

    // The argument is an absolute URL string. The string is assumed to be
    // already encoded.
    // FIXME: This constructor has a special case for strings that start with
    // "/", prepending "file://" to such strings; it would be good to get
    // rid of that special case.
    explicit KURL(const char*);

    // The argument is an absolute URL string. The string is assumed to be
    // already encoded.
    // FIXME: For characters with codes other than 0000-00FF will be chopped
    // off, so this call is currently not safe to use with arbitrary strings.
    // FIXME: This constructor has a special case for strings that start with
    // "/", prepending "file://" to such strings; it would be good to get
    // rid of that special case.
    explicit KURL(const String&);

    // Resolves the relative URL with the given base URL. If provided, the
    // TextEncoding is used to encode non-ASCII characers. The base URL can be
    // null or empty, in which case the relative URL will be interpreted as
    // absolute.
    // FIXME: If the base URL is invalid, this always creates an invalid
    // URL. Instead I think it would be better to treat all invalid base URLs
    // the same way we treate null and empty base URLs.
    KURL(const KURL& base, const String& relative);
    KURL(const KURL& base, const String& relative, const TextEncoding&);

    // FIXME: The above functions should be harmonized so that passing a
    // base of null or the empty string gives the same result as the
    // standard String constructor.
#ifdef USE_GOOGLE_URL_LIBRARY
    // For conversions for other structures that have already parsed and
    // canonicalized the URL. The input must be exactly what KURL would have
    // done with the same input.
    KURL(const char* canonical_spec, int canonical_spec_len,
         const url_parse::Parsed& parsed, bool is_valid);
#endif

    bool isValid() const { return m_isValid; }

    // Returns true if this URL has a path. Note that "http://foo.com/" has a
    // path of "/", so this function will return true. Only invalid or
    // non-hierarchical (like "javascript:") URLs will have no path.
    bool hasPath() const;

#ifdef USE_GOOGLE_URL_LIBRARY
    bool isNull() const { return m_url.utf8String().isNull(); }
    bool isEmpty() const { return m_url.utf8String().length() == 0; }

    const String& string() const { return m_url.string(); }
#else
    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

    const String& string() const { return m_string; }
#endif

    String protocol() const;
    String host() const;
    unsigned short port() const;
    String user() const;
    String pass() const;
    String path() const;
    String lastPathComponent() const;
    String query() const; // Includes the "?".
    String ref() const; // Does *not* include the "#".
    bool hasRef() const;

    String prettyURL() const;
    String fileSystemPath() const;

    // Returns true if the current URL's protocol is the same as the null-
    // terminated ASCII argument. The argument must be lower-case.
    bool protocolIs(const char*) const;
    bool isLocalFile() const;

    void setProtocol(const String&);
    void setHost(const String&);

    // Setting the port to 0 will clear any port from the URL.
    void setPort(unsigned short);

    // Input is like "foo.com" or "foo.com:8000".
    void setHostAndPort(const String&);

    void setUser(const String&);
    void setPass(const String&);

    // If you pass an empty path for HTTP or HTTPS URLs, the resulting path
    // will be "/".
    void setPath(const String&);

    // The query may begin with a question mark, or, if not, one will be added
    // for you. Setting the query to the empty string will leave a "?" in the
    // URL (with nothing after it). To clear the query, pass a null string.
    void setQuery(const String&);

    void setRef(const String&);
    void removeRef();
    
    friend bool equalIgnoringRef(const KURL&, const KURL&);

    friend bool protocolHostAndPortAreEqual(const KURL&, const KURL&);

    static bool protocolIs(const String&, const char*);

#ifdef USE_GOOGLE_URL_LIBRARY
    operator const String&() const { return m_url.string(); }
#if USE(JSC)
    operator KJS::UString() const { return m_url.string(); }
#endif

    unsigned hostStart() const;
    unsigned hostEnd() const;
    
    unsigned pathStart() const;
    unsigned pathEnd() const;
    unsigned pathAfterLastSlash() const;

#else  // !USE_GOOGLE_URL_LIBRARY
    operator const String&() const { return m_string; }
#if USE(JSC)
    operator KJS::UString() const { return m_string; }
#endif

    unsigned hostStart() const { return (m_passwordEnd == m_userStart) ? m_passwordEnd : m_passwordEnd + 1; }
    unsigned hostEnd() const { return m_hostEnd; }
    
    unsigned pathStart() const { return m_portEnd; }
    unsigned pathEnd() const { return m_pathEnd; }
    unsigned pathAfterLastSlash() const { return m_pathAfterLastSlash; }
#endif

#if PLATFORM(CF)
    KURL(CFURLRef);
    CFURLRef createCFURL() const;
#endif

#if PLATFORM(MAC)
    KURL(NSURL*);
    operator NSURL*() const;
#endif
#ifdef __OBJC__
#ifdef USE_GOOGLE_URL_LIBRARY
    operator NSString*() const { return string(); }
#else  // USE_GOOGLE_URL_LIBRARY
    operator NSString*() const { return m_string; }
#endif  // USE_GOOGLE_URL_LIBRARY
#endif

#if PLATFORM(QT)
    KURL(const QUrl&);
    operator QUrl() const;
#endif

#ifdef USE_GOOGLE_URL_LIBRARY
    // Getters for the parsed structure and its corresponding 8-bit string.
    const url_parse::Parsed& parsed() const { return m_parsed; }
    const CString& utf8String() const { return m_url.utf8String(); }
#endif

#ifndef NDEBUG
    void print() const;
#endif

private:
    void invalidate();
    bool isHierarchical() const;

    bool m_isValid;
#ifdef USE_GOOGLE_URL_LIBRARY
    // Initializes the object. This will call through to one of the backend
    // initializers below depending on whether the string's internal
    // representation is 8 or 16 bit.
    void init(const KURL& base, const String& relative,
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
    String componentString(const url_parse::Component& comp) const;

    // Replaces the given components, modifying the current URL. The current
    // URL must be valid.
    typedef url_canon::Replacements<wchar_t> Replacements;
    void replaceComponents(const Replacements& replacements);

    // Returns true if the scheme matches the given lowercase ASCII scheme.
    bool schemeIs(const char* lower_ascii_scheme) const;

    // We store the URL in UTF-8 (really, ASCII except for the ref which can be
    // UTF-8). WebKit wants Strings out of us.
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

    private:
        CString m_utf8;

        // Set to true when the caller set us using the ASCII setter. We can
        // be more efficient when we know there is no UTF-8 to worry about.
        // This flag is currently always correct, but should be changed to be a
        // hint (see setUtf8).
        bool m_utf8IsASCII;

        mutable bool m_stringIsValid;
        mutable String m_string;
    };

    URLString m_url;
    url_parse::Parsed m_parsed; // Indexes into the UTF-8 version of the string.

#else
    void init(const KURL&, const String&, const TextEncoding&);
    void copyToBuffer(Vector<char, 512>& buffer) const;
    
    // Parses the given URL. The originalString parameter allows for an
    // optimization: When the source is the same as the fixed-up string,
    // it will use the passed-in string instead of allocating a new one.
    void parse(const String&);
    void parse(const char* url, const String* originalString);

    String m_string;

    int m_schemeEnd;
    int m_userStart;
    int m_userEnd;
    int m_passwordEnd;
    int m_hostEnd;
    int m_portEnd;
    int m_pathAfterLastSlash;
    int m_pathEnd;
    int m_queryEnd;
    int m_fragmentEnd;
#endif

// See the following "#infdef KURL_DECORATE_GLOBALS" for an explanation.
#ifdef KURL_DECORATE_GLOBALS
  public:
    static const KURL& blankURL();
    // Note: protocolIs() is already defined, so omit it.
    static String mimeTypeFromDataURL(const String& url);
    static String decodeURLEscapeSequences(const String&);
    static String decodeURLEscapeSequences(const String&, const TextEncoding&);
    static String encodeWithURLEscapeSequences(const String&);
#endif // ifdef KURL_DECORATE_GLOBALS
};

bool operator==(const KURL&, const KURL&);
bool operator==(const KURL&, const String&);
bool operator==(const String&, const KURL&);
bool operator!=(const KURL&, const KURL&);
bool operator!=(const KURL&, const String&);
bool operator!=(const String&, const KURL&);

bool equalIgnoringRef(const KURL&, const KURL&);
bool protocolHostAndPortAreEqual(const KURL&, const KURL&);

// GKURL_unittest.cpp includes both KURL.cpp and GKURL.cpp.
// For that to work, global functions need to be avoided.
// (the other globals are okay since they include KURL in parameter list).
// The workaround is to move the globals into KURL's class namespace.
#ifndef KURL_DECORATE_GLOBALS

const KURL& blankURL();

// Functions to do URL operations on strings.
// These are operations that aren't faster on a parsed URL.

bool protocolIs(const String& url, const char* protocol);

String mimeTypeFromDataURL(const String& url);

// Unescapes the given string using URL escaping rules, given an optional
// encoding (defaulting to UTF-8 otherwise). DANGER: If the URL has "%00"
// in it, the resulting string will have embedded null characters!
String decodeURLEscapeSequences(const String&);
String decodeURLEscapeSequences(const String&, const TextEncoding&);

String encodeWithURLEscapeSequences(const String&);

#endif // ifndef KURL_DECORATE_GLOBALS

// Inlines.

#ifdef USE_GOOGLE_URL_LIBRARY

inline bool operator==(const KURL& a, const KURL& b)
{
    return a.utf8String() == b.utf8String();
}

inline bool operator!=(const KURL& a, const KURL& b)
{
    return a.utf8String() != b.utf8String();
}

#else

inline bool operator==(const KURL& a, const KURL& b)
{
    return a.string() == b.string();
}

#endif

inline bool operator==(const KURL& a, const String& b)
{
    return a.string() == b;
}

inline bool operator==(const String& a, const KURL& b)
{
    return a == b.string();
}

#ifndef USE_GOOGLE_URL_LIBRARY

inline bool operator!=(const KURL& a, const KURL& b)
{
    return a.string() != b.string();
}

#endif

inline bool operator!=(const KURL& a, const String& b)
{
    return a.string() != b;
}

inline bool operator!=(const String& a, const KURL& b)
{
    return a != b.string();
}

} // namespace WebCore

namespace WTF {

    // KURLHash is the default hash for String
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::KURL> {
        typedef WebCore::KURLHash Hash;
    };

} // namespace WTF

#endif // KURL_h
