// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_IHTMLDOCUMENT_IMPL_H__
#define WEBKIT_ACTIVEX_SHIM_IHTMLDOCUMENT_IMPL_H__

#include <mshtml.h>
#include "webkit/activex_shim/activex_util.h"

namespace activex_shim {

// TODO(ruijiang): Right now this is a dummy implementation of IHTMLDocument2.
// We should connect it with NPObject directly and implement necessary
// functions needed by controls.
class IHTMLDocument2Impl : public IHTMLDocument2 {
 public:
  // IHTMLDocument
  virtual HRESULT STDMETHODCALLTYPE get_Script(IDispatch **p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }

  // IHTMLDocument2
  virtual HRESULT STDMETHODCALLTYPE get_all(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_body(IHTMLElement** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_activeElement(IHTMLElement** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_images(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_applets(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_links(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_forms(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_anchors(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_title(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_title(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_scripts(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_designMode(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_designMode(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_selection(IHTMLSelectionObject** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_readyState(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_frames(IHTMLFramesCollection2** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_embeds(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_plugins(IHTMLElementCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_alinkColor(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_alinkColor(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_bgColor(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_bgColor(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_fgColor(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_fgColor(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_linkColor(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_linkColor(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_vlinkColor(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_vlinkColor(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_referrer(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_location(IHTMLLocation** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_lastModified(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_URL(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_URL(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_domain(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_domain(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_cookie(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_cookie(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_expando(VARIANT_BOOL v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_expando(VARIANT_BOOL* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_charset(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_charset(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_defaultCharset(BSTR v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_defaultCharset(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_mimeType(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_fileSize(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_fileCreatedDate(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_fileModifiedDate(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_fileUpdatedDate(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_security(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_protocol(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_nameProp(BSTR* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE write(SAFEARRAY* psarray) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE writeln(SAFEARRAY* psarray) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE open(BSTR url,
                                         VARIANT name,
                                         VARIANT features,
                                         VARIANT replace,
                                         IDispatch** pom_window_result) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE close() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE clear() {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE queryCommandSupported(BSTR cmd_id,
                                                          VARIANT_BOOL* ret) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE queryCommandEnabled(BSTR cmd_id,
                                                        VARIANT_BOOL* ret) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE queryCommandState(BSTR cmd_id,
                                                      VARIANT_BOOL* ret) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE queryCommandIndeterm(BSTR cmd_id,
                                                         VARIANT_BOOL* ret) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE queryCommandText(BSTR cmd_id,
                                                     BSTR* pcmd_text) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE queryCommandValue(BSTR cmd_id,
                                                      VARIANT* pcmd_value) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE execCommand(BSTR cmd_id,
                                                VARIANT_BOOL show_ui,
                                                VARIANT value,
                                                VARIANT_BOOL* ret) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE execCommandShowHelp(BSTR cmd_id,
                                                        VARIANT_BOOL* ret) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE createElement(BSTR e_tag,
                                                  IHTMLElement** new_elem) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onhelp(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onhelp(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onclick(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onclick(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_ondblclick(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_ondblclick(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onkeyup(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onkeyup(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onkeydown(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onkeydown(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onkeypress(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onkeypress(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onmouseup(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onmouseup(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onmousedown(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onmousedown(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onmousemove(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onmousemove(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onmouseout(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onmouseout(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onmouseover(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onmouseover(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onreadystatechange(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onreadystatechange(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onafterupdate(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onafterupdate(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onrowexit(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onrowexit(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onrowenter(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onrowenter(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_ondragstart(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_ondragstart(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onselectstart(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onselectstart(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE elementFromPoint(
      long x,
      long y,
      IHTMLElement** element_hit) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_parentWindow(IHTMLWindow2** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_styleSheets(
      IHTMLStyleSheetsCollection** p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onbeforeupdate(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onbeforeupdate(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE put_onerrorupdate(VARIANT v) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE get_onerrorupdate(VARIANT* p) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE toString(BSTR* str) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE createStyleSheet(
      BSTR href,
      long index,
      IHTMLStyleSheet** new_style_sheet) {
    TRACK_METHOD();
    return E_NOTIMPL;
  }
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_IHTMLDOCUMENT_IMPL_H__
