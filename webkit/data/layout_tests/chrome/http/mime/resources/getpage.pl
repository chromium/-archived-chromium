#!/usr/bin/perl -w
# 
# Copyright 2007 Google Inc.
# All Rights Reserved.
# Author: preethamr@google.com (Preetham Raghunanda)
# 
# Generates a html file using the URL query parameters that will be used to
# test MIME type handling.


use strict;
use CGI qw(:standard);

# Variable used in the script
my $query = new CGI;
my $type = "";
my $html_tag = "";
my $content = "";
my $switch = "";


# Check if the URL has query parameters.
# If it does, obtain their values.
if ($query->param()) {
  $type = $query->param('type');
  if ( $type eq "NULL") {
    $type = "";
  }
  $html_tag = $query->param('tag');
  $content = $query->param('content');
  $switch = $query->param('switch');
    print $type;
}

# print HTTP header with the current content-type
&print_header("$type");

# do not generate an HTML if 'nohtml' switch is used
if($switch eq "nohtml") {
  &print_file("$content");
  exit;
}

# Generate the HTML using the URL query parameters
print "<HTML><HEAD>";

if($type ne "") {
  print "<meta http-equiv=\"content-type\" content=\"$type\">";
}

print "<TITLE> Content-type : $type</TITLE>";
print "<BODY contenteditable>";
print "Rendered HTML successfully<br>";

# Add embed or image tag
if ($html_tag eq "embed") {
  print "<$html_tag src=\"$content\">";
  print "<NOEMBED> Your browser does not support embedded objects. </NOEMBED> ";
  print "</$html_tag>";
}elsif ( $html_tag ) {
  print "<$html_tag alt=\"ERROR : Image not rendered.\" src=\"$content\" />";
}

# Make sure to dumpAsText when run in the test controller
print <<SCRIPT;
<script>
  if(window.layoutTestController)    
    layoutTestController.dumpAsText();
</script>
SCRIPT

print "</BODY>\n</HTML>";

# print_header()
#   Generate the HTTP header with no-cache for this page.
#   HTTP content-type is the only parameter to this subroutine.
sub print_header()
{
  print "Status: 200 OK\r\n";

  if($_[0] ne "") {
    print "Content-type: $_[0]\r\n";
  }

  print "Cache-Control: no-cache\r\n";
  print "\r\n";
}

# print_file()
#   Prints the specified file on to the output stream.
#   Path of the file is the only parameter for this subroutine.
sub print_file()
{
  my $infile = $_[0];
  open IN, "< $infile" or die "ERROR: Content not available.";
  print <IN>;
  close IN;
}
