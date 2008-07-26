"Template processing that is more suitable for the specific development-time
and runtime requirements of AJAX based web applications.

Specifically, support the case to update a previous template processing
output with an incremental change to the original input data."

This is a branch of jstemplate.js and its dependencies from the Google Maps
code.

Uncalled functions have been removed from base.js, dom.js and util.js to make
the resulting javascript smaller.

compile.sh is a simple shell script used to generate the single compiled
jsfile.

jstemplate_compiled.js is the output after passing the code through
compile.sh.
