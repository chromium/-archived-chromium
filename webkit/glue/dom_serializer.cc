// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// How we handle the base tag better.
// Current status:
// At now the normal way we use to handling base tag is
// a) For those links which have corresponding local saved files, such as
// savable CSS, JavaScript files, they will be written to relative URLs which
// point to local saved file. Why those links can not be resolved as absolute
// file URLs, because if they are resolved as absolute URLs, after moving the
// file location from one directory to another directory, the file URLs will
// be dead links.
// b) For those links which have not corresponding local saved files, such as
// links in A, AREA tags, they will be resolved as absolute URLs.
// c) We comment all base tags when serialzing DOM for the page.
// FireFox also uses above way to handle base tag.
//
// Problem:
// This way can not handle the following situation:
// the base tag is written by JavaScript.
// For example. The page "www.yahoo.com" use
// "document.write('<base href="http://www.yahoo.com/"...');" to setup base URL
// of page when loading page. So when saving page as completed-HTML, we assume
// that we save "www.yahoo.com" to "c:\yahoo.htm". After then we load the saved
// completed-HTML page, then the JavaScript will insert a base tag
// <base href="http://www.yahoo.com/"...> to DOM, so all URLs which point to
// local saved resource files will be resolved as
// "http://www.yahoo.com/yahoo_files/...", which will cause all saved  resource
// files can not be loaded correctly. Also the page will be rendered ugly since
// all saved sub-resource files (such as CSS, JavaScript files) and sub-frame
// files can not be fetched.
// Now FireFox, IE and WebKit based Browser all have this problem.
//
// Solution:
// My solution is that we comment old base tag and write new base tag:
// <base href="." ...> after the previous commented base tag. In WebKit, it
// always uses the latest "href" attribute of base tag to set document's base
// URL. Based on this behavior, when we encounter a base tag, we comment it and
// write a new base tag <base href="."> after the previous commented base tag.
// The new added base tag can help engine to locate correct base URL for
// correctly loading local saved resource files. Also I think we need to inherit
// the base target value from document object when appending new base tag.
// If there are multiple base tags in original document, we will comment all old
// base tags and append new base tag after each old base tag because we do not
// know those old base tags are original content or added by JavaScript. If
// they are added by JavaScript, it means when loading saved page, the script(s)
// will still insert base tag(s) to DOM, so the new added base tag(s) can
// override the incorrect base URL and make sure we alway load correct local
// saved resource files.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "DocumentType.h"
#include "FrameLoader.h"
#include "Document.h"
#include "Element.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "markup.h"
#include "PlatformString.h"
#include "TextEncoding.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/dom_serializer.h"

#include "base/string_util.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/dom_operations_private.h"
#include "webkit/glue/dom_serializer_delegate.h"
#include "webkit/glue/entity_map.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"

namespace {

// Default "mark of the web" declaration
static const char* const kDefaultMarkOfTheWeb =
    "\n<!-- saved from url=(%04d)%s -->\n";

// Default meat content for writing correct charset declaration.
static const wchar_t* const kDefaultMetaContent =
    L"<META http-equiv=\"Content-Type\" content=\"text/html; charset=%ls\">";

// Notation of start comment.
static const wchar_t* const kStartCommentNotation = L"<!-- ";

// Notation of end comment.
static const wchar_t* const kEndCommentNotation = L" -->";

// Default XML declaration.
static const wchar_t* const kXMLDeclaration =
    L"<?xml version=\"%ls\" encoding=\"%ls\"%ls?>\n";

// Default base tag declaration
static const wchar_t* const kBaseTagDeclaration =
    L"<BASE href=\".\"%ls>";

static const wchar_t* const kBaseTargetDeclaration =
    L" target=\"%ls\"";

// Maximum length of data buffer which is used to temporary save generated
// html content data.
static const int kHtmlContentBufferLength = 65536;

// Check whether specified unicode has corresponding html/xml entity name.
// If yes, replace the character with the returned entity notation, if not
// then still use original character.
void ConvertCorrespondingSymbolToEntity(WebCore::String* result,
                                        const WebCore::String& value,
                                        bool in_html_doc) {
  unsigned len = value.length();
  const UChar* start_pos = value.characters();
  const UChar* cur_pos = start_pos;
  while (len--) {
    const char* entity_name =
        webkit_glue::EntityMap::GetEntityNameByCode(*cur_pos, in_html_doc);
    if (entity_name) {
      // Append content before entity code.
      if (cur_pos > start_pos)
        result->append(start_pos, cur_pos - start_pos);
      result->append("&");
      result->append(entity_name);
      result->append(";");
      start_pos = ++cur_pos;
    } else {
      cur_pos++;
    }
  }
  // Append the remaining content.
  if (cur_pos > start_pos)
    result->append(start_pos, cur_pos - start_pos);
}

}  // namespace

namespace webkit_glue {

// SerializeDomParam Constructor.
DomSerializer::SerializeDomParam::SerializeDomParam(
    const GURL& current_frame_gurl,
    const WebCore::TextEncoding& text_encoding,
    WebCore::Document* doc,
    const FilePath& directory_name)
    : current_frame_gurl(current_frame_gurl),
      text_encoding(text_encoding),
      doc(doc),
      directory_name(directory_name),
      has_doctype(false),
      has_checked_meta(false),
      skip_meta_element(NULL),
      is_in_script_or_style_tag(false),
      has_doc_declaration(false) {
  // Cache the value since we check it lots of times.
  is_html_document = doc->isHTMLDocument();
}

// Static.
std::string DomSerializer::GenerateMarkOfTheWebDeclaration(
    const GURL& url) {
  return StringPrintf(kDefaultMarkOfTheWeb,
                      url.spec().size(), url.spec().c_str());
}

// Static.
std::wstring DomSerializer::GenerateBaseTagDeclaration(
    const std::wstring& base_target) {
  std::wstring target_declaration = base_target.empty() ? L"" :
      StringPrintf(kBaseTargetDeclaration, base_target.c_str());
  return StringPrintf(kBaseTagDeclaration, target_declaration.c_str());
}

WebCore::String DomSerializer::PreActionBeforeSerializeOpenTag(
    const WebCore::Element* element, SerializeDomParam* param,
    bool* need_skip) {
  WebCore::String result;

  *need_skip = false;
  if (param->is_html_document) {
    // Skip the open tag of original META tag which declare charset since we
    // have overrided the META which have correct charset declaration after
    // serializing open tag of HEAD element.
    if (element->hasTagName(WebCore::HTMLNames::metaTag)) {
      const WebCore::HTMLMetaElement* meta =
          static_cast<const WebCore::HTMLMetaElement*>(element);
      // Check whether the META tag has declared charset or not.
      WebCore::String equiv = meta->httpEquiv();
      if (equalIgnoringCase(equiv, "content-type")) {
        WebCore::String content = meta->content();
        if (content.length() && content.contains("charset", false)) {
          // Find META tag declared charset, we need to skip it when
          // serializing DOM.
          param->skip_meta_element = element;
          *need_skip = true;
        }
      }
    } else if (element->hasTagName(WebCore::HTMLNames::htmlTag)) {
      // Check something before processing the open tag of HEAD element.
      // First we add doc type declaration if original doc has it.
      if (!param->has_doctype) {
        param->has_doctype = true;
        result += createMarkup(param->doc->doctype());
      }

      // Add MOTW declaration before html tag.
      // See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
      result += StdStringToString(GenerateMarkOfTheWebDeclaration(
          param->current_frame_gurl));
    } else if (element->hasTagName(WebCore::HTMLNames::baseTag)) {
      // Comment the BASE tag when serializing dom.
      result += StdWStringToString(kStartCommentNotation);
    }
  } else {
    // Write XML declaration.
    if (!param->has_doc_declaration) {
      param->has_doc_declaration = true;
      // Get encoding info.
      WebCore::String xml_encoding = param->doc->xmlEncoding();
      if (xml_encoding.isEmpty())
        xml_encoding = param->doc->frame()->loader()->encoding();
      if (xml_encoding.isEmpty())
        xml_encoding = WebCore::UTF8Encoding().name();
      std::wstring str_xml_declaration =
          StringPrintf(kXMLDeclaration,
                       StringToStdWString(param->doc->xmlVersion()).c_str(),
                       StringToStdWString(xml_encoding).c_str(),
                       param->doc->xmlStandalone() ? L" standalone=\"yes\"" :
                                                     L"");
      result += StdWStringToString(str_xml_declaration);
    }
    // Add doc type declaration if original doc has it.
    if (!param->has_doctype) {
      param->has_doctype = true;
      result += createMarkup(param->doc->doctype());
    }
  }

  return result;
}

WebCore::String DomSerializer::PostActionAfterSerializeOpenTag(
    const WebCore::Element* element, SerializeDomParam* param) {
  WebCore::String result;

  if (!param->is_html_document)
    return result;
  // Check after processing the open tag of HEAD element
  if (!param->has_checked_meta &&
      element->hasTagName(WebCore::HTMLNames::headTag)) {
    param->has_checked_meta = true;
    // Check meta element. WebKit only pre-parse the first 512 bytes
    // of the document. If the whole <HEAD> is larger and meta is the
    // end of head part, then this kind of pages aren't decoded correctly
    // because of this issue. So when we serialize the DOM, we need to
    // make sure the meta will in first child of head tag.
    // See http://bugs.webkit.org/show_bug.cgi?id=16621.
    // First we generate new content for writing correct META element.
    std::wstring str_meta =
        StringPrintf(kDefaultMetaContent,
                     ASCIIToWide(param->text_encoding.name()).c_str());
    result += StdWStringToString(str_meta);

    // Will search each META which has charset declaration, and skip them all
    // in PreActionBeforeSerializeOpenTag.
  } else if (element->hasTagName(WebCore::HTMLNames::scriptTag) ||
             element->hasTagName(WebCore::HTMLNames::styleTag)) {
    param->is_in_script_or_style_tag = true;
  }

  return result;
}

WebCore::String DomSerializer::PreActionBeforeSerializeEndTag(
    const WebCore::Element* element, SerializeDomParam* param,
    bool* need_skip) {
  WebCore::String result;

  *need_skip = false;
  if (!param->is_html_document)
    return result;
  // Skip the end tag of original META tag which declare charset.
  // Need not to check whether it's META tag since we guarantee
  // skip_meta_element is definitely META tag if it's not NULL.
  if (param->skip_meta_element == element) {
    *need_skip = true;
  } else if (element->hasTagName(WebCore::HTMLNames::scriptTag) ||
             element->hasTagName(WebCore::HTMLNames::styleTag)) {
    DCHECK(param->is_in_script_or_style_tag);
    param->is_in_script_or_style_tag = false;
  }

  return result;
}

// After we finish serializing end tag of a element, we give the target
// element a chance to do some post work to add some additional data.
WebCore::String DomSerializer::PostActionAfterSerializeEndTag(
    const WebCore::Element* element, SerializeDomParam* param) {
  WebCore::String result;

  if (!param->is_html_document)
    return result;
  // Comment the BASE tag when serializing DOM.
  if (element->hasTagName(WebCore::HTMLNames::baseTag)) {
    result += StdWStringToString(kEndCommentNotation);
    // Append a new base tag declaration.
    result += StdWStringToString(GenerateBaseTagDeclaration(
        webkit_glue::StringToStdWString(param->doc->baseTarget())));
  }

  return result;
}

void DomSerializer::SaveHtmlContentToBuffer(const WebCore::String& result,
                                            SerializeDomParam* param) {
  if (!result.length())
    return;
  // Convert the unicode content to target encoding
  WebCore::CString encoding_result = param->text_encoding.encode(
      result.characters(), result.length(), WebCore::EntitiesForUnencodables);

  // if the data buffer will be full, then send it out first.
  if (encoding_result.length() + data_buffer_.size() >
      data_buffer_.capacity()) {
    // Send data to delegate, tell it now we are serializing current frame.
    delegate_->DidSerializeDataForFrame(param->current_frame_gurl,
        data_buffer_, DomSerializerDelegate::CURRENT_FRAME_IS_NOT_FINISHED);
    data_buffer_.clear();
  }

  // Append result to data buffer.
  data_buffer_.append(CStringToStdString(encoding_result));
}

void DomSerializer::OpenTagToString(const WebCore::Element* element,
                                    SerializeDomParam* param) {
  bool need_skip;
  // Do pre action for open tag.
  WebCore::String result = PreActionBeforeSerializeOpenTag(element,
                                                           param,
                                                           &need_skip);
  if (need_skip)
    return;
  // Add open tag
  result += "<" + element->nodeName();
  // Go through all attributes and serialize them.
  const WebCore::NamedAttrMap *attrMap = element->attributes(true);
  if (attrMap) {
    unsigned numAttrs = attrMap->length();
    for (unsigned i = 0; i < numAttrs; i++) {
      result += " ";
      // Add attribute pair
      const WebCore::Attribute *attribute = attrMap->attributeItem(i);
      result += attribute->name().toString();
      result += "=\"";
      if (!attribute->value().isEmpty()) {
        // Check whether we need to replace some resource links
        // with local resource paths.
        const WebCore::QualifiedName& attr_name = attribute->name();
        // Check whether need to change the attribute which has link
        bool need_replace_link =
            ElementHasLegalLinkAttribute(element, attr_name);
        if (need_replace_link) {
          // First, get the absolute link
          const WebCore::String& attr_value = attribute->value();
          // For links start with "javascript:", we do not change it.
          if (attr_value.startsWith("javascript:", false)) {
            result += attr_value;
          } else {
            WebCore::String str_value = param->doc->completeURL(attr_value);
            std::string value(StringToStdString(str_value));
            // Check whether we local files for those link.
            LinkLocalPathMap::const_iterator it = local_links_.find(value);
            if (it != local_links_.end()) {
              // Replace the link when we have local files.
              FilePath::StringType path(FilePath::kCurrentDirectory);
              if (!param->directory_name.empty())
                path += FILE_PATH_LITERAL("/") + param->directory_name.value();
              path += FILE_PATH_LITERAL("/") + it->second.value();
              result += FilePathStringToString(path);
            } else {
              // If not found local path, replace it with absolute link.
              result += str_value;
            }
          }
        } else {
          ConvertCorrespondingSymbolToEntity(&result, attribute->value(),
              param->is_html_document);
        }
      }
      result += "\"";
    }
  }
  // Complete the open tag for element when it has child/children.
  if (element->hasChildNodes())
    result += ">";
  // Do post action for open tag.
  result += PostActionAfterSerializeOpenTag(element, param);
  // Save the result to data buffer.
  SaveHtmlContentToBuffer(result, param);
}

// Serialize end tag of an specified element.
void DomSerializer::EndTagToString(const WebCore::Element* element,
                                   SerializeDomParam* param) {
  bool need_skip;
  // Do pre action for end tag.
  WebCore::String result = PreActionBeforeSerializeEndTag(element,
                                                          param,
                                                          &need_skip);
  if (need_skip)
    return;
  // Write end tag when element has child/children.
  if (element->hasChildNodes()) {
    result += "</";
    result += element->nodeName();
    result += ">";
  } else {
    // Check whether we have to write end tag for empty element.
    if (param->is_html_document) {
      result += ">";
      const WebCore::HTMLElement* html_element =
          static_cast<const WebCore::HTMLElement*>(element);
      if (html_element->endTagRequirement() == WebCore::TagStatusRequired) {
        // We need to write end tag when it is required.
        result += "</";
        result += html_element->nodeName();
        result += ">";
      }
    } else {
      // For xml base document.
      result += " />";
    }
  }
  // Do post action for end tag.
  result += PostActionAfterSerializeEndTag(element, param);
  // Save the result to data buffer.
  SaveHtmlContentToBuffer(result, param);
}

void DomSerializer::BuildContentForNode(const WebCore::Node* node,
                                        SerializeDomParam* param) {
  switch (node->nodeType()) {
    case WebCore::Node::ELEMENT_NODE: {
      // Process open tag of element.
      OpenTagToString(static_cast<const WebCore::Element*>(node), param);
      // Walk through the children nodes and process it.
      for (const WebCore::Node *child = node->firstChild(); child != NULL;
           child = child->nextSibling())
        BuildContentForNode(child, param);
      // Process end tag of element.
      EndTagToString(static_cast<const WebCore::Element*>(node), param);
      break;
    }
    case WebCore::Node::TEXT_NODE: {
      SaveHtmlContentToBuffer(createMarkup(node), param);
      break;
    }
    case WebCore::Node::ATTRIBUTE_NODE:
    case WebCore::Node::DOCUMENT_NODE:
    case WebCore::Node::DOCUMENT_FRAGMENT_NODE: {
      // Should not exist.
      DCHECK(false);
      break;
    }
    // Document type node can be in DOM?
    case WebCore::Node::DOCUMENT_TYPE_NODE:
      param->has_doctype = true;
    default: {
      // For other type node, call default action.
      SaveHtmlContentToBuffer(createMarkup(node), param);
      break;
    }
  }
}

DomSerializer::DomSerializer(WebFrame* webframe,
                             bool recursive_serialization,
                             DomSerializerDelegate* delegate,
                             const std::vector<GURL>& links,
                             const std::vector<FilePath>& local_paths,
                             const FilePath& local_directory_name)
    : delegate_(delegate),
      recursive_serialization_(recursive_serialization),
      frames_collected_(false),
      local_directory_name_(local_directory_name) {
  // Must specify available webframe.
  DCHECK(webframe);
  specified_webframeimpl_ = static_cast<WebFrameImpl*>(webframe);
  // Make sure we have not-NULL delegate.
  DCHECK(delegate);
  // Build local resources map.
  DCHECK(links.size() == local_paths.size());
  std::vector<GURL>::const_iterator link_it = links.begin();
  std::vector<FilePath>::const_iterator path_it = local_paths.begin();
  for (; link_it != links.end(); ++link_it, ++path_it) {
    bool never_present = local_links_.insert(
        LinkLocalPathMap::value_type(link_it->spec(), *path_it)).
        second;
    DCHECK(never_present);
  }

  // Init data buffer.
  data_buffer_.reserve(kHtmlContentBufferLength);
  DCHECK(data_buffer_.empty());
}

void DomSerializer::CollectTargetFrames() {
  DCHECK(!frames_collected_);
  frames_collected_ = true;

  // First, process main frame.
  frames_.push_back(specified_webframeimpl_);
  // Return now if user only needs to serialize specified frame, not including
  // all sub-frames.
  if (!recursive_serialization_)
    return;
  // Collect all frames inside the specified frame.
  for (int i = 0; i < static_cast<int>(frames_.size()); ++i) {
    WebFrameImpl* current_frame = frames_[i];
    // Get current using document.
    WebCore::Document* current_doc = current_frame->frame()->document();
    // Go through sub-frames.
    RefPtr<WebCore::HTMLCollection> all = current_doc->all();
    for (WebCore::Node* node = all->firstItem(); node != NULL;
         node = all->nextItem()) {
      if (!node->isHTMLElement())
        continue;
      WebCore::Element* element = static_cast<WebCore::Element*>(node);
      // Check frame tag and iframe tag.
      bool is_frame_element;
      WebFrameImpl* web_frame = GetWebFrameImplFromElement(
          element, &is_frame_element);
      if (is_frame_element && web_frame)
        frames_.push_back(web_frame);
    }
  }
}

bool DomSerializer::SerializeDom() {
  // Collect target frames.
  if (!frames_collected_)
    CollectTargetFrames();
  bool did_serialization = false;
  // Get GURL for main frame.
  GURL main_page_gurl(KURLToGURL(
      specified_webframeimpl_->frame()->loader()->url()));

  // Go through all frames for serializing DOM for whole page, include
  // sub-frames.
  for (int i = 0; i < static_cast<int>(frames_.size()); ++i) {
    // Get current serializing frame.
    WebFrameImpl* current_frame = frames_[i];
    // Get current using document.
    WebCore::Document* current_doc = current_frame->frame()->document();
    // Get current frame's URL.
    const WebCore::KURL& current_frame_kurl =
        current_frame->frame()->loader()->url();
    GURL current_frame_gurl(KURLToGURL(current_frame_kurl));

    // Check whether we have done this document.
    if (local_links_.find(current_frame_gurl.spec()) != local_links_.end()) {
      // A new document, we will serialize it.
      did_serialization = true;
      // Get target encoding for current document.
      WebCore::String encoding = current_frame->frame()->loader()->encoding();
      // Create the text encoding object with target encoding.
      WebCore::TextEncoding text_encoding(encoding);
      // Construct serialize parameter for late processing document.
      SerializeDomParam param(
          current_frame_gurl,
          encoding.length() ? text_encoding : WebCore::UTF8Encoding(),
          current_doc,
          current_frame_gurl == main_page_gurl ?
                                local_directory_name_ :
                                FilePath());

      // Process current document.
      WebCore::Element* root_element = current_doc->documentElement();
      if (root_element)
        BuildContentForNode(root_element, &param);

      // Sink the remainder data and finish serializing current frame.
      delegate_->DidSerializeDataForFrame(current_frame_gurl, data_buffer_,
          DomSerializerDelegate::CURRENT_FRAME_IS_FINISHED);
      // Clear the buffer.
      data_buffer_.clear();
    }
  }

  // We have done call frames, so we send message to embedder to tell it that
  // frames are finished serializing.
  DCHECK(data_buffer_.empty());
  delegate_->DidSerializeDataForFrame(GURL(), data_buffer_,
      DomSerializerDelegate::ALL_FRAMES_ARE_FINISHED);

  return did_serialization;
}

}  // namespace webkit_glue
