/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "LocalizedStrings.h"
#include "PlatformString.h"
#include "IntSize.h"

#undef LOG

#include "base/logging.h"
#include "base/file_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

#include "webkit_strings.h"

using namespace WebCore;

inline String GetLocalizedString(int message_id) {
  const string16& str = webkit_glue::GetLocalizedString(message_id);
  return webkit_glue::String16ToString(str);
}

String WebCore::searchableIndexIntroduction() {
  return GetLocalizedString(IDS_SEARCHABLE_INDEX_INTRO);
}
String WebCore::submitButtonDefaultLabel() {
  return GetLocalizedString(IDS_FORM_SUBMIT_LABEL);
}
String WebCore::inputElementAltText() {
  return GetLocalizedString(IDS_FORM_INPUT_ALT);
}
String WebCore::resetButtonDefaultLabel() {
  return GetLocalizedString(IDS_FORM_RESET_LABEL);
}
String WebCore::fileButtonChooseFileLabel() {
  return GetLocalizedString(IDS_FORM_FILE_BUTTON_LABEL);
}
String WebCore::fileButtonNoFileSelectedLabel() {
  return GetLocalizedString(IDS_FORM_FILE_NO_FILE_LABEL);
}

String WebCore::searchMenuNoRecentSearchesText() {
  return GetLocalizedString(IDS_RECENT_SEARCHES_NONE);
}
String WebCore::searchMenuRecentSearchesText() {
  return GetLocalizedString(IDS_RECENT_SEARCHES);
}
String WebCore::searchMenuClearRecentSearchesText() {
  return GetLocalizedString(IDS_RECENT_SEARCHES_CLEAR);
}

// A11y strings.
String WebCore::AXWebAreaText() {
  return GetLocalizedString(IDS_AX_ROLE_WEB_AREA);
}
String WebCore::AXLinkText() {
  return GetLocalizedString(IDS_AX_ROLE_LINK);
}
String WebCore::AXListMarkerText() {
  return GetLocalizedString(IDS_AX_ROLE_LIST_MARKER);
}
String WebCore::AXImageMapText() {
  return GetLocalizedString(IDS_AX_ROLE_IMAGE_MAP);
}
String WebCore::AXHeadingText() {
  return GetLocalizedString(IDS_AX_ROLE_HEADING);
}
String WebCore::AXDefinitionListTermText() {
  NOTIMPLEMENTED();
  return String("term");
}
String WebCore::AXDefinitionListDefinitionText() {
  NOTIMPLEMENTED();
  return String("definition");
}
String WebCore::AXButtonActionVerb() {
  return GetLocalizedString(IDS_AX_BUTTON_ACTION_VERB);
}
String WebCore::AXRadioButtonActionVerb() {
  return GetLocalizedString(IDS_AX_RADIO_BUTTON_ACTION_VERB);
}
String WebCore::AXTextFieldActionVerb() {
  return GetLocalizedString(IDS_AX_TEXT_FIELD_ACTION_VERB);
}
String WebCore::AXCheckedCheckBoxActionVerb() {
  return GetLocalizedString(IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB);
}
String WebCore::AXUncheckedCheckBoxActionVerb() {
  return GetLocalizedString(IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB);
}
String WebCore::AXLinkActionVerb() {
  return GetLocalizedString(IDS_AX_LINK_ACTION_VERB);
}

// Used in FTPDirectoryDocument.cpp
String WebCore::unknownFileSizeText() {
  return String();
}

// The following two functions are not declared in LocalizedStrings.h.
// They are used by the menu for the HTML keygen tag.
namespace WebCore {
String keygenMenuHighGradeKeySize() {
  return GetLocalizedString(IDS_KEYGEN_HIGH_GRADE_KEY);
}
String keygenMenuMediumGradeKeySize() {
  return GetLocalizedString(IDS_KEYGEN_MED_GRADE_KEY);
}

// Used in ImageDocument.cpp as the title for pages when that page is an image.
String imageTitle(const String& filename, const IntSize& size) {
  // C3 97 is UTF-8 for U+00D7 (multiplication sign).
  std::string size_str = StringPrintf(" (%d\xC3\x97%d)",
                                      size.width(), size.height());
  return filename + webkit_glue::StdStringToString(size_str);
}

} //namespace WebCore

// We don't use these strings, so they return an empty String. We can't just
// make them asserts because webcore still calls them.
String WebCore::contextMenuItemTagOpenLinkInNewWindow() { return String(); }
String WebCore::contextMenuItemTagDownloadLinkToDisk() { return String(); }
String WebCore::contextMenuItemTagCopyLinkToClipboard() { return String(); }
String WebCore::contextMenuItemTagOpenImageInNewWindow() { return String(); }
String WebCore::contextMenuItemTagDownloadImageToDisk() { return String(); }
String WebCore::contextMenuItemTagCopyImageToClipboard() { return String(); }
String WebCore::contextMenuItemTagOpenFrameInNewWindow() { return String(); }
String WebCore::contextMenuItemTagCopy() { return String(); }
String WebCore::contextMenuItemTagGoBack() { return String(); }
String WebCore::contextMenuItemTagGoForward() { return String(); }
String WebCore::contextMenuItemTagStop() { return String(); }
String WebCore::contextMenuItemTagReload() { return String(); }
String WebCore::contextMenuItemTagCut() { return String(); }
String WebCore::contextMenuItemTagPaste() { return String(); }
String WebCore::contextMenuItemTagNoGuessesFound() { return String(); }
String WebCore::contextMenuItemTagIgnoreSpelling() { return String(); }
String WebCore::contextMenuItemTagLearnSpelling() { return String(); }
String WebCore::contextMenuItemTagSearchWeb() { return String(); }
String WebCore::contextMenuItemTagLookUpInDictionary() { return String(); }
String WebCore::contextMenuItemTagOpenLink() { return String(); }
String WebCore::contextMenuItemTagIgnoreGrammar() { return String(); }
String WebCore::contextMenuItemTagSpellingMenu() { return String(); }
String WebCore::contextMenuItemTagCheckSpelling() { return String(); }
String WebCore::contextMenuItemTagCheckSpellingWhileTyping() { return String(); }
String WebCore::contextMenuItemTagCheckGrammarWithSpelling() { return String(); }
String WebCore::contextMenuItemTagFontMenu() { return String(); }
String WebCore::contextMenuItemTagBold() { return String(); }
String WebCore::contextMenuItemTagItalic() { return String(); }
String WebCore::contextMenuItemTagUnderline() { return String(); }
String WebCore::contextMenuItemTagOutline() { return String(); }
String WebCore::contextMenuItemTagWritingDirectionMenu() { return String(); }
String WebCore::contextMenuItemTagTextDirectionMenu() { return String(); }
String WebCore::contextMenuItemTagDefaultDirection() { return String(); }
String WebCore::contextMenuItemTagLeftToRight() { return String(); }
String WebCore::contextMenuItemTagRightToLeft() { return String(); }
String WebCore::contextMenuItemTagInspectElement() { return String(); }
String WebCore::contextMenuItemTagShowSpellingPanel(bool show) { return String(); }
