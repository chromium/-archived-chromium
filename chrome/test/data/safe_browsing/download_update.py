#!/usr/bin/python

import urllib,sys


url = 'http://safebrowsing.clients.google.com/safebrowsing/downloads?client=googleclient&appver=1.0&pver=2.1'

if len(sys.argv) == 1:
  data = 'goog-phish-shavar;\ngoog-malware-shavar;\n'
else:
  post_data_file = sys.argv[1]
  file = open(post_data_file, "r")
  data = file.read()
  file.close

response = urllib.urlopen(url, data)

response_file = open("response", "r+")
response_file.write(response.read())
response_file.seek(0)

counter = 0

for line in response_file:
  if not line.startswith('u:'):
    continue

  chunk_url = 'http://' + line[2:]
  filename = chunk_url[chunk_url.rfind('/') + 1:]
  filename =  "%03d" % counter + filename[0:filename.rfind('_')]
  counter += 1

  urllib.urlretrieve(chunk_url, filename)

response_file.close()
