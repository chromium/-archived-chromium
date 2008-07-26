// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Some helper functions for working with the clipboard and IDataObjects.

#include <shlobj.h>
#include <string>
#include <vector>

class ClipboardUtil {
 public:
  /////////////////////////////////////////////////////////////////////////////
  // Clipboard formats.
  static FORMATETC* GetUrlFormat();
  static FORMATETC* GetUrlWFormat();
  static FORMATETC* GetMozUrlFormat();
  static FORMATETC* GetPlainTextFormat();
  static FORMATETC* GetPlainTextWFormat();
  static FORMATETC* GetFilenameFormat();
  static FORMATETC* GetFilenameWFormat();
  // MS HTML Format
  static FORMATETC* GetHtmlFormat();
  // Firefox text/html
  static FORMATETC* GetTextHtmlFormat();
  static FORMATETC* GetCFHDropFormat();
  static FORMATETC* GetFileDescriptorFormat();
  static FORMATETC* GetFileContentFormatZero();
  static FORMATETC* GetWebKitSmartPasteFormat();

  /////////////////////////////////////////////////////////////////////////////
  // These methods check to see if |data_object| has the requested type.
  // Returns true if it does.
  static bool HasUrl(IDataObject* data_object);
  static bool HasFilenames(IDataObject* data_object);
  static bool HasPlainText(IDataObject* data_object);

  /////////////////////////////////////////////////////////////////////////////
  // Helper methods to extract information from an IDataObject.  These methods
  // return true if the requested data type is found in |data_object|.
  static bool GetUrl(IDataObject* data_object,
      std::wstring* url, std::wstring* title);
  static bool GetFilenames(IDataObject* data_object,
                           std::vector<std::wstring>* filenames);
  static bool GetPlainText(IDataObject* data_object, std::wstring* plain_text);
  static bool GetCFHtml(IDataObject* data_object, std::wstring* cf_html);
  static bool GetTextHtml(IDataObject* data_object, std::wstring* text_html);
  static bool GetFileContents(IDataObject* data_object,
                              std::wstring* filename,
                              std::string* file_contents);
};
