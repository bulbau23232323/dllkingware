#pragma once

#include <cstddef>
#include <cstdint>

namespace kingware {

// "NotImplemented" means no check has been performed; it is not an offline
// verdict. A real adapter must report only states that it has actually
// checked.
enum class DriverState { NotImplemented, Disconnected, Connected, Error };

// Integration boundary for the memory backend. Two adapters implement it:
// the Rizg Yaw kernel driver (DeviceIoControl against \\.\RizgYaw) and the
// user-mode fallback (OpenProcess + ReadProcessMemory). A successful read
// must fill the entire buffer; on failure the caller must discard the buffer.
class MemoryReader {
public:
    virtual ~MemoryReader() = default;
    virtual DriverState status() const noexcept = 0;
    virtual bool read(std::uint64_t address, void* destination, std::size_t bytes) noexcept = 0;
    virtual bool write(std::uint64_t address, const void* source, std::size_t bytes) noexcept = 0;
    // Human readable reason for the current status, shown in the UI.
    virtual const char* detail() const noexcept { return ""; }
};

class UnavailableMemoryReader final : public MemoryReader {
public:
    DriverState status() const noexcept override { return DriverState::NotImplemented; }
    bool read(std::uint64_t, void*, std::size_t) noexcept override { return false; }
    bool write(std::uint64_t, const void*, std::size_t) noexcept override { return false; }
};

} // namespace kingware
