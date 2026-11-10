#pragma once

#include <cstddef>
#include <cstdint>

namespace kingware {

// "NotImplemented" means no check has been performed; it is not an offline
// verdict. A real adapter must report only states that it has actually checked.
enum class DriverState { NotImplemented, Disconnected, Connected, Error };

// Integration boundary for an existing, separately supplied driver. A future
// implementation must define its own device/IOCTL protocol, process binding,
// lifetime and access control. A successful read must fill the entire buffer;
// on failure the caller must discard the buffer. Nothing here opens a device.
class MemoryReader {
public:
    virtual ~MemoryReader() = default;
    virtual DriverState status() const noexcept = 0;
    virtual bool read(std::uint64_t address, void* destination, std::size_t bytes) noexcept = 0;
};

class UnavailableMemoryReader final : public MemoryReader {
public:
    DriverState status() const noexcept override { return DriverState::NotImplemented; }
    bool read(std::uint64_t, void*, std::size_t) noexcept override { return false; }
};

} // namespace kingware
