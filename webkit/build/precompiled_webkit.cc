// There is no need to include anything here. You may ask: Why? Well that's a
// valid question.
//
// The answer is that since we are using the /FI compiler option, not including
// the PCH file here asserts that the /FI option is indeed correctly set.
//
// "/FI [header file]" means force include. The specified include file will be
// included before each compilation unit (.c/.cc file).
