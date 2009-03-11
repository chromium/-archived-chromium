// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file holds definitions related to the ntdll API.

#ifndef SANDBOX_SRC_NT_INTERNALS_H__
#define SANDBOX_SRC_NT_INTERNALS_H__

#include <windows.h>

typedef LONG NTSTATUS;
#define NT_SUCCESS(st) (st >= 0)

#define STATUS_SUCCESS                ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_OVERFLOW        ((NTSTATUS)0x80000005L)
#define STATUS_UNSUCCESSFUL           ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED        ((NTSTATUS)0xC0000002L)
#ifndef STATUS_INVALID_PARAMETER
// It is now defined in Windows 2008 SDK.
#define STATUS_INVALID_PARAMETER      ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_CONFLICTING_ADDRESSES  ((NTSTATUS)0xC0000018L)
#define STATUS_ACCESS_DENIED          ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL       ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_NAME_NOT_FOUND  ((NTSTATUS)0xC0000034L)
#define STATUS_PROCEDURE_NOT_FOUND    ((NTSTATUS)0xC000007AL)
#define STATUS_INVALID_IMAGE_FORMAT   ((NTSTATUS)0xC000007BL)
#define STATUS_NO_TOKEN               ((NTSTATUS)0xC000007CL)

#define CURRENT_PROCESS ((HANDLE) -1)
#define CURRENT_THREAD  ((HANDLE) -2)
#define NtCurrentProcess CURRENT_PROCESS

typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _STRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR Buffer;
} STRING;
typedef STRING *PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;
typedef CONST PSTRING PCANSI_STRING;

typedef STRING OEM_STRING;
typedef PSTRING POEM_STRING;
typedef CONST STRING* PCOEM_STRING;

#define OBJ_CASE_INSENSITIVE 0x00000040L

typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes(p, n, a, r, s) { \
  (p)->Length = sizeof(OBJECT_ATTRIBUTES);\
  (p)->RootDirectory = r;\
  (p)->Attributes = a;\
  (p)->ObjectName = n;\
  (p)->SecurityDescriptor = s;\
  (p)->SecurityQualityOfService = NULL;\
}

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  };
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

// -----------------------------------------------------------------------
// File IO

// Create disposition values.

#define FILE_SUPERSEDE                          0x00000000
#define FILE_OPEN                               0x00000001
#define FILE_CREATE                             0x00000002
#define FILE_OPEN_IF                            0x00000003
#define FILE_OVERWRITE                          0x00000004
#define FILE_OVERWRITE_IF                       0x00000005
#define FILE_MAXIMUM_DISPOSITION                0x00000005

// Create/open option flags.

#define FILE_DIRECTORY_FILE                     0x00000001
#define FILE_WRITE_THROUGH                      0x00000002
#define FILE_SEQUENTIAL_ONLY                    0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING          0x00000008

#define FILE_SYNCHRONOUS_IO_ALERT               0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT            0x00000020
#define FILE_NON_DIRECTORY_FILE                 0x00000040
#define FILE_CREATE_TREE_CONNECTION             0x00000080

#define FILE_COMPLETE_IF_OPLOCKED               0x00000100
#define FILE_NO_EA_KNOWLEDGE                    0x00000200
#define FILE_OPEN_REMOTE_INSTANCE               0x00000400
#define FILE_RANDOM_ACCESS                      0x00000800

#define FILE_DELETE_ON_CLOSE                    0x00001000
#define FILE_OPEN_BY_FILE_ID                    0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT             0x00004000
#define FILE_NO_COMPRESSION                     0x00008000

#define FILE_RESERVE_OPFILTER                   0x00100000
#define FILE_OPEN_REPARSE_POINT                 0x00200000
#define FILE_OPEN_NO_RECALL                     0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY          0x00800000

typedef NTSTATUS (WINAPI *NtCreateFileFunction)(
  OUT PHANDLE FileHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  OUT PIO_STATUS_BLOCK IoStatusBlock,
  IN PLARGE_INTEGER AllocationSize OPTIONAL,
  IN ULONG FileAttributes,
  IN ULONG ShareAccess,
  IN ULONG CreateDisposition,
  IN ULONG CreateOptions,
  IN PVOID EaBuffer OPTIONAL,
  IN ULONG EaLength);

typedef NTSTATUS (WINAPI *NtOpenFileFunction)(
  OUT PHANDLE FileHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  OUT PIO_STATUS_BLOCK IoStatusBlock,
  IN ULONG ShareAccess,
  IN ULONG OpenOptions);

typedef NTSTATUS (WINAPI *NtCloseFunction)(
  IN HANDLE Handle);

typedef enum _FILE_INFORMATION_CLASS {
  FileRenameInformation = 10
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef struct _FILE_RENAME_INFORMATION {
  BOOLEAN ReplaceIfExists;
  HANDLE RootDirectory;
  ULONG FileNameLength;
  WCHAR FileName[1];
} FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;

typedef NTSTATUS (WINAPI *NtSetInformationFileFunction)(
  IN HANDLE FileHandle,
  OUT PIO_STATUS_BLOCK IoStatusBlock,
  IN PVOID FileInformation,
  IN ULONG Length,
  IN FILE_INFORMATION_CLASS FileInformationClass);

typedef struct FILE_BASIC_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef NTSTATUS (WINAPI *NtQueryAttributesFileFunction)(
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  OUT PFILE_BASIC_INFORMATION FileAttributes);

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

typedef NTSTATUS (WINAPI *NtQueryFullAttributesFileFunction)(
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  OUT PFILE_NETWORK_OPEN_INFORMATION FileAttributes);

// -----------------------------------------------------------------------
// Sections

typedef NTSTATUS (WINAPI *NtCreateSectionFunction)(
  OUT PHANDLE SectionHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
  IN PLARGE_INTEGER MaximumSize OPTIONAL,
  IN ULONG SectionPageProtection,
  IN ULONG AllocationAttributes,
  IN HANDLE FileHandle OPTIONAL);

typedef ULONG SECTION_INHERIT;
#define ViewShare 1
#define ViewUnmap 2

typedef NTSTATUS (WINAPI *NtMapViewOfSectionFunction)(
  IN HANDLE SectionHandle,
  IN HANDLE ProcessHandle,
  IN OUT PVOID *BaseAddress,
  IN ULONG_PTR ZeroBits,
  IN SIZE_T CommitSize,
  IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
  IN OUT PSIZE_T ViewSize,
  IN SECTION_INHERIT InheritDisposition,
  IN ULONG AllocationType,
  IN ULONG Win32Protect);

typedef NTSTATUS (WINAPI *NtUnmapViewOfSectionFunction)(
  IN HANDLE ProcessHandle,
  IN PVOID BaseAddress);

typedef enum _SECTION_INFORMATION_CLASS {
  SectionBasicInformation = 0,
  SectionImageInformation
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG Attributes;
  LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef NTSTATUS (WINAPI *NtQuerySectionFunction)(
  IN HANDLE SectionHandle,
  IN SECTION_INFORMATION_CLASS SectionInformationClass,
  OUT PVOID SectionInformation,
  IN ULONG SectionInformationLength,
  OUT PULONG ReturnLength OPTIONAL);

// -----------------------------------------------------------------------
// Process and Thread

typedef struct _CLIENT_ID {
  PVOID UniqueProcess;
  PVOID UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef NTSTATUS (WINAPI *NtOpenThreadFunction) (
  OUT PHANDLE ThreadHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  IN PCLIENT_ID ClientId);

typedef NTSTATUS (WINAPI *NtOpenProcessFunction) (
  OUT PHANDLE ProcessHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  IN PCLIENT_ID ClientId);

typedef enum _THREAD_INFORMATION_CLASS {
  ThreadBasicInformation,
  ThreadTimes,
  ThreadPriority,
  ThreadBasePriority,
  ThreadAffinityMask,
  ThreadImpersonationToken,
  ThreadDescriptorTableEntry,
  ThreadEnableAlignmentFaultFixup,
  ThreadEventPair,
  ThreadQuerySetWin32StartAddress,
  ThreadZeroTlsCell,
  ThreadPerformanceCount,
  ThreadAmILastThread,
  ThreadIdealProcessor,
  ThreadPriorityBoost,
  ThreadSetTlsArrayAddress,
  ThreadIsIoPending,
  ThreadHideFromDebugger
} THREAD_INFORMATION_CLASS, *PTHREAD_INFORMATION_CLASS;

typedef NTSTATUS (WINAPI *NtSetInformationThreadFunction) (
  IN HANDLE ThreadHandle,
  IN THREAD_INFORMATION_CLASS ThreadInformationClass,
  IN PVOID ThreadInformation,
  IN ULONG ThreadInformationLength);

// Partial definition only:
typedef enum _PROCESSINFOCLASS {
  ProcessBasicInformation = 0
} PROCESSINFOCLASS;

typedef PVOID PPEB;
typedef PVOID KPRIORITY;

typedef struct _PROCESS_BASIC_INFORMATION {
  NTSTATUS ExitStatus;
  PPEB PebBaseAddress;
  KAFFINITY AffinityMask;
  KPRIORITY BasePriority;
  ULONG UniqueProcessId;
  ULONG InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

typedef NTSTATUS (WINAPI *NtQueryInformationProcessFunction)(
  IN HANDLE ProcessHandle,
  IN PROCESSINFOCLASS ProcessInformationClass,
  OUT PVOID ProcessInformation,
  IN ULONG ProcessInformationLength,
  OUT PULONG ReturnLength OPTIONAL);

typedef NTSTATUS (WINAPI *NtOpenThreadTokenFunction) (
  IN HANDLE ThreadHandle,
  IN ACCESS_MASK DesiredAccess,
  IN BOOLEAN OpenAsSelf,
  OUT PHANDLE TokenHandle);

typedef NTSTATUS (WINAPI *NtOpenThreadTokenExFunction) (
  IN HANDLE ThreadHandle,
  IN ACCESS_MASK DesiredAccess,
  IN BOOLEAN OpenAsSelf,
  IN ULONG HandleAttributes,
  OUT PHANDLE TokenHandle);

typedef NTSTATUS (WINAPI *NtOpenProcessTokenFunction) (
  IN HANDLE ProcessHandle,
  IN ACCESS_MASK DesiredAccess,
  OUT PHANDLE TokenHandle);

typedef NTSTATUS (WINAPI *NtOpenProcessTokenExFunction) (
  IN HANDLE ProcessHandle,
  IN ACCESS_MASK DesiredAccess,
  IN ULONG HandleAttributes,
  OUT PHANDLE TokenHandle);

// -----------------------------------------------------------------------
// Registry

typedef NTSTATUS (WINAPI *NtCreateKeyFunction)(
  OUT PHANDLE KeyHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  IN ULONG TitleIndex,
  IN PUNICODE_STRING Class OPTIONAL,
  IN ULONG CreateOptions,
  OUT PULONG Disposition OPTIONAL);

typedef NTSTATUS (WINAPI *NtOpenKeyFunction)(
  OUT PHANDLE KeyHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes);

typedef NTSTATUS (WINAPI *NtOpenKeyExFunction)(
  OUT PHANDLE KeyHandle,
  IN ACCESS_MASK DesiredAccess,
  IN POBJECT_ATTRIBUTES ObjectAttributes,
  IN DWORD unknown);  // TODO(nsylvain): define this. bug 7611

// -----------------------------------------------------------------------
// Memory

// Don't really need this structure right now.
typedef PVOID PRTL_HEAP_PARAMETERS;

typedef PVOID (WINAPI *RtlCreateHeapFunction)(
  IN ULONG Flags,
  IN PVOID HeapBase OPTIONAL,
  IN SIZE_T ReserveSize OPTIONAL,
  IN SIZE_T CommitSize OPTIONAL,
  IN PVOID Lock OPTIONAL,
  IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL);

typedef PVOID (WINAPI *RtlDestroyHeapFunction)(
  IN PVOID HeapHandle);

typedef PVOID (WINAPI *RtlAllocateHeapFunction)(
  IN PVOID HeapHandle,
  IN ULONG Flags,
  IN SIZE_T Size);

typedef BOOLEAN (WINAPI *RtlFreeHeapFunction)(
  IN PVOID HeapHandle,
  IN ULONG Flags,
  IN PVOID HeapBase);

typedef NTSTATUS (WINAPI *NtAllocateVirtualMemoryFunction) (
  IN HANDLE ProcessHandle,
  IN OUT PVOID *BaseAddress,
  IN ULONG_PTR ZeroBits,
  IN OUT PSIZE_T RegionSize,
  IN ULONG AllocationType,
  IN ULONG Protect);

typedef NTSTATUS (WINAPI *NtFreeVirtualMemoryFunction) (
  IN HANDLE ProcessHandle,
  IN OUT PVOID *BaseAddress,
  IN OUT PSIZE_T RegionSize,
  IN ULONG FreeType);

typedef enum _MEMORY_INFORMATION_CLASS {
  MemoryBasicInformation = 0,
  MemoryWorkingSetList,
  MemorySectionName,
  MemoryBasicVlmInformation
} MEMORY_INFORMATION_CLASS;

typedef struct _MEMORY_SECTION_NAME {  // Information Class 2
  UNICODE_STRING SectionFileName;
} MEMORY_SECTION_NAME, *PMEMORY_SECTION_NAME;

typedef NTSTATUS (WINAPI *NtQueryVirtualMemoryFunction)(
  IN HANDLE ProcessHandle,
  IN PVOID BaseAddress,
  IN MEMORY_INFORMATION_CLASS MemoryInformationClass,
  OUT PVOID MemoryInformation,
  IN ULONG MemoryInformationLength,
  OUT PULONG ReturnLength OPTIONAL);

typedef NTSTATUS (WINAPI *NtProtectVirtualMemoryFunction)(
  IN HANDLE ProcessHandle,
  IN OUT PVOID* BaseAddress,
  IN OUT PSIZE_T ProtectSize,
  IN ULONG NewProtect,
  OUT PULONG OldProtect);

// -----------------------------------------------------------------------
// Objects

typedef enum _OBJECT_INFORMATION_CLASS {
  ObjectBasicInformation,
  ObjectNameInformation,
  ObjectTypeInformation,
  ObjectAllInformation,
  ObjectDataInformation
} OBJECT_INFORMATION_CLASS, *POBJECT_INFORMATION_CLASS;

typedef struct _OBJECT_BASIC_INFORMATION {
  ULONG Attributes;
  ACCESS_MASK GrantedAccess;
  ULONG HandleCount;
  ULONG PointerCount;
  ULONG PagedPoolUsage;
  ULONG NonPagedPoolUsage;
  ULONG Reserved[3];
  ULONG NameInformationLength;
  ULONG TypeInformationLength;
  ULONG SecurityDescriptorLength;
  LARGE_INTEGER CreateTime;
} OBJECT_BASIC_INFORMATION, *POBJECT_BASIC_INFORMATION;

typedef struct _OBJECT_NAME_INFORMATION {
  UNICODE_STRING ObjectName;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef NTSTATUS (WINAPI *NtQueryObjectFunction)(
  IN HANDLE Handle,
  IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
  OUT PVOID ObjectInformation OPTIONAL,
  IN ULONG ObjectInformationLength,
  OUT PULONG ReturnLength OPTIONAL);

typedef NTSTATUS (WINAPI *NtDuplicateObjectFunction)(
  IN HANDLE SourceProcess,
  IN HANDLE SourceHandle,
  IN HANDLE TargetProcess,
  OUT PHANDLE TargetHandle,
  IN ACCESS_MASK DesiredAccess,
  IN ULONG Attributes,
  IN ULONG Options);

typedef NTSTATUS (WINAPI *NtSignalAndWaitForSingleObjectFunction)(
  IN HANDLE HandleToSignal,
  IN HANDLE HandleToWait,
  IN BOOLEAN Alertable,
  IN PLARGE_INTEGER Timeout OPTIONAL);

// -----------------------------------------------------------------------
// Strings

typedef int (__cdecl *_strnicmpFunction)(
  IN const char* _Str1,
  IN const char* _Str2,
  IN size_t _MaxCount);

typedef size_t  (__cdecl *strlenFunction)(
  IN const char * _Str);

typedef size_t (__cdecl *wcslenFunction)(
  IN const wchar_t* _Str);

typedef NTSTATUS (WINAPI *RtlAnsiStringToUnicodeStringFunction)(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PANSI_STRING  SourceString,
  IN BOOLEAN  AllocateDestinationString);

typedef LONG (WINAPI *RtlCompareUnicodeStringFunction)(
  IN PCUNICODE_STRING  String1,
  IN PCUNICODE_STRING  String2,
  IN BOOLEAN  CaseInSensitive);

typedef VOID (WINAPI *RtlInitUnicodeStringFunction) (
  IN OUT PUNICODE_STRING DestinationString,
  IN PCWSTR SourceString);

#endif  // SANDBOX_SRC_NT_INTERNALS_H__
