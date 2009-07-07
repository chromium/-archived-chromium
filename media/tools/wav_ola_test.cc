// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This application is a test for AudioRendererAlgorithmOLA. It reads in a
// specified wav file (so far only 8, 16 and 32 bit are supported) and uses
// ARAO to scale the playback by a specified rate. Then it outputs the result
// to the specified location. Command line calls should be as follows:
//
//    wav_ola_test RATE INFILE OUTFILE

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/filters/audio_renderer_algorithm_ola.h"

using file_util::ScopedFILE;
using media::AudioRendererAlgorithmOLA;
using media::DataBuffer;

const size_t kDefaultWindowSize = 4096;

struct WavHeader {
  int32 riff;
  int32 chunk_size;
  char unused0[8];
  int32 subchunk1_size;
  int16 audio_format;
  int16 channels;
  int32 sample_rate;
  char unused1[6];
  int16 bit_rate;
  char unused2[4];
  int32 subchunk2_size;
};

// Dummy class to feed data to OLA algorithm. Necessary to create callback.
class Dummy {
 public:
  Dummy(FILE* in, AudioRendererAlgorithmOLA* ola)
      : input_(in),
        ola_(ola) {
  }

  void ReadDataForAlg() {
    scoped_refptr<DataBuffer> b(new DataBuffer());
    uint8* buf = b->GetWritableData(kDefaultWindowSize);
    if (fread(buf, 1, kDefaultWindowSize, input_) > 0) {
      ola_->EnqueueBuffer(b.get());
    }
  }

 private:
  FILE* input_;
  AudioRendererAlgorithmOLA* ola_;

  DISALLOW_COPY_AND_ASSIGN(Dummy);
};

int main(int argc, const char** argv) {
  AudioRendererAlgorithmOLA ola;
  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  std::vector<std::wstring> filenames(cmd_line->GetLooseValues());
  if (filenames.empty()) {
    std::cerr << "Usage: alg_test RATE INFILE OUTFILE\n"
              << std::endl;
    return 1;
  }

  // Retrieve command line options.
  std::string in_path(WideToUTF8(filenames[1]));
  std::string out_path(WideToUTF8(filenames[2]));
  double playback_rate = 0.0;

  // Determine speed of rerecord.
  if (!StringToDouble(WideToUTF8(filenames[0]), &playback_rate))
    playback_rate = 0.0;

  // Open input file.
  ScopedFILE input(file_util::OpenFile(in_path.c_str(), "rb"));
  if (!(input.get())) {
    LOG(ERROR) << "could not open input";
    return 1;
  }

  // Open output file.
  ScopedFILE output(file_util::OpenFile(out_path.c_str(), "wb"));
  if (!(output.get())) {
    LOG(ERROR) << "could not open output";
    return 1;
  }

  // Read in header.
  WavHeader wav;
  if (fread(&wav, sizeof(wav), 1, input.get()) < 1) {
    LOG(ERROR) << "could not read WAV header";
    return 1;
  }

  // Instantiate dummy class and callback to feed data to |ola|.
  Dummy guy(input.get(), &ola);
  AudioRendererAlgorithmOLA::RequestReadCallback* cb =
      NewCallback(&guy, &Dummy::ReadDataForAlg);
  ola.Initialize(wav.channels,
                 wav.bit_rate,
                 static_cast<float>(playback_rate),
                 cb);

  // Print out input format.
  std::cout << in_path << "\n"
            << "Channels: " << wav.channels << "\n"
            << "Sample Rate: " << wav.sample_rate << "\n"
            << "Bit Rate: " << wav.bit_rate << "\n"
            << "\n"
            << "Scaling audio by " << playback_rate << "x..." << std::endl;

  // Write the header back out again.
  if (fwrite(&wav, sizeof(wav), 1, output.get()) < 1) {
    LOG(ERROR) << "could not write WAV header";
    return 1;
  }

  // Create buffer to be filled by |ola|.
  scoped_refptr<DataBuffer> buffer(new DataBuffer());
  uint8* buf = buffer->GetWritableData(kDefaultWindowSize);

  // Keep track of bytes written to disk and bytes copied to |b|.
  size_t bytes_written = 0;
  size_t bytes;
  while ((bytes = ola.FillBuffer(buffer.get())) > 0) {
    if (fwrite(buf, 1, bytes, output.get()) != bytes) {
      LOG(ERROR) << "could not write data after " << bytes_written;
    } else {
      bytes_written += bytes;
    }
  }

  // Seek back to the beginning of our output file and update the header.
  wav.chunk_size = 36 + bytes_written;
  wav.subchunk1_size = 16;
  wav.subchunk2_size = bytes_written;
  fseek(output.get(), 0, SEEK_SET);
  if (fwrite(&wav, sizeof(wav), 1, output.get()) < 1) {
    LOG(ERROR) << "could not write wav header.";
    return 1;
  }

  return 0;
}
