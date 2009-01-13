// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "AnimationController.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "Document.h"
#include "Element.h"
#include "EventListener.h"
#include "EventNames.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMetaElement.h"
#include "HTMLOptionElement.h"
#include "HTMLNames.h"
#include "KURL.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/string_util.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_autocomplete_listener.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

using WebCore::String;

namespace {

// Structure for storage the unique set of all savable resource links for
// making sure that no duplicated resource link in final result. The consumer
// of the SavableResourcesUniqueCheck is responsible for keeping these pointers
// valid for the lifetime of the SavableResourcesUniqueCheck instance.
struct SavableResourcesUniqueCheck {
  // Unique set of all sub resource links.
  std::set<GURL>* resources_set;
  // Unique set of all frame links.
  std::set<GURL>* frames_set;
  // Collection of all frames we go through when getting all savable resource
  // links.
  std::vector<WebFrameImpl*>* frames;

  SavableResourcesUniqueCheck()
      : resources_set(NULL),
        frames_set(NULL),
        frames(NULL) {}

  SavableResourcesUniqueCheck(std::set<GURL>* resources_set,
      std::set<GURL>* frames_set, std::vector<WebFrameImpl*>* frames)
      : resources_set(resources_set),
        frames_set(frames_set),
        frames(frames) {}
};

// Get all savable resource links from current element. One element might
// have more than one resource link. It is possible to have some links
// in one CSS stylesheet.
void GetSavableResourceLinkForElement(WebCore::Element* element,
    WebCore::Document* current_doc, SavableResourcesUniqueCheck* unique_check,
    webkit_glue::SavableResourcesResult* result) {
  // Handle frame and iframe tag.
  bool is_frame_element;
  WebFrameImpl* web_frame =
      webkit_glue::GetWebFrameImplFromElement(element, &is_frame_element);
  if (is_frame_element) {
    if (web_frame)
      unique_check->frames->push_back(web_frame);
    return;
  }
  // Check whether the node has sub resource URL or not.
  const WebCore::AtomicString* value =
      webkit_glue::GetSubResourceLinkFromElement(element);
  if (!value)
    return;
  // Get absolute URL.
  GURL u(webkit_glue::KURLToGURL(current_doc->completeURL((*value).string())));
  // ignore invalid URL
  if (!u.is_valid())
    return;
  // Ignore those URLs which are not standard protocols. Because FTP
  // protocol does no have cache mechanism, we will skip all
  // sub-resources if they use FTP protocol.
  if (!u.SchemeIs("http") && !u.SchemeIs("https") && !u.SchemeIs("file"))
    return;
  // Ignore duplicated resource link.
  if (!unique_check->resources_set->insert(u).second)
    return;
  result->resources_list->push_back(u);
  // Insert referrer for above new resource link.
  if (current_doc->frame()) {
    GURL u(webkit_glue::KURLToGURL(
        WebCore::KURL(current_doc->frame()->loader()->outgoingReferrer())));
    result->referrers_list->push_back(u);
  } else {
    // Insert blank referrer.
    result->referrers_list->push_back(GURL());
  }
}

// Get all savable resource links from current WebFrameImpl object pointer.
void GetAllSavableResourceLinksForFrame(WebFrameImpl* current_frame,
    SavableResourcesUniqueCheck* unique_check,
    webkit_glue::SavableResourcesResult* result) {
  // Get current frame's URL.
  const WebCore::KURL& current_frame_kurl =
      current_frame->frame()->loader()->url();
  GURL current_frame_gurl(webkit_glue::KURLToGURL(current_frame_kurl));

  // If url of current frame is invalid or not standard protocol, ignore it.
  if (!current_frame_gurl.is_valid())
    return;
  if (!current_frame_gurl.SchemeIs("http") &&
      !current_frame_gurl.SchemeIs("https") &&
      !current_frame_gurl.SchemeIs("ftp") &&
      !current_frame_gurl.SchemeIs("file"))
    return;
  // If find same frame we have recorded, ignore it.
  if (!unique_check->frames_set->insert(current_frame_gurl).second)
    return;

  // Get current using document.
  WebCore::Document* current_doc = current_frame->frame()->document();
  // Go through all descent nodes.
  PassRefPtr<WebCore::HTMLCollection> all = current_doc->all();
  // Go through all node in this frame.
  for (WebCore::Node* node = all->firstItem(); node != NULL;
       node = all->nextItem()) {
    // We only save HTML resources.
    if (!node->isHTMLElement())
      continue;
    WebCore::Element* element = static_cast<WebCore::Element*>(node);
    GetSavableResourceLinkForElement(element,
                                     current_doc,
                                     unique_check,
                                     result);
  }
}

template <class HTMLNodeType>
HTMLNodeType* CastHTMLElement(WebCore::Node* node,
                              const WebCore::QualifiedName& name) {
  if (node->isHTMLElement() &&
      static_cast<typename WebCore::HTMLElement*>(node)->hasTagName(name)) {
    return static_cast<HTMLNodeType*>(node);
  }
  return NULL;
}

}  // namespace

namespace webkit_glue {

// Map element name to a list of pointers to corresponding elements to simplify
// form filling.
typedef std::map<std::wstring, RefPtr<WebCore::HTMLInputElement> >
    FormElementRefMap;

// Utility struct for form lookup and autofill. When we parse the DOM to lookup
// a form, in addition to action and origin URL's we have to compare all
// necessary form elements. To avoid having to look these up again when we want
// to fill the form, the FindFormElements function stores the pointers
// in a FormElements* result, referenced to ensure they are safe to use.
struct FormElements {
  RefPtr<WebCore::HTMLFormElement> form_element;
  FormElementRefMap input_elements;
  FormElements() : form_element(NULL) {
  }
};

typedef std::vector<FormElements*> FormElementsList;

static bool FillFormToUploadFileImpl(WebCore::HTMLFormElement* fe,
                                     const FileUploadData& data) {
  std::vector<WebCore::HTMLInputElement*> changed;
  PassRefPtr<WebCore::HTMLCollection> elements = fe->elements();
  int i, c;

  bool file_found = false;
  bool submit_found = false;

  // We reference the form element itself just in case it is destroyed by one
  // of the onLoad() handler.
  fe->ref();

  for (i = 0, c = elements->length(); i < c; ++i) {
    WebCore::HTMLInputElement* ie =
        static_cast<WebCore::HTMLInputElement*>(elements->item(i));

    std::wstring name = StringToStdWString(ie->name());
    std::wstring id = StringToStdWString(ie->id());

    if (!file_found &&
        ie->inputType() == WebCore::HTMLInputElement::FILE &&
        (name == data.file_name || id == data.file_name)) {
      ie->setValueFromRenderer(StdWStringToString(data.file_path));
      ie->ref();
      changed.push_back(ie);
      file_found = true;
    } else if (!submit_found &&
               ie->inputType() == WebCore::HTMLInputElement::SUBMIT &&
               (name == data.submit_name || id == data.submit_name)) {
      ie->setActivatedSubmit(true);
      submit_found = true;
    } else {
      FormValueMap::const_iterator val = data.other_form_values.find(name);
      if (val != data.other_form_values.end()) {
        ie->setValueFromRenderer(StdWStringToString(val->second));
        ie->ref();
        changed.push_back(ie);
      } else {
        val = data.other_form_values.find(id);
        if (val != data.other_form_values.end()) {
          ie->setValueFromRenderer(StdWStringToString(val->second));
          ie->ref();
          changed.push_back(ie);
        }
      }
    }
  }

  // Call all the onChange functions.
  std::vector<WebCore::HTMLInputElement*>::iterator changed_ie;
  for (changed_ie = changed.begin(); changed_ie != changed.end();
       ++changed_ie) {
    (*changed_ie)->onChange();
    (*changed_ie)->deref();
  }

  // If we found both the file and the submit button, let's submit.
  if (file_found && submit_found) {
    fe->submit();
  }

  fe->deref();

  // This operation is successful if the file input has been
  // configured.
  return file_found;
}

bool FillFormToUploadFile(WebView* view, const FileUploadData& data) {
  WebFrame* main_frame = view->GetMainFrame();
  if (!main_frame)
    return false;
  WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);
  WebCore::Frame* frame = main_frame_impl->frame();
  WebCore::Frame* f;
  for (f = frame; f; f = f->tree()->traverseNext()) {
    WebCore::Document* doc = f->document();
    if (doc->isHTMLDocument()) {
      PassRefPtr<WebCore::HTMLCollection> forms = doc->forms();
      int i, c;
      for (i = 0, c = forms->length(); i < c; ++i) {
        WebCore::HTMLFormElement* fe =
            static_cast<WebCore::HTMLFormElement*>(forms->item(i));
        std::wstring name = StringToStdWString(fe->name());
        std::wstring id = StringToStdWString(fe->id());
        if (data.form_name.empty() ||
            id == data.form_name || name == data.form_name) {
          if (FillFormToUploadFileImpl(fe, data))
            return true;
        }
      }
    }
  }
  return false;
}

// Internal implementation of FillForm API.
static bool FillFormImpl(FormElements* fe, const FormData& data, bool submit) {
  if (!fe->form_element->autoComplete())
    return false;  
 
  FormValueMap data_map;
  for (unsigned int i = 0; i < data.elements.size(); i++) {
    data_map[data.elements[i]] = data.values[i];
  }

  bool submit_found = false;
  for (FormElementRefMap::iterator it = fe->input_elements.begin();
       it != fe->input_elements.end(); ++it) {
    if (it->first == data.submit) {
      it->second->setActivatedSubmit(true);
      submit_found = true;
      continue;
    }
    if (!it->second->value().isEmpty())  // Don't overwrite pre-filled values.
      continue;
    it->second->setValue(StdWStringToString(data_map[it->first]));
    it->second->setAutofilled(true);
    it->second->onChange();
  }

  if (submit && submit_found) {
    fe->form_element->submit();
    return true;
  }
  return false;
}

// Helper function to cast a Node as an HTMLInputElement.
static WebCore::HTMLInputElement* GetNodeAsInputElement(WebCore::Node* node) {
  DCHECK(node->nodeType() == WebCore::Node::ELEMENT_NODE);
  DCHECK(static_cast<WebCore::Element*>(node)->hasTagName(
      WebCore::HTMLNames::inputTag));
  return static_cast<WebCore::HTMLInputElement*>(node);
}

// Helper to search the given form element for the specified input elements
// in |data|, and add results to |result|.
static bool FindFormInputElements(WebCore::HTMLFormElement* fe,
                                  const FormData& data,
                                  FormElements* result) {
  Vector<RefPtr<WebCore::Node> > temp_elements;
  // Loop through the list of elements we need to find on the form in
  // order to autofill it. If we don't find any one of them, abort 
  // processing this form; it can't be the right one.
  for (size_t j = 0; j < data.elements.size(); j++, temp_elements.clear()) {
    fe->getNamedElements(StdWStringToString(data.elements[j]), 
                                            temp_elements);
    if (temp_elements.isEmpty()) {
      // We didn't find a required element. This is not the right form.
      // Make sure no input elements from a partially matched form
      // in this iteration remain in the result set.
      // Note: clear will remove a reference from each InputElement.
      result->input_elements.clear();
      return false;
    }
    // This element matched, add it to our temporary result. It's possible
    // there are multiple matches, but for purposes of identifying the form
    // one suffices and if some function needs to deal with multiple
    // matching elements it can get at them through the FormElement*.
    // Note: This assignment adds a reference to the InputElement.
    result->input_elements[data.elements[j]] =
        GetNodeAsInputElement(temp_elements[0].get());
  }
  return true;
}

// Helper to locate form elements identified by |data|.
static void FindFormElements(WebView* view,
                             const FormData& data,
                             FormElementsList* results) {
  DCHECK(view);
  DCHECK(results);
  WebFrame* main_frame = view->GetMainFrame();
  if (!main_frame)
    return;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);
  WebCore::Frame* frame = main_frame_impl->frame();

  // Loop through each frame.
  for (WebCore::Frame* f = frame; f; f = f->tree()->traverseNext()) {
    WebCore::Document* doc = f->document();
    if (!doc->isHTMLDocument()) 
      continue;

    GURL full_origin(StringToStdString(doc->documentURI()));
    if (data.origin != full_origin.ReplaceComponents(rep))
      continue;

    WebCore::FrameLoader* loader = f->loader();
    if (loader == NULL)
      continue;
 
    PassRefPtr<WebCore::HTMLCollection> forms = doc->forms();
    for (size_t i = 0; i < forms->length(); ++i) { 
      WebCore::HTMLFormElement* fe =
          static_cast<WebCore::HTMLFormElement*>(forms->item(i));

      // Action URL must match.
      GURL full_action(KURLToGURL(loader->completeURL(fe->action())));
      if (data.action != full_action.ReplaceComponents(rep))
        continue;

      scoped_ptr<FormElements> curr_elements(new FormElements);
      if (!FindFormInputElements(fe, data, curr_elements.get()))
        continue;

      // We found the right element.
      // Note: this assignment adds a reference to |fe|.
      curr_elements->form_element = fe;
      results->push_back(curr_elements.release());
    }
  }
}

bool FillForm(WebView* view, const FormData& data) {
  FormElementsList forms;
  FindFormElements(view, data, &forms);
  bool success = false;
  if (!forms.empty())
    success = FillFormImpl(forms[0], data, false);
  
  // TODO(timsteele): Move STLDeleteElements to base/ and have FormElementsList
  // use that.
  FormElementsList::iterator iter;
  for (iter = forms.begin(); iter != forms.end(); ++iter)
    delete *iter;
  return success;
}

void FillPasswordForm(WebView* view,
                      const PasswordFormDomManager::FillData& data) {
  FormElementsList forms;
  // We own the FormElements* in forms.
  FindFormElements(view, data.basic_data, &forms);  
  FormElementsList::iterator iter;
  for (iter = forms.begin(); iter != forms.end(); ++iter) {
    // TODO(timsteele): Move STLDeleteElements to base/ and have
    // FormElementsList use that.
    scoped_ptr<FormElements> form_elements(*iter);

    // False param to FillFormByAction is so we don't auto-submit password
    // forms. If wait_for_username is true, we don't want to initially fill
    // the form until the user types in a valid username.
    if (!data.wait_for_username)
      FillFormImpl(form_elements.get(), data.basic_data, false);

    // Attach autocomplete listener to enable selecting alternate logins.
    // First, get pointers to username element.
    WebCore::HTMLInputElement* username_element =
        form_elements->input_elements[data.basic_data.elements[0]].get();

    // Get pointer to password element. (We currently only support single
    // password forms).
    WebCore::HTMLInputElement* password_element =
        form_elements->input_elements[data.basic_data.elements[1]].get();

    WebFrameLoaderClient* frame_loader_client =
        static_cast<WebFrameLoaderClient*>(username_element->document()->
                                           frame()->loader()->client());
    WebFrameImpl* webframe_impl = frame_loader_client->webframe();
    webframe_impl->RegisterPasswordListener(
        username_element,
        new PasswordAutocompleteListener(
            new HTMLInputDelegate(username_element),
            new HTMLInputDelegate(password_element),
            data));
  }
}

WebCore::HTMLLinkElement* CastToHTMLLinkElement(WebCore::Node* node) {
  return CastHTMLElement<WebCore::HTMLLinkElement>(node,
      WebCore::HTMLNames::linkTag);
}

WebCore::HTMLMetaElement* CastToHTMLMetaElement(WebCore::Node* node) {
  return CastHTMLElement<WebCore::HTMLMetaElement>(node,
      WebCore::HTMLNames::metaTag);
}

WebCore::HTMLOptionElement* CastToHTMLOptionElement(WebCore::Node* node) {
  return CastHTMLElement<WebCore::HTMLOptionElement>(node,
      WebCore::HTMLNames::optionTag);
}

WebFrameImpl* GetWebFrameImplFromElement(WebCore::Element* element,
                                         bool* is_frame_element) {
  *is_frame_element = false;
  if (element->hasTagName(WebCore::HTMLNames::iframeTag) ||
      element->hasTagName(WebCore::HTMLNames::frameTag)) {
    *is_frame_element = true;
    if (element->isFrameOwnerElement()) {
      // Check whether this frame has content.
      WebCore::HTMLFrameOwnerElement* frame_element =
          static_cast<WebCore::HTMLFrameOwnerElement*>(element);
      WebCore::Frame* content_frame = frame_element->contentFrame();
      return content_frame ? WebFrameImpl::FromFrame(content_frame) : NULL;
    }
  }
  return NULL;
}

const WebCore::AtomicString* GetSubResourceLinkFromElement(
    const WebCore::Element* element) {
  const WebCore::QualifiedName* attribute_name = NULL;
  if (element->hasTagName(WebCore::HTMLNames::imgTag) ||
      element->hasTagName(WebCore::HTMLNames::scriptTag) ||
      element->hasTagName(WebCore::HTMLNames::linkTag)) {
    // Get value.
    if (element->hasTagName(WebCore::HTMLNames::linkTag)) {
    // If the link element is not linked to css, ignore it.
      const WebCore::HTMLLinkElement* link =
          static_cast<const WebCore::HTMLLinkElement*>(element);
      if (!link->sheet())
        return NULL;
      // TODO(jnd). Add support for extracting links of sub-resources which
      // are inside style-sheet such as @import, url(), etc.
      // See bug: http://b/issue?id=1111667.
      attribute_name = &WebCore::HTMLNames::hrefAttr;
    } else {
      attribute_name = &WebCore::HTMLNames::srcAttr;
    }
  } else if (element->hasTagName(WebCore::HTMLNames::inputTag)) {
    const WebCore::HTMLInputElement* input =
        static_cast<const WebCore::HTMLInputElement*>(element);
    if (input->inputType() == WebCore::HTMLInputElement::IMAGE) {
      attribute_name = &WebCore::HTMLNames::srcAttr;
    }
  } else if (element->hasTagName(WebCore::HTMLNames::bodyTag) ||
             element->hasTagName(WebCore::HTMLNames::tableTag) ||
             element->hasTagName(WebCore::HTMLNames::trTag) ||
             element->hasTagName(WebCore::HTMLNames::tdTag)) {
    attribute_name = &WebCore::HTMLNames::backgroundAttr;
  } else if (element->hasTagName(WebCore::HTMLNames::blockquoteTag) ||
             element->hasTagName(WebCore::HTMLNames::qTag) ||
             element->hasTagName(WebCore::HTMLNames::delTag) ||
             element->hasTagName(WebCore::HTMLNames::insTag)) {
    attribute_name = &WebCore::HTMLNames::citeAttr;
  }
  if (!attribute_name)
    return NULL;
  const WebCore::AtomicString* value =
      &element->getAttribute(*attribute_name);
  // If value has content and not start with "javascript:" then return it,
  // otherwise return NULL.
  if (value && !value->isEmpty() &&
      !value->startsWith("javascript:", false))
    return value;

  return NULL;
}

bool ElementHasLegalLinkAttribute(const WebCore::Element* element,
                                  const WebCore::QualifiedName& attr_name) {
  if (attr_name == WebCore::HTMLNames::srcAttr) {
    // Check src attribute.
    if (element->hasTagName(WebCore::HTMLNames::imgTag) ||
        element->hasTagName(WebCore::HTMLNames::scriptTag) ||
        element->hasTagName(WebCore::HTMLNames::iframeTag) ||
        element->hasTagName(WebCore::HTMLNames::frameTag))
      return true;
    if (element->hasTagName(WebCore::HTMLNames::inputTag)) {
      const WebCore::HTMLInputElement* input =
          static_cast<const WebCore::HTMLInputElement*>(element);
      if (input->inputType() == WebCore::HTMLInputElement::IMAGE)
        return true;
    }
  } else if (attr_name == WebCore::HTMLNames::hrefAttr) {
    // Check href attribute.
    if (element->hasTagName(WebCore::HTMLNames::linkTag) ||
        element->hasTagName(WebCore::HTMLNames::aTag) ||
        element->hasTagName(WebCore::HTMLNames::areaTag))
      return true;
  } else if (attr_name == WebCore::HTMLNames::actionAttr) {
    if (element->hasTagName(WebCore::HTMLNames::formTag))
      return true;
  } else if (attr_name == WebCore::HTMLNames::backgroundAttr) {
    if (element->hasTagName(WebCore::HTMLNames::bodyTag) ||
        element->hasTagName(WebCore::HTMLNames::tableTag) ||
        element->hasTagName(WebCore::HTMLNames::trTag) ||
        element->hasTagName(WebCore::HTMLNames::tdTag))
      return true;
  } else if (attr_name == WebCore::HTMLNames::citeAttr) {
    if (element->hasTagName(WebCore::HTMLNames::blockquoteTag) ||
        element->hasTagName(WebCore::HTMLNames::qTag) ||
        element->hasTagName(WebCore::HTMLNames::delTag) ||
        element->hasTagName(WebCore::HTMLNames::insTag))
      return true;
  } else if (attr_name == WebCore::HTMLNames::classidAttr ||
             attr_name == WebCore::HTMLNames::dataAttr) {
    if (element->hasTagName(WebCore::HTMLNames::objectTag))
      return true;
  } else if (attr_name == WebCore::HTMLNames::codebaseAttr) {
    if (element->hasTagName(WebCore::HTMLNames::objectTag) ||
        element->hasTagName(WebCore::HTMLNames::appletTag))
      return true;
  }
  return false;
}

WebFrameImpl* GetWebFrameImplFromWebViewForSpecificURL(WebView* view,
                                                       const GURL& page_url) {
  WebFrame* main_frame = view->GetMainFrame();
  if (!main_frame)
    return NULL;
  WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);

  std::vector<WebFrameImpl*> frames;
  // First, process main frame.
  frames.push_back(main_frame_impl);
  // Collect all frames inside the specified frame.
  for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
    WebFrameImpl* current_frame = frames[i];
    // Get current using document.
    WebCore::Document* current_doc = current_frame->frame()->document();
    // Check whether current frame is target or not.
    const WebCore::KURL& current_frame_kurl =
        current_frame->frame()->loader()->url();
    GURL current_frame_gurl(KURLToGURL(current_frame_kurl));
    if (page_url == current_frame_gurl)
      return current_frame;
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
        frames.push_back(web_frame);
    }
  }

  return NULL;
}

// Get all savable resource links from current webview, include main
// frame and sub-frame
bool GetAllSavableResourceLinksForCurrentPage(WebView* view,
    const GURL& page_url, SavableResourcesResult* result) {
  WebFrame* main_frame = view->GetMainFrame();
  if (!main_frame)
    return false;
  WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);

  std::set<GURL> resources_set;
  std::set<GURL> frames_set;
  std::vector<WebFrameImpl*> frames;
  SavableResourcesUniqueCheck unique_check(&resources_set,
                                           &frames_set,
                                           &frames);

  GURL main_page_gurl(KURLToGURL(main_frame_impl->frame()->loader()->url()));

  // Make sure we are saving same page between embedder and webkit.
  // If page has being navigated, embedder will get three empty vector,
  // which will make the saving page job ended.
  if (page_url != main_page_gurl)
    return true;

  // First, process main frame.
  frames.push_back(main_frame_impl);

  // Check all resource in this page, include sub-frame.
  for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
    // Get current frame's all savable resource links.
    GetAllSavableResourceLinksForFrame(frames[i], &unique_check, result);
  }

  // Since frame's src can also point to sub-resources link, so it is possible
  // that some URLs in frames_list are also in resources_list. For those
  // URLs, we will remove it from frame_list, only keep them in resources_list.
  for (std::set<GURL>::iterator it = frames_set.begin();
       it != frames_set.end(); ++it) {
    // Append unique frame source to savable frame list.
    if (resources_set.find(*it) == resources_set.end())
      result->frames_list->push_back(*it);
  }

  return true;
}

// Sizes a single size (the width or height) from a 'sizes' attribute. A size
// matches must match the following regex: [1-9][0-9]*.
static int ParseSingleIconSize(const std::wstring& text) {
  // Size must not start with 0, and be between 0 and 9.
  if (text.empty() || !(text[0] >= L'1' && text[0] <= L'9'))
    return 0;
  // Make sure all chars are from 0-9.
  for (size_t i = 1; i < text.length(); ++i) {
    if (!(text[i] >= L'0' && text[i] <= L'9'))
      return 0;
  }
  int output;
  if (!StringToInt(text, &output))
    return 0;
  return output;
}

// Parses an icon size. An icon size must match the following regex:
// [1-9][0-9]*x[1-9][0-9]*.
// If the input couldn't be parsed, a size with a width/height < 0 is returned.
static gfx::Size ParseIconSize(const std::wstring& text) {
  std::vector<std::wstring> sizes;
  SplitStringDontTrim(text, L'x', &sizes);
  if (sizes.size() != 2)
    return gfx::Size();

  return gfx::Size(ParseSingleIconSize(sizes[0]),
                   ParseSingleIconSize(sizes[1]));
}

bool ParseIconSizes(const std::wstring& text,
                    std::vector<gfx::Size>* sizes,
                    bool* is_any) {
  *is_any = false;
  std::vector<std::wstring> size_strings;
  SplitStringAlongWhitespace(text, &size_strings);
  for (size_t i = 0; i < size_strings.size(); ++i) {
    if (size_strings[i] == L"any") {
      *is_any = true;
    } else {
      gfx::Size size = ParseIconSize(size_strings[i]);
      if (size.width() <= 0 || size.height() <= 0)
        return false;  // Bogus size.
      sizes->push_back(size);
    }
  }
  if (*is_any && !sizes->empty()) {
    // If is_any is true, it must occur by itself.
    return false;
  }
  return (*is_any || !sizes->empty());
}

static void AddInstallIcon(WebCore::HTMLLinkElement* link,
                           std::vector<WebApplicationInfo::IconInfo>* icons) {
  String href = link->href();
  if (href.isEmpty() || href.isNull())
    return;

  GURL url(webkit_glue::StringToStdString(href));
  if (!url.is_valid())
    return;

  const String sizes_attr = "sizes";
  if (!link->hasAttribute(sizes_attr))
    return;

  bool is_any = false;
  std::vector<gfx::Size> icon_sizes;
  if (!ParseIconSizes(webkit_glue::StringToStdWString(
      link->getAttribute(sizes_attr)), &icon_sizes, &is_any) || is_any ||
      icon_sizes.size() != 1) {
    return;
  }
  WebApplicationInfo::IconInfo icon_info;
  icon_info.width = icon_sizes[0].width();
  icon_info.height = icon_sizes[0].height();
  icon_info.url = url;
  icons->push_back(icon_info);
}

void GetApplicationInfo(WebView* view, WebApplicationInfo* app_info) {
  WebFrame* main_frame = view->GetMainFrame();
  if (!main_frame)
    return;
  WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);

  WebCore::HTMLHeadElement* head;
  if (!main_frame_impl->frame() ||
      !main_frame_impl->frame()->document() ||
      !(head = main_frame_impl->frame()->document()->head())) {
    return;
  }
  WTF::PassRefPtr<WebCore::HTMLCollection> children = head->children();
  for (unsigned i = 0; i < children->length(); ++i) {
    WebCore::Node* child = children->item(i);
    WebCore::HTMLLinkElement* link = CastHTMLElement<WebCore::HTMLLinkElement>(
        child, WebCore::HTMLNames::linkTag);
    if (link) {
      if (link->isIcon())
        AddInstallIcon(link, &app_info->icons);
    } else {
      WebCore::HTMLMetaElement* meta =
          CastHTMLElement<WebCore::HTMLMetaElement>(
              child, WebCore::HTMLNames::metaTag);
      if (meta) {
        if (meta->name() == String("application-name")) {
          app_info->title = webkit_glue::StringToStdWString(meta->content());
        } else if (meta->name() == String("description")) {
          app_info->description =
              webkit_glue::StringToStdWString(meta->content());
        } else if (meta->name() == String("application-url")) {
          std::string url = webkit_glue::StringToStdString(meta->content());
          GURL main_url = main_frame->GetURL();
          app_info->app_url = main_url.is_valid() ?
              main_url.Resolve(url) : GURL(url);
          if (!app_info->app_url.is_valid())
            app_info->app_url = GURL();
        }
      }
    }
  }
}

bool PauseAnimationAtTimeOnElementWithId(WebView* view,
                                         const std::string& animation_name,
                                         double time,
                                         const std::string& element_id) {
  WebFrame* web_frame = view->GetMainFrame();
  if (!web_frame)
    return false;

  WebCore::Frame* frame = static_cast<WebFrameImpl*>(web_frame)->frame();
  WebCore::AnimationController* controller = frame->animation();
  if (!controller)
    return false;

  WebCore::Element* element =
      frame->document()->getElementById(StdStringToString(element_id));
  if (!element)
    return false;

  return controller->pauseAnimationAtTime(element->renderer(),
                                          StdStringToString(animation_name),
                                          time);
}

bool PauseTransitionAtTimeOnElementWithId(WebView* view,
                                          const std::string& property_name,
                                          double time,
                                          const std::string& element_id) {
  WebFrame* web_frame = view->GetMainFrame();
  if (!web_frame)
    return false;

  WebCore::Frame* frame = static_cast<WebFrameImpl*>(web_frame)->frame();
  WebCore::AnimationController* controller = frame->animation();
  if (!controller)
    return false;

  WebCore::Element* element =
      frame->document()->getElementById(StdStringToString(element_id));
  if (!element)
    return false;

  return controller->pauseTransitionAtTime(element->renderer(),
                                           StdStringToString(property_name),
                                           time);
}

bool ElementDoesAutoCompleteForElementWithId(WebView* view,
                                             const std::string& element_id) {
  WebFrame* web_frame = view->GetMainFrame();
  if (!web_frame)
    return false;

  WebCore::Frame* frame = static_cast<WebFrameImpl*>(web_frame)->frame();
  WebCore::Element* element =
      frame->document()->getElementById(StdStringToString(element_id));
  if (!element || !element->hasLocalName(WebCore::HTMLNames::inputTag))
    return false;

  WebCore::HTMLInputElement* input_element =
      static_cast<WebCore::HTMLInputElement*>(element);
  return input_element->autoComplete();
}

} // webkit_glue
