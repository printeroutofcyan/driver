#pragma once

/*
 * Shared definitions for the SwissKnife Windows driver and user-mode client.
 * This header is written so it can be included from both kernel and user space.
 */

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#define SWISSKNIFE_DEVICE_NAME        L"\\Device\\SwissKnife"
#define SWISSKNIFE_SYMBOLIC_NAME      L"\\DosDevices\\SwissKnife"
#define SWISSKNIFE_USER_SYMBOLIC_NAME L"\\\\.\\SwissKnife"

#define SWISSKNIFE_POOL_TAG 'kwsS'

/* IOCTL definition block */
#define IOCTL_SWISSKNIFE_GET_DRIVER_INFO \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SWISSKNIFE_GET_SYSTEM_OVERVIEW \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SWISSKNIFE_ENUMERATE_PROCESSES \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SWISSKNIFE_ENUMERATE_MODULES \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SWISSKNIFE_READ_FILE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SWISSKNIFE_QUERY_REGISTRY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SWISSKNIFE_RANDOM_BYTES \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SWISSKNIFE_MAX_IMAGE_NAME 260
#define SWISSKNIFE_MAX_MODULE_NAME 256
#define SWISSKNIFE_MAX_REG_PATH 512
#define SWISSKNIFE_MAX_REG_VALUE 256

#pragma pack(push, 1)
typedef struct _SWISSKNIFE_DRIVER_INFO {
    ULONG VersionMajor;
    ULONG VersionMinor;
    ULONG BuildNumber;
    WCHAR FriendlyName[64];
    WCHAR Description[128];
} SWISSKNIFE_DRIVER_INFO, *PSWISSKNIFE_DRIVER_INFO;

typedef struct _SWISSKNIFE_SYSTEM_OVERVIEW {
    ULONGLONG ActiveProcessorMask;
    ULONG NumberOfProcessors;
    ULONGLONG TotalPhysicalPages;
    ULONGLONG AvailablePages;
    ULONG PageSize;
    ULONGLONG SystemUptimeMilliseconds;
    ULONGLONG InterruptTime100Ns;
} SWISSKNIFE_SYSTEM_OVERVIEW, *PSWISSKNIFE_SYSTEM_OVERVIEW;

typedef struct _SWISSKNIFE_PROCESS_ENTRY {
    ULONG ProcessId;
    ULONG ParentProcessId;
    ULONG ThreadCount;
    ULONGLONG CreateTime;
    ULONGLONG UserTime;
    ULONGLONG KernelTime;
    WCHAR ImageName[SWISSKNIFE_MAX_IMAGE_NAME];
} SWISSKNIFE_PROCESS_ENTRY, *PSWISSKNIFE_PROCESS_ENTRY;

typedef struct _SWISSKNIFE_MODULE_ENTRY {
    ULONGLONG BaseAddress;
    ULONG ImageSize;
    ULONG Flags;
    ULONG LoadOrderIndex;
    WCHAR ImageName[SWISSKNIFE_MAX_MODULE_NAME];
} SWISSKNIFE_MODULE_ENTRY, *PSWISSKNIFE_MODULE_ENTRY;

typedef struct _SWISSKNIFE_FILE_REQUEST {
    ULONG PathLength;       /* Length in bytes including terminating null. */
    ULONGLONG Offset;       /* Byte offset into the file. */
    ULONG ReadLength;       /* Number of bytes to read. */
    WCHAR Path[1];          /* In-place variable length buffer. */
} SWISSKNIFE_FILE_REQUEST, *PSWISSKNIFE_FILE_REQUEST;

typedef struct _SWISSKNIFE_FILE_RESPONSE {
    ULONG BytesRead;
    UCHAR Data[1];
} SWISSKNIFE_FILE_RESPONSE, *PSWISSKNIFE_FILE_RESPONSE;

typedef struct _SWISSKNIFE_REGISTRY_REQUEST {
    ULONG KeyPathLength;    /* In bytes including terminating null. */
    ULONG ValueNameLength;  /* In bytes including terminating null. */
    WCHAR Buffer[1];        /* Key path followed by value name. */
} SWISSKNIFE_REGISTRY_REQUEST, *PSWISSKNIFE_REGISTRY_REQUEST;

typedef struct _SWISSKNIFE_REGISTRY_RESPONSE {
    ULONG Type;             /* REG_* type */
    ULONG DataLength;
    UCHAR Data[1];
} SWISSKNIFE_REGISTRY_RESPONSE, *PSWISSKNIFE_REGISTRY_RESPONSE;

typedef struct _SWISSKNIFE_RANDOM_REQUEST {
    ULONG Seed;
    ULONG Length;           /* Number of random bytes requested. */
} SWISSKNIFE_RANDOM_REQUEST, *PSWISSKNIFE_RANDOM_REQUEST;

typedef struct _SWISSKNIFE_RANDOM_RESPONSE {
    ULONG SeedUsed;
    ULONG BytesGenerated;
    UCHAR Data[1];
} SWISSKNIFE_RANDOM_RESPONSE, *PSWISSKNIFE_RANDOM_RESPONSE;
#pragma pack(pop)

#ifndef _KERNEL_MODE
/* Helper macro so the user-mode client can calculate required buffer sizes. */
#define SWISSKNIFE_FILE_REQUEST_SIZE(pathChars) (sizeof(SWISSKNIFE_FILE_REQUEST) + ((pathChars) * sizeof(WCHAR)))
#define SWISSKNIFE_REGISTRY_REQUEST_SIZE(keyChars, valueChars) \
    (sizeof(SWISSKNIFE_REGISTRY_REQUEST) + (((keyChars) + (valueChars)) * sizeof(WCHAR)))
#endif

