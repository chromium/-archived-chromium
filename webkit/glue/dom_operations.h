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

#ifndef WEBKIT_GLUE_DOM_OPERATIONS_H__
#define WEBKIT_GLUE_DOM_OPERATIONS_H__

#include <string>
#include <map>

#include "base/gfx/size.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/password_form_dom_manager.h"

struct FormData;
class WebFrameImpl;
class WebView;

namespace WebCore {
class AtomicString;
class Document;
class Element;
class Node;
class QualifiedName;
class String;
}

// A collection of operations that access the underlying WebKit DOM directly.
namespace webkit_glue {

// Automatically fill a form to upload a file.
//
// Look in all frames for a form with the name or ID |form_name|. If the form is
// found, set the input type=file with name or ID equal to |file_name| to
// |file_path|. If |form_name| is empty, look for any form containing the
// provided submit button.
//
// If |submit_name| is non empty and a submit button with a matching name or ID
// exists, the form is submitted using that submit button.  If any form input
// has a name or ID matching an |other_form_values| key, it will be set to the
// corresponding value.
//
// Return true if a form was found and processed.
typedef std::map<std::wstring, std::wstring> FormValueMap;
struct FileUploadData {
  std::wstring file_path;
  std::wstring form_name;
  std::wstring file_name;
  std::wstring submit_name;
  FormValueMap other_form_values;
};

bool FillFormToUploadFile(WebView* view, const FileUploadData& data);

// Fill in a form identified by form |data|.
bool FillForm(WebView* view, const FormData& data);

// Fill matching password forms and trigger autocomplete in the case of multiple
// matching logins.
void FillPasswordForm(WebView* view,
                      const PasswordFormDomManager::FillData& data);

// If node is an HTML node with a tag name of name it is casted to HTMLNodeType
// and returned. If node is not an HTML node or the tag name is not name
// NULL is returned.
template <class HTMLNodeType>
HTMLNodeType* CastHTMLElement(WebCore::Node* node,
                              const WebCore::QualifiedName& name) {
  if (node->isHTMLElement() &&
      static_cast<WebCore::HTMLElement*>(node)->hasTagName(name)) {
    return static_cast<HTMLNodeType*>(node);
  }
  return NULL;
}

// If element is HTML:IFrame or HTML:Frame, then return the WebFrameImpl
// object corresponding to the content frame, otherwise return NULL.
// The parameter is_frame_element indicates whether the input element
// is frame/iframe element or not.
WebFrameImpl* GetWebFrameImplFromElement(WebCore::Element* element,
                                         bool* is_frame_element);


// If element is img, script or input type=image, then return its link refer
// to the "src" attribute. If element is link, then return its link refer to
// the "href" attribute. If element is body, table, tr, td, then return its
// link refer to the "background" attribute. If element is blockquote, q, del,
// ins, then return its link refer to the "cite" attribute. Otherwise return
// NULL.
const WebCore::AtomicString* GetSubResourceLinkFromElement(
    const WebCore::Element* element);

// For img, script, iframe, frame element, when attribute name is src,
// for link, a, area element, when attribute name is href,
// for form element, when attribute name is action,
// for input, type=image, when attribute name is src,
// for body, table, tr, td, when attribute name is background,
// for blockquote, q, del, ins, when attribute name is cite,
// we can consider the attribute value has legal link.
bool ElementHasLegalLinkAttribute(const WebCore::Element* element,
                                  const WebCore::QualifiedName& attr_name);

// Get pointer of WebFrameImpl from webview according to specific URL.
WebFrameImpl* GetWebFrameImplFromWebViewForSpecificURL(WebView* view,
                                                       const GURL& page_url);

// Structure for storage the result of getting all savable resource links
// for current page. The consumer of the SavableResourcesResult is responsible
// for keeping these pointers valid for the lifetime of the
// SavableResourcesResult instance.
struct SavableResourcesResult {
  // vector which contains all savable links of sub resource.
  std::vector<GURL>* resources_list;
  // vector which contains corresponding all referral links of sub resource,
  // it matched with links one by one.
  std::vector<GURL>* referrers_list;
  // vector which contains all savable links of main frame and sub frames.
  std::vector<GURL>* frames_list;

  // Constructor.
  SavableResourcesResult(std::vector<GURL>* resources_list,
                         std::vector<GURL>* referrers_list,
                         std::vector<GURL>* frames_list)
      : resources_list(resources_list),
        referrers_list(referrers_list),
        frames_list(frames_list) { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SavableResourcesResult);
};

// Get all savable resource links from current webview, include main frame
// and sub-frame. After collecting all savable resource links, this function
// will send those links to embedder. Return value indicates whether we get
// all saved resource links successfully.
bool GetAllSavableResourceLinksForCurrentPage(WebView* view,
    const GURL& page_url, SavableResourcesResult* savable_resources_result);

// Structure used when installing a web page as an app. Populated via
// GetApplicationInfo.
struct WebApplicationInfo {
  struct IconInfo {
    GURL url;
    int width;
    int height;
  };

  // Title of the application. This is set from the meta tag whose name is
  // 'application-name'.
  std::wstring title;

  // Description of the application. This is set from the meta tag whose name
  // is 'description'.
  std::wstring description;

  // URL for the app. This is set from the meta tag whose name is
  // 'application-url'.
  GURL app_url;

  // Set of available icons. This is set for all link tags whose rel=icon. Only
  // icons that have a non-zero (width and/or height) are added.
  std::vector<IconInfo> icons;
};

// Parses the icon's size attribute as defined in the HTML 5 spec. Returns true
// on success, false on errors. On success either all the sizes specified in
// the attribute are added to sizes, or is_any is set to true.
//
// You shouldn't have a need to invoke this directly, it's public for testing.
bool ParseIconSizes(const std::wstring& text,
                    std::vector<gfx::Size>* sizes,
                    bool* is_any);

// Gets the application info for the specified page. See the description of
// WebApplicationInfo for details as to where each field comes from.
void GetApplicationInfo(WebView* view, WebApplicationInfo* app_info);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_DOM_OPERATIONS_H__
