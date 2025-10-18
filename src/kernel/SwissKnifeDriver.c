#define _KERNEL_MODE
#include "../../include/SwissKnifeCommon.h"

#include <ntstrsafe.h>

/* Forward declarations */
static VOID SwissKnifeUnload(_In_ PDRIVER_OBJECT DriverObject);
static NTSTATUS SwissKnifeCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
static NTSTATUS SwissKnifeDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

static VOID SwissKnifeFillDriverInfo(_Out_writes_bytes_(OutputBufferLength) PSWISSKNIFE_DRIVER_INFO Info, _In_ ULONG OutputBufferLength);
static NTSTATUS SwissKnifeQuerySystemOverview(_Out_writes_bytes_(OutputBufferLength) PSWISSKNIFE_SYSTEM_OVERVIEW Overview, _In_ ULONG OutputBufferLength);
static NTSTATUS SwissKnifeEnumerateProcesses(_Out_writes_bytes_(OutputBufferLength) PVOID Buffer, _In_ ULONG OutputBufferLength, _Out_ PULONG BytesWritten);
static NTSTATUS SwissKnifeEnumerateModules(_Out_writes_bytes_(OutputBufferLength) PVOID Buffer, _In_ ULONG OutputBufferLength, _Out_ PULONG BytesWritten);
static NTSTATUS SwissKnifeHandleFileRead(_In_reads_bytes_(InputBufferLength) PVOID InputBuffer, _In_ ULONG InputBufferLength,
                                        _Out_writes_bytes_(OutputBufferLength) PVOID OutputBuffer, _In_ ULONG OutputBufferLength,
                                        _Out_ PULONG BytesWritten);
static NTSTATUS SwissKnifeHandleRegistryQuery(_In_reads_bytes_(InputBufferLength) PVOID InputBuffer, _In_ ULONG InputBufferLength,
                                             _Out_writes_bytes_(OutputBufferLength) PVOID OutputBuffer, _In_ ULONG OutputBufferLength,
                                             _Out_ PULONG BytesWritten);
static NTSTATUS SwissKnifeHandleRandomBytes(_In_reads_bytes_(InputBufferLength) PVOID InputBuffer, _In_ ULONG InputBufferLength,
                                           _Out_writes_bytes_(OutputBufferLength) PVOID OutputBuffer, _In_ ULONG OutputBufferLength,
                                           _Out_ PULONG BytesWritten);

extern ULONG MmNumberOfPhysicalPages;
extern ULONG MmAvailablePages;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SwissKnifeUnload)
#pragma alloc_text(PAGE, SwissKnifeCreateClose)
#pragma alloc_text(PAGE, SwissKnifeDeviceControl)
#pragma alloc_text(PAGE, SwissKnifeQuerySystemOverview)
#pragma alloc_text(PAGE, SwissKnifeEnumerateProcesses)
#pragma alloc_text(PAGE, SwissKnifeEnumerateModules)
#pragma alloc_text(PAGE, SwissKnifeHandleFileRead)
#pragma alloc_text(PAGE, SwissKnifeHandleRegistryQuery)
#pragma alloc_text(PAGE, SwissKnifeHandleRandomBytes)
#endif

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLink;
    PDEVICE_OBJECT deviceObject = NULL;

    RtlInitUnicodeString(&deviceName, SWISSKNIFE_DEVICE_NAME);
    status = IoCreateDevice(DriverObject,
                            0,
                            &deviceName,
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlInitUnicodeString(&symbolicLink, SWISSKNIFE_SYMBOLIC_NAME);
    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->DriverUnload = SwissKnifeUnload;
    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) {
        DriverObject->MajorFunction[i] = SwissKnifeCreateClose;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = SwissKnifeCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = SwissKnifeCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SwissKnifeDeviceControl;

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static VOID
SwissKnifeUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    PAGED_CODE();

    UNICODE_STRING symbolicLink;
    RtlInitUnicodeString(&symbolicLink, SWISSKNIFE_SYMBOLIC_NAME);
    IoDeleteSymbolicLink(&symbolicLink);

    IoDeleteDevice(DriverObject->DeviceObject);
}

static NTSTATUS
SwissKnifeCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PAGED_CODE();

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS
SwissKnifeDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PAGED_CODE();

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioControlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputBufferLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG outputBufferLength = stack->Parameters.DeviceIoControl.OutputBufferLength;

    ULONG bytesWritten = 0;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    switch (ioControlCode) {
    case IOCTL_SWISSKNIFE_GET_DRIVER_INFO:
        if (outputBufferLength < sizeof(SWISSKNIFE_DRIVER_INFO)) {
            status = STATUS_BUFFER_TOO_SMALL;
            bytesWritten = sizeof(SWISSKNIFE_DRIVER_INFO);
        } else {
            SwissKnifeFillDriverInfo((PSWISSKNIFE_DRIVER_INFO)outputBuffer, outputBufferLength);
            bytesWritten = sizeof(SWISSKNIFE_DRIVER_INFO);
            status = STATUS_SUCCESS;
        }
        break;
    case IOCTL_SWISSKNIFE_GET_SYSTEM_OVERVIEW:
        status = SwissKnifeQuerySystemOverview((PSWISSKNIFE_SYSTEM_OVERVIEW)outputBuffer, outputBufferLength);
        if (NT_SUCCESS(status)) {
            bytesWritten = sizeof(SWISSKNIFE_SYSTEM_OVERVIEW);
        } else if (status == STATUS_BUFFER_TOO_SMALL) {
            bytesWritten = sizeof(SWISSKNIFE_SYSTEM_OVERVIEW);
        }
        break;
    case IOCTL_SWISSKNIFE_ENUMERATE_PROCESSES:
        status = SwissKnifeEnumerateProcesses(outputBuffer, outputBufferLength, &bytesWritten);
        break;
    case IOCTL_SWISSKNIFE_ENUMERATE_MODULES:
        status = SwissKnifeEnumerateModules(outputBuffer, outputBufferLength, &bytesWritten);
        break;
    case IOCTL_SWISSKNIFE_READ_FILE:
        status = SwissKnifeHandleFileRead(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
        break;
    case IOCTL_SWISSKNIFE_QUERY_REGISTRY:
        status = SwissKnifeHandleRegistryQuery(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
        break;
    case IOCTL_SWISSKNIFE_RANDOM_BYTES:
        status = SwissKnifeHandleRandomBytes(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesWritten;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static VOID
SwissKnifeFillDriverInfo(
    _Out_writes_bytes_(OutputBufferLength) PSWISSKNIFE_DRIVER_INFO Info,
    _In_ ULONG OutputBufferLength
    )
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    RtlZeroMemory(Info, sizeof(*Info));
    Info->VersionMajor = 1;
    Info->VersionMinor = 0;
    Info->BuildNumber = 1;
    (void)RtlStringCchCopyW(Info->FriendlyName, RTL_NUMBER_OF(Info->FriendlyName), L"SwissKnife Kernel Service");
    (void)RtlStringCchCopyW(Info->Description, RTL_NUMBER_OF(Info->Description),
        L"Multi-purpose diagnostics, inspection, and data-gathering driver");
}

static NTSTATUS
SwissKnifeQuerySystemOverview(
    _Out_writes_bytes_(OutputBufferLength) PSWISSKNIFE_SYSTEM_OVERVIEW Overview,
    _In_ ULONG OutputBufferLength
    )
{
    PAGED_CODE();

    if (OutputBufferLength < sizeof(SWISSKNIFE_SYSTEM_OVERVIEW)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlZeroMemory(Overview, sizeof(*Overview));

    KAFFINITY activeProcessors = KeQueryActiveProcessors();
    ULONG processorCount = KeQueryActiveProcessorCount(NULL);

    Overview->ActiveProcessorMask = activeProcessors;
    Overview->NumberOfProcessors = processorCount;
    Overview->TotalPhysicalPages = (ULONGLONG)MmNumberOfPhysicalPages;
    Overview->AvailablePages = (ULONGLONG)MmAvailablePages;
    Overview->PageSize = PAGE_SIZE;

    LARGE_INTEGER interruptTime = KeQueryUnbiasedInterruptTime();
    Overview->InterruptTime100Ns = interruptTime.QuadPart;
    Overview->SystemUptimeMilliseconds = (interruptTime.QuadPart / 10000ULL);

    return STATUS_SUCCESS;
}

static NTSTATUS
SwissKnifeEnumerateProcesses(
    _Out_writes_bytes_(OutputBufferLength) PVOID Buffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesWritten
    )
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    ULONG requiredSize = 0;
    ULONG bufferSize = 0x10000;
    PVOID queryBuffer = NULL;

    *BytesWritten = 0;

    do {
        if (queryBuffer != NULL) {
            ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
            queryBuffer = NULL;
        }

        queryBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, bufferSize, SWISSKNIFE_POOL_TAG);
        if (queryBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        status = ZwQuerySystemInformation(SystemProcessInformation, queryBuffer, bufferSize, &bufferSize);
    } while (status == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(status)) {
        if (queryBuffer != NULL) {
            ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
        }
        return status;
    }

    PSYSTEM_PROCESS_INFORMATION processInfo = (PSYSTEM_PROCESS_INFORMATION)queryBuffer;
    while (TRUE) {
        requiredSize += sizeof(SWISSKNIFE_PROCESS_ENTRY);
        if (processInfo->NextEntryOffset == 0) {
            break;
        }
        processInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)processInfo + processInfo->NextEntryOffset);
    }

    if (OutputBufferLength < requiredSize) {
        *BytesWritten = requiredSize;
        ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlZeroMemory(Buffer, OutputBufferLength);
    PSWISSKNIFE_PROCESS_ENTRY outEntry = (PSWISSKNIFE_PROCESS_ENTRY)Buffer;
    ULONG remaining = OutputBufferLength;

    processInfo = (PSYSTEM_PROCESS_INFORMATION)queryBuffer;
    while (TRUE) {
        if (remaining < sizeof(SWISSKNIFE_PROCESS_ENTRY)) {
            ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
            return STATUS_BUFFER_TOO_SMALL;
        }

        outEntry->ProcessId = (ULONG)(ULONG_PTR)processInfo->UniqueProcessId;
        outEntry->ParentProcessId = (ULONG)(ULONG_PTR)processInfo->InheritedFromUniqueProcessId;
        outEntry->ThreadCount = processInfo->NumberOfThreads;
        outEntry->CreateTime = processInfo->CreateTime.QuadPart;
        outEntry->UserTime = processInfo->UserTime.QuadPart;
        outEntry->KernelTime = processInfo->KernelTime.QuadPart;

        if (processInfo->ImageName.Buffer != NULL && processInfo->ImageName.Length > 0) {
            ULONG copyLength = min(processInfo->ImageName.Length, sizeof(outEntry->ImageName) - sizeof(WCHAR));
            RtlZeroMemory(outEntry->ImageName, sizeof(outEntry->ImageName));
            RtlCopyMemory(outEntry->ImageName, processInfo->ImageName.Buffer, copyLength);
            outEntry->ImageName[copyLength / sizeof(WCHAR)] = UNICODE_NULL;
        } else {
            (void)RtlStringCchCopyW(outEntry->ImageName, RTL_NUMBER_OF(outEntry->ImageName), L"<unnamed>");
        }

        remaining -= sizeof(SWISSKNIFE_PROCESS_ENTRY);
        outEntry++;

        if (processInfo->NextEntryOffset == 0) {
            break;
        }

        processInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)processInfo + processInfo->NextEntryOffset);
    }

    *BytesWritten = requiredSize;
    ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
    return STATUS_SUCCESS;
}

static NTSTATUS
SwissKnifeEnumerateModules(
    _Out_writes_bytes_(OutputBufferLength) PVOID Buffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesWritten
    )
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    ULONG bufferSize = 0x20000;
    PVOID queryBuffer = NULL;

    *BytesWritten = 0;

    do {
        if (queryBuffer != NULL) {
            ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
            queryBuffer = NULL;
        }

        queryBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, bufferSize, SWISSKNIFE_POOL_TAG);
        if (queryBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        status = ZwQuerySystemInformation(SystemModuleInformation, queryBuffer, bufferSize, &bufferSize);
    } while (status == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(status)) {
        if (queryBuffer != NULL) {
            ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
        }
        return status;
    }

    PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)queryBuffer;
    ULONG requiredSize = modules->NumberOfModules * sizeof(SWISSKNIFE_MODULE_ENTRY);

    if (OutputBufferLength < requiredSize) {
        *BytesWritten = requiredSize;
        ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlZeroMemory(Buffer, OutputBufferLength);
    PSWISSKNIFE_MODULE_ENTRY outEntry = (PSWISSKNIFE_MODULE_ENTRY)Buffer;

    for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
        PRTL_PROCESS_MODULE_INFORMATION module = &modules->Modules[i];
        outEntry[i].BaseAddress = (ULONGLONG)(ULONG_PTR)module->ImageBase;
        outEntry[i].ImageSize = module->ImageSize;
        outEntry[i].Flags = module->Flags;
        outEntry[i].LoadOrderIndex = module->LoadOrderIndex;

        if (module->FullPathName[0] != '\0') {
            ANSI_STRING ansiName;
            UNICODE_STRING unicodeName;
            RtlInitAnsiString(&ansiName, (PCSZ)module->FullPathName);
            RtlZeroMemory(outEntry[i].ImageName, sizeof(outEntry[i].ImageName));
            unicodeName.Buffer = outEntry[i].ImageName;
            unicodeName.MaximumLength = sizeof(outEntry[i].ImageName);
            unicodeName.Length = 0;
            (void)RtlAnsiStringToUnicodeString(&unicodeName, &ansiName, FALSE);
        }
    }

    *BytesWritten = requiredSize;
    ExFreePoolWithTag(queryBuffer, SWISSKNIFE_POOL_TAG);
    return STATUS_SUCCESS;
}

static NTSTATUS
SwissKnifeHandleFileRead(
    _In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_(OutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesWritten
    )
{
    PAGED_CODE();

    *BytesWritten = 0;

    if (InputBuffer == NULL || OutputBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (InputBufferLength < sizeof(SWISSKNIFE_FILE_REQUEST)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PSWISSKNIFE_FILE_REQUEST request = (PSWISSKNIFE_FILE_REQUEST)InputBuffer;

    if (request->PathLength < sizeof(WCHAR) || request->PathLength > (InputBufferLength - FIELD_OFFSET(SWISSKNIFE_FILE_REQUEST, Path))) {
        return STATUS_INVALID_PARAMETER;
    }

    if (OutputBufferLength < sizeof(SWISSKNIFE_FILE_RESPONSE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    UNICODE_STRING path;
    path.Length = (USHORT)(request->PathLength - sizeof(WCHAR));
    path.MaximumLength = (USHORT)request->PathLength;
    path.Buffer = request->Path;

    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    IO_STATUS_BLOCK ioStatus = { 0 };
    HANDLE fileHandle = NULL;
    NTSTATUS status = ZwCreateFile(&fileHandle,
                                   FILE_GENERIC_READ,
                                   &objectAttributes,
                                   &ioStatus,
                                   NULL,
                                   FILE_ATTRIBUTE_NORMAL,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   FILE_OPEN,
                                   FILE_SYNCHRONOUS_IO_NONALERT,
                                   NULL,
                                   0);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    PUCHAR output = (PUCHAR)OutputBuffer;
    PSWISSKNIFE_FILE_RESPONSE response = (PSWISSKNIFE_FILE_RESPONSE)output;
    ULONG maxPayload = OutputBufferLength - FIELD_OFFSET(SWISSKNIFE_FILE_RESPONSE, Data);
    ULONG toRead = min(request->ReadLength, maxPayload);

    LARGE_INTEGER byteOffset;
    byteOffset.QuadPart = request->Offset;

    status = ZwReadFile(fileHandle,
                        NULL,
                        NULL,
                        NULL,
                        &ioStatus,
                        response->Data,
                        toRead,
                        &byteOffset,
                        NULL);

    ZwClose(fileHandle);

    if (status == STATUS_END_OF_FILE) {
        response->BytesRead = 0;
        *BytesWritten = sizeof(SWISSKNIFE_FILE_RESPONSE);
        return STATUS_SUCCESS;
    }

    if (!NT_SUCCESS(status)) {
        return status;
    }

    response->BytesRead = (ULONG)ioStatus.Information;
    *BytesWritten = FIELD_OFFSET(SWISSKNIFE_FILE_RESPONSE, Data) + response->BytesRead;
    return STATUS_SUCCESS;
}

static NTSTATUS
SwissKnifeHandleRegistryQuery(
    _In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_(OutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesWritten
    )
{
    PAGED_CODE();

    *BytesWritten = 0;

    if (InputBuffer == NULL || OutputBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (InputBufferLength < sizeof(SWISSKNIFE_REGISTRY_REQUEST)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PSWISSKNIFE_REGISTRY_REQUEST request = (PSWISSKNIFE_REGISTRY_REQUEST)InputBuffer;

    ULONG minLength = FIELD_OFFSET(SWISSKNIFE_REGISTRY_REQUEST, Buffer) + request->KeyPathLength + request->ValueNameLength;
    if (request->KeyPathLength < sizeof(WCHAR) ||
        request->ValueNameLength < sizeof(WCHAR) ||
        minLength > InputBufferLength) {
        return STATUS_INVALID_PARAMETER;
    }

    PWCHAR valueBuffer = (PWCHAR)((PUCHAR)request->Buffer + request->KeyPathLength);

    UNICODE_STRING keyPath;
    keyPath.Length = (USHORT)(request->KeyPathLength - sizeof(WCHAR));
    keyPath.MaximumLength = (USHORT)request->KeyPathLength;
    keyPath.Buffer = request->Buffer;

    UNICODE_STRING valueName;
    valueName.Length = (USHORT)(request->ValueNameLength - sizeof(WCHAR));
    valueName.MaximumLength = (USHORT)request->ValueNameLength;
    valueName.Buffer = valueBuffer;

    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, &keyPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    HANDLE keyHandle = NULL;
    NTSTATUS status = ZwOpenKey(&keyHandle, KEY_QUERY_VALUE, &attributes);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ULONG querySize = 0;
    status = ZwQueryValueKey(keyHandle, &valueName, KeyValueFullInformation, NULL, 0, &querySize);
    if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_BUFFER_OVERFLOW) {
        ZwClose(keyHandle);
        return status;
    }

    PKEY_VALUE_FULL_INFORMATION keyInfo = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePoolWithTag(PagedPool, querySize, SWISSKNIFE_POOL_TAG);
    if (keyInfo == NULL) {
        ZwClose(keyHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = ZwQueryValueKey(keyHandle, &valueName, KeyValueFullInformation, keyInfo, querySize, &querySize);
    ZwClose(keyHandle);

    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(keyInfo, SWISSKNIFE_POOL_TAG);
        return status;
    }

    ULONG payloadLength = keyInfo->DataLength;
    ULONG requiredLength = FIELD_OFFSET(SWISSKNIFE_REGISTRY_RESPONSE, Data) + payloadLength;

    if (OutputBufferLength < requiredLength) {
        *BytesWritten = requiredLength;
        ExFreePoolWithTag(keyInfo, SWISSKNIFE_POOL_TAG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    PSWISSKNIFE_REGISTRY_RESPONSE response = (PSWISSKNIFE_REGISTRY_RESPONSE)OutputBuffer;
    response->Type = keyInfo->Type;
    response->DataLength = payloadLength;
    if (payloadLength > 0) {
        RtlCopyMemory(response->Data, ((PUCHAR)keyInfo) + keyInfo->DataOffset, payloadLength);
    }

    *BytesWritten = requiredLength;
    ExFreePoolWithTag(keyInfo, SWISSKNIFE_POOL_TAG);
    return STATUS_SUCCESS;
}

static NTSTATUS
SwissKnifeHandleRandomBytes(
    _In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_(OutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesWritten
    )
{
    PAGED_CODE();

    *BytesWritten = 0;

    if (InputBuffer == NULL || OutputBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (InputBufferLength < sizeof(SWISSKNIFE_RANDOM_REQUEST) ||
        OutputBufferLength < sizeof(SWISSKNIFE_RANDOM_RESPONSE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PSWISSKNIFE_RANDOM_REQUEST request = (PSWISSKNIFE_RANDOM_REQUEST)InputBuffer;
    PSWISSKNIFE_RANDOM_RESPONSE response = (PSWISSKNIFE_RANDOM_RESPONSE)OutputBuffer;

    ULONG maxPayload = OutputBufferLength - FIELD_OFFSET(SWISSKNIFE_RANDOM_RESPONSE, Data);
    ULONG toGenerate = min(request->Length, maxPayload);

    ULONG seed = request->Seed;
    if (seed == 0) {
        LARGE_INTEGER time = KeQueryUnbiasedInterruptTime();
        seed = (ULONG)(time.LowPart ^ time.HighPart);
    }

    ULONG workingSeed = seed;
    for (ULONG i = 0; i < toGenerate; ++i) {
        ULONG value = RtlRandomEx(&workingSeed);
        response->Data[i] = (UCHAR)(value & 0xFF);
    }

    response->SeedUsed = seed;
    response->BytesGenerated = toGenerate;
    *BytesWritten = FIELD_OFFSET(SWISSKNIFE_RANDOM_RESPONSE, Data) + toGenerate;

    return STATUS_SUCCESS;
}

