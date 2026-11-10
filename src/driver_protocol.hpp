#pragma once

#include <cstdint>

// Shared description of the Rizg Yaw kernel driver interface.
//
// The driver is a minimal read/write memory provider for the external game
// client. It exposes exactly three operations: attach to a process, resolve a
// module base address and copy memory to or from the target. Nothing in this
// protocol touches files, the registry, the network or any other process.
//
// Device path (user mode):  \\.\RizgYaw
// Device name (kernel mode): \Device\RizgYaw
// Symbolic link:             \DosDevices\RizgYaw

namespace kingware::driver {

inline constexpr wchar_t kDevicePath[] = L"\\\\.\\RizgYaw";

inline constexpr std::uint32_t kIoctlType = 0x8000u; // FILE_DEVICE_UNKNOWN

enum class Operation : std::uint32_t {
    Attach = 1,        // bind the driver to a target process id
    Detach = 2,        // release the target process
    Read = 3,          // copy memory out of the target
    Write = 4,         // copy memory into the target
    ModuleBase = 5,    // resolve the base address of a loaded module
    Ping = 6,          // health check, returns the driver version
};

inline constexpr std::uint32_t make_control_code(Operation operation) {
    return kIoctlType | (static_cast<std::uint32_t>(operation) << 2);
}

inline constexpr std::uint32_t control_code(Operation operation) {
    return make_control_code(operation);
}

#pragma pack(push, 1)
struct AttachRequest {
    std::uint32_t process_id;
    std::uint32_t reserved;
};

struct DetachRequest {
    std::uint32_t reserved;
};

struct MemoryRequest {
    std::uint64_t address;
    std::uint32_t size;
    std::uint32_t reserved;
};

struct ModuleBaseRequest {
    wchar_t module_name[64];
};

struct StatusResponse {
    std::int32_t status;          // 0 = success, negative = error code
    std::uint32_t bytes;          // bytes transferred for read/write
    std::uint64_t address;        // resolved base for module queries
    std::uint32_t driver_version; // incremented for every protocol change
};
#pragma pack(pop)

// The user-mode side sends the request structure first and receives the
// response structure in the same buffer, which keeps the driver simple and
// makes the protocol easy to audit.
inline constexpr std::uint32_t kDriverVersion = 1;

} // namespace kingware::driver
