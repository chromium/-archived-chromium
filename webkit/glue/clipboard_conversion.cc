// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/clipboard_conversion.h"

#include "build/build_config.h"

#include "ChromiumDataObject.h"
#include "ClipboardUtilitiesChromium.h"
#include "KURL.h"
#include "SharedBuffer.h"
#include <wtf/Vector.h>

#include "webkit/glue/glue_util.h"

namespace {

// TODO(tc): CF_HTML logic should move into the browser process since it only
// makes sense on Windows.
#if defined(OS_WIN)
void append(WTF::Vector<char>& vector, const WebCore::CString& string) {
  vector.append(string.data(), string.length());
}
// Documentation for the CF_HTML format is available at
// http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp
void markupToCF_HTML(const WebCore::String& markup,
                     const WebCore::String& src_url,
                     WTF::Vector<char>& result) {
  if (markup.isEmpty())
    return;

  #define MAX_DIGITS 10
  #define MAKE_NUMBER_FORMAT_1(digits) MAKE_NUMBER_FORMAT_2(digits)
  #define MAKE_NUMBER_FORMAT_2(digits) "%0" #digits "u"
  #define NUMBER_FORMAT MAKE_NUMBER_FORMAT_1(MAX_DIGITS)

  static const char* header = "Version:0.9\n"
      "StartHTML:" NUMBER_FORMAT "\n"
      "EndHTML:" NUMBER_FORMAT "\n"
      "StartFragment:" NUMBER_FORMAT "\n"
      "EndFragment:" NUMBER_FORMAT "\n";
  static const char* source_url_prefix = "SourceURL:";

  static const char* start_markup = "<HTML>\n<BODY>\n<!--StartFragment-->\n";
  static const char* end_markup = "\n<!--EndFragment-->\n</BODY>\n</HTML>";

  WebCore::CString source_url_utf8 = (src_url == WebCore::blankURL()) ?
      "" : src_url.utf8();
  WebCore::CString markup_utf8 = markup.utf8();

  // calculate offsets
  size_t start_html_offset = strlen(header) - strlen(NUMBER_FORMAT) * 4 +
      MAX_DIGITS * 4;
  if (source_url_utf8.length()) {
    start_html_offset += strlen(source_url_prefix) +
        source_url_utf8.length() + 1;
  }
  size_t start_fragment_offset = start_html_offset + strlen(start_markup);
  size_t end_fragment_offset = start_fragment_offset + markup_utf8.length();
  size_t end_html_offset = end_fragment_offset + strlen(end_markup);

  append(result, WebCore::String::format(header, start_html_offset,
         end_html_offset, start_fragment_offset, end_fragment_offset).utf8());
  if (source_url_utf8.length()) {
    append(result, source_url_prefix);
    append(result, source_url_utf8);
    result.append('\n');
  }
  append(result, start_markup);
  append(result, markup_utf8);
  append(result, end_markup);

  #undef MAX_DIGITS
  #undef MAKE_NUMBER_FORMAT_1
  #undef MAKE_NUMBER_FORMAT_2
  #undef NUMBER_FORMAT
}
#endif

std::wstring HTMLToCFHTML(const WebCore::String& html,
                          const WebCore::KURL& url) {
#if defined(OS_WIN)
  Vector<char> cf_html_data;
  markupToCF_HTML(html, url, cf_html_data);
  WebCore::String cf_html_str(cf_html_data.data(), cf_html_data.size());
  return webkit_glue::StringToStdWString(cf_html_str);
#else
  return std::wstring();
#endif
}

WebCore::String CFHTMLToHTML(const std::wstring& cf_html_wstr) {
#if defined(OS_WIN)
  WebCore::String cf_html = webkit_glue::StdWStringToString(cf_html_wstr);
  // Obtain baseURL if present.
  WebCore::String src_url_str("sourceURL:");
  WebCore::String src_url;
  unsigned int line_start = cf_html.find(src_url_str, 0, false);
  if (line_start != -1) {
    unsigned int src_end = cf_html.find("\n", line_start, false);
    unsigned int src_start = line_start + src_url_str.length();
    WebCore::String raw_src_url = cf_html.substring(src_start, src_end - src_start);
    replaceNBSPWithSpace(raw_src_url);
    src_url = raw_src_url.stripWhiteSpace();
  }

  // find the markup between "<!--StartFragment -->" and "<!--EndFragment
  // -->", accounting for browser quirks
  unsigned markup_start = cf_html.find("<html", 0, false);
  unsigned tag_start = cf_html.find("startfragment", markup_start, false);
  unsigned fragment_start = cf_html.find('>', tag_start) + 1;
  unsigned tag_end = cf_html.find("endfragment", fragment_start, false);
  unsigned fragment_end = cf_html.reverseFind('<', tag_end);
  WebCore::String markup = cf_html.substring(fragment_start,
      fragment_end - fragment_start).stripWhiteSpace();
  return markup;
#else
  return WebCore::String();
#endif
}

}  // namespace

namespace webkit_glue {

WebDropData ChromiumDataObjectToWebDropData(
    WebCore::ChromiumDataObject* data_object) {
  WebDropData drop_data;
  drop_data.url = KURLToGURL(data_object->url);
  drop_data.url_title = StringToStdWString(data_object->url_title);

  for (size_t i = 0; i < data_object->filenames.size(); ++i) {
    drop_data.filenames.push_back(StringToStdWString(
        data_object->filenames[i]));
  }

  drop_data.plain_text = StringToStdWString(data_object->plain_text);

  drop_data.cf_html = HTMLToCFHTML(data_object->text_html, data_object->url);

  drop_data.text_html = StringToStdWString(data_object->text_html);

  drop_data.file_description_filename = StringToStdWString(
      data_object->file_content_filename);
  if (data_object->file_content) {
    drop_data.file_contents.assign(data_object->file_content->data(),
        data_object->file_content->size());
  }

  return drop_data;
}

PassRefPtr<WebCore::ChromiumDataObject> WebDropDataToChromiumDataObject(
    const WebDropData& drop_data) {
  RefPtr<WebCore::ChromiumDataObject> data_object =
      WebCore::ChromiumDataObject::create();
  data_object->url = GURLToKURL(drop_data.url);
  data_object->url_title = StdWStringToString(drop_data.url_title);

  for (size_t i = 0; i < drop_data.filenames.size(); ++i) {
    data_object->filenames.append(StdWStringToString(drop_data.filenames[i]));
  }

  data_object->plain_text = StdWStringToString(drop_data.plain_text);

  data_object->text_html = StdWStringToString(drop_data.text_html);
  if (data_object->text_html.isEmpty()) {
    // Use CF_HTML only if there's no text/html data.
    data_object->text_html = CFHTMLToHTML(drop_data.cf_html);
  }

  data_object->file_content_filename = StdWStringToString(
      drop_data.file_description_filename);
  if (!drop_data.file_contents.empty()) {
    data_object->file_content =
        WebCore::SharedBuffer::create(drop_data.file_contents.data(),
                                      drop_data.file_contents.size());
  }

  return data_object;
}

}  // namespace webkit_glue
