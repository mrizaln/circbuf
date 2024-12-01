#ifndef CIRCBUF_ERROR_HPP
#define CIRCBUF_ERROR_HPP

#include <stdexcept>
#include <format>

namespace circbuf
{
    struct Error : public std::logic_error
    {
        Error(const std::string& what)
            : std::logic_error(what)
        {
        }
    };
}

namespace circbuf::error
{
    struct BufferFull : public ::circbuf::Error
    {
        BufferFull(std::size_t capacity)
            : Error{ std::format("Buffer is full with capacity {}", capacity) }
        {
        }
    };

    struct BufferEmpty : public ::circbuf::Error
    {
        BufferEmpty(std::size_t capacity)
            : Error{ std::format("Buffer is empty with capacity {}", capacity) }
        {
        }
    };

    struct ZeroCapacity : public ::circbuf::Error
    {
        ZeroCapacity(const std::string& what)
            : Error{ std::format("Capacity must be greater than zero: {}", what) }
        {
        }
    };

    struct OutOfRange : public ::circbuf::Error
    {
        OutOfRange(const std::string& what, std::size_t index, std::size_t size)
            : Error{ std::format("Index {} out of range [0, {}): {}", index, size, what) }
        {
        }
    };

    struct NotLinearizedNotFull : public ::circbuf::Error
    {
        NotLinearizedNotFull(const std::string& what)
            : Error{ std::format("Buffer is not linearized and not full: {}", what) }
        {
        }
    };
}

#endif /* end of include guard: CIRCBUF_ERROR_HPP */
