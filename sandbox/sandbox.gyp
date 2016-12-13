# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'conditions': [
    [ 'OS=="linux"', {
      'targets': [
        {
          'target_name': 'chrome_sandbox',
          'type': 'executable',
          'sources': [
            'linux/suid/sandbox.cc',
          ],
        }
      ],
    }],
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'sandbox',
          'type': '<(library)',
          'dependencies': [
            '../testing/gtest.gyp:gtest',
            '../base/base.gyp:base',
          ],
          'msvs_guid': '881F6A97-D539-4C48-B401-DF04385B2343',
          'sources': [
            'src/acl.cc',
            'src/acl.h',
            'src/dep.cc',
            'src/dep.h',
            'src/job.cc',
            'src/job.h',
            'src/restricted_token.cc',
            'src/restricted_token.h',
            'src/restricted_token_utils.cc',
            'src/restricted_token_utils.h',
            'src/security_level.h',
            'src/sid.cc',
            'src/sid.h',
            'src/eat_resolver.cc',
            'src/eat_resolver.h',
            'src/interception.cc',
            'src/interception.h',
            'src/interception_agent.cc',
            'src/interception_agent.h',
            'src/interception_internal.h',
            'src/pe_image.cc',
            'src/pe_image.h',
            'src/resolver.cc',
            'src/resolver.h',
            'src/service_resolver.cc',
            'src/service_resolver.h',
            'src/sidestep_resolver.cc',
            'src/sidestep_resolver.h',
            'src/target_interceptions.cc',
            'src/target_interceptions.h',
            'src/Wow64.cc',
            'src/Wow64.h',
            'src/sidestep\ia32_modrm_map.cpp',
            'src/sidestep\ia32_opcode_map.cpp',
            'src/sidestep\mini_disassembler.cpp',
            'src/sidestep\mini_disassembler.h',
            'src/sidestep\mini_disassembler_types.h',
            'src/sidestep\preamble_patcher.h',
            'src/sidestep\preamble_patcher_with_stub.cpp',
            'src/nt_internals.h',
            'src/policy_target.cc',
            'src/policy_target.h',
            'src/sandbox_nt_types.h',
            'src/sandbox_nt_util.cc',
            'src/sandbox_nt_util.h',
            'src/filesystem_dispatcher.cc',
            'src/filesystem_dispatcher.h',
            'src/filesystem_interception.cc',
            'src/filesystem_interception.h',
            'src/filesystem_policy.cc',
            'src/filesystem_policy.h',
            'src/named_pipe_dispatcher.cc',
            'src/named_pipe_dispatcher.h',
            'src/named_pipe_interception.cc',
            'src/named_pipe_interception.h',
            'src/named_pipe_policy.cc',
            'src/named_pipe_policy.h',
            'src/policy_params.h',
            'src/process_thread_dispatcher.cc',
            'src/process_thread_dispatcher.h',
            'src/process_thread_interception.cc',
            'src/process_thread_interception.h',
            'src/process_thread_policy.cc',
            'src/process_thread_policy.h',
            'src/registry_dispatcher.cc',
            'src/registry_dispatcher.h',
            'src/registry_interception.cc',
            'src/registry_interception.h',
            'src/registry_policy.cc',
            'src/registry_policy.h',
            'src/sync_dispatcher.cc',
            'src/sync_dispatcher.h',
            'src/sync_interception.cc',
            'src/sync_interception.h',
            'src/sync_policy.cc',
            'src/sync_policy.h',
            'src/crosscall_client.h',
            'src/crosscall_params.h',
            'src/crosscall_server.cc',
            'src/crosscall_server.h',
            'src/ipc_tags.h',
            'src/sharedmem_ipc_client.cc',
            'src/sharedmem_ipc_client.h',
            'src/sharedmem_ipc_server.cc',
            'src/sharedmem_ipc_server.h',
            'src/policy_engine_opcodes.cc',
            'src/policy_engine_opcodes.h',
            'src/policy_engine_params.h',
            'src/policy_engine_processor.cc',
            'src/policy_engine_processor.h',
            'src/policy_low_level.cc',
            'src/policy_low_level.h',
            'src/sandbox_policy_base.cc',
            'src/sandbox_policy_base.h',
            'src/broker_services.cc',
            'src/broker_services.h',
            'src/internal_types.h',
            'src/policy_broker.cc',
            'src/policy_broker.h',
            'src/sandbox.cc',
            'src/sandbox.h',
            'src/sandbox_factory.h',
            'src/sandbox_policy.h',
            'src/sandbox_types.h',
            'src/sandbox_utils.cc',
            'src/sandbox_utils.h',
            'src/shared_handles.cc',
            'src/shared_handles.h',
            'src/target_process.cc',
            'src/target_process.h',
            'src/target_services.cc',
            'src/target_services.h',
            'src/win2k_threadpool.cc',
            'src/win2k_threadpool.h',
            'src/win_utils.cc',
            'src/win_utils.h',
            'src/window.h',
            'src/window.cc',

            # Precompiled headers.
            'src/stdafx.cc',
            'src/stdafx.h',
          ],
          'include_dirs': [
            '..',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'wow_helper/wow_helper.exe',
                'wow_helper/wow_helper.pdb',
              ],
            },
          ],
          'configurations': {
            'Debug': {
              'msvs_precompiled_header': 'src/stdafx.h',
              'msvs_precompiled_source': 'src/stdafx.cc',
            },
          },
          'direct_dependent_settings': {
            'include_dirs': [
              'src',
              '..',
            ],
          },
        },
        {
          'target_name': 'sbox_integration_tests',
          'type': 'executable',
          'dependencies': [
            'sandbox',
            '../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'tests/common/controller.cc',
            'tests/common/controller.h',
            'tests/integration_tests/integration_tests.cc',
            'src/dep_test.cc',
            'src/file_policy_test.cc',
            'tests/integration_tests/integration_tests_test.cc',
            'src/integrity_level_test.cc',
            'src/ipc_ping_test.cc',
            'src/named_pipe_policy_test.cc',
            'src/policy_target_test.cc',
            'src/process_policy_test.cc',
            'src/registry_policy_test.cc',
            'src/sync_policy_test.cc',
            'src/unload_dll_test.cc',

            # Precompiled headers.
            'tests/integration_tests/stdafx.cc',
            'tests/integration_tests/stdafx.h',
          ],
          'configurations': {
            'Debug': {
              'msvs_precompiled_header': 'tests/integration_tests/stdafx.h',
              'msvs_precompiled_source': 'tests/integration_tests/stdafx.cc',
            },
          },
        },
        {
          'target_name': 'sbox_validation_tests',
          'type': 'executable',
          'dependencies': [
            'sandbox',
            '../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'tests/common/controller.cc',
            'tests/common/controller.h',
            'tests/validation_tests/unit_tests.cc',
            'tests/validation_tests/commands.cc',
            'tests/validation_tests/commands.h',
            'tests/validation_tests/suite.cc',

            # Precompiled headers.
            'tests/validation_tests/stdafx.cc',
            'tests/validation_tests/stdafx.h',
          ],
          'configurations': {
            'Debug': {
              'msvs_precompiled_header': 'tests/validation_tests/stdafx.h',
              'msvs_precompiled_source': 'tests/validation_tests/stdafx.cc',
            },
          },
        },
        {
          'target_name': 'sbox_unittests',
          'type': 'executable',
          'dependencies': [
            'sandbox',
            '../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'tests/unit_tests/unit_tests.cc',
            'src/interception_unittest.cc',
            'src/pe_image_unittest.cc',
            'src/service_resolver_unittest.cc',
            'src/restricted_token_unittest.cc',
            'src/job_unittest.cc',
            'src/sid_unittest.cc',
            'src/policy_engine_unittest.cc',
            'src/policy_low_level_unittest.cc',
            'src/policy_opcodes_unittest.cc',
            'src/ipc_unittest.cc',
            'src/threadpool_unittest.cc',

            # Precompiled headers.
            'tests/unit_tests/stdafx.cc',
            'tests/unit_tests/stdafx.h',
          ],
          'configurations': {
            'Debug': {
              'msvs_precompiled_header': 'tests/unit_tests/stdafx.h',
              'msvs_precompiled_source': 'tests/unit_tests/stdafx.cc',
            },
          },
        },
        {
          'target_name': 'sandbox_poc',
          'type': 'executable',
          'dependencies': [
            'sandbox',
            'pocdll',
          ],
          'sources': [
            'sandbox_poc/main_ui_window.cc',
            'sandbox_poc/main_ui_window.h',
            'sandbox_poc/resource.h',
            'sandbox_poc/sandbox.cc',
            'sandbox_poc/sandbox.h',
            'sandbox_poc/sandbox.ico',
            'sandbox_poc/sandbox.rc',

            # Precompiled headers.
            'sandbox_poc/stdafx.cc',
            'sandbox_poc/stdafx.h',
          ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
          'configurations': {
            'Debug': {
              'msvs_precompiled_header': 'sandbox_poc/stdafx.h',
              'msvs_precompiled_source': 'sandbox_poc/stdafx.cc',
            },
          },
        },
        {
          'target_name': 'pocdll',
          'type': 'shared_library',
          'sources': [
            'sandbox_poc/pocdll/exports.h',
            'sandbox_poc/pocdll/fs.cc',
            'sandbox_poc/pocdll/handles.cc',
            'sandbox_poc/pocdll/invasive.cc',
            'sandbox_poc/pocdll/network.cc',
            'sandbox_poc/pocdll/pocdll.cc',
            'sandbox_poc/pocdll/processes_and_threads.cc',
            'sandbox_poc/pocdll/registry.cc',
            'sandbox_poc/pocdll/spyware.cc',
            'sandbox_poc/pocdll/utils.h',

            # Precompiled headers.
            'sandbox_poc/pocdll/stdafx.cc',
            'sandbox_poc/pocdll/stdafx.h',
          ],
          'defines': [
            'POCDLL_EXPORTS',
          ],
          'include_dirs': [
            '..',
          ],
          'configurations': {
            'Debug': {
              'msvs_precompiled_header': 'sandbox_poc/pocdll/stdafx.h',
              'msvs_precompiled_source': 'sandbox_poc/pocdll/stdafx.cc',
            },
          },
        },
      ],
    }],
  ],
}
