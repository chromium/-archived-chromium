deps = {
  "src/breakpad/src":
    "http://google-breakpad.googlecode.com/svn/trunk/src@285",

  "src/googleurl":
    "http://google-url.googlecode.com/svn/trunk@93",

  "src/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@63",

  "src/third_party/WebKit":
    "/trunk/third_party/WebKit@19",

  "src/third_party/cygwin":
    "/trunk/deps/third_party/cygwin@1788",

  "src/third_party/icu38":
    "/trunk/deps/third_party/icu38@2463",

  "src/third_party/python_24":
    "/trunk/deps/third_party/python_24@1790",

  "src/third_party/svn":
    "/trunk/deps/third_party/svn@1791",

  "src/v8":
    "http://v8.googlecode.com/svn/trunk@329",

  "src/webkit/data/layout_tests/LayoutTests":
    "http://svn.webkit.org/repository/webkit/branches/Safari-3-1-branch/LayoutTests@31256",
}

include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",

  # For now, we allow ICU to be included by specifying "unicode/...", although
  # this should probably change.
  "+unicode"
]

# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "breakpad",
   "sdch",
   "skia",
   "testing",
   "third_party",
   "v8",
]
