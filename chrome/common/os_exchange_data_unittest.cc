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

#include <atlbase.h>
#include <shlobj.h>

#include "base/pickle.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/win_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test OSExchangeDataTest;

// Test setting/getting using the OSExchangeData API
TEST(OSExchangeDataTest, StringDataGetAndSet) {
  OSExchangeData* data = new OSExchangeData;
  std::wstring input = L"I can has cheezburger?";
  data->SetString(input);

  OSExchangeData* data2 = new OSExchangeData(data);
  std::wstring output;
  EXPECT_TRUE(data2->GetString(&output));
  EXPECT_EQ(input, output);
  std::string url_spec = "http://www.goats.com/";
  GURL url(url_spec);
  std::wstring title;
  EXPECT_FALSE(data2->GetURLAndTitle(&url, &title));
  // No URLs in |data|, so url should be untouched.
  EXPECT_EQ(url_spec, url.spec());
  // data gets freed when data2 releases the ref on it
  delete data2;
}

// Test getting using the IDataObject COM API
TEST(OSExchangeDataTest, StringDataAccessViaCOM) {
  OSExchangeData* data = new OSExchangeData;
  std::wstring input = L"O hai googlz.";
  data->SetString(input);
  CComPtr<IDataObject> com_data(data);

  FORMATETC format_etc =
      { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&format_etc));

  STGMEDIUM medium;
  EXPECT_EQ(S_OK, com_data->GetData(&format_etc, &medium));
  std::wstring output =
      ScopedHGlobal<wchar_t>(medium.hGlobal).get();
  EXPECT_EQ(input, output);
  ReleaseStgMedium(&medium);

  // data is freed automatically by CComPtr.
}

// Test setting using the IDataObject COM API
TEST(OSExchangeDataTest, StringDataWritingViaCOM) {
  OSExchangeData* data = new OSExchangeData;
  std::wstring input = L"http://www.google.com/";

  CComPtr<IDataObject> com_data(data);

  // Store data in the object using the COM SetData API.
  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  STGMEDIUM medium;
  medium.tymed = TYMED_HGLOBAL;
  HGLOBAL glob = GlobalAlloc(GPTR, sizeof(wchar_t) * (input.size() + 1));
  size_t stringsz = input.size();
  SIZE_T sz = GlobalSize(glob);
  ScopedHGlobal<wchar_t> global_lock(glob);
  wchar_t* buffer_handle = global_lock.get();
  wcscpy_s(buffer_handle, input.size() + 1, input.c_str());
  medium.hGlobal = glob;
  medium.pUnkForRelease = NULL;
  EXPECT_EQ(S_OK, com_data->SetData(&format_etc, &medium, TRUE));

  // Construct a new object with the old object so that we can use our access
  // APIs.
  OSExchangeData* data2 = new OSExchangeData(com_data);
  EXPECT_TRUE(data2->HasURL());
  GURL url_from_data;
  std::wstring title;
  EXPECT_TRUE(data2->GetURLAndTitle(&url_from_data, &title));
  GURL reference_url(input);
  EXPECT_EQ(reference_url.spec(), url_from_data.spec());
  // deleting data2 will free data because it holds a ref to it.
  delete data2;
}

TEST(OSExchangeDataTest, URLDataAccessViaCOM) {
  OSExchangeData* data = new OSExchangeData;
  GURL url("http://www.google.com/");
  data->SetURL(url, L"");
  CComPtr<IDataObject> com_data(data);

  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&format_etc));

  STGMEDIUM medium;
  EXPECT_EQ(S_OK, com_data->GetData(&format_etc, &medium));
  std::wstring output =
      ScopedHGlobal<wchar_t>(medium.hGlobal).get();
  EXPECT_EQ(url.spec(), WideToUTF8(output));
  ReleaseStgMedium(&medium);
}

TEST(OSExchangeDataTest, MultipleFormatsViaCOM) {
  OSExchangeData* data = new OSExchangeData;
  std::string url_spec = "http://www.google.com/";
  GURL url(url_spec);
  std::wstring text = L"O hai googlz.";
  data->SetURL(url, L"Google");
  data->SetString(text);

  CComPtr<IDataObject> com_data(data);

  CLIPFORMAT cfstr_ineturl = RegisterClipboardFormat(CFSTR_INETURL);
  FORMATETC url_format_etc =
      { cfstr_ineturl, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&url_format_etc));
  FORMATETC text_format_etc =
      { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  EXPECT_EQ(S_OK, com_data->QueryGetData(&text_format_etc));

  STGMEDIUM medium;
  EXPECT_EQ(S_OK, com_data->GetData(&url_format_etc, &medium));
  std::wstring output_url =
      ScopedHGlobal<wchar_t>(medium.hGlobal).get();
  EXPECT_EQ(url.spec(), WideToUTF8(output_url));
  ReleaseStgMedium(&medium);

  // The text is supposed to be the raw text of the URL, _NOT_ the value of
  // |text|! This is because the URL is added first and thus takes precedence!
  EXPECT_EQ(S_OK, com_data->GetData(&text_format_etc, &medium));
  std::wstring output_text =
      ScopedHGlobal<wchar_t>(medium.hGlobal).get();
  EXPECT_EQ(url_spec, WideToUTF8(output_text));
  ReleaseStgMedium(&medium);
}

TEST(OSExchangeDataTest, EnumerationViaCOM) {
  OSExchangeData* data = new OSExchangeData;
  data->SetURL(GURL("http://www.google.com/"), L"");
  data->SetString(L"O hai googlz.");

  CLIPFORMAT cfstr_file_group_descriptor =
      RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
  CLIPFORMAT text_x_moz_url = RegisterClipboardFormat(L"text/x-moz-url");

  CComPtr<IDataObject> com_data(data);
  CComPtr<IEnumFORMATETC> enumerator;
  EXPECT_EQ(S_OK, com_data->EnumFormatEtc(DATADIR_GET, &enumerator));

  // Test that we can get one item.
  {
    // Explictly don't reset the first time, to verify the creation state is
    // OK.
    ULONG retrieved = 0;
    FORMATETC elements_array[1];
    EXPECT_EQ(S_OK, enumerator->Next(1,
        reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
    EXPECT_EQ(1, retrieved);
    EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
  }

  // Test that we can get one item with a NULL retrieved value.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    FORMATETC elements_array[1];
    EXPECT_EQ(S_OK, enumerator->Next(1,
        reinterpret_cast<FORMATETC*>(&elements_array), NULL));
    EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
  }

  // Test that we can get two items.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    ULONG retrieved = 0;
    FORMATETC elements_array[2];
    EXPECT_EQ(S_OK, enumerator->Next(2,
        reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
    EXPECT_EQ(2, retrieved);
    EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
    EXPECT_EQ(cfstr_file_group_descriptor, elements_array[1].cfFormat);
  }

  // Test that we can skip the first item.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    EXPECT_EQ(S_OK, enumerator->Skip(1));
    ULONG retrieved = 0;
    FORMATETC elements_array[1];
    EXPECT_EQ(S_OK, enumerator->Next(1,
        reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
    EXPECT_EQ(1, retrieved);
    EXPECT_EQ(cfstr_file_group_descriptor, elements_array[0].cfFormat);
  }

  // Test that we can skip the first item, and create a clone that matches in
  // this state, and modify the original without affecting the clone.
  {
    EXPECT_EQ(S_OK, enumerator->Reset());
    EXPECT_EQ(S_OK, enumerator->Skip(1));
    CComPtr<IEnumFORMATETC> cloned_enumerator;
    EXPECT_EQ(S_OK, enumerator->Clone(&cloned_enumerator));
    EXPECT_EQ(S_OK, enumerator->Reset());

    {
      ULONG retrieved = 0;
      FORMATETC elements_array[1];
      EXPECT_EQ(S_OK, cloned_enumerator->Next(1,
          reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
      EXPECT_EQ(1, retrieved);
      EXPECT_EQ(cfstr_file_group_descriptor, elements_array[0].cfFormat);
    }

    {
      ULONG retrieved = 0;
      FORMATETC elements_array[1];
      EXPECT_EQ(S_OK, enumerator->Next(1,
          reinterpret_cast<FORMATETC*>(&elements_array), &retrieved));
      EXPECT_EQ(1, retrieved);
      EXPECT_EQ(text_x_moz_url, elements_array[0].cfFormat);
    }
  }
}

TEST(OSExchangeDataTest, TestURLExchangeFormats) {
  OSExchangeData* data = new OSExchangeData;
  std::string url_spec = "http://www.google.com/";
  GURL url(url_spec);
  std::wstring url_title = L"Google";
  data->SetURL(url, url_title);
  std::wstring output;

  OSExchangeData* data2 = new OSExchangeData(data);

  // URL spec and title should match
  GURL output_url;
  std::wstring output_title;
  EXPECT_TRUE(data2->GetURLAndTitle(&output_url, &output_title));
  EXPECT_EQ(url_spec, output_url.spec());
  EXPECT_EQ(url_title, output_title);
  std::wstring output_string;

  // URL should be the raw text response
  EXPECT_TRUE(data2->GetString(&output_string));
  EXPECT_EQ(url_spec, WideToUTF8(output_string));

  // File contents access via COM
  CComPtr<IDataObject> com_data(data);
  {
    CLIPFORMAT cfstr_file_contents = RegisterClipboardFormat(CFSTR_FILECONTENTS);
    FORMATETC format_etc =
        { cfstr_file_contents, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    EXPECT_EQ(S_OK, com_data->QueryGetData(&format_etc));

    STGMEDIUM medium;
    EXPECT_EQ(S_OK, com_data->GetData(&format_etc, &medium));
    std::string output =
        ScopedHGlobal<char>(medium.hGlobal).get();
    std::string file_contents = "[InternetShortcut]\r\nURL=" + url_spec + "\r\n";
    EXPECT_EQ(file_contents, output);
    ReleaseStgMedium(&medium);
  }

  // Need to manually free data2 since we never stuff it into a COMPtr.
  delete data2;
}

TEST(OSExchangeDataTest, TestPickledData) {
  CLIPFORMAT test_cf = RegisterClipboardFormat(L"chrome/test");

  Pickle saved_pickle;
  saved_pickle.WriteInt(1);
  saved_pickle.WriteInt(2);
  scoped_refptr<OSExchangeData> data(new OSExchangeData());
  data->SetPickledData(test_cf, saved_pickle);

  scoped_refptr<OSExchangeData> copy(new OSExchangeData(data.get()));
  EXPECT_TRUE(copy->HasFormat(test_cf));

  Pickle restored_pickle;
  EXPECT_TRUE(copy->GetPickledData(test_cf, &restored_pickle));
  void* p_iterator = NULL;
  int value;
  EXPECT_TRUE(restored_pickle.ReadInt(&p_iterator, &value));
  EXPECT_EQ(1, value);
  EXPECT_TRUE(restored_pickle.ReadInt(&p_iterator, &value));
  EXPECT_EQ(2, value);
}

TEST(OSExchangeDataTest, FileContents) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);
  std::string file_contents("data\0with\0nulls", 15);
  data->SetFileContents(L"filename.txt", file_contents);

  scoped_refptr<OSExchangeData> copy(new OSExchangeData(data.get()));
  std::wstring filename;
  std::string read_contents;
  EXPECT_TRUE(copy->GetFileContents(&filename, &read_contents));
  EXPECT_EQ(L"filename.txt", filename);
  EXPECT_EQ(file_contents, read_contents);
}

TEST(OSExchangeDataTest, Html) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);
  std::wstring html(L"Version:0.9\nStartHTML:71\nEndHTML:160\n"
      L"StartFragment:130\nEndFragment:150\n<HTML>\n<BODY>\n"
      L"<!--StartFragment-->\n<b>bold.</b> <i><b>This is bold italic.</b></i>\n"
      L"<!--EndFragment-->\n</BODY>\n</HTML>");
  data->SetCFHtml(html);

  scoped_refptr<OSExchangeData> copy(new OSExchangeData(data.get()));
  std::wstring read_html;
  EXPECT_TRUE(copy->GetCFHtml(&read_html));
  EXPECT_EQ(html, read_html);
}

TEST(OSExchangeDataTest, SetURLWithMaxPath) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);
  std::wstring long_title(L'a', MAX_PATH + 1);
  data->SetURL(GURL("http://google.com"), long_title);
}

TEST(OSExchangeDataTest, ProvideURLForPlainTextURL) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);
  data->SetString(L"http://google.com");

  scoped_ptr<OSExchangeData> data2(new OSExchangeData(data.get()));
  ASSERT_TRUE(data2->HasURL());
  GURL read_url;
  std::wstring title;
  EXPECT_TRUE(data2->GetURLAndTitle(&read_url, &title));
  EXPECT_EQ(GURL("http://google.com"), read_url);
}
