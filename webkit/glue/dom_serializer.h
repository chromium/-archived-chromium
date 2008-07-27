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

#ifndef WEBKIT_GLUE_DOM_SERIALIZER_H__
#define WEBKIT_GLUE_DOM_SERIALIZER_H__

#include <string>
#include <hash_map>

#include "googleurl/src/gurl.h"

class WebFrame;
class WebFrameImpl;

namespace WebCore {
class Document;
class Element;
class Node;
class String;
class TextEncoding;
}

namespace webkit_glue {

class DomSerializerDelegate;

// Get html data by serializing all frames of current page with lists
// which contain all resource links that have local copy.
// contain all saved auxiliary files included all sub frames and resources.
// This function will find out all frames and serialize them to HTML data.
// We have a data buffer to temporary saving generated html data. We will
// sequentially call WebViewDelegate::SendSerializedHtmlData once the data
// buffer is full. See comments of WebViewDelegate::SendSerializedHtmlData
// for getting more information.
class DomSerializer {
 public:
  // Do serialization action. Return false means no available frame has been
  // serialized, otherwise return true.
  bool SerializeDom();
  // The parameter specifies which frame need to be serialized.
  // The parameter recursive_serialization specifies whether we need to
  // serialize all sub frames of the specified frame or not.
  // The parameter delegate specifies the pointer of interface
  // DomSerializerDelegate provide sink interface which can receive the
  // individual chunks of data to be saved.
  // The parameter links contain original URLs of all saved links.
  // The parameter local_paths contain corresponding local file paths of all
  // saved links, which matched with vector:links one by one.
  // The parameter local_directory_name is relative path of directory which
  // contain all saved auxiliary files included all sub frames and resources.
  DomSerializer(WebFrame* webframe,
                bool recursive_serialization,
                DomSerializerDelegate* delegate,
                const std::vector<std::wstring>& links,
                const std::vector<std::wstring>& local_paths,
                const std::wstring& local_directory_name);

  // Generate the MOTW declaration.
  static std::wstring GenerateMarkOfTheWebDeclaration(const std::wstring& url);
  // Generate the default base tag declaration.
  static std::wstring GenerateBaseTagDeclaration(
      const std::wstring& base_target);

 private:
  // Specified frame which need to be serialized;
  WebFrameImpl* specified_webframeimpl_;
  // This hash_map is used to map resource URL of original link to its local
  // file path.
  typedef stdext::hash_map<std::wstring, std::wstring> LinkLocalPathMap;
  // local_links_ include all pair of local resource path and corresponding
  // original link.
  LinkLocalPathMap local_links_;
  // Pointer of DomSerializerDelegate
  DomSerializerDelegate* delegate_;
  // Data buffer for saving result of serialized DOM data.
  std::string data_buffer_;
  // Passing true to recursive_serialization_ indicates we will serialize not
  // only the specified frame but also all sub-frames in the specific frame.
  // Otherwise we only serialize the specified frame excluded all sub-frames.
  bool recursive_serialization_;
  // Flag indicates whether we have collected all frames which need to be
  // serialized or not;
  bool frames_collected_;
  // Local directory name of all local resource files.
  const std::wstring& local_directory_name_;
  // Vector for saving all frames which need to be serialized.
  std::vector<WebFrameImpl*> frames_;

  struct SerializeDomParam {
    // Frame URL of current processing document presented by GURL
    const GURL& current_frame_gurl;
    // Frame URL of current processing document presented by std::wstring.
    const std::wstring& current_frame_wurl;
    // Current using text encoding object.
    const WebCore::TextEncoding& text_encoding;

    // Document object of current frame.
    WebCore::Document* doc;
    // Local directory name of all local resource files.
    const std::wstring& directory_name;

    // Flag indicates current doc is html document or not. It's a cache value
    // of Document.isHTMLDocument().
    bool is_html_document;
    // Flag which indicate whether we have met document type declaration.
    bool has_doctype;
    // Flag which indicate whether will process meta issue.
    bool has_checked_meta;
    // This meta element need to be skipped when serializing DOM.
    const WebCore::Element* skip_meta_element;
    // Flag indicates we are in script or style tag.
    bool is_in_script_or_style_tag;
    // Flag indicates whether we have written xml document declaration.
    // It is only used in xml document
    bool has_doc_declaration;

    // Constructor.
    SerializeDomParam(
        const GURL& current_frame_gurl,
        const std::wstring& current_frame_wurl,
        const WebCore::TextEncoding& text_encoding,
        WebCore::Document* doc,
        const std::wstring& directory_name);

   private:
    DISALLOW_EVIL_CONSTRUCTORS(SerializeDomParam);
  };

  // Collect all target frames which need to be serialized.
  void CollectTargetFrames();
  // Before we begin serializing open tag of a element, we give the target
  // element a chance to do some work prior to add some additional data.
  WebCore::String PreActionBeforeSerializeOpenTag(
      const WebCore::Element* element,
      SerializeDomParam* param,
      bool* need_skip);
  // After we finish serializing open tag of a element, we give the target
  // element a chance to do some post work to add some additional data.
  WebCore::String PostActionAfterSerializeOpenTag(
      const WebCore::Element* element,
      SerializeDomParam* param);
  // Before we begin serializing end tag of a element, we give the target
  // element a chance to do some work prior to add some additional data.
  WebCore::String PreActionBeforeSerializeEndTag(
      const WebCore::Element* element,
      SerializeDomParam* param, bool* need_skip);
  // After we finish serializing end tag of a element, we give the target
  // element a chance to do some post work to add some additional data.
  WebCore::String PostActionAfterSerializeEndTag(
      const WebCore::Element* element,
      SerializeDomParam* param);
  // Save generated html content to data buffer.
  void SaveHtmlContentToBuffer(const WebCore::String& result,
                               SerializeDomParam* param);
  // Serialize open tag of an specified element.
  void OpenTagToString(const WebCore::Element* element,
                       SerializeDomParam* param);
  // Serialize end tag of an specified element.
  void EndTagToString(const WebCore::Element* element,
                      SerializeDomParam* param);
  // Build content for a specified node
  void BuildContentForNode(const WebCore::Node* node,
                           SerializeDomParam* param);

  DISALLOW_EVIL_CONSTRUCTORS(DomSerializer);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_DOM_SERIALIZER_H__
