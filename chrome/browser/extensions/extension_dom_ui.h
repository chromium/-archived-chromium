#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DOM_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DOM_UI_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"

// This class implements DOMUI for extensions and allows extensions to put UI in
// the main tab contents area.
class ExtensionDOMUI 
    : public DOMUI,
      public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit ExtensionDOMUI(TabContents* tab_contents);
  ExtensionFunctionDispatcher* extension_function_dispatcher() const {
    return extension_function_dispatcher_.get();
  }

  // DOMUI
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content,
                                   int request_id,
                                   bool has_callback);

  // ExtensionFunctionDispatcher::Delegate
  virtual Browser* GetBrowser();

 private:
  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DOM_UI_H_
