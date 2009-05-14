// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_OS_EXCHANGE_DATA_H_
#define APP_OS_EXCHANGE_DATA_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <objidl.h>
#include "base/scoped_comptr_win.h"
#endif

#include <string>
#include <vector>

#include "base/basictypes.h"

class GURL;
class Pickle;

///////////////////////////////////////////////////////////////////////////////
//
// OSExchangeData
//  An object that holds interchange data to be sent out to OS services like
//  clipboard, drag and drop, etc. This object exposes an API that clients can
//  use to specify raw data and its high level type. This object takes care of
//  translating that into something the OS can understand.
//
///////////////////////////////////////////////////////////////////////////////
#if defined(OS_WIN)
class OSExchangeData : public IDataObject {
#else
class OSExchangeData {
#endif
 public:
#if defined(OS_WIN)
  // Returns true if source has plain text that is a valid url.
  static bool HasPlainTextURL(IDataObject* source);

  // Returns true if source has plain text that is a valid URL and sets url to
  // that url.
  static bool GetPlainTextURL(IDataObject* source, GURL* url);

  explicit OSExchangeData(IDataObject* source);
#endif

  OSExchangeData();
  virtual ~OSExchangeData();

  // These functions add data to the OSExchangeData object of various Chrome
  // types. The OSExchangeData object takes care of translating the data into
  // a format suitable for exchange with the OS.
  // NOTE WELL: Typically, a data object like this will contain only one of the
  //            following types of data. In cases where more data is held, the
  //            order in which these functions are called is _important_!
  //       ---> The order types are added to an OSExchangeData object controls
  //            the order of enumeration in our IEnumFORMATETC implementation!
  //            This comes into play when selecting the best (most preferable)
  //            data type for insertion into a DropTarget.
  void SetString(const std::wstring& data);
  // A URL can have an optional title in some exchange formats.
  void SetURL(const GURL& url, const std::wstring& title);
  // A full path to a file
  void SetFilename(const std::wstring& full_path);
#if defined(OS_WIN)
  // Adds pickled data of the specified format.
  void SetPickledData(CLIPFORMAT format, const Pickle& data);
#endif
  // Adds the bytes of a file (CFSTR_FILECONTENTS and CFSTR_FILEDESCRIPTOR).
  void SetFileContents(const std::wstring& filename,
                       const std::string& file_contents);
  // Adds a snippet of HTML.  |html| is just raw html but this sets both
  // text/html and CF_HTML.
  void SetHtml(const std::wstring& html, const GURL& base_url);

  // These functions retrieve data of the specified type. If data exists, the
  // functions return and the result is in the out parameter. If the data does
  // not exist, the out parameter is not touched. The out parameter cannot be
  // NULL.
  bool GetString(std::wstring* data) const;
  bool GetURLAndTitle(GURL* url, std::wstring* title) const;
  // Return the path of a file, if available.
  bool GetFilename(std::wstring* full_path) const;
#if defined(OS_WIN)
  bool GetPickledData(CLIPFORMAT format, Pickle* data) const;
#endif
  bool GetFileContents(std::wstring* filename,
                       std::string* file_contents) const;
  bool GetHtml(std::wstring* html, GURL* base_url) const;

  // Test whether or not data of certain types is present, without actually
  // returning anything.
  bool HasString() const;
  bool HasURL() const;
  bool HasURLTitle() const;
  bool HasFile() const;
#if defined(OS_WIN)
  bool HasFormat(CLIPFORMAT format) const;

  // IDataObject implementation:
  HRESULT __stdcall GetData(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall GetDataHere(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall QueryGetData(FORMATETC* format_etc);
  HRESULT __stdcall GetCanonicalFormatEtc(
      FORMATETC* format_etc, FORMATETC* result);
  HRESULT __stdcall SetData(
      FORMATETC* format_etc, STGMEDIUM* medium, BOOL should_release);
  HRESULT __stdcall EnumFormatEtc(
      DWORD direction, IEnumFORMATETC** enumerator);
  HRESULT __stdcall DAdvise(
      FORMATETC* format_etc, DWORD advf, IAdviseSink* sink, DWORD* connection);
  HRESULT __stdcall DUnadvise(DWORD connection);
  HRESULT __stdcall EnumDAdvise(IEnumSTATDATA** enumerator);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();
#endif

 private:
#if defined(OS_WIN)
  // FormatEtcEnumerator only likes us for our StoredDataMap typedef.
  friend class FormatEtcEnumerator;

  // Our internal representation of stored data & type info.
  struct StoredDataInfo {
    FORMATETC format_etc;
    STGMEDIUM* medium;
    bool owns_medium;

    StoredDataInfo(CLIPFORMAT cf, STGMEDIUM* a_medium) {
      format_etc.cfFormat = cf;
      format_etc.dwAspect = DVASPECT_CONTENT;
      format_etc.lindex = -1;
      format_etc.ptd = NULL;
      format_etc.tymed = a_medium->tymed;

      owns_medium = true;

      medium = a_medium;
    }

    ~StoredDataInfo() {
      if (owns_medium) {
        ReleaseStgMedium(medium);
        delete medium;
      }
    }
  };

  typedef std::vector<StoredDataInfo*> StoredData;
  StoredData contents_;

  ScopedComPtr<IDataObject> source_object_;

  LONG ref_count_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OSExchangeData);
};

#endif  // APP_OS_EXCHANGE_DATA_H_
