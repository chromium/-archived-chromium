This test is forked from Apple, we removed the request for 255.255.255.255
without any port number, which will fail or succeed depending on your proxy.
Instead, we go straight to testing port 1.

The expected results are Chrome-specific. They will need to be generated for
Mac (which will report, for example NSURLRequest* instead of WebRequest*). We
currently don't have a way to say a test is forked in pending, and also has
Chrome-specific results.
