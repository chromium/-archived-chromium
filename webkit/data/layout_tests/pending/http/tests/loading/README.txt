crash-multiple-family-fontface.html
-------------------------------------------
This test should be removed once the issue is fixed upstream (https://bugs.webkit.org/show_bug.cgi?id=18378)
Chrome has a hack in its forked webkit to avoid crashing (http://b/1063835)
Note: this test is must be under http/ to trigger the crash (custom font cannot be local).
