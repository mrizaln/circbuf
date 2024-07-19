#include <algorithm>
#include <concepts>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

#ifndef CIRCULAR_BUFFER_ENABLE_THROW
#    define CIRCULAR_BUFFER_ENABLE_THROW 0
#else
#    undef CIRCULAR_BUFFER_ENABLE_THROW
#    define CIRCULAR_BUFFER_ENABLE_THROW 1
#endif

namespace circbuf
{
    template <typename T>
        requires std::default_initializable<T> and std::movable<T>
    class CircularBuffer
    {
    public:
        template <bool isConst = false>
        class Iterator;

        friend class Iterator<false>;
        friend class Iterator<true>;

        enum class ResizePolicy
        {
            DISCARD_OLD,
            DISCARD_NEW,
        };

        static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

        explicit CircularBuffer(std::size_t capacity) noexcept
            : m_buffer{ std::make_unique<T[]>(capacity) }
            , m_capacity{ capacity }
        {
        }

        CircularBuffer(CircularBuffer&&) noexcept            = default;
        CircularBuffer& operator=(CircularBuffer&&) noexcept = default;

        CircularBuffer(const CircularBuffer& other) noexcept
            requires std::copyable<T>
            : m_buffer{ std::make_unique<T[]>(other.m_capacity) }
            , m_capacity{ other.m_capacity }
            , m_begin{ other.m_begin }
            , m_end{ other.m_end }
        {
            std::copy(other.m_buffer.get(), other.m_buffer.get() + other.m_capacity, m_buffer.get());
        }

        CircularBuffer& operator=(CircularBuffer other) noexcept
            requires std::copyable<T>
        {
            swap(other);
            return *this;
        }

        [[nodiscard]] std::size_t        capacity() const noexcept { return m_capacity; }
        [[nodiscard]] std::span<const T> buf() const noexcept { return { m_buffer.get(), capacity() }; }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_end == npos ? capacity() : (m_end + capacity() - m_begin) % capacity();
        }

        T& at(std::size_t pos)
        {
#if CIRCULAR_BUFFER_ENABLE_THROW
            if (pos >= size()) {
                throw std::out_of_range{ "Pos index is out of range of the CircularBuffer" };
            }
#endif
            auto realpos = toAbsolutePos(pos);
            return m_buffer[realpos];
        }

        const T& at(std::size_t pos) const
        {
#if CIRCULAR_BUFFER_ENABLE_THROW
            if (pos >= size()) {
                throw std::out_of_range{ "Pos index is out of range of the CircularBuffer" };
            }
#endif
            auto realpos = toAbsolutePos(pos);
            return m_buffer[realpos];
        }

        Iterator<> push(T&& value) noexcept
        {
            auto current = m_begin;

            // this branch only taken when the buffer is not full
            if (m_end != npos) {
                current           = m_end;
                m_buffer[current] = std::move(value);
                if (++m_end == capacity()) {
                    m_end = 0;
                }
                if (m_end == m_begin) {
                    m_end = npos;
                }
            } else {
                current           = m_begin;
                m_buffer[current] = std::move(value);
                if (++m_begin == capacity()) {
                    m_begin = 0;
                }
            }

            return { this, toRelativePos(current) };
        }

        [[nodiscard]] std::optional<T> pop() noexcept
        {
            if (size() == 0) {
                return std::nullopt;
            }

            std::optional<T> value{ std::in_place, std::move(m_buffer[m_begin]) };
            if (m_end == npos) {
                m_end = m_begin;
            }
            if (++m_begin == capacity()) {
                m_begin = 0;
            }

            return value;
        }

        CircularBuffer& linearize() noexcept
        {
            if (m_begin == 0 && m_end == npos) {
                return *this;
            }

            std::rotate(m_buffer.get(), m_buffer.get() + m_begin, m_buffer.get() + capacity());
            m_end = m_end != npos ? (m_end + capacity() - m_begin) % capacity() : npos;

            m_begin = 0;

            return *this;
        }

        [[nodiscard]] CircularBuffer linearizeCopy() const noexcept
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
        void resize(std::size_t newCapacity, ResizePolicy policy = ResizePolicy::DISCARD_OLD) noexcept
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

        [[nodiscard]] Iterator<>     begin() noexcept { return { this, 0 }; }
        [[nodiscard]] Iterator<>     end() noexcept { return { this, npos }; }
        [[nodiscard]] Iterator<true> begin() const noexcept { return { this, 0 }; }
        [[nodiscard]] Iterator<true> end() const noexcept { return { this, npos }; }
        [[nodiscard]] Iterator<true> cbegin() const noexcept { return { this, 0 }; }
        [[nodiscard]] Iterator<true> cend() const noexcept { return { this, npos }; }

    private:
        std::size_t toAbsolutePos(std::size_t pos) const noexcept { return (pos + m_begin) % capacity(); }
        std::size_t toRelativePos(std::size_t pos) const noexcept
        {
            return (pos + capacity() - m_begin) % capacity();
        }

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
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using const_pointer     = const T*;
        using reference         = T&;
        using const_reference   = const T&;

        // internal use
        using BufferPtr = std::conditional_t<IsConst, const CircularBuffer*, CircularBuffer*>;

        Iterator() noexcept                      = default;
        Iterator(const Iterator&)                = default;
        Iterator& operator=(const Iterator&)     = default;
        Iterator(Iterator&&) noexcept            = default;
        Iterator& operator=(Iterator&&) noexcept = default;

        Iterator(BufferPtr buffer, std::size_t index)
            : m_buffer{ buffer }
            , m_index{ index }
            , m_size{ buffer->size() }
        {
        }

        // special constructor for const iterator from non-const iterator
        Iterator(Iterator<false>& other)
            requires IsConst
            : m_buffer{ other.m_buffer }
            , m_index{ other.m_index }
        {
        }

        auto operator<=>(const Iterator&) const = default;

        Iterator& operator+=(difference_type n)
        {
            // casted n possibly become very large if it was negative, but when it was added to m_pos, m_pos
            // will wraparound anyway since it was unsigned
            m_index += static_cast<std::size_t>(n);

            // detect wrap-around then set to sentinel value
            if (m_index >= m_size) {
                m_index = CircularBuffer::npos;
            }

            return *this;
        }

        Iterator& operator-=(difference_type n)
        {
            if (m_index == CircularBuffer::npos) {
                m_index = m_size;
            }
            return (*this) += -n;
        }

        Iterator& operator++() { return (*this) += 1; }
        Iterator& operator--() { return (*this) -= 1; }

        Iterator operator++(int)
        {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        Iterator operator--(int)
        {
            auto copy = *this;
            --(*this);
            return copy;
        }

        reference operator*() const
        {
#if CIRCULAR_BUFFER_ENABLE_THROW
            if (m_buffer == nullptr || m_index == CircularBuffer::npos) {
                throw std::out_of_range{ "Out of bound access: CircularBuffer iterator has reached the end" };
            }
#endif
            return m_buffer->at(m_index);
        }

        pointer operator->() const
        {
#if CIRCULAR_BUFFER_ENABLE_THROW
            if (m_buffer == nullptr || m_index == CircularBuffer::npos) {
                throw std::out_of_range{ "Out of bound access: CircularBuffer iterator has reached the end" };
            }
#endif
            return &m_buffer->at(m_index);
        }

        reference operator[](difference_type n) const { return *(*this + n); }

        friend Iterator operator+(const Iterator& lhs, difference_type n) { return Iterator{ lhs } += n; }
        friend Iterator operator+(difference_type n, const Iterator& rhs) { return rhs + n; }
        friend Iterator operator-(const Iterator& lhs, difference_type n) { return Iterator{ lhs } -= n; }

        friend difference_type operator-(const Iterator& lhs, const Iterator& rhs)
        {
            auto lpos = lhs.m_index == CircularBuffer::npos ? lhs.m_size : lhs.m_index;
            auto rpos = rhs.m_index == CircularBuffer::npos ? rhs.m_size : rhs.m_index;

            return static_cast<difference_type>(lpos) - static_cast<difference_type>(rpos);
        }

    private:
        BufferPtr   m_buffer = nullptr;
        std::size_t m_index  = CircularBuffer::npos;
        std::size_t m_size   = 0;
    };

    static_assert(std::forward_iterator<CircularBuffer<int>::Iterator<false>>);
    static_assert(std::forward_iterator<CircularBuffer<int>::Iterator<true>>);
}
