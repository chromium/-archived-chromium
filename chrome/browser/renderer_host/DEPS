include_rules = [
  # DO NOT ALLOW tab_contents INCLUDES FROM THIS DIRECTORY! The renderer_host
  # layer should be usable in contexts other than inside TabContents. Instead,
  # you should call upward through the RenderViewHostDelegate interface. If
  # your test needs some TabContents code, you can put it in renderer_host/test
  # which can include more stuff for testing purposes.
  #
  # If somebody adds an include and you're fixing the build, please revert them
  # instead of commenting out this rule.
  "-chrome/browser/tab_contents",
]

