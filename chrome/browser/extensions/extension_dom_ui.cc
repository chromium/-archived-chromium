#include "chrome/browser/extensions/extension_dom_ui.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/bindings_policy.h"

ExtensionDOMUI::ExtensionDOMUI(TabContents* tab_contents)
    : DOMUI(tab_contents) {
  // TODO(aa): It would be cool to show the extension's icon in here.
  hide_favicon_ = true;
  should_hide_url_ = true;
  bindings_ = BindingsPolicy::EXTENSION;
}

void ExtensionDOMUI::RenderViewCreated(RenderViewHost* render_view_host) {
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(render_view_host, this,
                                      tab_contents()->GetURL()));
}

void ExtensionDOMUI::ProcessDOMUIMessage(const std::string& message,
                                         const std::string& content,
                                         int request_id,
                                         bool has_callback) {
  extension_function_dispatcher_->HandleRequest(message, content, request_id,
                                                has_callback);
}

Browser* ExtensionDOMUI::GetBrowser() {
  return static_cast<Browser*>(tab_contents()->delegate());
}
