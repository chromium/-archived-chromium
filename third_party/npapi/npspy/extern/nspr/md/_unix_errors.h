/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef prunixerrors_h___
#define prunixerrors_h___

#include <unistd.h>
#include <stddef.h>

PR_BEGIN_EXTERN_C

NSPR_API(void) _MD_unix_map_default_error(int err);
#define	_PR_MD_MAP_DEFAULT_ERROR	_MD_unix_map_default_error

NSPR_API(void) _MD_unix_map_opendir_error(int err);
#define	_PR_MD_MAP_OPENDIR_ERROR	_MD_unix_map_opendir_error

NSPR_API(void) _MD_unix_map_closedir_error(int err);
#define	_PR_MD_MAP_CLOSEDIR_ERROR	_MD_unix_map_closedir_error

NSPR_API(void) _MD_unix_readdir_error(int err);
#define	_PR_MD_MAP_READDIR_ERROR	_MD_unix_readdir_error

NSPR_API(void) _MD_unix_map_unlink_error(int err);
#define	_PR_MD_MAP_UNLINK_ERROR	_MD_unix_map_unlink_error

NSPR_API(void) _MD_unix_map_stat_error(int err);
#define	_PR_MD_MAP_STAT_ERROR	_MD_unix_map_stat_error

NSPR_API(void) _MD_unix_map_fstat_error(int err);
#define	_PR_MD_MAP_FSTAT_ERROR	_MD_unix_map_fstat_error

NSPR_API(void) _MD_unix_map_rename_error(int err);
#define	_PR_MD_MAP_RENAME_ERROR	_MD_unix_map_rename_error

NSPR_API(void) _MD_unix_map_access_error(int err);
#define	_PR_MD_MAP_ACCESS_ERROR	_MD_unix_map_access_error

NSPR_API(void) _MD_unix_map_mkdir_error(int err);
#define	_PR_MD_MAP_MKDIR_ERROR	_MD_unix_map_mkdir_error

NSPR_API(void) _MD_unix_map_rmdir_error(int err);
#define	_PR_MD_MAP_RMDIR_ERROR	_MD_unix_map_rmdir_error

NSPR_API(void) _MD_unix_map_read_error(int err);
#define	_PR_MD_MAP_READ_ERROR	_MD_unix_map_read_error

NSPR_API(void) _MD_unix_map_write_error(int err);
#define	_PR_MD_MAP_WRITE_ERROR	_MD_unix_map_write_error

NSPR_API(void) _MD_unix_map_lseek_error(int err);
#define	_PR_MD_MAP_LSEEK_ERROR	_MD_unix_map_lseek_error

NSPR_API(void) _MD_unix_map_fsync_error(int err);
#define	_PR_MD_MAP_FSYNC_ERROR	_MD_unix_map_fsync_error

NSPR_API(void) _MD_unix_map_close_error(int err);
#define	_PR_MD_MAP_CLOSE_ERROR	_MD_unix_map_close_error

NSPR_API(void) _MD_unix_map_socket_error(int err);
#define	_PR_MD_MAP_SOCKET_ERROR	_MD_unix_map_socket_error

NSPR_API(void) _MD_unix_map_socketavailable_error(int err);
#define	_PR_MD_MAP_SOCKETAVAILABLE_ERROR	_MD_unix_map_socketavailable_error

NSPR_API(void) _MD_unix_map_recv_error(int err);
#define	_PR_MD_MAP_RECV_ERROR	_MD_unix_map_recv_error

NSPR_API(void) _MD_unix_map_recvfrom_error(int err);
#define	_PR_MD_MAP_RECVFROM_ERROR	_MD_unix_map_recvfrom_error

NSPR_API(void) _MD_unix_map_send_error(int err);
#define	_PR_MD_MAP_SEND_ERROR	_MD_unix_map_send_error

NSPR_API(void) _MD_unix_map_sendto_error(int err);
#define	_PR_MD_MAP_SENDTO_ERROR	_MD_unix_map_sendto_error

NSPR_API(void) _MD_unix_map_writev_error(int err);
#define	_PR_MD_MAP_WRITEV_ERROR	_MD_unix_map_writev_error

NSPR_API(void) _MD_unix_map_accept_error(int err);
#define	_PR_MD_MAP_ACCEPT_ERROR	_MD_unix_map_accept_error

NSPR_API(void) _MD_unix_map_connect_error(int err);
#define	_PR_MD_MAP_CONNECT_ERROR	_MD_unix_map_connect_error

NSPR_API(void) _MD_unix_map_bind_error(int err);
#define	_PR_MD_MAP_BIND_ERROR	_MD_unix_map_bind_error

NSPR_API(void) _MD_unix_map_listen_error(int err);
#define	_PR_MD_MAP_LISTEN_ERROR	_MD_unix_map_listen_error

NSPR_API(void) _MD_unix_map_shutdown_error(int err);
#define	_PR_MD_MAP_SHUTDOWN_ERROR	_MD_unix_map_shutdown_error

NSPR_API(void) _MD_unix_map_socketpair_error(int err);
#define	_PR_MD_MAP_SOCKETPAIR_ERROR	_MD_unix_map_socketpair_error

NSPR_API(void) _MD_unix_map_getsockname_error(int err);
#define	_PR_MD_MAP_GETSOCKNAME_ERROR	_MD_unix_map_getsockname_error

NSPR_API(void) _MD_unix_map_getpeername_error(int err);
#define	_PR_MD_MAP_GETPEERNAME_ERROR	_MD_unix_map_getpeername_error

NSPR_API(void) _MD_unix_map_getsockopt_error(int err);
#define	_PR_MD_MAP_GETSOCKOPT_ERROR	_MD_unix_map_getsockopt_error

NSPR_API(void) _MD_unix_map_setsockopt_error(int err);
#define	_PR_MD_MAP_SETSOCKOPT_ERROR	_MD_unix_map_setsockopt_error

NSPR_API(void) _MD_unix_map_open_error(int err);
#define	_PR_MD_MAP_OPEN_ERROR	_MD_unix_map_open_error

NSPR_API(void) _MD_unix_map_mmap_error(int err);
#define	_PR_MD_MAP_MMAP_ERROR	_MD_unix_map_mmap_error

NSPR_API(void) _MD_unix_map_gethostname_error(int err);
#define	_PR_MD_MAP_GETHOSTNAME_ERROR	_MD_unix_map_gethostname_error

NSPR_API(void) _MD_unix_map_select_error(int err);
#define	_PR_MD_MAP_SELECT_ERROR	_MD_unix_map_select_error

NSPR_API(void) _MD_unix_map_poll_error(int err);
#define _PR_MD_MAP_POLL_ERROR _MD_unix_map_poll_error

NSPR_API(void) _MD_unix_map_poll_revents_error(int err);
#define _PR_MD_MAP_POLL_REVENTS_ERROR _MD_unix_map_poll_revents_error

NSPR_API(void) _MD_unix_map_flock_error(int err);
#define	_PR_MD_MAP_FLOCK_ERROR	_MD_unix_map_flock_error

NSPR_API(void) _MD_unix_map_lockf_error(int err);
#define	_PR_MD_MAP_LOCKF_ERROR	_MD_unix_map_lockf_error

PR_END_EXTERN_C

#endif /* prunixerrors_h___ */
