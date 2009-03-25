// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__
#define WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__

#include <deque>

#include "DOMWindow.h"
#include "EditorClient.h"

#include "base/task.h"

namespace WebCore {
class Frame;
class HTMLInputElement;
class Node;
class PlatformKeyboardEvent;
}

class WebView;
class WebViewImpl;

class EditorClientImpl : public WebCore::EditorClient {
 public:
  EditorClientImpl(WebView* web_view);
  virtual ~EditorClientImpl();
  virtual void pageDestroyed();

  virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*);
  virtual bool smartInsertDeleteEnabled();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual bool isContinuousSpellCheckingEnabled();
  virtual void toggleContinuousSpellChecking();
  virtual bool isGrammarCheckingEnabled();
  virtual void toggleGrammarChecking();
  virtual int spellCheckerDocumentTag();

  virtual bool isEditable();

  virtual bool shouldBeginEditing(WebCore::Range* range);
  virtual bool shouldEndEditing(WebCore::Range* range);
  virtual bool shouldInsertNode(WebCore::Node* node, WebCore::Range* range,
                                WebCore::EditorInsertAction action);
  virtual bool shouldInsertText(const WebCore::String& text, WebCore::Range* range,
                                WebCore::EditorInsertAction action);
  virtual bool shouldDeleteRange(WebCore::Range* range);
  virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange,
                                         WebCore::Range* toRange,
                                         WebCore::EAffinity affinity,
                                         bool stillSelecting);
  virtual bool shouldApplyStyle(WebCore::CSSStyleDeclaration* style,
                                WebCore::Range* range);
  virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*);

  virtual void didBeginEditing();
  virtual void respondToChangedContents();
  virtual void respondToChangedSelection();
  virtual void didEndEditing();
  virtual void didWriteSelectionToPasteboard();
  virtual void didSetSelectionTypesForPasteboard();

  virtual void registerCommandForUndo(PassRefPtr<WebCore::EditCommand>);
  virtual void registerCommandForRedo(PassRefPtr<WebCore::EditCommand>);
  virtual void clearUndoRedoOperations();

  virtual bool canUndo() const;
  virtual bool canRedo() const;

  virtual void undo();
  virtual void redo();

  virtual const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
  virtual bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
  virtual void handleKeyboardEvent(WebCore::KeyboardEvent*);
  virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*);

  virtual void textFieldDidBeginEditing(WebCore::Element*);
  virtual void textFieldDidEndEditing(WebCore::Element*);
  virtual void textDidChangeInTextField(WebCore::Element*);
  virtual bool doTextFieldCommandFromEvent(WebCore::Element*,WebCore::KeyboardEvent*);
  virtual void textWillBeDeletedInTextField(WebCore::Element*);
  virtual void textDidChangeInTextArea(WebCore::Element*);

  virtual void ignoreWordInSpellDocument(const WebCore::String&);
  virtual void learnWord(const WebCore::String&);
  virtual void checkSpellingOfString(const UChar*, int length,
                                     int* misspellingLocation,
                                     int* misspellingLength);
  virtual void checkGrammarOfString(const UChar*, int length,
                                    WTF::Vector<WebCore::GrammarDetail>&,
                                    int* badGrammarLocation,
                                    int* badGrammarLength);
  virtual void updateSpellingUIWithGrammarString(const WebCore::String&,
                                                 const WebCore::GrammarDetail& detail);
  virtual void updateSpellingUIWithMisspelledWord(const WebCore::String&);
  virtual void showSpellingUI(bool show);
  virtual bool spellingUIIsShowing();
  virtual void getGuessesForWord(const WebCore::String&,
                                 WTF::Vector<WebCore::String>& guesses);
  virtual void setInputMethodState(bool enabled);

  void SetUseEditorDelegate(bool value) { use_editor_delegate_ = value; }

  // It would be better to add these methods to the objects they describe, but
  // those are in WebCore and therefore inaccessible.
  virtual std::wstring DescribeOrError(int number,
                                       WebCore::ExceptionCode ec);
  virtual std::wstring DescribeOrError(WebCore::Node* node,
                                       WebCore::ExceptionCode ec);
  virtual std::wstring Describe(WebCore::Range* range);
  virtual std::wstring Describe(WebCore::Node* node);
  virtual std::wstring Describe(WebCore::EditorInsertAction action);
  virtual std::wstring Describe(WebCore::EAffinity affinity);
  virtual std::wstring Describe(WebCore::CSSStyleDeclaration* style);

  // Shows the autofill popup for |node| if it is an HTMLInputElement and it is
  // empty.  This is called when you press the up or down arrow in a text field
  // or when clicking an already focused text-field.
  // Returns true if the autofill popup has been scheduled to be shown, false
  // otherwise.
  virtual bool ShowAutofillForNode(WebCore::Node* node);

 private:
  void ModifySelection(WebCore::Frame* frame,
                       WebCore::KeyboardEvent* event);

  // Popups an autofill menu for |input_element| is applicable.
  // |autofill_on_empty_value| indicates whether the autofill should be shown
  // when the text-field is empty.
  // Returns true if the autofill popup has been scheduled to be shown, false
  // otherwise.
  bool Autofill(WebCore::HTMLInputElement* input_element,
                bool autofill_on_empty_value);

  // This method is invoked later by Autofill() as when Autofill() is invoked
  // (from one of the EditorClient callback) the carret position is not
  // reflecting the last text change yet and we need it to decide whether or not
  // to show the autofill popup.
  void DoAutofill(WebCore::HTMLInputElement* input_element,
                  bool autofill_on_empty_value,
                  bool backspace);

 protected:
  WebViewImpl* web_view_;
  bool use_editor_delegate_;
  bool in_redo_;

  typedef std::deque<WTF::RefPtr<WebCore::EditCommand> > EditCommandStack;
  EditCommandStack undo_stack_;
  EditCommandStack redo_stack_;

 private:
  // Returns whether or not the focused control needs spell-checking.
  // Currently, this function just retrieves the focused node and determines
  // whether or not it is a <textarea> element or an element whose
  // contenteditable attribute is true.
  // TODO(hbono): Bug 740540: This code just implements the default behavior
  // proposed in this issue. We should also retrieve "spellcheck" attributes
  // for text fields and create a flag to over-write the default behavior.
  bool ShouldSpellcheckByDefault();

  // Whether the last entered key was a backspace.
  bool backspace_pressed_;

  // This flag is set to false if spell check for this editor is manually
  // turned off. The default setting is SPELLCHECK_AUTOMATIC.
  enum {
    SPELLCHECK_AUTOMATIC,
    SPELLCHECK_FORCED_ON,
    SPELLCHECK_FORCED_OFF
  };
  int spell_check_this_field_status_;

  // The method factory used to post autofill related tasks.
  ScopedRunnableMethodFactory<EditorClientImpl> autofill_factory_;
};

#endif  // WEBKIT_GLUE_EDITOR_CLIENT_IMPL_H__
