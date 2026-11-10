// Rizg Yaw kernel driver.
//
// Purpose: give the external cheat a way to read and write the memory of a
// single user-mode process (the Rust client) that the driver was explicitly
// attached to. The driver does nothing else: no file system filter, no
// registry hook, no network code, and no access to any process other than the
// one attached through IOCTL_RZ_ATTACH.
//
// The protocol is described in ../src/driver_protocol.hpp and is shared with
// the user-mode client. Every request is buffered, so a request and its answer
// travel through a single system buffer.
//
// Building: Windows Driver Kit. Open driver/RizgYaw.vcxproj in Visual Studio
// with the "Windows Driver Kit" component installed, or use the WDK command
// line:
//
//   cl /c /kernel /GS- /Zl /W4 RizgYaw.c
//   link /driver /entry:DriverEntry /subsystem:native RizgYaw.obj
//
// The resulting RizgYaw.sys must be signed (WHQL or an attestation signature)
// before it loads on a current Windows release. For local testing enable test
// mode (bcdedit /set testsigning on) and install a self-signed certificate.

#include <ntddk.h>
#include <ntstrsafe.h>

#define RZ_DEVICE_NAME     L"\\Device\\RizgYaw"
#define RZ_DOS_NAME        L"\\DosDevices\\RizgYaw"
#define RZ_POOL_TAG        'zRyR'

// Kept in sync with kingware::driver in ../src/driver_protocol.hpp.
#define RZ_IOCTL_TYPE              0x8000u
#define RZ_OP_ATTACH               1
#define RZ_OP_DETACH               2
#define RZ_OP_READ                 3
#define RZ_OP_WRITE                4
#define RZ_OP_MODULE_BASE          5
#define RZ_OP_PING                 6

#define RZ_IOCTL(op) (CTL_CODE(RZ_IOCTL_TYPE, (op), METHOD_BUFFERED, FILE_ANY_ACCESS))

typedef struct _RZ_ATTACH_REQUEST {
    ULONG ProcessId;
    ULONG Reserved;
} RZ_ATTACH_REQUEST, *PRZ_ATTACH_REQUEST;

typedef struct _RZ_MEMORY_REQUEST {
    ULONGLONG Address;
    ULONG Size;
    ULONG Reserved;
} RZ_MEMORY_REQUEST, *PRZ_MEMORY_REQUEST;

typedef struct _RZ_MODULE_REQUEST {
    WCHAR ModuleName[64];
} RZ_MODULE_REQUEST, *PRZ_MODULE_REQUEST;

typedef struct _RZ_STATUS_RESPONSE {
    LONG Status;
    ULONG Bytes;
    ULONGLONG Address;
    ULONG DriverVersion;
} RZ_STATUS_RESPONSE, *PRZ_STATUS_RESPONSE;

typedef struct _RZ_DRIVER_STATE {
    PEPROCESS Process;      // attached target, referenced
    HANDLE ProcessHandle;   // kernel handle used for the copy operations
} RZ_DRIVER_STATE, *PRZ_DRIVER_STATE;

static RZ_DRIVER_STATE g_state;
static FAST_MUTEX g_lock;
static volatile LONG g_attached;

static NTSTATUS RzAttach(PRZ_ATTACH_REQUEST request)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING path;
    HANDLE handle = NULL;

    if (request->ProcessId == 0) return STATUS_INVALID_PARAMETER;

    // The path is only a placeholder for the object attributes; the process
    // is selected by its identifier.
    RtlInitUnicodeString(&path, L"");
    InitializeObjectAttributes(&attributes, &path, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = ZwOpenProcess(&handle,
                           PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                           &attributes,
                           (HANDLE)(ULONG_PTR)request->ProcessId);
    if (!NT_SUCCESS(status)) return status;

    ExAcquireFastMutex(&g_lock);
    if (g_state.Process != NULL) {
        ObDereferenceObject(g_state.Process);
        g_state.Process = NULL;
    }
    if (g_state.ProcessHandle != NULL) {
        ZwClose(g_state.ProcessHandle);
        g_state.ProcessHandle = NULL;
    }
    g_state.ProcessHandle = handle;
    g_state.Process = PsGetProcessFromProcessHandle(handle);
    if (g_state.Process != NULL) {
        ObReferenceObject(g_state.Process);
    }
    InterlockedExchange(&g_attached, g_state.Process != NULL ? 1 : 0);
    ExReleaseFastMutex(&g_lock);

    return g_state.Process != NULL ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
}

static NTSTATUS RzDetach(void)
{
    ExAcquireFastMutex(&g_lock);
    if (g_state.Process != NULL) {
        ObDereferenceObject(g_state.Process);
        g_state.Process = NULL;
    }
    if (g_state.ProcessHandle != NULL) {
        ZwClose(g_state.ProcessHandle);
        g_state.ProcessHandle = NULL;
    }
    InterlockedExchange(&g_attached, 0);
    ExReleaseFastMutex(&g_lock);
    return STATUS_SUCCESS;
}

// Finds the base address of a loaded module of the attached process.
static NTSTATUS RzModuleBase(PRZ_MODULE_REQUEST request, ULONGLONG* base)
{
    NTSTATUS status;
    ULONG length = 0;
    PRTL_PROCESS_MODULE_INFORMATION modules = NULL;
    PRTL_PROCESS_MODULE_INFORMATION current = NULL;
    ULONG index = 0;
    SIZE_T allocation = 0;

    *base = 0;

    status = ZwQuerySystemInformation(SystemModuleInformation, NULL, 0, &length);
    if (status != STATUS_INFO_LENGTH_MISMATCH || length == 0) return status;

    allocation = length + 64 * 1024;
    modules = ExAllocatePool2(POOL_FLAG_PAGED, allocation, RZ_POOL_TAG);
    if (modules == NULL) return STATUS_INSUFFICIENT_RESOURCES;

    status = ZwQuerySystemInformation(SystemModuleInformation, modules, (ULONG)allocation, &length);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(modules, RZ_POOL_TAG);
        return status;
    }

    current = RtlImageDirectoryEntryToData(modules, RTL_IMAGE_DIRECTORY_ENTRY_IN_LOADED_MODULES, TRUE);
    if (current == NULL) {
        ExFreePoolWithTag(modules, RZ_POOL_TAG);
        return STATUS_NOT_FOUND;
    }

    for (index = 0; index < modules->NumberOfModules; ++index) {
        PRTL_PROCESS_MODULE_INFORMATION entry = &modules->Modules[index];
        UNICODE_STRING name;
        RtlInitUnicodeString(&name, entry->ModuleName);
        if (RtlCompareUnicodeString(&name, &request->ModuleName, TRUE) == 0) {
            *base = (ULONGLONG)(ULONG_PTR)entry->DllBase;
            break;
        }
    }

    ExFreePoolWithTag(modules, RZ_POOL_TAG);
    return *base != 0 ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

static NTSTATUS RzCopy(PRZ_MEMORY_REQUEST request, void* buffer, SIZE_T buffer_size, BOOLEAN to_target)
{
    NTSTATUS status;
    SIZE_T transferred = 0;

    if (request->Address == 0 || request->Size == 0) return STATUS_INVALID_PARAMETER;
    if (request->Size > buffer_size) return STATUS_BUFFER_TOO_SMALL;
    if (InterlockedCompareExchange(&g_attached, 0, 0) == 0) return STATUS_DEVICE_NOT_CONNECTED;

    ExAcquireFastMutex(&g_lock);
    if (g_state.Process == NULL) {
        ExReleaseFastMutex(&g_lock);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    if (to_target) {
        status = MmCopyVirtualMemory(IoGetCurrentProcess(), buffer, g_state.Process,
                                     (PVOID)(ULONG_PTR)request->Address, request->Size,
                                     KernelMode, &transferred);
    } else {
        status = MmCopyVirtualMemory(g_state.Process, (PVOID)(ULONG_PTR)request->Address,
                                     IoGetCurrentProcess(), buffer, request->Size,
                                     KernelMode, &transferred);
    }
    ExReleaseFastMutex(&g_lock);

    if (!NT_SUCCESS(status)) return status;
    return transferred == request->Size ? STATUS_SUCCESS : STATUS_PARTIAL_COPY;
}

static NTSTATUS RZDispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG input_length = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG output_length = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID buffer = irp->AssociatedIrp.SystemBuffer;
    PRZ_STATUS_RESPONSE response = (PRZ_STATUS_RESPONSE)buffer;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    UNREFERENCED_PARAMETER(device);

    if (buffer == NULL || output_length < sizeof(RZ_STATUS_RESPONSE)) {
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(response, sizeof(RZ_STATUS_RESPONSE));
    response->DriverVersion = 1;

    switch (code) {
    case RZ_IOCTL(RZ_OP_ATTACH):
        if (input_length < sizeof(RZ_ATTACH_REQUEST)) { status = STATUS_INVALID_PARAMETER; break; }
        status = RzAttach((PRZ_ATTACH_REQUEST)buffer);
        break;
    case RZ_IOCTL(RZ_OP_DETACH):
        status = RzDetach();
        break;
    case RZ_IOCTL(RZ_OP_READ):
        if (input_length < sizeof(RZ_MEMORY_REQUEST)) { status = STATUS_INVALID_PARAMETER; break; }
        status = RzCopy((PRZ_MEMORY_REQUEST)buffer,
                        (PUCHAR)buffer + sizeof(RZ_MEMORY_REQUEST),
                        output_length - sizeof(RZ_MEMORY_REQUEST), FALSE);
        break;
    case RZ_IOCTL(RZ_OP_WRITE):
        if (input_length < sizeof(RZ_MEMORY_REQUEST) + sizeof(RZ_MEMORY_REQUEST)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = RzCopy((PRZ_MEMORY_REQUEST)buffer,
                        (PUCHAR)buffer + sizeof(RZ_MEMORY_REQUEST),
                        input_length - sizeof(RZ_MEMORY_REQUEST), TRUE);
        break;
    case RZ_IOCTL(RZ_OP_MODULE_BASE):
        if (input_length < sizeof(RZ_MODULE_REQUEST)) { status = STATUS_INVALID_PARAMETER; break; }
        status = RzModuleBase((PRZ_MODULE_REQUEST)buffer, &response->Address);
        break;
    case RZ_IOCTL(RZ_OP_PING):
        status = STATUS_SUCCESS;
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    response->Status = (LONG)status;
    if (code == RZ_IOCTL(RZ_OP_READ) && NT_SUCCESS(status)) {
        response->Bytes = ((PRZ_MEMORY_REQUEST)buffer)->Size;
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = sizeof(RZ_STATUS_RESPONSE);
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS RZCreate(PDRIVER_OBJECT driver, PDEVICE_OBJECT* device)
{
    UNICODE_STRING path;
    NTSTATUS status;

    RtlInitUnicodeString(&path, RZ_DEVICE_NAME);
    status = IoCreateDevice(driver, 0, &path, FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN, FALSE, device);
    if (!NT_SUCCESS(status)) return status;

    (*device)->Flags |= DO_BUFFERED_IO;
    (*device)->Characteristics |= FILE_CLUSTERED;
    return STATUS_SUCCESS;
}

static VOID RZUnload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING link;

    RzDetach();
    IoDeleteDevice(driver->DeviceObject);
    RtlInitUnicodeString(&link, RZ_DOS_NAME);
    IoDeleteSymbolicLink(&link);
    if (g_lock != NULL) {
        ExDeleteFastMutex(g_lock);
        g_lock = NULL;
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path)
{
    PDEVICE_OBJECT device = NULL;
    NTSTATUS status;
    UNICODE_STRING link;
    ULONG index;

    UNREFERENCED_PARAMETER(registry_path);

    g_lock = ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FAST_MUTEX), RZ_POOL_TAG);
    if (g_lock == NULL) return STATUS_INSUFFICIENT_RESOURCES;
    ExInitializeFastMutex(g_lock);

    for (index = 0; index <= IRP_MJ_MAXIMUM_FUNCTION; ++index) {
        driver->MajorFunction[index] = RZDispatch;
    }
    driver->DriverUnload = RZUnload;

    status = RZCreate(driver, &device);
    if (!NT_SUCCESS(status)) {
        ExDeleteFastMutex(g_lock);
        g_lock = NULL;
        return status;
    }

    RtlInitUnicodeString(&link, RZ_DOS_NAME);
    status = IoCreateSymbolicLink(&link, &device->DeviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(device);
        ExDeleteFastMutex(g_lock);
        g_lock = NULL;
        return status;
    }

    device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
