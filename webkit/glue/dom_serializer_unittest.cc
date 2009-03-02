// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Document.h"
#include "DocumentType.h"
#include "Element.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLHeadElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "markup.h"
#include "SharedBuffer.h"
#include "SubstituteData.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/dom_operations_private.h"
#include "webkit/glue/dom_serializer.h"
#include "webkit/glue/dom_serializer_delegate.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class DomSerializerTests : public TestShellTest,
                           public webkit_glue::DomSerializerDelegate {
 public:
  DomSerializerTests()
    : local_directory_name_(FILE_PATH_LITERAL("./dummy_files/")) { }

  // DomSerializerDelegate.
  void DidSerializeDataForFrame(const GURL& frame_url,
      const std::string& data, PageSavingSerializationStatus status) {
    // If the all frames are finished saving, check all finish status
    if (status == ALL_FRAMES_ARE_FINISHED) {
      SerializationFinishStatusMap::iterator it =
          serialization_finish_status_.begin();
      for (; it != serialization_finish_status_.end(); ++it)
        ASSERT_TRUE(it->second);
      serialized_ = true;
      return;
    }

    // Check finish status of current frame.
    SerializationFinishStatusMap::iterator it =
        serialization_finish_status_.find(frame_url.spec());
    // New frame, set initial status as false.
    if (it == serialization_finish_status_.end())
      serialization_finish_status_[frame_url.spec()] = false;

    it = serialization_finish_status_.find(frame_url.spec());
    ASSERT_TRUE(it != serialization_finish_status_.end());
    // In process frame, finish status should be false.
    ASSERT_FALSE(it->second);

    // Add data to corresponding frame's content.
    serialized_frame_map_[frame_url.spec()] += data;

    // Current frame is completed saving, change the finish status.
    if (status == CURRENT_FRAME_IS_FINISHED)
      it->second = true;
  }

  bool HasSerializedFrame(const GURL& frame_url) {
    return serialized_frame_map_.find(frame_url.spec()) !=
           serialized_frame_map_.end();
  }

  const std::string& GetSerializedContentForFrame(
      const GURL& frame_url) {
    return serialized_frame_map_[frame_url.spec()];
  }

  // Load web page according to specific URL.
  void LoadPageFromURL(const GURL& page_url) {
    // Load the test file.
    test_shell_->ResetTestController();
    test_shell_->LoadURL(UTF8ToWide(page_url.spec()).c_str());
    test_shell_->WaitTestFinished();
  }

  // Load web page according to input content and relative URLs within
  // the document.
  void LoadContents(const std::string& contents,
                    const GURL& base_url,
                    const WebCore::String encoding_info) {
    test_shell_->ResetTestController();
    // If input encoding is empty, use UTF-8 as default encoding.
    if (encoding_info.isEmpty()) {
      test_shell_->webView()->GetMainFrame()->LoadHTMLString(contents,
                                                             base_url);
    } else {
      // Do not use WebFrame.LoadHTMLString because it assumes that input
      // html contents use UTF-8 encoding.
      WebFrameImpl* web_frame =
          static_cast<WebFrameImpl*>(test_shell_->webView()->GetMainFrame());
      ASSERT_TRUE(web_frame != NULL);
      int len = static_cast<int>(contents.size());
      RefPtr<WebCore::SharedBuffer> buf(
          WebCore::SharedBuffer::create(contents.data(), len));

      WebCore::SubstituteData subst_data(
          buf, WebCore::String("text/html"), encoding_info, WebCore::KURL());
      WebCore::ResourceRequest request(webkit_glue::GURLToKURL(base_url),
                                       WebCore::CString());
      web_frame->frame()->loader()->load(request, subst_data, false);
    }

    test_shell_->WaitTestFinished();
  }

  // Serialize page DOM according to specific page URL. The parameter
  // recursive_serialization indicates whether we will serialize all
  // sub-frames.
  void SerializeDomForURL(const GURL& page_url,
                          bool recursive_serialization) {
    // Find corresponding WebFrameImpl according to page_url.
    WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), page_url);
    ASSERT_TRUE(web_frame != NULL);
    // Add input file URl to links_.
    links_.push_back(page_url);
    // Add dummy file path to local_path_.
    local_paths_.push_back(FilePath(FILE_PATH_LITERAL("c:\\dummy.htm")));
    // Start serializing DOM.
    webkit_glue::DomSerializer dom_serializer(web_frame,
        recursive_serialization, this, links_, local_paths_,
        local_directory_name_);
    ASSERT_TRUE(dom_serializer.SerializeDom());
    ASSERT_TRUE(serialized_);
  }

 private:
  // Map frame_url to corresponding serialized_content.
  typedef base::hash_map<std::string, std::string> SerializedFrameContentMap;
  SerializedFrameContentMap serialized_frame_map_;
  // Map frame_url to corresponding status of serialization finish.
  typedef base::hash_map<std::string, bool> SerializationFinishStatusMap;
  SerializationFinishStatusMap serialization_finish_status_;
  // Flag indicates whether the process of serializing DOM is finished or not.
  bool serialized_;
  // The links_ contain dummy original URLs of all saved links.
  std::vector<GURL> links_;
  // The local_paths_ contain dummy corresponding local file paths of all saved
  // links, which matched links_ one by one.
  std::vector<FilePath> local_paths_;
  // The local_directory_name_ is dummy relative path of directory which
  // contain all saved auxiliary files included all sub frames and resources.
  const FilePath local_directory_name_;

 protected:
  // testing::Test
  virtual void SetUp() {
    TestShellTest::SetUp();
    serialized_ = false;
  }

  virtual void TearDown() {
    TestShellTest::TearDown();
  }
};

// Helper function for checking whether input node is META tag. Return true
// means it is META element, otherwise return false. The parameter charset_info
// return actual charset info if the META tag has charset declaration.
bool IsMetaElement(const WebCore::Node* node, WebCore::String* charset_info) {
  if (!node->isHTMLElement())
    return false;
  if (!(static_cast<const WebCore::HTMLElement*>(node))->hasTagName(
      WebCore::HTMLNames::metaTag))
    return false;
  charset_info->remove(0, charset_info->length());
  const WebCore::HTMLMetaElement* meta =
      static_cast<const WebCore::HTMLMetaElement*>(node);
  // Check the META charset declaration.
  WebCore::String equiv = meta->httpEquiv();
  if (equalIgnoringCase(equiv, "content-type")) {
    WebCore::String content = meta->content();
    int pos = content.find("charset", 0, false);
    if (pos > -1) {
      // Add a dummy charset declaration to charset_info, which indicates this
      // META tag has charset declaration although we do not get correct value
      // yet.
      charset_info->append("has-charset-declaration");
      int remaining_length = content.length() - pos - 7;
      if (!remaining_length)
        return true;
      const UChar* start_pos = content.characters() + pos + 7;
      // Find "=" symbol.
      while (remaining_length--)
        if (*start_pos++ == L'=')
          break;
      // Skip beginning space.
      while (remaining_length) {
        if (*start_pos > 0x0020)
          break;
        ++start_pos;
        --remaining_length;
      }
      if (!remaining_length)
        return true;
      const UChar* end_pos = start_pos;
      // Now we find out the start point of charset info. Search the end point.
      while (remaining_length--) {
        if (*end_pos <= 0x0020 || *end_pos == L';')
          break;
        ++end_pos;
      }
      // Get actual charset info.
      *charset_info = WebCore::String(start_pos,
          static_cast<unsigned>(end_pos - start_pos));
      return true;
    }
  }
  return true;
}

// If original contents have document type, the serialized contents also have
// document type.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithDocType) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path, L"dom_serializer/youtube_1.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Make sure original contents have document type.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->doctype() != NULL);
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->frame()->loader()->encoding());
  // Make sure serialized contents still have document type.
  web_frame =
      static_cast<WebFrameImpl*>(test_shell_->webView()->GetMainFrame());
  doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->doctype() != NULL);
}

// If original contents do not have document type, the serialized contents
// also do not have document type.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithoutDocType) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path, L"dom_serializer/youtube_2.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Make sure original contents do not have document type.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->doctype() == NULL);
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->frame()->loader()->encoding());
  // Make sure serialized contents do not have document type.
  web_frame =
      static_cast<WebFrameImpl*>(test_shell_->webView()->GetMainFrame());
  doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->doctype() == NULL);
}

// Serialize XML document which has all 5 built-in entities. After
// finishing serialization, the serialized contents should be same
// with original XML document.
TEST_F(DomSerializerTests, SerialzeXMLDocWithBuiltInEntities) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path, L"dom_serializer/note.xml");
  // Read original contents for later comparison.
  std::string orginal_contents;
  ASSERT_TRUE(file_util::ReadFileToString(page_file_path, &orginal_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Compare the serialized contents with original contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  ASSERT_EQ(orginal_contents, serialized_contents);
}

// When serializing DOM, we add MOTW declaration before html tag.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithAddingMOTW) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path, L"dom_serializer/youtube_2.htm");
  // Read original contents for later comparison .
  std::string orginal_contents;
  ASSERT_TRUE(file_util::ReadFileToString(page_file_path, &orginal_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Make sure original contents does not have MOTW;
  std::string motw_declaration =
      webkit_glue::DomSerializer::GenerateMarkOfTheWebDeclaration(file_url);
  ASSERT_FALSE(motw_declaration.empty());
  // The encoding of original contents is ISO-8859-1, so we convert the MOTW
  // declaration to ASCII and search whether original contents has it or not.
  ASSERT_TRUE(std::string::npos ==
      orginal_contents.find(motw_declaration));
  // Load the test file.
  LoadPageFromURL(file_url);
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Make sure the serialized contents have MOTW ;
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  ASSERT_FALSE(std::string::npos ==
      serialized_contents.find(motw_declaration));
}

// When serializing DOM, we will add the META which have correct charset
// declaration as first child of HEAD element for resolving WebKit bug:
// http://bugs.webkit.org/show_bug.cgi?id=16621 even the original document
// does not have META charset declaration.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithNoMetaCharsetInOriginalDoc) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path, L"dom_serializer/youtube_1.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);

  // Make sure there is no META charset declaration in original document.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  WebCore::HTMLHeadElement* head_ele = doc->head();
  ASSERT_TRUE(head_ele != NULL);
  // Go through all children of HEAD element.
  WebCore::String charset_info;
  for (const WebCore::Node *child = head_ele->firstChild(); child != NULL;
       child = child->nextSibling())
    if (IsMetaElement(child, &charset_info))
      ASSERT_TRUE(charset_info.isEmpty());

  // Do serialization.
  SerializeDomForURL(file_url, false);

  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->frame()->loader()->encoding());
  // Make sure the first child of HEAD element is META which has charset
  // declaration in serialized contents.
  web_frame =
      static_cast<WebFrameImpl*>(test_shell_->webView()->GetMainFrame());
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  head_ele = doc->head();
  ASSERT_TRUE(head_ele != NULL);
  WebCore::Node* meta_node = head_ele->firstChild();
  ASSERT_TRUE(meta_node != NULL);
  // Get meta charset info.
  ASSERT_TRUE(IsMetaElement(meta_node, &charset_info));
  ASSERT_TRUE(!charset_info.isEmpty());
  ASSERT_TRUE(charset_info == web_frame->frame()->loader()->encoding());

  // Make sure no more additional META tags which have charset declaration.
  for (const WebCore::Node *child = meta_node->nextSibling(); child != NULL;
       child = child->nextSibling())
    if (IsMetaElement(child, &charset_info))
      ASSERT_TRUE(charset_info.isEmpty());
}

// When serializing DOM, if the original document has multiple META charset
// declaration, we will add the META which have correct charset declaration
// as first child of HEAD element and remove all original META charset
// declarations.
TEST_F(DomSerializerTests,
       SerialzeHTMLDOMWithMultipleMetaCharsetInOriginalDoc) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path, L"dom_serializer/youtube_2.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);

  // Make sure there are multiple META charset declarations in original
  // document.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  WebCore::HTMLHeadElement* head_ele = doc->head();
  ASSERT_TRUE(head_ele != NULL);
  // Go through all children of HEAD element.
  int charset_declaration_count = 0;
  WebCore::String charset_info;
  for (const WebCore::Node *child = head_ele->firstChild(); child != NULL;
       child = child->nextSibling()) {
    if (IsMetaElement(child, &charset_info) && !charset_info.isEmpty())
      charset_declaration_count++;
  }
  // The original doc has more than META tags which have charset declaration.
  ASSERT(charset_declaration_count > 1);

  // Do serialization.
  SerializeDomForURL(file_url, false);

  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->frame()->loader()->encoding());
  // Make sure only first child of HEAD element is META which has charset
  // declaration in serialized contents.
  web_frame =
      static_cast<WebFrameImpl*>(test_shell_->webView()->GetMainFrame());
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  head_ele = doc->head();
  ASSERT_TRUE(head_ele != NULL);
  WebCore::Node* meta_node = head_ele->firstChild();
  ASSERT_TRUE(meta_node != NULL);
  // Get meta charset info.
  ASSERT_TRUE(IsMetaElement(meta_node, &charset_info));
  ASSERT_TRUE(!charset_info.isEmpty());
  ASSERT_TRUE(charset_info == web_frame->frame()->loader()->encoding());

  // Make sure no more additional META tags which have charset declaration.
  for (const WebCore::Node *child = meta_node->nextSibling(); child != NULL;
       child = child->nextSibling())
    if (IsMetaElement(child, &charset_info))
      ASSERT_TRUE(charset_info.isEmpty());
}

// Test situation of html entities in text when serializing HTML DOM.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithEntitiesInText) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path,
                          L"dom_serializer/htmlentities_in_text.htm");
  // Read original contents for later comparison .
  std::string orginal_contents;
  ASSERT_TRUE(file_util::ReadFileToString(page_file_path, &orginal_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Get BODY's text content in DOM.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  WebCore::HTMLElement* body_ele = doc->body();
  ASSERT_TRUE(body_ele != NULL);
  WebCore::Node* text_node = body_ele->firstChild();
  ASSERT_TRUE(text_node->isTextNode());
  ASSERT_TRUE(createMarkup(text_node) == "&amp;&lt;&gt;\"\'");
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Compare the serialized contents with original contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  // Because we add MOTW when serializing DOM, so before comparison, we also
  // need to add MOTW to original_contents.
  std::string motw_declaration =
    webkit_glue::DomSerializer::GenerateMarkOfTheWebDeclaration(file_url);
  orginal_contents = motw_declaration + orginal_contents;
  ASSERT_EQ(orginal_contents, serialized_contents);
}

// Test situation of html entities in attribute value when serializing
// HTML DOM.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithEntitiesInAttributeValue) {
  std::wstring page_file_path = data_dir_;
  file_util::AppendToPath(&page_file_path,
      L"dom_serializer/htmlentities_in_attribute_value.htm");
  // Read original contents for later comparison.
  std::string orginal_contents;
  ASSERT_TRUE(file_util::ReadFileToString(page_file_path, &orginal_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Get value of BODY's title attribute in DOM.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  WebCore::HTMLElement* body_ele = doc->body();
  ASSERT_TRUE(body_ele != NULL);
  const WebCore::String& value = body_ele->getAttribute(
      WebCore::HTMLNames::titleAttr);
  ASSERT_TRUE(value == WebCore::String("&<>\"\'"));
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Compare the serialized contents with original contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  // Because we add MOTW when serializing DOM, so before comparison, we also
  // need to add MOTW to original_contents.
  std::string motw_declaration =
    webkit_glue::DomSerializer::GenerateMarkOfTheWebDeclaration(file_url);
  orginal_contents = motw_declaration + orginal_contents;
  ASSERT_EQ(serialized_contents, orginal_contents);
}

// Test situation of BASE tag in original document when serializing HTML DOM.
// When serializing, we should comment the BASE tag, append a new BASE tag.
// rewrite all the savable URLs to relative local path, and change other URLs
// to absolute URLs.
TEST_F(DomSerializerTests, SerialzeHTMLDOMWithBaseTag) {
  // There are total 2 available base tags in this test file.
  const int kTotalBaseTagCountInTestFile = 2;

  FilePath page_file_path = FilePath::FromWStringHack(data_dir_).AppendASCII(
      "dom_serializer");
  file_util::EnsureEndsWithSeparator(&page_file_path);

  // Get page dir URL which is base URL of this file.
  GURL path_dir_url = net::FilePathToFileURL(page_file_path.ToWStringHack());
  // Get file path.
  page_file_path =
      page_file_path.AppendASCII("html_doc_has_base_tag.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path.ToWStringHack());
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Since for this test, we assume there is no savable sub-resource links for
  // this test file, also all links are relative URLs in this test file, so we
  // need to check those relative URLs and make sure document has BASE tag.
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromWebViewForSpecificURL(
          test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebCore::Document* doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  // Go through all descent nodes.
  RefPtr<WebCore::HTMLCollection> all = doc->all();
  int original_base_tag_count = 0;
  for (WebCore::Node* node = all->firstItem(); node != NULL;
       node = all->nextItem()) {
    if (!node->isHTMLElement())
      continue;
    WebCore::Element* element = static_cast<WebCore::Element*>(node);
    if (element->hasTagName(WebCore::HTMLNames::baseTag)) {
      original_base_tag_count++;
    } else {
      // Get link.
      const WebCore::AtomicString* value =
          webkit_glue::GetSubResourceLinkFromElement(element);
      if (!value && element->hasTagName(WebCore::HTMLNames::aTag)) {
        value = &element->getAttribute(WebCore::HTMLNames::hrefAttr);
        if (value->isEmpty())
          value = NULL;
      }
      // Each link is relative link.
      if (value) {
        GURL link(WideToUTF8(webkit_glue::StringToStdWString(value->string())));
        ASSERT_TRUE(link.scheme().empty());
      }
    }
  }
  ASSERT_EQ(original_base_tag_count, kTotalBaseTagCountInTestFile);
  // Make sure in original document, the base URL is not equal with the
  // |path_dir_url|.
  GURL original_base_url(
      WideToUTF8(webkit_glue::StringToStdWString(doc->baseURL())));
  ASSERT_NE(original_base_url, path_dir_url);

  // Do serialization.
  SerializeDomForURL(file_url, false);

  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->frame()->loader()->encoding());

  // Make sure all links are absolute URLs and doc there are some number of
  // BASE tags in serialized HTML data. Each of those BASE tags have same base
  // URL which is as same as URL of current test file.
  web_frame =
      static_cast<WebFrameImpl*>(test_shell_->webView()->GetMainFrame());
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->frame()->document();
  ASSERT_TRUE(doc->isHTMLDocument());
  // Go through all descent nodes.
  all = doc->all();
  int new_base_tag_count = 0;
  for (WebCore::Node* node = all->firstItem(); node != NULL;
       node = all->nextItem()) {
    if (!node->isHTMLElement())
      continue;
    WebCore::Element* element = static_cast<WebCore::Element*>(node);
    if (element->hasTagName(WebCore::HTMLNames::baseTag)) {
      new_base_tag_count++;
    } else {
      // Get link.
      const WebCore::AtomicString* value =
          webkit_glue::GetSubResourceLinkFromElement(element);
      if (!value && element->hasTagName(WebCore::HTMLNames::aTag)) {
        value = &element->getAttribute(WebCore::HTMLNames::hrefAttr);
        if (value->isEmpty())
          value = NULL;
      }
      // Each link is absolute link.
      if (value) {
        GURL link(WideToUTF8(webkit_glue::StringToStdWString(value->string())));
        ASSERT_FALSE(link.scheme().empty());
      }
    }
  }
  // We have one more added BASE tag which is generated by JavaScript.
  ASSERT_EQ(new_base_tag_count, original_base_tag_count + 1);
  // Make sure in new document, the base URL is equal with the |path_dir_url|.
  GURL new_base_url(
      webkit_glue::StringToStdString(doc->baseURL()));
  ASSERT_EQ(new_base_url, path_dir_url);
}

}  // namespace
