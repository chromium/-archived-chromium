# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
    CCFLAGS = [
        '/Wp64',
    ],
)

input_files = [
    'base/address_list.cc',
    'base/auth_cache.cc',
    'base/base64.cc',
    'base/bzip2_filter.cc',
    'base/client_socket_factory.cc',
    'base/client_socket_handle.cc',
    'base/client_socket_pool.cc',
    'base/cookie_monster.cc',
    'base/cookie_policy.cc',
    'base/data_url.cc',
    'base/directory_lister.cc',
    'base/dns_resolution_observer.cc',
    'base/escape.cc',
    'base/ev_root_ca_metadata.cc',
    'base/filter.cc',
    'base/gzip_filter.cc',
    'base/gzip_header.cc',
    'base/host_resolver.cc',
    'base/listen_socket.cc',
    'base/mime_sniffer.cc',
    'base/mime_util.cc',
    'base/mime_util_win.cc',
    'base/net_errors.cc',
    'base/net_module.cc',
    'base/net_util.cc',
    'base/registry_controlled_domain.cc',
    'base/ssl_client_socket.cc',
    'base/ssl_config_service.cc',
    'base/tcp_client_socket.cc',
    'base/telnet_server.cc',
    'base/upload_data.cc',
    'base/upload_data_stream.cc',
    'base/wininet_util.cc',
    'base/winsock_init.cc',
    'base/x509_certificate.cc',
    'disk_cache/backend_impl.cc',
    'disk_cache/block_files.cc',
    'disk_cache/entry_impl.cc',
    'disk_cache/file.cc',
    'disk_cache/file_lock.cc',
    'disk_cache/hash.cc',
    'disk_cache/mapped_file.cc',
    'disk_cache/mem_backend_impl.cc',
    'disk_cache/mem_entry_impl.cc',
    'disk_cache/mem_rankings.cc',
    'disk_cache/rankings.cc',
    'disk_cache/stats.cc',
    'disk_cache/trace.cc',
    'http/cert_status_cache.cc',
    'http/http_chunked_decoder.cc',
    'http/http_cache.cc',
    'http/http_network_layer.cc',
    'http/http_network_transaction.cc',
    'http/http_proxy_resolver_fixed.cc',
    'http/http_proxy_resolver_winhttp.cc',
    'http/http_proxy_service.cc',
    'http/http_response_headers.cc',
    'http/http_transaction_winhttp.cc',
    'http/http_util.cc',
    'http/http_vary_data.cc',
    'http/winhttp_request_throttle.cc',
    'url_request/mime_sniffer_proxy.cc',
    'url_request/url_request.cc',
    'url_request/url_request_about_job.cc',
    'url_request/url_request_error_job.cc',
    'url_request/url_request_file_dir_job.cc',
    'url_request/url_request_file_job.cc',
    'url_request/url_request_filter.cc',
    'url_request/url_request_ftp_job.cc',
    'url_request/url_request_http_job.cc',
    'url_request/url_request_inet_job.cc',
    'url_request/url_request_job.cc',
    'url_request/url_request_job_manager.cc',
    'url_request/url_request_job_metrics.cc',
    'url_request/url_request_job_tracker.cc',
    'url_request/url_request_simple_job.cc',
    'url_request/url_request_test_job.cc',
    'url_request/url_request_view_cache_job.cc',
]

#env_p = env.Clone(
#    PCHSTOP='precompiled_net.h',
#    PDB = 'vc80.pdb',
#)
#pch, obj = env_p.PCH(['net.pch', 'precompiled_net.obj'], 'precompiled_net.cc')
#env_p['PCH'] = pch

#env.ChromeStaticLibrary('net', input_files + [obj])

# TODO(bradnelson): This step generates file precompiled_net.pch.ib_tag
#                   possibly only on incredibuild, scons doesn't know this.
env_p = env.Clone()
env_p.Append(CCFLAGS='/Ylnet')
pch, obj = env_p.PCH('precompiled_net.pch', 'build/precompiled_net.cc')
env['PCH'] = pch
env['PCHSTOP'] = 'precompiled_net.h'
env.Append(CCPCHFLAGS = ['/FIprecompiled_net.h'])

env.ChromeStaticLibrary('net', input_files + [obj])


env_tests.Prepend(
    CPPPATH = [
        '..',
    ],
    CPPDEFINES = [
        'UNIT_TEST',
        '_WIN32_WINNT=0x0600',
        'WINVER=0x0600',
        '_HAS_EXCEPTIONS=0',
    ],
    LIBS = [
        'googleurl',
        'base',
        'gtest',
        'bzip2',
        'icuuc',
        'modp_b64',
        'zlib',
        'net',
    ]
)

env_tests.Prepend(
    CCFLAGS = [
        '/TP',
        '/WX',
        '/Wp64',
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

env_tests.Append(
    CPPPATH = [
        '$GTEST_DIR/include',
    ],
)


unittest_files = [
    'base/auth_cache_unittest.cc',
    'base/base64_unittest.cc',
    'base/bzip2_filter_unittest.cc',
    'base/client_socket_pool_unittest.cc',
    'base/cookie_monster_unittest.cc',
    'base/cookie_policy_unittest.cc',
    'base/data_url_unittest.cc',
    'base/directory_lister_unittest.cc',
    'base/escape_unittest.cc',
    'base/gzip_filter_unittest.cc',
    'base/mime_sniffer_unittest.cc',
    'base/mime_util_unittest.cc',
    'base/net_util_unittest.cc',
    'base/registry_controlled_domain_unittest.cc',
    'base/ssl_config_service_unittest.cc',
    'base/ssl_client_socket_unittest.cc',
    'base/tcp_client_socket_unittest.cc',
    'base/wininet_util_unittest.cc',
    'disk_cache/addr_unittest.cc',
    'disk_cache/backend_unittest.cc',
    'disk_cache/block_files_unittest.cc',
    'disk_cache/disk_cache_test_base.cc',
    'disk_cache/disk_cache_test_util.cc',
    'disk_cache/entry_unittest.cc',
    'disk_cache/mapped_file_unittest.cc',
    'disk_cache/storage_block_unittest.cc',
    'http/http_cache_unittest.cc',
    'http/http_network_layer_unittest.cc',
    'http/http_network_transaction_unittest.cc',
    'http/http_response_headers_unittest.cc',
    'http/http_transaction_unittest.cc',
    'http/http_transaction_winhttp_unittest.cc',
    'http/http_util_unittest.cc',
    'http/http_vary_data_unittest.cc',
    'http/winhttp_request_throttle_unittest.cc',
    'url_request/url_request_unittest.cc',
    '$BASE_DIR/run_all_unittests.obj',
]

net_unittests = env_tests.ChromeTestProgram(
    ['net_unittests.exe',
    'net_unittests.ilk',
    'net_unittests.pdb'],
    unittest_files
)



stress_cache = env_tests.ChromeTestProgram(
    ['stress_cache.exe',
    'stress_cache.ilk',
    'stress_cache.pdb'],
    ['disk_cache/stress_cache.cc',
    'disk_cache/disk_cache_test_util.cc']
)


crash_cache = env_tests.ChromeTestProgram(
    ['crash_cache.exe',
    'crash_cache.ilk',
    'crash_cache.pdb'],
    ['tools/crash_cache/crash_cache.cc',
    'disk_cache/disk_cache_test_util.cc']
)


net_perftests = env_tests.ChromeTestProgram(
    ['net_perftests.exe',
    'net_perftests.ilk',
    'net_perftests.pdb'],
    ['disk_cache/disk_cache_test_util.cc',
    'disk_cache/disk_cache_perftest.cc',
    'base/cookie_monster_perftest.cc',
    # TODO(sgk): avoid using .cc from base directly
    '$BASE_DIR/run_all_perftests$OBJSUFFIX',
    '$BASE_DIR/perftimer$OBJSUFFIX']
)


# Create install of tests.
installed_tests = env.Install(
  '$TARGET_ROOT',
  net_unittests + stress_cache + crash_cache + net_perftests
)


env_res.Append(
    CPPPATH = [
        '..',
    ],
    RCFLAGS = [
        ['/l', '0x409'],
    ],
)

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


sconscript_files = [
    'tools/tld_cleanup/SConscript',
]

SConscript(sconscript_files, exports=['env'])


# Setup alias for building all parts of net.
env.Alias('net', ['.', installed_tests, '../icudt38.dll'])

