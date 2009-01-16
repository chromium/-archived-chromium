// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_parser.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "googleurl/src/gurl.h"
#include "libxml/parser.h"
#include "libxml/xmlwriter.h"

namespace {

//
// NOTE: libxml uses the UTF-8 encoding. As 0-127 of UTF-8 corresponds
// to that of char, the following names are all in terms of char. This avoids
// having to convert to wide, then do comparisons

// Defines for element names of the OSD document:
static const char kURLElement[] = "Url";
static const char kParamElement[] = "Param";
static const char kShortNameElement[] = "ShortName";
static const char kDescriptionElement[] = "Description";
static const char kImageElement[] = "Image";
static const char kOpenSearchDescriptionElement[] = "OpenSearchDescription";
static const char kFirefoxSearchDescriptionElement[] = "SearchPlugin";
static const char kLanguageElement[] = "Language";
static const char kInputEncodingElement[] = "InputEncoding";

// Various XML attributes used.
static const char kURLTypeAttribute[] = "type";
static const char kURLTemplateAttribute[] = "template";
static const char kImageTypeAttribute[] = "type";
static const char kImageWidthAttribute[] = "width";
static const char kImageHeightAttribute[] = "height";
static const char kURLIndexOffsetAttribute[] = "indexOffset";
static const char kURLPageOffsetAttribute[] = "pageOffset";
static const char kParamNameAttribute[] = "name";
static const char kParamValueAttribute[] = "value";
static const char kParamMethodAttribute[] = "method";

// Mime type for search results.
static const char kHTMLType[] = "text/html";

// Mime type for as you type suggestions.
static const char kSuggestionType[] = "application/x-suggestions+json";

// Namespace identifier.
static const char kOSDNS[] = "xmlns";

// The namespace for documents we understand.
static const char kNameSpace[] = "http://a9.com/-/spec/opensearch/1.1/";

// Removes the namespace from the specified |name|, ex: os:Url -> Url.
static void PruneNamespace(std::string* name) {
  size_t index = name->find_first_of(":");
  if (index != std::string::npos)
    name->erase(0, index + 1);
}

//
// To minimize memory overhead while parsing, a SAX style parser is used.
// ParsingContext is used to maintain the state we're in the document
// while parsing.
class ParsingContext {
 public:
  // Enum of the known element types.
  enum ElementType {
    UNKNOWN,
    OPEN_SEARCH_DESCRIPTION,
    URL,
    PARAM,
    SHORT_NAME,
    DESCRIPTION,
    IMAGE,
    LANGUAGE,
    INPUT_ENCODING,
  };

  enum Method {
    GET,
    POST
  };

  // Key/value of a Param node.
  typedef std::pair<std::string, std::string> Param;

  ParsingContext(TemplateURLParser::ParameterFilter* parameter_filter,
                 TemplateURL* url)
      : url_(url),
        parameter_filter_(parameter_filter),
        method_(GET),
        suggestion_method_(GET),
        is_suggest_url_(false),
        derive_image_from_url_(false) {
    if (kElementNameToElementTypeMap == NULL)
      InitMapping();
  }

  // Invoked when an element starts.
  void PushElement(const std::string& element) {
    ElementType type;
    if (kElementNameToElementTypeMap->find(element) ==
        kElementNameToElementTypeMap->end()) {
      type = UNKNOWN;
    } else {
      type = (*kElementNameToElementTypeMap)[element];
    }
    elements_.push_back(type);
  }

  void PopElement() {
    elements_.pop_back();
  }

  // Returns the current ElementType.
  ElementType GetKnownType() {
    if (elements_.size() == 2 && elements_[0] == OPEN_SEARCH_DESCRIPTION)
      return elements_[1];

    // We only expect PARAM nodes under the Url node
    if (elements_.size() == 3 && elements_[0] == OPEN_SEARCH_DESCRIPTION &&
        elements_[1] == URL && elements_[2] == PARAM)
      return PARAM;

    return UNKNOWN;
  }

  TemplateURL* template_url() { return url_; }

  void AddImageRef(const std::wstring& type, int width, int height) {
    if (width > 0 && height > 0)
      current_image_.reset(new TemplateURL::ImageRef(type, width, height));
  }

  void EndImage() {
    current_image_.reset();
  }

  void SetImageURL(const std::wstring& url) {
    if (current_image_.get()) {
      current_image_->url = GURL(WideToUTF8(url));
      url_->add_image_ref(*current_image_);
      current_image_.reset();
    }
  }

  void ResetString() {
    string_.clear();
  }

  void AppendString(const std::wstring& string) {
    string_ += string;
  }

  const std::wstring& GetString() {
    return string_;
  }

  void ResetExtraParams() {
    extra_params_.clear();
  }

  void AddExtraParams(const std::string& key, const std::string& value) {
    if (parameter_filter_ && !parameter_filter_->KeepParameter(key, value))
      return;
    extra_params_.push_back(Param(key, value));
  }

  const std::vector<Param>& extra_params() const { return extra_params_; }

  void set_is_suggestion(bool value) { is_suggest_url_ = value; }
  bool is_suggestion() const { return is_suggest_url_; }

  TemplateURLParser::ParameterFilter* parameter_filter() const {
    return parameter_filter_;
  }

  void set_derive_image_from_url(bool derive_image_from_url) {
    derive_image_from_url_ = derive_image_from_url;
  }

  void set_method(Method method) { method_ = method; }
  Method method() { return method_; }

  void set_suggestion_method(Method method) { suggestion_method_ = method; }
  Method suggestion_method() { return suggestion_method_; }

  // Builds the image URL from the Template search URL if no image URL has been
  // set.
  void DeriveImageFromURL() {
    if (derive_image_from_url_ &&
        url_->GetFavIconURL().is_empty() && url_->url()) {
      GURL url(WideToUTF8(url_->url()->url()));  // More url's please...
      url_->SetFavIconURL(TemplateURL::GenerateFaviconURL(url));
    }
  }

 private:
  static void InitMapping() {
    kElementNameToElementTypeMap = new std::map<std::string,ElementType>;
    (*kElementNameToElementTypeMap)[kURLElement] = URL;
    (*kElementNameToElementTypeMap)[kParamElement] = PARAM;
    (*kElementNameToElementTypeMap)[kShortNameElement] = SHORT_NAME;
    (*kElementNameToElementTypeMap)[kDescriptionElement] = DESCRIPTION;
    (*kElementNameToElementTypeMap)[kImageElement] = IMAGE;
    (*kElementNameToElementTypeMap)[kOpenSearchDescriptionElement] =
        OPEN_SEARCH_DESCRIPTION;
    (*kElementNameToElementTypeMap)[kFirefoxSearchDescriptionElement] =
        OPEN_SEARCH_DESCRIPTION;
    (*kElementNameToElementTypeMap)[kLanguageElement] =
        LANGUAGE;
    (*kElementNameToElementTypeMap)[kInputEncodingElement] =
        INPUT_ENCODING;
  }

  // Key is UTF8 encoded.
  static std::map<std::string,ElementType>* kElementNameToElementTypeMap;
  // TemplateURL supplied to Read method. It's owned by the caller, so we
  // don't need to free it.
  TemplateURL* url_;
  std::vector<ElementType> elements_;
  scoped_ptr<TemplateURL::ImageRef> current_image_;

  // Character content for the current element.
  std::wstring string_;

  TemplateURLParser::ParameterFilter* parameter_filter_;

  // The list of parameters parsed in the Param nodes of a Url node.
  std::vector<Param> extra_params_;

  // The HTTP methods used.
  Method method_;
  Method suggestion_method_;

  // If true, we are currently parsing a suggest URL, otherwise it is an HTML
  // search.  Note that we don't need a stack as Url nodes cannot be nested.
  bool is_suggest_url_;

  // Whether we should derive the image from the URL (when images are data
  // URLs).
  bool derive_image_from_url_;

  DISALLOW_EVIL_CONSTRUCTORS(ParsingContext);
};

//static
std::map<std::string,ParsingContext::ElementType>*
    ParsingContext::kElementNameToElementTypeMap = NULL;

std::wstring XMLCharToWide(const xmlChar* value) {
  return UTF8ToWide(std::string((const char*)value));
}

std::wstring XMLCharToWide(const xmlChar* value, int length) {
  return UTF8ToWide(std::string((const char*)value, length));
}

std::string XMLCharToString(const xmlChar* value) {
  return std::string((const char*)value);
}

// Returns true if input_encoding contains a valid input encoding string. This
// doesn't verify that we have a valid encoding for the string, just that the
// string contains characters that constitute a valid input encoding.
bool IsValidEncodingString(const std::string& input_encoding) {
  if (input_encoding.empty())
    return false;

  if (!IsAsciiAlpha(input_encoding[0]))
    return false;

  for (size_t i = 1, max = input_encoding.size(); i < max; ++i) {
    char c = input_encoding[i];
    if (!IsAsciiAlpha(c) && !IsAsciiDigit(c) && c != '.' && c != '_' &&
        c != '-') {
      return false;
    }
  }
  return true;
}

void ParseURL(const xmlChar** atts, ParsingContext* context) {
  if (!atts)
    return;

  TemplateURL* turl = context->template_url();
  const xmlChar** attributes = atts;
  std::wstring template_url;
  bool is_post = false;
  bool is_html_url = false;
  bool is_suggest_url = false;
  int index_offset = 1;
  int page_offset = 1;

  while (*attributes) {
    std::string name(XMLCharToString(*attributes));
    const xmlChar* value = attributes[1];
    if (name == kURLTypeAttribute) {
      std::string type = XMLCharToString(value);
      is_html_url = (type == kHTMLType);
      is_suggest_url = (type == kSuggestionType);
    } else if (name == kURLTemplateAttribute) {
      template_url = XMLCharToWide(value);
    } else if (name == kURLIndexOffsetAttribute) {
      index_offset = std::max(1, StringToInt(XMLCharToWide(value)));
    } else if (name == kURLPageOffsetAttribute) {
      page_offset = std::max(1, StringToInt(XMLCharToWide(value)));
    } else if (name == kParamMethodAttribute) {
      is_post = LowerCaseEqualsASCII(XMLCharToString(value), "post");
    }
    attributes += 2;
  }
  if (is_html_url) {
    turl->SetURL(template_url, index_offset, page_offset);
    context->set_is_suggestion(false);
    if (is_post)
      context->set_method(ParsingContext::POST);
  } else if (is_suggest_url) {
    turl->SetSuggestionsURL(template_url, index_offset, page_offset);
    context->set_is_suggestion(true);
    if (is_post)
      context->set_suggestion_method(ParsingContext::POST);
  }
}

void ParseImage(const xmlChar** atts, ParsingContext* context) {
  if (!atts)
    return;

  const xmlChar** attributes = atts;
  int width = 0;
  int height = 0;
  std::wstring type;
  while (*attributes) {
    std::string name(XMLCharToString(*attributes));
    const xmlChar* value = attributes[1];
    if (name == kImageTypeAttribute) {
      type = XMLCharToWide(value);
    } else if (name == kImageWidthAttribute) {
      width = StringToInt(XMLCharToWide(value));
    } else if (name == kImageHeightAttribute) {
      height = StringToInt(XMLCharToWide(value));
    }
    attributes += 2;
  }
  if (width > 0 && height > 0 && !type.empty()) {
    // Valid Image URL.
    context->AddImageRef(type, width, height);
  }
}

void ParseParam(const xmlChar** atts, ParsingContext* context) {
  if (!atts)
    return;

  const xmlChar** attributes = atts;
  std::wstring type;
  std::string key, value;
  while (*attributes) {
    std::string name(XMLCharToString(*attributes));
    const xmlChar* val = attributes[1];
    if (name == kParamNameAttribute) {
      key = XMLCharToString(val);
    } else if (name == kParamValueAttribute) {
      value = XMLCharToString(val);
    }
    attributes += 2;
  }
  if (!key.empty())
    context->AddExtraParams(key, value);
}

static void AppendParamToQuery(const std::string& key,
                               const std::string& value,
                               std::string* query) {
  if (!query->empty())
   query->append("&");
  if (!key.empty()) {
   query->append(key);
   query->append("=");
  }
  query->append(value);
}

void ProcessURLParams(ParsingContext* context) {
  TemplateURL* t_url = context->template_url();
  const TemplateURLRef* t_url_ref =
      context->is_suggestion() ? t_url->suggestions_url() :
                                 t_url->url();
  if (!t_url_ref)
    return;

  if (!context->parameter_filter() && context->extra_params().empty())
    return;

  GURL url(WideToUTF8(t_url_ref->url()));
  // If there is a parameter filter, parse the existing URL and remove any
  // unwanted parameter.
  TemplateURLParser::ParameterFilter* filter = context->parameter_filter();
  std::string new_query;
  bool modified = false;
  if (filter) {
    url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
    url_parse::Component key, value;
    const char* url_spec = url.spec().c_str();
    while (url_parse::ExtractQueryKeyValue(url_spec, &query, &key, &value)) {
      std::string key_str(url_spec, key.begin, key.len);
      std::string value_str(url_spec, value.begin, value.len);
      if (filter->KeepParameter(key_str, value_str)) {
        AppendParamToQuery(key_str, value_str, &new_query);
      } else {
        modified = true;
      }
    }
  }
  if (!modified)
    new_query = url.query();

  // Add the extra parameters if any.
  const std::vector<ParsingContext::Param>& params = context->extra_params();
  if (!params.empty()) {
    modified = true;
    std::vector<ParsingContext::Param>::const_iterator iter;
    for (iter = params.begin(); iter != params.end(); ++iter)
      AppendParamToQuery(iter->first, iter->second, &new_query);
  }

  if (modified) {
    GURL::Replacements repl;
    repl.SetQueryStr(new_query);
    url = url.ReplaceComponents(repl);
    if (context->is_suggestion()) {
      t_url->SetSuggestionsURL(UTF8ToWide(url.spec()),
                               t_url_ref->index_offset(),
                               t_url_ref->page_offset());
    } else {
      t_url->SetURL(UTF8ToWide(url.spec()),
                    t_url_ref->index_offset(),
                    t_url_ref->page_offset());
    }
  }
}

void StartElementImpl(void *ctx, const xmlChar *name, const xmlChar **atts) {
  ParsingContext* context = reinterpret_cast<ParsingContext*>(ctx);
  std::string node_name((const char*)name);
  PruneNamespace(&node_name);
  context->PushElement(node_name);
  switch (context->GetKnownType()) {
    case ParsingContext::URL:
      context->ResetExtraParams();
      ParseURL(atts, context);
      break;
    case ParsingContext::IMAGE:
      ParseImage(atts, context);
      break;
    case ParsingContext::PARAM:
      ParseParam(atts, context);
      break;
    default:
      break;
  }
  context->ResetString();
}

void EndElementImpl(void *ctx, const xmlChar *name) {
  ParsingContext* context = reinterpret_cast<ParsingContext*>(ctx);
  switch (context->GetKnownType()) {
    case ParsingContext::SHORT_NAME:
      context->template_url()->set_short_name(context->GetString());
      break;
    case ParsingContext::DESCRIPTION:
      context->template_url()->set_description(context->GetString());
      break;
    case ParsingContext::IMAGE: {
      GURL image_url(WideToUTF8(context->GetString()));
      if (image_url.SchemeIs("data")) {
        // TODO (jcampan): bug 1169256: when dealing with data URL, we need to
        // decode the data URL in the renderer. For now, we'll just point to the
        // fav icon from the URL.
        context->set_derive_image_from_url(true);
      } else {
        context->SetImageURL(context->GetString());
      }
      context->EndImage();
      break;
    }
    case ParsingContext::LANGUAGE:
      context->template_url()->add_language(context->GetString());
      break;
    case ParsingContext::INPUT_ENCODING: {
      std::string input_encoding = WideToASCII(context->GetString());
      if (IsValidEncodingString(input_encoding))
        context->template_url()->add_input_encoding(input_encoding);
      break;
    }
    case ParsingContext::URL:
      ProcessURLParams(context);
      break;
    default:
      break;
  }
  context->ResetString();
  context->PopElement();
}

void CharactersImpl(void *ctx, const xmlChar *ch, int len) {
  ParsingContext* context = reinterpret_cast<ParsingContext*>(ctx);
  context->AppendString(XMLCharToWide(ch, len));
}

// Returns true if the ref is null, or the url wrapped by ref is
// valid with a spec of http/https.
bool IsHTTPRef(const TemplateURLRef* ref) {
  if (ref == NULL)
    return true;
  GURL url(WideToUTF8(ref->url()));
  return (url.is_valid() && (url.SchemeIs("http") || url.SchemeIs("https")));
}

// Returns true if the TemplateURL is legal. A legal TemplateURL is one
// where all URLs have a spec of http/https.
bool IsLegal(TemplateURL* url) {
  if (!IsHTTPRef(url->url()) || !IsHTTPRef(url->suggestions_url()))
    return false;
  // Make sure all the image refs are legal.
  const std::vector<TemplateURL::ImageRef>& image_refs = url->image_refs();
  for (size_t i = 0; i < image_refs.size(); i++) {
    GURL image_url(image_refs[i].url);
    if (!image_url.is_valid() ||
        !(image_url.SchemeIs("http") || image_url.SchemeIs("https"))) {
      return false;
    }
  }
  return true;
}

} // namespace

// static
bool TemplateURLParser::Parse(const unsigned char* data, size_t length,
                              TemplateURLParser::ParameterFilter* param_filter,
                              TemplateURL* url) {
  DCHECK(url);
  // xmlSubstituteEntitiesDefault(1) makes it so that &amp; isn't mapped to
  // &#38; . Unfortunately xmlSubstituteEntitiesDefault effects global state.
  // If this becomes problematic we'll need to provide our own entity
  // type for &amp;, or strip out &#34; by hand after parsing.
  int last_sub_entities_value = xmlSubstituteEntitiesDefault(1);
  ParsingContext context(param_filter, url);
  xmlSAXHandler sax_handler;
  memset(&sax_handler, 0, sizeof(sax_handler));
  sax_handler.startElement = &StartElementImpl;
  sax_handler.endElement = &EndElementImpl;
  sax_handler.characters = &CharactersImpl;
  xmlSAXUserParseMemory(&sax_handler, &context,
                        reinterpret_cast<const char*>(data),
                        static_cast<int>(length));
  xmlSubstituteEntitiesDefault(last_sub_entities_value);
  // If the image was a data URL, use the favicon from the search URL instead.
  // (see TODO inEndElementImpl()).
  context.DeriveImageFromURL();

  // TODO(jcampan): http://b/issue?id=1196285 we do not support search engines
  //                that use POST yet.
  if (context.method() == ParsingContext::POST)
    return false;
  if (context.suggestion_method() == ParsingContext::POST)
    url->SetSuggestionsURL(L"", 0, 0);

  if (!url->short_name().empty() && !url->description().empty()) {
    // So far so good, make sure the urls are http.
    return IsLegal(url);
  }
  return false;
}


