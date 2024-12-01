#ifndef CIRCBUF_RAW_BUFFER_HPP
#define CIRCBUF_RAW_BUFFER_HPP

#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

#ifndef CIRCBUF_RAW_BUFFER_DEBUG
#    ifdef NDEBUG
#        define CIRCBUF_RAW_BUFFER_DEBUG 0
#    else
#        define CIRCBUF_RAW_BUFFER_DEBUG 1
#        include <vector>
#        include <algorithm>
#    endif
#endif

namespace circbuf::detail
{
    // an encapsulation of a raw buffer/memory that propagates the constness of the buffer to the elements
    template <typename T>
    class RawBuffer
    {
    public:
        RawBuffer() = default;

        explicit RawBuffer(std::size_t size);
        ~RawBuffer();

        RawBuffer(RawBuffer&& other) noexcept;
        RawBuffer& operator=(RawBuffer&& other) noexcept;

        RawBuffer(const RawBuffer&)            = delete;
        RawBuffer& operator=(const RawBuffer&) = delete;

        template <typename... Ts>
        T& construct(std::size_t offset, Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts...>);

        void destroy(std::size_t offset) noexcept;

        auto*       data() noexcept { return &at(0); }
        const auto* data() const noexcept { return &std::as_const(at(0)); }

        auto&        at(std::size_t pos) & noexcept { return m_data[pos]; }
        auto&&       at(std::size_t pos) && noexcept { return m_data[pos]; }
        const auto&  at(std::size_t pos) const& noexcept { return std::as_const(m_data[pos]); }
        const auto&& at(std::size_t pos) const&& noexcept { return std::as_const(m_data[pos]); }

        std::size_t size() const noexcept { return m_size; }

    private:
        [[no_unique_address]] std::allocator<T> m_allocator = {};

        T*          m_data = nullptr;
        std::size_t m_size = 0;

#if CIRCBUF_RAW_BUFFER_DEBUG
        std::vector<bool> m_constructed = {};
#endif
    };
}

// -----------------------------------------------------------------------------
// implementation detail
// -----------------------------------------------------------------------------

namespace circbuf::detail
{
    template <typename T>
    RawBuffer<T>::RawBuffer(std::size_t size)
        : m_data{ m_allocator.allocate(size) }
        , m_size{ size }
#if CIRCBUF_RAW_BUFFER_DEBUG
        , m_constructed(size, false)
#endif
    {
    }

    template <typename T>
    RawBuffer<T>::~RawBuffer()
    {
        if (m_data == nullptr) {
            return;
        }

#if CIRCBUF_RAW_BUFFER_DEBUG
        assert(
            std::all_of(
                m_constructed.begin(), m_constructed.end(), [](bool constructed) { return !constructed; }
            )
            && "Not all elements are destructed"
        );
#endif

        m_allocator.deallocate(m_data, m_size);
        m_data = nullptr;
    }

    template <typename T>
    RawBuffer<T>::RawBuffer(RawBuffer&& other) noexcept
        : m_data{ std::exchange(other.m_data, nullptr) }
        , m_size{ std::exchange(other.m_size, 0) }
#if CIRCBUF_RAW_BUFFER_DEBUG
        , m_constructed{ std::exchange(other.m_constructed, {}) }
#endif
    {
    }

    template <typename T>
    RawBuffer<T>& RawBuffer<T>::operator=(RawBuffer&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (m_data) {
            m_allocator.deallocate(m_data, m_size);
        }

        m_data = std::exchange(other.m_data, nullptr);
        m_size = std::exchange(other.m_size, 0);

#if CIRCBUF_RAW_BUFFER_DEBUG
        m_constructed = std::exchange(other.m_constructed, {});
#endif

        return *this;
    }

    template <typename T>
    template <typename... Ts>
    T& RawBuffer<T>::construct(
        std::size_t offset,
        Ts&&... args
    ) noexcept(std::is_nothrow_constructible_v<T, Ts...>)
    {
#if CIRCBUF_RAW_BUFFER_DEBUG
        assert(!m_constructed[offset] && "Element not constructed");
        m_constructed[offset] = true;
#endif
        return *std::construct_at(m_data + offset, std::forward<Ts>(args)...);
    }

    template <typename T>
    void RawBuffer<T>::destroy(std::size_t offset) noexcept
    {
#if CIRCBUF_RAW_BUFFER_DEBUG
        assert(m_constructed[offset] && "Element not constructed");
        m_constructed[offset] = false;
#endif
        std::destroy_at(m_data + offset);
    }
}

#endif /* end of include guard: CIRCBUF_RAW_BUFFER_HPP */
