These test files would normally go in the parent directory next to the
corresponding implementation files. However, they depend on the tab_contents
directory which we don't want the normal code to use. So we put them in this
separate directory with the tab_contents includes allowed by DEPS from here only.
