// C:\Users\Admin\Desktop\potasium moged\source potasium\driver\main.c
#include "definitions.h"

// Globals
UNICODE_STRING DeviceName;
UNICODE_STRING SymbolicLinkName;
OS_INFO g_OsInfo;
UCHAR g_PteDumpData[0x380];

#define g_PteBaseDump (*(ULONG64*)(g_PteDumpData + 0x360))

// Forward Declarations
NTSTATUS DriverInit(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDefault(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

VOID InitPersistentDump(VOID);
NTSTATUS InitOsInfo(POS_INFO OsInfo);
ULONG64 GetPteAddress(ULONG64 VirtualAddress);
ULONG FindVadNode(PVOID* VadRoot, ULONG_PTR Vpn, PVOID* OutNode);
NTSTATUS FindVad(PEPROCESS Process, ULONG_PTR VirtualAddress, PVOID* OutVad);
NTSTATUS AllocateMdlMemory(PVOID* BaseAddress, SIZE_T Size, ULONG Protect);

NTSTATUS IoctlProtectVad(PVOID Buffer);
NTSTATUS IoctlAllocateMemory(PVOID Buffer);
NTSTATUS IoctlWriteMemory(PVOID Buffer);
NTSTATUS IoctlReadMemory(PVOID Buffer);
NTSTATUS IoctlCreateThread(PVOID Buffer);

// sub_140001454: Capture persistent processor state to extract MmPteBase
VOID InitPersistentDump(VOID)
{
    CONTEXT context;
    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_ALL;

    RtlCaptureContext(&context);

    PVOID pool = ExAllocatePoolWithTag(NonPagedPool, 0x40000, POOL_TAG_SWAP);
    if (pool) {
        KeCapturePersistentThreadState(&context, NULL, 0, 0, 0, 0, 0, pool);

        // Copy 0x380 bytes from pool + 0x2080
        memcpy(g_PteDumpData, (PUCHAR)pool + 0x2080, sizeof(g_PteDumpData));

        ExFreePoolWithTag(pool, 0);
    }
}

// sub_14000122C: Detect OS version, VadRoot offset, and calculate dynamic PTE/PDE bases
NTSTATUS InitOsInfo(POS_INFO OsInfo)
{
    RTL_OSVERSIONINFOEXW versionInfo;
    memset(&versionInfo, 0, sizeof(versionInfo));

    if (!OsInfo) {
        return STATUS_PORT_NOT_SET; // 0xC0000141
    }

    memset(OsInfo, 0, sizeof(OS_INFO));
    versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

    OsInfo->PdeBase = 0xFFFFF6FB40000000ULL;
    OsInfo->PteBase = 0xFFFFF68000000000ULL;

    NTSTATUS status = RtlGetVersion((PRTL_OSVERSIONINFOW)&versionInfo);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ULONG verCode = (versionInfo.dwMajorVersion << 8) | (versionInfo.dwMinorVersion << 4) | (versionInfo.wServicePackMajor);
    OsInfo->Initialized = 1;
    OsInfo->OsVersion = verCode;

    if (verCode == 0xA00) {
        ULONG build = versionInfo.dwBuildNumber;
        if (build == 19041) {
            OsInfo->OsVersion = 2568;
            OsInfo->VadRootOffset = 0x7D8;
        } else if (build == 19042) {
            OsInfo->OsVersion = 2569;
            OsInfo->VadRootOffset = 0x7D8;
        } else if (build == 19044 || build == 19045 || build == 22000 || build == 22621 || build == 22631) {
            OsInfo->OsVersion = build;
            OsInfo->VadRootOffset = 0x7D8;
        } else if (build == 26100 || build == 26200 || build == 26300 || build == 28000) {
            OsInfo->OsVersion = build;
            OsInfo->VadRootOffset = 0x558;
        } else {
            return STATUS_NOT_SUPPORTED; // 0xC00000BB
        }
    } else {
        if (verCode < 0xA01) {
            goto CheckBase;
        }
    }

    OsInfo->PteBase = g_PteBaseDump;
    OsInfo->PdeBase = (g_PteBaseDump & 0xFFFFFF8000000000ULL) | ((g_PteBaseDump >> 9) & 0x7FFFFFFFFFULL);

CheckBase:
    if (OsInfo->PdeBase < 0xFFFF080000000000ULL || OsInfo->PteBase < 0xFFFF080000000000ULL) {
        return STATUS_UNSUCCESSFUL; // 0xC0000001
    }

    if (OsInfo->VadRootOffset == 0) {
        return 0xC000A004;
    }

    return STATUS_SUCCESS;
}

// sub_1400013DC: Virtual Address to PTE address translation with Large Page check
ULONG64 GetPteAddress(ULONG64 VirtualAddress)
{
    ULONG64 pdeOffset = (VirtualAddress >> 18) & 0x3FFFFFF8ULL;

    if (g_OsInfo.OsVersion < 0xA01) {
        ULONG64 pde = pdeOffset + 0xFFFFF6FB40000000ULL;
        if (*(CHAR*)pde >= 0) {
            return ((VirtualAddress >> 9) & 0x7FFFFFFFF8ULL) + 0xFFFFF68000000000ULL;
        }
        return pde;
    } else {
        ULONG64 pde = pdeOffset + g_OsInfo.PdeBase;
        if (*(CHAR*)pde >= 0) {
            return ((VirtualAddress >> 9) & 0x7FFFFFFFF8ULL) + g_OsInfo.PteBase;
        }
        return pde;
    }
}

// sub_1400016E8: Balanced tree traversal to find VAD containing VPN
ULONG FindVadNode(PVOID* VadRoot, ULONG_PTR Vpn, PVOID* OutNode)
{
    PVOID node = *VadRoot;
    if (!node) {
        return 0;
    }

    while (TRUE) {
        ULONG startingVpn = *(ULONG*)((PUCHAR)node + 0x18);
        if (Vpn < startingVpn) {
            PVOID left = *(PVOID*)node;
            if (!left) {
                *OutNode = node;
                return 2;
            }
            node = left;
            continue;
        }

        ULONG endingVpn = *(ULONG*)((PUCHAR)node + 0x1C);
        if (Vpn <= endingVpn) {
            *OutNode = node;
            return 1;
        }

        PVOID right = *(PVOID*)((PUCHAR)node + 0x08);
        if (!right) {
            *OutNode = node;
            return 3;
        }
        node = right;
    }
}

// sub_14000153C: Search VAD tree in target EPROCESS
NTSTATUS FindVad(PEPROCESS Process, ULONG_PTR VirtualAddress, PVOID* OutVad)
{
    if (!Process || !OutVad) {
        return STATUS_INVALID_PARAMETER; // 0xC000000D
    }

    PVOID* pVadRoot = (PVOID*)((PUCHAR)Process + g_OsInfo.VadRootOffset);
    PVOID vad = *pVadRoot;

    if (FindVadNode(pVadRoot, VirtualAddress >> 12, &vad) == 1) {
        *OutVad = vad;
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND; // 0xC0000225
}

// sub_140001000: Kernel MDL allocation, user space mapping, VAD protection fixup, NX stripping
NTSTATUS AllocateMdlMemory(PVOID* BaseAddress, SIZE_T Size, ULONG Protect)
{
    ULONG64 requestedBase = (ULONG64)*BaseAddress & 0xFFFFFFFFFFFFF000ULL;
    *BaseAddress = NULL;
    SIZE_T alignedSize = (Size + 0xFFF + (requestedBase & 0xFFF)) & 0xFFFFFFFFFFFFF000ULL;

    PVOID pool = ExAllocatePool2(POOL_FLAG_NON_PAGED, alignedSize, POOL_TAG_ALNL);
    if (!pool) {
        return STATUS_INSUFFICIENT_RESOURCES; // 0xC0000017
    }

    memset(pool, 0, alignedSize);

    PMDL mdl = IoAllocateMdl(pool, (ULONG)alignedSize, FALSE, FALSE, NULL);
    if (!mdl) {
        ExFreePoolWithTag(pool, 0);
        return STATUS_INSUFFICIENT_RESOURCES; // 0xC0000017
    }

    MmBuildMdlForNonPagedPool(mdl);

    // First attempt: Map at requested address with SEH
    __try {
        *BaseAddress = MmMapLockedPagesSpecifyCache(mdl, UserMode, MmCached, (PVOID)requestedBase, FALSE, NormalPagePriority | 0x10);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *BaseAddress = NULL;
    }

    // Fallback: Map anywhere
    if (!*BaseAddress) {
        __try {
            *BaseAddress = MmMapLockedPagesSpecifyCache(mdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority | 0x10);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            *BaseAddress = NULL;
        }
    }

    if (!*BaseAddress) {
        IoFreeMdl(mdl);
        ExFreePoolWithTag(pool, 0);
        return STATUS_NONE_MAPPED; // 0xC0000073
    }

    ULONG vadProtect = 0;
    for (ULONG i = 0; i < 32; i++) {
        if (ProtectionTable[i] == Protect) {
            vadProtect = i;
            break;
        }
    }

    PEPROCESS currentProcess = IoGetCurrentProcess();
    PVOID vad = NULL;
    if (NT_SUCCESS(FindVad(currentProcess, (ULONG_PTR)*BaseAddress, &vad))) {
        ULONG flags = *(ULONG*)((PUCHAR)vad + 0x30);
        flags ^= (flags ^ (vadProtect << 7)) & 0xF80;
        *(ULONG*)((PUCHAR)vad + 0x30) = flags;
    }

    // Strip NX bit if executable protection requested
    if ((Protect & 0xF0) != 0) {
        ULONG64 currentVa = (ULONG64)*BaseAddress;
        ULONG64 endVa = currentVa + alignedSize;
        while (currentVa < endVa) {
            PULONG64 pte = (PULONG64)GetPteAddress(currentVa);
            *pte &= ~0x8000000000000000ULL;
            currentVa += PAGE_SIZE;
        }
    }

    return STATUS_SUCCESS;
}

// sub_140001734: IOCTL 0x225918 - Allocate Virtual Memory
NTSTATUS IoctlAllocateMemory(PVOID Buffer)
{
    PALLOC_REQUEST req = (PALLOC_REQUEST)Buffer;
    if (req->Magic != DRIVER_MAGIC || req->ProcessId == 0) {
        return STATUS_ACCESS_DENIED; // 0xC0000022
    }

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(LONG_PTR)req->ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return STATUS_INVALID_PARAMETER; // 0xC000000D
    }

    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);

    SIZE_T regionSize = req->Size;
    PVOID baseAddress = NULL;

    if (req->UseMdl == 0) {
        status = ZwAllocateVirtualMemory(
            ZwCurrentProcess(),
            &baseAddress,
            0,
            &regionSize,
            req->AllocationType,
            req->Protect
        );
    } else {
        status = AllocateMdlMemory(&baseAddress, regionSize, req->Protect);
        if (!NT_SUCCESS(status)) {
            _mm_lfence();
            KeUnstackDetachProcess(&apcState);
            ObDereferenceObject(process);
            return status;
        }
    }

    _mm_lfence();
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);

    *req->OutBaseAddress = baseAddress;
    return status;
}

// sub_140001824: IOCTL 0x225914 - Modify VAD / Strip Image Name or Protection
NTSTATUS IoctlProtectVad(PVOID Buffer)
{
    PVAD_REQUEST req = (PVAD_REQUEST)Buffer;
    if (req->Magic != DRIVER_MAGIC || req->ProcessId == 0) {
        return STATUS_ACCESS_DENIED; // 0xC0000022
    }

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(LONG_PTR)req->ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return STATUS_INVALID_PARAMETER; // 0xC000000D
    }

    PVOID* pVadRoot = (PVOID*)((PUCHAR)process + g_OsInfo.VadRootOffset);
    PVOID vad = *pVadRoot;
    ULONG result = FindVadNode(pVadRoot, req->Address >> 12, &vad);

    PVOID foundVad = (result == 1) ? vad : NULL;
    status = (result != 1) ? STATUS_NOT_FOUND : STATUS_SUCCESS;

    if (result == 1) {
        ULONG vadType = (*(ULONG*)((PUCHAR)foundVad + 0x30)) & 0x70;
        if (vadType == 0x20) { // VadImageMap
            ULONG_PTR subSection = *(ULONG_PTR*)((PUCHAR)foundVad + 0x44);
            if (!subSection) {
                ObDereferenceObject(process);
                return STATUS_PORT_NOT_SET; // 0xC0000141
            }
            ULONG_PTR controlArea = *(ULONG_PTR*)subSection;
            if (!controlArea) {
                ObDereferenceObject(process);
                return STATUS_PORT_NOT_SET; // 0xC0000141
            }
            ULONG_PTR fileObject = *(ULONG_PTR*)(controlArea + 0x40);
            if (!fileObject) {
                ObDereferenceObject(process);
                return STATUS_PORT_NOT_SET; // 0xC0000141
            }
            fileObject &= ~0xFULL;
            PWCHAR pBuf = *(PWCHAR*)(fileObject + 0x60);
            if (pBuf) {
                *pBuf = 0;
            }
            *(USHORT*)(fileObject + 0x58) = 0;
            status = STATUS_SUCCESS;
        } else if (vadType == 0x10) {
            *(ULONG*)((PUCHAR)foundVad + 0x30) &= 0xFFFFF07F;
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_PARAMETER; // 0xC000000D
        }
    }

    _mm_lfence();
    ObDereferenceObject(process);
    return status;
}

// sub_140001930: IOCTL 0x225920 - Read Virtual Memory
NTSTATUS IoctlReadMemory(PVOID Buffer)
{
    PREAD_REQUEST req = (PREAD_REQUEST)Buffer;
    if (req->Magic != DRIVER_MAGIC || req->ProcessId == 0) {
        return STATUS_ACCESS_DENIED; // 0xC0000022
    }

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(LONG_PTR)req->ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return STATUS_INVALID_PARAMETER; // 0xC000000D
    }

    SIZE_T bytesCopied = 0;
    __try {
        status = MmCopyVirtualMemory(
            process,
            (PVOID)req->Address,
            IoGetCurrentProcess(),
            (PVOID)req->Buffer,
            req->Size,
            KernelMode,
            &bytesCopied
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    ObDereferenceObject(process);
    return status;
}

// sub_1400019E8: IOCTL 0x22591C - Write Virtual Memory
NTSTATUS IoctlWriteMemory(PVOID Buffer)
{
    PWRITE_REQUEST req = (PWRITE_REQUEST)Buffer;
    if (req->Magic != DRIVER_MAGIC || req->ProcessId == 0) {
        return STATUS_ACCESS_DENIED; // 0xC0000022
    }

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(LONG_PTR)req->ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return STATUS_INVALID_PARAMETER; // 0xC000000D
    }

    SIZE_T bytesCopied = 0;
    __try {
        status = MmCopyVirtualMemory(
            IoGetCurrentProcess(),
            (PVOID)req->Buffer,
            process,
            (PVOID)req->Address,
            req->Size,
            KernelMode,
            &bytesCopied
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    ObDereferenceObject(process);
    return status;
}

// sub_140001AAC: IOCTL 0x225924 - Create Thread via RtlCreateUserThread
NTSTATUS IoctlCreateThread(PVOID Buffer)
{
    PCREATE_THREAD_REQUEST req = (PCREATE_THREAD_REQUEST)Buffer;
    if (req->Magic != DRIVER_MAGIC || req->ProcessId == 0) {
        return STATUS_ACCESS_DENIED; // 0xC0000022
    }

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(LONG_PTR)req->ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return STATUS_INVALID_PARAMETER; // 0xC000000D
    }

    CLIENT_ID clientId;
    clientId.UniqueProcess = (HANDLE)(LONG_PTR)req->ProcessId;
    clientId.UniqueThread = NULL;

    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

    HANDLE processHandle = NULL;
    status = ZwOpenProcess(&processHandle, PROCESS_ALL_ACCESS, &objAttr, &clientId);
    if (!NT_SUCCESS(status)) {
        _mm_lfence();
        ObDereferenceObject(process);
        return status;
    }

    HANDLE threadHandle = NULL;
    CLIENT_ID outClientId;
    memset(&outClientId, 0, sizeof(outClientId));

    status = RtlCreateUserThread(
        processHandle,
        NULL,
        TRUE,
        0,
        0,
        0,
        (PVOID)req->StartAddress,
        NULL,
        &threadHandle,
        &outClientId
    );

    if (!NT_SUCCESS(status)) {
        _mm_lfence();
        ZwClose(processHandle);
        ObDereferenceObject(process);
        return status;
    }

    ZwClose(processHandle);
    ObDereferenceObject(process);
    return STATUS_SUCCESS;
}

// sub_140001C00: Device Control Dispatch
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG inLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST; // 0xC0000010
    ULONG_PTR info = 0;

    switch (ioctl)
    {
    case IOCTL_PROTECT_VAD: // 0x225914
        if (buffer) {
            if (inLength == sizeof(VAD_REQUEST)) {
                status = IoctlProtectVad(buffer);
                info = sizeof(VAD_REQUEST);
            } else {
                status = STATUS_INFO_LENGTH_MISMATCH; // 0xC0000004
            }
        } else {
            status = STATUS_INVALID_PARAMETER; // 0xC000000D
        }
        break;

    case IOCTL_ALLOCATE_MEMORY: // 0x225918
        if (buffer) {
            if (inLength == sizeof(ALLOC_REQUEST)) {
                status = IoctlAllocateMemory(buffer);
                info = sizeof(ALLOC_REQUEST);
            } else {
                status = STATUS_INFO_LENGTH_MISMATCH; // 0xC0000004
            }
        } else {
            status = STATUS_INVALID_PARAMETER; // 0xC000000D
        }
        break;

    case IOCTL_WRITE_MEMORY: // 0x22591C
        if (buffer) {
            if (inLength == sizeof(WRITE_REQUEST)) {
                status = IoctlWriteMemory(buffer);
                info = sizeof(WRITE_REQUEST);
            } else {
                status = STATUS_INFO_LENGTH_MISMATCH; // 0xC0000004
            }
        } else {
            status = STATUS_INVALID_PARAMETER; // 0xC000000D
        }
        break;

    case IOCTL_READ_MEMORY: // 0x225920
        if (buffer) {
            if (inLength == sizeof(READ_REQUEST)) {
                status = IoctlReadMemory(buffer);
                info = sizeof(READ_REQUEST);
            } else {
                status = STATUS_INFO_LENGTH_MISMATCH; // 0xC0000004
            }
        } else {
            status = STATUS_INVALID_PARAMETER; // 0xC000000D
        }
        break;

    case IOCTL_CREATE_THREAD: // 0x225924
        if (buffer) {
            if (inLength == sizeof(CREATE_THREAD_REQUEST)) {
                status = IoctlCreateThread(buffer);
                info = sizeof(CREATE_THREAD_REQUEST);
            } else {
                status = STATUS_INFO_LENGTH_MISMATCH; // 0xC0000004
            }
        } else {
            status = STATUS_INVALID_PARAMETER; // 0xC000000D
        }
        break;

    default:
        break;
    }

    Irp->IoStatus.Information = info;
    Irp->IoStatus.Status = status;
    IofCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// sub_140001D00: Create & Close Dispatch
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IofCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// sub_140001D20: Default Unsupported Dispatch
NTSTATUS DispatchDefault(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED; // 0xC00000BB
    Irp->IoStatus.Information = 0;
    IofCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_SUPPORTED;
}

// sub_1400016C0: Driver Unload
VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    IoDeleteSymbolicLink(&SymbolicLinkName);
    IoDeleteDevice(DriverObject->DeviceObject);
}

// sub_1400015A0: Driver Initialization callback for IoCreateDriver
NTSTATUS DriverInit(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    PDEVICE_OBJECT DeviceObject = NULL;

    InitPersistentDump();
    NTSTATUS status = InitOsInfo(&g_OsInfo);
    if (!NT_SUCCESS(status)) {
        _mm_lfence();
        return status;
    }

    RtlInitUnicodeString(&DeviceName, DEVICE_NAME);
    RtlInitUnicodeString(&SymbolicLinkName, SYMBOLIC_NAME);

    status = IoCreateDevice(
        DriverObject,
        0,
        &DeviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &DeviceObject
    );
    if (!NT_SUCCESS(status)) {
        _mm_lfence();
        return status;
    }

    status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
    if (NT_SUCCESS(status)) {
        for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
            DriverObject->MajorFunction[i] = DispatchDefault;
        }

        DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
        DriverObject->DriverUnload = DriverUnload;

        DeviceObject->Flags |= DO_BUFFERED_IO;
        DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

        return STATUS_SUCCESS;
    } else {
        _mm_lfence();
        IoDeleteDevice(DeviceObject);
        return status;
    }
}

// 0x140001D40: DriverEntry
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    return IoCreateDriver(NULL, DriverInit);
}
