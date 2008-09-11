# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

Import('env')

env_res = env.Clone()
env_tests = env.Clone()
env = env.Clone()

env.Prepend(
    CPPPATH = [
        '$ZLIB_DIR',
        '$ICU38_DIR/public/common',
        '$ICU38_DIR/public/i18n',
        '..',
    ],
)

env.Append(
    CPPDEFINES = [
        'U_STATIC_IMPLEMENTATION',
    ],
)


# These net files work on *all* platforms; files that don't work
# cross-platform live below.
input_files = [
    'base/address_list.cc',
    'base/auth_cache.cc',
    'base/base64.cc',
    'base/bzip2_filter.cc',
    'base/client_socket_handle.cc',
    'base/client_socket_pool.cc',
    'base/cookie_monster.cc',
    'base/cookie_policy.cc',
    'base/data_url.cc',
    'base/escape.cc',
    'base/ev_root_ca_metadata.cc',
    'base/filter.cc',
    'base/gzip_filter.cc',
    'base/gzip_header.cc',
    'base/host_resolver.cc',
    'base/mime_sniffer.cc',
    'base/mime_util.cc',
    'base/net_errors.cc',
    'base/net_module.cc',
    'base/net_util.cc',
    'base/registry_controlled_domain.cc',
    'base/upload_data.cc',
    'disk_cache/backend_impl.cc',
    'disk_cache/block_files.cc',
    'disk_cache/entry_impl.cc',
    'disk_cache/file_lock.cc',
    'disk_cache/hash.cc',
    'disk_cache/mem_backend_impl.cc',
    'disk_cache/mem_entry_impl.cc',
    'disk_cache/mem_rankings.cc',
    'disk_cache/rankings.cc',
    'disk_cache/stats.cc',
    'disk_cache/trace.cc',
    'http/cert_status_cache.cc',
    'http/http_cache.cc',
    'http/http_chunked_decoder.cc',
    'http/http_response_headers.cc',
    'http/http_util.cc',
    'http/http_vary_data.cc',
    'url_request/mime_sniffer_proxy.cc',
    'url_request/url_request.cc',
    'url_request/url_request_about_job.cc',
    'url_request/url_request_error_job.cc',
    'url_request/url_request_http_job.cc',
    'url_request/url_request_job.cc',
    'url_request/url_request_job_metrics.cc',
    'url_request/url_request_job_tracker.cc',
    'url_request/url_request_simple_job.cc',
    'url_request/url_request_test_job.cc',
    'url_request/url_request_view_cache_job.cc',
]

if env['PLATFORM'] == 'win32':
  input_files.extend([
      'base/client_socket_factory.cc',
      'base/directory_lister.cc',
      'base/dns_resolution_observer.cc',
      'base/listen_socket.cc',
      'base/ssl_client_socket.cc',
      'base/ssl_config_service.cc',
      'base/tcp_client_socket.cc',
      'base/telnet_server.cc',
      'base/upload_data_stream.cc',
      'base/wininet_util.cc',
      'base/winsock_init.cc',
      'base/x509_certificate.cc',
      'http/http_network_layer.cc',
      'http/http_network_transaction.cc',
      'http/http_transaction_winhttp.cc',
      'http/winhttp_request_throttle.cc',
      'proxy/proxy_resolver_fixed.cc',
      'proxy/proxy_resolver_winhttp.cc',
      'proxy/proxy_service.cc',
      'url_request/url_request_file_dir_job.cc',
      'url_request/url_request_file_job.cc',
      'url_request/url_request_filter.cc',
      'url_request/url_request_ftp_job.cc',
      'url_request/url_request_inet_job.cc',
      'url_request/url_request_job_manager.cc',
  ])

if env['PLATFORM'] == 'win32':
  env.Append(
      CCFLAGS = [
          '/Wp64',
      ],
  )
  input_files.extend([
      'base/net_util_win.cc',
      'base/platform_mime_util_win.cc',
      'disk_cache/cache_util_win.cc',
      'disk_cache/file_win.cc',
      'disk_cache/mapped_file_win.cc',
      'disk_cache/os_file_win.cc',
  ])

if env['PLATFORM'] == 'darwin':
  input_files.extend([
      'base/platform_mime_util_mac.mm',
  ])

if env['PLATFORM'] == 'posix':
  input_files.extend([
      # TODO(tc): gnome-vfs? xdgmime? /etc/mime.types?
      'base/platform_mime_util_linux.cc',
  ])

if env['PLATFORM'] in ('darwin', 'posix'):
  input_files.extend([
      'base/net_util_posix.cc',
      'disk_cache/cache_util_posix.cc',
      'disk_cache/file_posix.cc',
      'disk_cache/mapped_file_posix.cc',
      'disk_cache/os_file_posix.cc',
  ])

if env['PLATFORM'] == 'win32':
  # TODO(bradnelson): This step generates file precompiled_net.pch.ib_tag
  #                   possibly only on incredibuild, scons doesn't know this.
  env_p = env.Clone()
  env_p.Append(CCFLAGS='/Ylnet')
  pch, obj = env_p.PCH('precompiled_net.pch', 'build/precompiled_net.cc')
  env['PCH'] = pch
  env['PCHSTOP'] = 'precompiled_net.h'
  env.Append(CCPCHFLAGS = ['/FIprecompiled_net.h'])
  input_files += [obj]

env.ChromeStaticLibrary('net', input_files)


env_tests.Prepend(
    CPPPATH = [
        '..',
    ],
    CPPDEFINES = [
        'UNIT_TEST',
    ],
    LIBS = [          # On Linux, dependencies must follow dependents, so...
        'net',        # net must come before base and modp_b64
        'bzip2',      # bzip2 must come before base
        'base',
        'googleurl',
        'gtest',
        'icuuc',
        'modp_b64',
        'zlib',
    ]
)

env_tests.Append(
    CPPPATH = [
        '$GTEST_DIR/include',
    ],
)

if env['PLATFORM'] == 'win32':
  env_tests.Prepend(
      CCFLAGS = [
          '/TP',
          '/WX',
          '/Wp64',
      ],
      CPPDEFINES = [
          '_WIN32_WINNT=0x0600',
          'WINVER=0x0600',
          '_HAS_EXCEPTIONS=0',
      ],
      LINKFLAGS = [
          '/DELAYLOAD:"dwmapi.dll"',
          '/DELAYLOAD:"uxtheme.dll"',
          '/MACHINE:X86',
          '/FIXED:No',
          '/safeseh',
          '/dynamicbase',
          '/ignore:4199',
          '/nxcompat',
      ],
  )


unittest_files = [
    'base/auth_cache_unittest.cc',
    'base/base64_unittest.cc',
    'base/bzip2_filter_unittest.cc',
    'base/client_socket_pool_unittest.cc',
    'base/cookie_monster_unittest.cc',
    'base/data_url_unittest.cc',
    'base/escape_unittest.cc',
    'base/gzip_filter_unittest.cc',
    'base/host_resolver_unittest.cc',
    'base/mime_sniffer_unittest.cc',
    'base/mime_util_unittest.cc',
    'base/net_util_unittest.cc',
    'base/registry_controlled_domain_unittest.cc',
    'base/run_all_unittests.cc',
    'disk_cache/addr_unittest.cc',
    'disk_cache/block_files_unittest.cc',
    'disk_cache/disk_cache_test_base.cc',
    'disk_cache/disk_cache_test_util.cc',
    'disk_cache/entry_unittest.cc',
    'disk_cache/mapped_file_unittest.cc',
    'disk_cache/storage_block_unittest.cc',
    'http/http_chunked_decoder_unittest.cc',
    'http/http_response_headers_unittest.cc',
    'http/http_vary_data_unittest.cc',
]

if env['PLATFORM'] == 'win32':
  unittest_files.extend([
      'base/cookie_policy_unittest.cc',
      'base/directory_lister_unittest.cc',
      'base/ssl_config_service_unittest.cc',
      'base/ssl_client_socket_unittest.cc',
      'base/tcp_client_socket_unittest.cc',
      'base/wininet_util_unittest.cc',
      'disk_cache/backend_unittest.cc',
      'http/http_cache_unittest.cc',
      'http/http_network_layer_unittest.cc',
      'http/http_network_transaction_unittest.cc',
      'http/http_transaction_unittest.cc',
      'http/http_transaction_winhttp_unittest.cc',
      'http/http_util_unittest.cc',
      'http/winhttp_request_throttle_unittest.cc',
      'url_request/url_request_unittest.cc',
  ])

if env['PLATFORM'] == 'darwin':
  unittest_files.extend([
      '../base/platform_test_mac.o',
  ])

net_unittests = env_tests.ChromeTestProgram('net_unittests', unittest_files)

install_targets = net_unittests[:]

if env['PLATFORM'] == 'win32':
  stress_cache = env_tests.ChromeTestProgram(
      'stress_cache',
      ['disk_cache/stress_cache.cc',
       'disk_cache/disk_cache_test_util.cc']
  )

  crash_cache = env_tests.ChromeTestProgram(
      'crash_cache',
      ['tools/crash_cache/crash_cache.cc',
       'disk_cache/disk_cache_test_util.cc']
  )

  net_perftests = env_tests.ChromeTestProgram(
      'net_perftests',
      ['disk_cache/disk_cache_test_util.cc',
       'disk_cache/disk_cache_perftest.cc',
       'base/cookie_monster_perftest.cc',
       # TODO(sgk): avoid using .cc from base directly
       '$BASE_DIR/run_all_perftests$OBJSUFFIX',
       '$BASE_DIR/perftimer$OBJSUFFIX']
  )

  install_targets.extend([
      stress_cache,
      crash_cache,
      net_perftests,
  ])


# Create install of tests.
installed_tests = env.Install('$TARGET_ROOT', install_targets)


if env['PLATFORM'] == 'win32':
  env_res.Append(
      CPPPATH = [
          '..',
      ],
      RCFLAGS = [
          ['/l', '0x409'],
      ],
  )

  # TODO: Need to figure out what we're doing with external resources on
  # linux.
  # This dat file needed by net_resources is generated.
  tld_names_clean = env_res.Command('net/effective_tld_names_clean.dat',
                                   ['base/effective_tld_names.dat',
                                    'tools/tld_cleanup/tld_cleanup.exe'],
                                   '${SOURCES[1]} ${SOURCES[0]} $TARGET')
  rc = env_res.Command('net_resources.rc',
                       'base/net_resources.rc',
                       Copy('$TARGET', '$SOURCE'))
  net_resources = env_res.RES(rc)
  env_res.Depends(rc, tld_names_clean)

  # TODO: We need to port tld_cleanup before this will work on other
  # platforms.
  sconscript_files = [
      'tools/tld_cleanup/SConscript',
  ]

  SConscript(sconscript_files, exports=['env'])


# Setup alias for building all parts of net.
if env['PLATFORM'] == 'win32':
  icudata = '../icudt38.dll'
else:
  icudata = '../icudt38l.dat'
env.Alias('net', ['.', installed_tests, icudata])
