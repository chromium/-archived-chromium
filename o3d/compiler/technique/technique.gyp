# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'technique_out_dir': '<(SHARED_INTERMEDIATE_DIR)/technique',
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'parser_generator',
      'type': 'none',
      'rules': [
        {
          'rule_name': 'technique_parser',
          'extension': 'g3pl',
          'inputs' : [
            '../../../<(antlrdir)/lib/antlr-3.1.1.jar',
          ],
          'outputs': [
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Lexer.c',
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Lexer.h',
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Parser.c',
            '<(technique_out_dir)/<(RULE_INPUT_ROOT)Parser.h',
          ],
          'action': [
            'java',
            '-cp', '../../../<(antlrdir)/lib/antlr-3.1.1.jar',
            'org.antlr.Tool',
            '<(RULE_INPUT_PATH)',
            '-fo', '<(technique_out_dir)',
          ],
        },
      ],
      'sources': [
        'Technique.g3pl',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(technique_out_dir)',
        ],
      },
    },
    {
      'target_name': 'technique',
      'type': 'static_library',
      'dependencies': [
        'parser_generator',
        '../../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../../base/base.gyp:base',
        '../../core/core.gyp:o3dCore',
      ],
      'include_dirs': [
        '<(technique_out_dir)',
      ],
      'sources': [
        '<(technique_out_dir)/TechniqueLexer.c',
        '<(technique_out_dir)/TechniqueLexer.h',
        '<(technique_out_dir)/TechniqueParser.c',
        '<(technique_out_dir)/TechniqueParser.h',
        'technique_error.cc',
        'technique_error.h',
        'technique_parser.cc',
        'technique_parser.h',
        'technique_structures.cc',
        'technique_structures.h',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAs': '2',
        },
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(technique_out_dir)',
        ],
      },
    },
  ],
}
