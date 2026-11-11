// C:\Users\Admin\Desktop\potasium moged\source potasium\driver\definitions.h
#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <windef.h>
#include <intrin.h>

#define DRIVER_MAGIC 0x0AD02A7B

#define IOCTL_PROTECT_VAD       0x225914
#define IOCTL_ALLOCATE_MEMORY   0x225918
#define IOCTL_WRITE_MEMORY      0x22591C
#define IOCTL_READ_MEMORY       0x225920
#define IOCTL_CREATE_THREAD     0x225924

#define DEVICE_NAME             L"\\Device\\$rdp"
#define SYMBOLIC_NAME           L"\\DosDevices\\Global\\$rdp"

#define POOL_TAG_SWAP           0x73766170 // 'pavs'
#define POOL_TAG_ALNL           0x4E4C4541 // 'AELN'

// Request structures
#pragma pack(push, 1)

typedef struct _VAD_REQUEST {
    ULONG Magic;              // +0x00: 0x0AD02A7B
    ULONG ProcessId;          // +0x04: Target process ID
    ULONG64 Address;          // +0x08: Target address
    ULONG64 Reserved;         // +0x10: Reserved padding
} VAD_REQUEST, *PVAD_REQUEST;

typedef struct _ALLOC_REQUEST {
    ULONG Magic;              // +0x00: 0x0AD02A7B
    ULONG ProcessId;          // +0x04: Target process ID
    ULONG Reserved0;          // +0x08: Padding
    ULONG Reserved1;          // +0x0C: Padding
    ULONG64 Size;             // +0x10: RegionSize
    ULONG Protect;            // +0x18: Page protection
    ULONG AllocationType;     // +0x1C: Allocation type (MEM_COMMIT | MEM_RESERVE)
    UCHAR UseMdl;             // +0x20: 0 = ZwAllocateVirtualMemory, 1 = MDL custom mapping
    UCHAR Padding[7];         // +0x21: Padding
    PVOID* OutBaseAddress;    // +0x28: Pointer to output allocated base address
} ALLOC_REQUEST, *PALLOC_REQUEST;

typedef struct _WRITE_REQUEST {
    ULONG Magic;              // +0x00: 0x0AD02A7B
    ULONG ProcessId;          // +0x04: Target process ID
    ULONG64 Address;          // +0x08: Target address (destination)
    ULONG Size;               // +0x10: Number of bytes to write
    ULONG Reserved;           // +0x14: Padding
    ULONG64 Buffer;           // +0x18: Local caller buffer (source)
} WRITE_REQUEST, *PWRITE_REQUEST;

typedef struct _READ_REQUEST {
    ULONG Magic;              // +0x00: 0x0AD02A7B
    ULONG ProcessId;          // +0x04: Target process ID
    ULONG64 Address;          // +0x08: Target address (source)
    ULONG Size;               // +0x10: Number of bytes to read
    ULONG Reserved;           // +0x14: Padding
    ULONG64 Buffer;           // +0x18: Local caller buffer (destination)
} READ_REQUEST, *PREAD_REQUEST;

typedef struct _CREATE_THREAD_REQUEST {
    ULONG Magic;              // +0x00: 0x0AD02A7B
    ULONG ProcessId;          // +0x04: Target process ID
    ULONG64 StartAddress;     // +0x08: Start routine virtual address
} CREATE_THREAD_REQUEST, *PCREATE_THREAD_REQUEST;

#pragma pack(pop)

// OS Information & Paging
typedef struct _OS_INFO {
    ULONG OsVersion;          // +0x00: Windows build number / version code
    ULONG Reserved0;          // +0x04
    UCHAR Initialized;        // +0x08: Initialized flag (= 1)
    UCHAR Reserved1[19];      // +0x09..+0x1B
    ULONG VadRootOffset;      // +0x1C: EPROCESS.VadRoot offset (0x7D8 or 0x558)
    UCHAR Reserved2[32];      // +0x20..+0x3F
    ULONG64 PdeBase;          // +0x40: PDE Base address
    ULONG64 PteBase;          // +0x48: PTE Base address
} OS_INFO, *POS_INFO;

// Win32 Protection to MMVAD_FLAGS.Protection (5-bit) mapping
static const ULONG ProtectionTable[32] = {
    PAGE_NOACCESS,                                  // 0x00 (1)
    PAGE_READONLY,                                  // 0x01 (2)
    PAGE_EXECUTE,                                   // 0x02 (16)
    PAGE_EXECUTE_READ,                              // 0x03 (32)
    PAGE_READWRITE,                                 // 0x04 (4)
    PAGE_WRITECOPY,                                 // 0x05 (8)
    PAGE_EXECUTE_READWRITE,                         // 0x06 (64)
    PAGE_EXECUTE_WRITECOPY,                         // 0x07 (128)
    PAGE_NOACCESS,                                  // 0x08 (1)
    PAGE_NOCACHE | PAGE_READONLY,                   // 0x09 (514)
    PAGE_NOCACHE | PAGE_EXECUTE,                    // 0x0A (528)
    PAGE_NOCACHE | PAGE_EXECUTE_READ,               // 0x0B (544)
    PAGE_NOCACHE | PAGE_READWRITE,                  // 0x0C (516)
    PAGE_NOCACHE | PAGE_WRITECOPY,                  // 0x0D (520)
    PAGE_NOCACHE | PAGE_EXECUTE_READWRITE,          // 0x0E (576)
    PAGE_NOCACHE | PAGE_EXECUTE_WRITECOPY,          // 0x0F (640)
    PAGE_NOACCESS,                                  // 0x10 (1)
    PAGE_GUARD | PAGE_READONLY,                     // 0x11 (258)
    PAGE_GUARD | PAGE_EXECUTE,                      // 0x12 (272)
    PAGE_GUARD | PAGE_EXECUTE_READ,                 // 0x13 (288)
    PAGE_GUARD | PAGE_READWRITE,                    // 0x14 (260)
    PAGE_GUARD | PAGE_WRITECOPY,                    // 0x15 (264)
    PAGE_GUARD | PAGE_EXECUTE_READWRITE,            // 0x16 (320)
    PAGE_GUARD | PAGE_EXECUTE_WRITECOPY,            // 0x17 (384)
    PAGE_NOACCESS,                                  // 0x18 (1)
    PAGE_WRITECOMBINE | PAGE_READONLY,              // 0x19 (1026)
    PAGE_WRITECOMBINE | PAGE_EXECUTE,               // 0x1A (1040)
    PAGE_WRITECOMBINE | PAGE_EXECUTE_READ,          // 0x1B (1056)
    PAGE_WRITECOMBINE | PAGE_READWRITE,             // 0x1C (1028)
    PAGE_WRITECOMBINE | PAGE_WRITECOPY,             // 0x1D (1032)
    PAGE_WRITECOMBINE | PAGE_EXECUTE_READWRITE,     // 0x1E (1088)
    PAGE_WRITECOMBINE | PAGE_EXECUTE_WRITECOPY      // 0x1F (1152)
};

// Undocumented kernel exports & declarations
NTKERNELAPI NTSTATUS NTAPI IoCreateDriver(
    IN PUNICODE_STRING DriverName OPTIONAL,
    IN PDRIVER_INITIALIZE InitializationFunction
);

NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
    IN PEPROCESS SourceProcess,
    IN PVOID SourceAddress,
    IN PEPROCESS TargetProcess,
    OUT PVOID TargetAddress,
    IN SIZE_T BufferSize,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PSIZE_T NumberOfBytesCopied
);

NTKERNELAPI ULONG64 NTAPI KeCapturePersistentThreadState(
    IN PCONTEXT Context,
    IN PKTHREAD Thread OPTIONAL,
    IN ULONG BugCheckCode,
    IN ULONG_PTR BugCheckParameter1,
    IN ULONG_PTR BugCheckParameter2,
    IN ULONG_PTR BugCheckParameter3,
    IN ULONG_PTR BugCheckParameter4,
    OUT PVOID VirtualAddress
);

NTKERNELAPI NTSTATUS NTAPI RtlCreateUserThread(
    IN HANDLE ProcessHandle,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL,
    IN BOOLEAN CreateSuspended,
    IN ULONG ZeroBits OPTIONAL,
    IN SIZE_T MaximumStackSize OPTIONAL,
    IN SIZE_T CommittedStackSize OPTIONAL,
    IN PVOID StartAddress,
    IN PVOID Parameter OPTIONAL,
    OUT PHANDLE ThreadHandle OPTIONAL,
    OUT PCLIENT_ID ClientId OPTIONAL
);

NTKERNELAPI NTSTATUS NTAPI ZwAllocateVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID* BaseAddress,
    IN ULONG_PTR ZeroBits,
    IN OUT PSIZE_T RegionSize,
    IN ULONG AllocationType,
    IN ULONG Protect
);
