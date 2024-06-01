#include <algorithm>
#include <concepts>
#include <limits>
#include <memory>
#include <optional>
#include <span>

#ifndef CIRCULAR_BUFFER_ENABLE_ITERATOR_THROW
#    define CIRCULAR_BUFFER_ENABLE_ITERATOR_THROW 0
#else
#    undef CIRCULAR_BUFFER_ENABLE_ITERATOR_THROW
#    define CIRCULAR_BUFFER_ENABLE_ITERATOR_THROW 1
#endif

template <typename T>
    requires std::default_initializable<T> and std::movable<T>
class CircularBuffer
{
public:
    template <bool isConst = false>
    class Iterator;

    friend class Iterator<false>;
    friend class Iterator<true>;

    // STL compliance
    using value_type = T;

    enum class ResizePolicy
    {
        DISCARD_OLD,
        DISCARD_NEW,
    };

    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    CircularBuffer(std::size_t capacity) noexcept
        : m_buffer{ std::make_unique<T[]>(capacity) }
        , m_capacity{ capacity }
    {
    }

    CircularBuffer(CircularBuffer&&) noexcept            = default;
    CircularBuffer& operator=(CircularBuffer&&) noexcept = default;

    CircularBuffer(const CircularBuffer& other)
        requires std::copyable<T>
        : m_buffer{ std::make_unique<T[]>(other.m_capacity) }
        , m_capacity{ other.m_capacity }
        , m_begin{ other.m_begin }
        , m_end{ other.m_end }
    {
        std::copy(other.m_buffer.get(), other.m_buffer.get() + other.m_capacity, m_buffer.get());
    }

    CircularBuffer& operator=(CircularBuffer other)
        requires std::copyable<T>
    {
        swap(other);
        return *this;
    }

    std::size_t        capacity() const noexcept { return m_capacity; }
    std::span<const T> buf() const noexcept { return { m_buffer.get(), capacity() }; }

    std::size_t size() const noexcept
    {
        return m_end == npos ? capacity() : (m_end + capacity() - m_begin) % capacity();
    }

    Iterator<> push(T&& event) noexcept
    {
        auto current = m_begin;

        // this branch only taken when the buffer is not full
        if (m_end != npos) {
            current           = m_end;
            m_buffer[current] = std::move(event);
            if (++m_end == capacity()) {
                m_end = 0;
            }
            if (m_end == m_begin) {
                m_end = npos;
            }
        } else {
            current           = m_begin;
            m_buffer[current] = std::move(event);
            if (++m_begin == capacity()) {
                m_begin = 0;
            }
        }

        return { this, current };
    }

    std::optional<T> pop() noexcept
    {
        if (size() == 0) {
            return std::nullopt;
        }

        std::optional<T> event{ std::in_place, std::move(m_buffer[m_begin]) };
        if (m_end == npos) {
            m_end = m_begin;
        }
        if (++m_begin == capacity()) {
            m_begin = 0;
        }

        return event;
    }

    CircularBuffer& linearize()
    {
        if (m_begin == 0 && m_end == npos) {
            return *this;
        }

        std::rotate(m_buffer.get(), m_buffer.get() + m_begin, m_buffer.get() + capacity());
        m_end = m_end != npos ? (m_end + capacity() - m_begin) % capacity() : npos;

        m_begin = 0;

        return *this;
    }

    CircularBuffer linearizeCopy() const
        requires std::copyable<T>
    {
        CircularBuffer result{ m_capacity };
        std::rotate_copy(
            m_buffer.get(), m_buffer.get() + m_begin, m_buffer.get() + capacity(), result.m_buffer.get()
        );

        result.m_end   = m_end != npos ? (m_end + m_capacity - m_begin) % m_capacity : npos;
        result.m_begin = 0;

        return result;
    }

    void reset() noexcept
    {
        m_begin = 0;
        m_end   = 0;
    }

    // reset the pointers and clear the buffer (set all elements to T{})
    void clear() noexcept
    {
        m_begin = 0;
        m_end   = 0;

        if constexpr (std::is_trivially_default_constructible_v<T>) {
            std::fill(m_buffer.get(), m_buffer.get() + m_capacity, T{});
        } else {
            for (std::size_t i = 0; i < m_capacity; ++i) {
                m_buffer[i] = T{};
            }
        }
    }

    void swap(CircularBuffer& other) noexcept
    {
        std::swap(m_buffer, other.m_buffer);
        std::swap(m_capacity, other.m_capacity);
        std::swap(m_begin, other.m_begin);
        std::swap(m_end, other.m_end);
    }

    // ResizePolicy only used when newCapacity < capacity()
    void resize(std::size_t newCapacity, ResizePolicy policy = ResizePolicy::DISCARD_OLD)
    {
        if (newCapacity == capacity()) {
            return;
        }

        if (size() == 0) {
            m_buffer   = std::make_unique<T[]>(newCapacity);
            m_capacity = newCapacity;
            m_begin    = 0;
            m_end      = 0;

            return;
        }

        if (newCapacity > capacity()) {
            const auto b = [buf = m_buffer.get()](std::size_t offset) {
                return std::move_iterator{ buf + offset };
            };

            auto buffer = std::make_unique<T[]>(newCapacity);
            std::rotate_copy(b(0), b(m_begin), b(capacity()), buffer.get());
            m_buffer   = std::move(buffer);
            m_end      = m_end == npos ? capacity() : (m_end + capacity() - m_begin) % capacity();
            m_begin    = 0;
            m_capacity = newCapacity;

            return;
        }

        auto buffer = std::make_unique<T[]>(newCapacity);
        auto count  = size();
        auto offset = count <= newCapacity ? 0ul : count - newCapacity;

        switch (policy) {
        case ResizePolicy::DISCARD_OLD: {
            auto begin = (m_begin + offset) % capacity();
            for (std::size_t i = 0; i < std::min(newCapacity, count); ++i) {
                buffer[i] = std::move(m_buffer[(begin + i) % capacity()]);
            }
        } break;
        case ResizePolicy::DISCARD_NEW: {
            auto end = m_end == npos ? m_begin : m_end;
            end      = (end + capacity() - offset) % capacity();
            for (std::size_t i = std::min(newCapacity, count); i-- > 0;) {
                end       = (end + capacity() - 1) % capacity();
                buffer[i] = std::move(m_buffer[end]);
            }
        } break;
        }

        m_buffer   = std::move(buffer);
        m_capacity = newCapacity;
        m_begin    = 0;
        m_end      = count <= newCapacity ? count : npos;
    }

    Iterator<>     begin() noexcept { return { this, m_begin }; }
    Iterator<>     end() noexcept { return { this, m_end }; }
    Iterator<true> begin() const noexcept { return { this, m_begin }; }
    Iterator<true> end() const noexcept { return { this, m_end }; }
    Iterator<true> cbegin() const noexcept { return { this, m_begin }; }
    Iterator<true> cend() const noexcept { return { this, m_end }; }

private:
    std::unique_ptr<T[]> m_buffer;
    std::size_t          m_capacity = 0;
    std::size_t          m_begin    = 0;
    std::size_t          m_end      = 0;
};

template <typename T>
    requires std::default_initializable<T> and std::movable<T>
template <bool IsConst>
class CircularBuffer<T>::Iterator
{
public:
    // STL compliance
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using const_pointer     = const T*;
    using reference         = T&;
    using const_reference   = const T&;

    // internal use
    using Buffer = std::conditional_t<IsConst, const CircularBuffer, CircularBuffer>;
    using Value  = std::conditional_t<IsConst, const T, T>;

    Iterator(Buffer* bufferPtr, std::size_t index)
        : m_bufferPtr{ bufferPtr }
        , m_index{ index }
    {
    }

    // special constructor for const iterator from non-const iterator
    Iterator(Iterator<false>& other)
        requires IsConst
        : m_bufferPtr{ other.m_bufferPtr }
        , m_index{ other.m_index }
    {
    }

    Iterator() noexcept                      = default;
    Iterator(const Iterator&)                = default;
    Iterator& operator=(const Iterator&)     = default;
    Iterator(Iterator&&) noexcept            = default;
    Iterator& operator=(Iterator&&) noexcept = default;

    Iterator& operator++()
    {
        if (m_index == Buffer::npos) {
            return *this;
        }

        if (++m_index == m_bufferPtr->capacity()) {
            m_index = 0;
        }

        if (m_index == m_index_original) {
            m_index = Buffer::npos;
        }

        return *this;
    }

    Iterator operator++(int) const
    {
        Iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    Value& operator*() const
    {
#if CIRCULAR_BUFFER_ENABLE_ITERATOR_THROW
        if (m_index == Buffer::npos) {
            throw std::out_of_range{ "out of bound access; iterator has reached the end" };
        }
#endif

        return m_bufferPtr->m_buffer[m_index];
    };

    Value* operator->() const
    {
#if CIRCULAR_BUFFER_ENABLE_ITERATOR_THROW
        if (m_index == Buffer::npos) {
            throw std::out_of_range{ "out of bound access; iterator has reached the end" };
        }
#endif

        return &m_bufferPtr->m_buffer[m_index];
    };

    template <bool IsConst2>
    bool operator==(const Iterator<IsConst2>& other) const
    {
        return m_bufferPtr == other.m_bufferPtr && m_index == other.m_index;
    }

    template <bool IsConst2>
    bool operator!=(const Iterator<IsConst2>& other) const
    {
        return !(*this == other);
    }

    operator std::size_t() { return m_index; };

private:
    Buffer*     m_bufferPtr = nullptr;
    std::size_t m_index     = npos;

    // workaround for when m_bufferPtr->m_begin == m_bufferPtr->m_end && size() == capacity()
    std::size_t m_index_original = m_index;
};

static_assert(std::forward_iterator<CircularBuffer<int>::Iterator<false>>);
static_assert(std::forward_iterator<CircularBuffer<int>::Iterator<true>>);
