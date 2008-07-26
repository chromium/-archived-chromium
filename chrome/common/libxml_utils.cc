// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/common/libxml_utils.h"
#include "base/logging.h"

#include "libxml/xmlreader.h"

std::string XmlStringToStdString(const xmlChar* xmlstring) {
  // xmlChar*s are UTF-8, so this cast is safe.
  if (xmlstring)
    return std::string(reinterpret_cast<const char*>(xmlstring));
  else
    return "";
}

XmlReader::XmlReader()
    : reader_(NULL),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      error_func_(this, &XmlReader::GenericErrorCallback) {
}

XmlReader::~XmlReader() {
  if (reader_)
    xmlFreeTextReader(reader_);
}

/* static */ void XmlReader::GenericErrorCallback(void* context,
                                                  const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  char buffer[1 << 9];
  vsnprintf_s(buffer, arraysize(buffer), _TRUNCATE, msg, args);

  XmlReader* reader = static_cast<XmlReader*>(context);
  reader->errors_.append(buffer);
}

bool XmlReader::Load(const std::string& input) {
  const int kParseOptions =
      XML_PARSE_RECOVER |  // recover on errors
      XML_PARSE_NONET;     // forbid network access
  // TODO(evanm): Verify it's OK to pass NULL for the URL and encoding.
  // The libxml code allows for these, but it's unclear what effect is has.
  reader_ = xmlReaderForMemory(input.data(), static_cast<int>(input.size()),
                               NULL, NULL, kParseOptions);
  return reader_ != NULL;
}

bool XmlReader::LoadFile(const std::string& file_path) {

  const int kParseOptions =
      XML_PARSE_RECOVER |  // recover on errors
      XML_PARSE_NONET;     // forbid network access
  reader_ = xmlReaderForFile(file_path.c_str(), NULL, kParseOptions);
  return reader_ != NULL;
}

bool XmlReader::NodeAttribute(const char* name, std::string* out) {
  xmlChar* value = xmlTextReaderGetAttribute(reader_, BAD_CAST name);
  if (!value)
    return false;
  *out = XmlStringToStdString(value);
  xmlFree(value);
  return true;
}

bool XmlReader::ReadElementContent(std::string* content) {
  DCHECK(NodeType() == XML_READER_TYPE_ELEMENT);
  const int start_depth = Depth();

  if (xmlTextReaderIsEmptyElement(reader_)) {
    // Empty tag.  We succesfully read the content, but it's
    // empty.
    *content = "";
    // Advance past this empty tag.
    if (!Read())
      return false;
    return true;
  }

  // Advance past opening element tag.
  if (!Read())
    return false;

  // Read the content.  We read up until we hit a closing tag at the
  // same level as our starting point.
  while (NodeType() != XML_READER_TYPE_END_ELEMENT || Depth() != start_depth) {
    *content += XmlStringToStdString(xmlTextReaderConstValue(reader_));
    if (!Read())
      return false;
  }

  // Advance past ending element tag.
  DCHECK_EQ(NodeType(), XML_READER_TYPE_END_ELEMENT);
  if (!Read())
    return false;

  return true;
}

bool XmlReader::SkipToElement() {
  do {
    switch (NodeType()) {
    case XML_READER_TYPE_ELEMENT:
      return true;
    case XML_READER_TYPE_END_ELEMENT:
      return false;
    default:
      // Skip all other node types.
      continue;
    }
  } while (Read());
  return false;
}


// XmlWriter functions

XmlWriter::XmlWriter() :
    writer_(NULL) {}

XmlWriter::~XmlWriter() {
  if (writer_)
    xmlFreeTextWriter(writer_);
  if (buffer_)
    xmlBufferFree(buffer_);
}

void XmlWriter::StartWriting() {
  buffer_ = xmlBufferCreate();
  writer_ = xmlNewTextWriterMemory(buffer_, 0);
  xmlTextWriterSetIndent(writer_, 1);
  xmlTextWriterStartDocument(writer_, NULL, NULL, NULL);
}

void XmlWriter::StopWriting() {
  xmlTextWriterEndDocument(writer_);
  xmlFreeTextWriter(writer_);
  writer_ = NULL;
}
