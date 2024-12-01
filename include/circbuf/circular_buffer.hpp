#ifndef CIRCBUF_CIRCULAR_BUFFER_HPP
#define CIRCBUF_CIRCULAR_BUFFER_HPP

#include "circbuf/detail/raw_buffer.hpp"
#include "circbuf/error.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace circbuf
{
    template <typename T>
    concept CircularBufferElement = std::movable<T> or std::copyable<T>;

    enum class BufferResizePolicy
    {
        DiscardOld,
        DiscardNew,
    };

    // only apply when BufferCapacityPolicy == FixedCapacity and BufferStorePolicy == ReplaceOnFull
    enum class BufferInsertPolicy
    {
        DiscardHead,    // discard the head element when the buffer is full
        DiscardTail,    // discard the tail element when the buffer is full
    };

    enum class BufferPolicy
    {
        ReplaceOnFull,    // fixed capacity, push_back replace head, push_front replace tail
        ThrowOnFull,      // fixed capacity, throw on full
    };

    template <CircularBufferElement T>
    class CircularBuffer
    {
    public:
        template <bool IsConst>
        class [[nodiscard]] Iterator;    // random access iterator

        friend class Iterator<false>;
        friend class Iterator<true>;

        using Element = T;

        // STL compatibility/compliance [breaking my style, big sad...]
        using value_type      = Element;
        using iterator        = Iterator<false>;
        using const_iterator  = Iterator<true>;
        using pointer         = T*;
        using const_pointer   = const T*;
        using reference       = T&;
        using const_reference = const T&;
        using size_type       = std::size_t;

        CircularBuffer() = default;
        ~CircularBuffer() { clear(); };

        CircularBuffer(std::size_t capacity, BufferPolicy policy = BufferPolicy::ReplaceOnFull);

        CircularBuffer(CircularBuffer&& other) noexcept;
        CircularBuffer& operator=(CircularBuffer&& other) noexcept;

        CircularBuffer(const CircularBuffer& other)
            requires std::copyable<T>;
        CircularBuffer& operator=(const CircularBuffer& other)
            requires std::copyable<T>;

        BufferPolicy& policy() noexcept { return m_policy; }

        void swap(CircularBuffer& other) noexcept;
        void clear() noexcept;

        void resize(std::size_t new_capacity, BufferResizePolicy policy = BufferResizePolicy::DiscardOld);

        T& insert(std::size_t pos, T&& value, BufferInsertPolicy policy = BufferInsertPolicy::DiscardHead);
        T  remove(std::size_t pos);

        T& push_front(const T& value);
        T& push_front(T&& value);
        T& push_back(const T& value);
        T& push_back(T&& value);
        T  pop_front();
        T  pop_back();

        CircularBuffer& linearize() noexcept;

        // copied buffer will have the policy set to the parameter
        [[nodiscard]] CircularBuffer linearize_copy(BufferPolicy policy) const noexcept
            requires std::copyable<T>;

        std::size_t size() const noexcept;
        std::size_t capacity() const noexcept { return m_buffer.size(); }

        std::span<T>       data();
        std::span<const T> data() const;

        auto&       at(std::size_t pos);
        const auto& at(std::size_t pos) const;

        auto&       front();
        const auto& front() const;

        auto&       back();
        const auto& back() const;

        bool empty() const { return size() == 0; }
        bool full() const { return size() == capacity(); }
        bool linearized() const { return m_head == 0; };

        auto begin() noexcept { return Iterator<false>(this, 0); }
        auto begin() const noexcept { return Iterator<true>(this, 0); }

        auto end() noexcept { return Iterator<false>(this, npos); }
        auto end() const noexcept { return Iterator<true>(this, npos); }

        Iterator<true> cbegin() const noexcept { return begin(); }
        Iterator<true> cend() const noexcept { return begin(); }

    private:
        static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

        detail::RawBuffer<T> m_buffer = {};
        std::size_t          m_head   = 0;
        std::size_t          m_tail   = npos;
        BufferPolicy         m_policy = {};

        std::size_t increment(std::size_t& index);
        std::size_t decrement(std::size_t& index);
    };
}

// -----------------------------------------------------------------------------
// implementation detail
// -----------------------------------------------------------------------------

namespace circbuf
{
    template <CircularBufferElement T>
    CircularBuffer<T>::CircularBuffer(std::size_t capacity, BufferPolicy policy)
        : m_buffer{ capacity }
        , m_head{ 0 }
        , m_tail{ capacity == 0 ? npos : 0 }
        , m_policy{ policy }
    {
    }

    template <CircularBufferElement T>
    CircularBuffer<T>::CircularBuffer(const CircularBuffer& other)
        requires std::copyable<T>
        : m_buffer{ other.capacity() }
        , m_head{ 0 }
        , m_tail{ other.size() }
        , m_policy{ other.m_policy }
    {
        std::size_t pos = 0;
        for (const auto& copy : other) {
            m_buffer.construct(pos++, T{ copy });    // copy performed here
        }
    }

    template <CircularBufferElement T>
    CircularBuffer<T>& CircularBuffer<T>::operator=(const CircularBuffer& other)
        requires std::copyable<T>
    {
        if (this == &other) {
            return *this;
        }

        clear();

        m_buffer = detail::RawBuffer<T>{ other.capacity() };
        m_head   = 0;
        m_tail   = other.size();
        m_policy = other.m_policy;

        std::size_t pos = 0;
        for (const auto& copy : other) {
            m_buffer.construct(pos++, T{ copy });    // copy performed here
        }

        return *this;
    }

    template <CircularBufferElement T>
    CircularBuffer<T>::CircularBuffer(CircularBuffer&& other) noexcept
        : m_buffer{ std::exchange(other.m_buffer, {}) }
        , m_head{ std::exchange(other.m_head, 0) }
        , m_tail{ std::exchange(other.m_tail, npos) }
        , m_policy{ std::exchange(other.m_policy, {}) }
    {
    }

    template <CircularBufferElement T>
    CircularBuffer<T>& CircularBuffer<T>::operator=(CircularBuffer&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        clear();

        m_buffer = std::exchange(other.m_buffer, {});
        m_head   = std::exchange(other.m_head, 0);
        m_tail   = std::exchange(other.m_tail, npos);
        m_policy = std::exchange(other.m_policy, {});

        return *this;
    }

    template <CircularBufferElement T>
    void CircularBuffer<T>::swap(CircularBuffer& other) noexcept
    {
        std::swap(m_buffer, other.m_buffer);
        std::swap(m_head, other.m_head);
        std::swap(m_tail, other.m_tail);
        std::swap(m_policy, other.m_policy);
    }

    template <CircularBufferElement T>
    void CircularBuffer<T>::clear() noexcept
    {
        for (std::size_t i = 0; i < size(); ++i) {
            m_buffer.destroy((m_head + i) % capacity());
        }

        m_head = 0;
        m_tail = 0;
    }

    // TODO: add condition when
    // - size < capacity && size < new_capacity
    // - size < capacity && size > new_capacity
    template <CircularBufferElement T>
    void CircularBuffer<T>::resize(std::size_t new_capacity, BufferResizePolicy policy)
    {
        if (new_capacity == 0) {
            clear();
            m_tail = npos;
            return;
        }

        if (new_capacity == capacity()) {
            return;
        }

        if (empty()) {
            m_buffer = detail::RawBuffer<T>{ new_capacity };
            m_head   = 0;
            m_tail   = 0;

            return;
        }

        if (new_capacity > capacity()) {
            detail::RawBuffer<T> buffer{ new_capacity };
            for (std::size_t i = 0; i < size(); ++i) {
                auto idx = (m_head + i) % capacity();
                buffer.construct(i, std::move(m_buffer.at(idx)));
                m_buffer.destroy(i);
            }

            m_tail   = m_tail == npos ? capacity() : (m_tail + capacity() - m_head) % capacity();
            m_buffer = std::move(buffer);
            m_head   = 0;

            return;
        }

        auto buffer = detail::RawBuffer<T>{ new_capacity };
        auto count  = size();
        auto offset = count <= new_capacity ? 0ul : count - new_capacity;

        switch (policy) {
        case BufferResizePolicy::DiscardOld: {
            auto begin = (m_head + offset) % capacity();
            for (std::size_t i = 0; i < std::min(new_capacity, count); ++i) {
                auto idx = (begin + i) % capacity();
                buffer.construct(i, std::move(m_buffer.at(idx)));
                m_buffer.destroy(idx);
            }
        } break;
        case BufferResizePolicy::DiscardNew: {
            auto end = m_tail == npos ? m_head : m_tail;
            end      = (end + capacity() - offset) % capacity();
            for (auto i = std::min(new_capacity, count); i-- > 0;) {
                end = (end + capacity() - 1) % capacity();
                buffer.construct(i, std::move(m_buffer.at(end)));
                m_buffer.destroy(end);
            }
        } break;
        }

        m_buffer = std::move(buffer);
        m_head   = 0;
        m_tail   = count <= new_capacity ? count : npos;
    }

    template <CircularBufferElement T>
    T& CircularBuffer<T>::insert(std::size_t pos, T&& value, BufferInsertPolicy policy)
    {
        if (capacity() == 0) {
            throw error::ZeroCapacity{ "Can't push to a buffer with zero capacity" };
        }

        if (m_tail == npos and m_policy == BufferPolicy::ThrowOnFull) {
            throw error::BufferFull{ capacity() };
        }

        if (m_tail == npos) {
            switch (policy) {
            case BufferInsertPolicy::DiscardHead: pop_front(); break;
            case BufferInsertPolicy::DiscardTail: pop_back(); break;
            }
        }
        pos = (m_head + pos) % capacity();

        // TODO: each element shifted towards the right, but it can be optimized to conditionally shift to the
        // left or to the right depending on the position

        auto current = m_tail;
        T*   element = nullptr;

        if (pos != m_tail) {
            auto prev = current;
            m_buffer.construct(current, std::move(m_buffer.at(decrement(prev))));
            current = prev;

            while (current != pos) {
                auto prev            = current;
                m_buffer.at(current) = std::move(m_buffer.at(decrement(prev)));
                current              = prev;
            }
            element = &(m_buffer.at(current) = std::move(value));
        } else {
            element = &(m_buffer.construct(current, std::move(value)));
        }

        if (increment(m_tail) == m_head) {
            m_tail = npos;
        }

        return *element;
    }

    template <CircularBufferElement T>
    T CircularBuffer<T>::remove(std::size_t pos)
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }

        if (pos >= size()) {
            throw error::OutOfRange{ "Cannot remove at index greater than or equal to size", pos, size() };
        }

        const auto count   = size() - pos - 1;
        auto       realpos = (m_head + pos) % capacity();
        auto       value   = std::move(m_buffer.at(realpos));

        for (std::size_t i = 0; i < count; ++i) {
            auto current         = (realpos + i) % capacity();
            auto next            = (realpos + i + 1) % capacity();
            m_buffer.at(current) = std::move(m_buffer.at(next));
        }

        m_buffer.destroy((realpos + count) % capacity());

        if (m_tail == npos) {
            m_tail = m_head;
        }
        decrement(m_tail);

        return value;
    }

    template <CircularBufferElement T>
    T& CircularBuffer<T>::push_front(const T& value)
    {
        return push_front(T{ value });    // copy made here
    }

    template <CircularBufferElement T>
    T& CircularBuffer<T>::push_front(T&& value)
    {
        if (capacity() == 0) {
            throw error::ZeroCapacity{ "Can't push to a buffer with zero capacity" };
        }

        if (m_tail == npos and m_policy == BufferPolicy::ThrowOnFull) {
            throw error::BufferFull{ capacity() };
        }

        auto current = m_head == 0 ? capacity() - 1 : m_head - 1;

        if (m_tail != npos) {
            m_buffer.construct(current, std::move(value));
            m_head = current;
            if (current == m_tail) {
                m_tail = npos;
            }
        } else {
            m_buffer.at(current) = std::move(value);
            m_head               = current;
        }

        return m_buffer.at(current);
    }

    template <CircularBufferElement T>
    T& CircularBuffer<T>::push_back(const T& value)
    {
        return push_back(T{ value });    // copy made here
    };

    template <CircularBufferElement T>
    T& CircularBuffer<T>::push_back(T&& value)
    {
        if (capacity() == 0) {
            throw error::ZeroCapacity{ "Can't push to a buffer with zero capacity" };
        }

        if (m_tail == npos and m_policy == BufferPolicy::ThrowOnFull) {
            throw error::BufferFull{ capacity() };
        }

        auto current = m_head;

        // this branch only taken when the buffer is not full
        if (m_tail != npos) {
            current = m_tail;
            m_buffer.construct(current, std::move(value));    // new entry -> construct
            if (increment(m_tail) == m_head) {
                m_tail = npos;
            }
        } else {
            current              = m_head;
            m_buffer.at(current) = std::move(value);    // already existing entry -> assign
            increment(m_head);
        }

        return m_buffer.at(current);
    }

    template <CircularBufferElement T>
    T CircularBuffer<T>::pop_front()
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }

        auto value = std::move(m_buffer.at(m_head));
        m_buffer.destroy(m_head);

        if (m_tail == npos) {
            m_tail = m_head;
        }
        increment(m_head);

        return value;
    }

    template <CircularBufferElement T>
    T CircularBuffer<T>::pop_back()
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }

        auto index = m_tail == npos ? (m_head == 0 ? capacity() - 1 : m_head - 1)
                                    : (m_tail == 0 ? capacity() - 1 : m_tail - 1);

        auto value = std::move(m_buffer.at(index));
        m_buffer.destroy(index);

        m_tail = index;

        return value;
    }

    template <CircularBufferElement T>
    CircularBuffer<T>& CircularBuffer<T>::linearize() noexcept
    {
        if (linearized() or empty()) {
            return *this;
        }

        if (full()) {
            std::rotate(m_buffer.data(), m_buffer.data() + m_head, m_buffer.data() + capacity());
            return *this;
        }

        auto prev_size = size();

        if (m_head < m_tail or m_tail == 0)
        // - the initialized memory is contiguous, the uninitialized memory is split between them
        // - the uninitialized memory is at the beginning of the buffer
        {
            // we can go straight to moving the initialized memory to the beginning of the buffer
            std::size_t start = 0;
            std::size_t end   = m_tail == 0 ? capacity() : m_tail;
            for (std::size_t i = m_head; i < end; ++i) {
                m_buffer.construct(start++, std::move(m_buffer.at(i)));
                m_buffer.destroy(i);
            }

            m_head = 0;
            m_tail = prev_size;
        } else
        // - the uninitialized memory is contiguous, the initialized memory is split between them
        {
            // TODO: find out more efficient way to do this

            // we need to move the uninitialized memory "hole" to the end of the buffer first
            auto uninit_start = m_tail;
            auto uninit_size  = capacity() - prev_size;

            for (std::size_t i = m_head; i < capacity(); ++i) {
                m_buffer.construct(uninit_start++, std::move(m_buffer.at(i)));
                m_buffer.destroy(i);
            }

            std::rotate(m_buffer.data(), m_buffer.data() + m_head - uninit_size, m_buffer.data() + prev_size);

            m_head = 0;
            m_tail = prev_size;
        }

        return *this;
    }

    template <CircularBufferElement T>
    CircularBuffer<T> CircularBuffer<T>::linearize_copy(BufferPolicy policy) const noexcept
        requires std::copyable<T>
    {
        auto copy     = CircularBuffer{ *this };
        copy.m_policy = policy;

        return copy;
    }

    template <CircularBufferElement T>
    std::size_t CircularBuffer<T>::size() const noexcept
    {
        return m_tail == npos ? capacity() : (m_tail + capacity() - m_head) % capacity();
    }

    template <CircularBufferElement T>
    std::span<T> CircularBuffer<T>::data()
    {
        if (not linearized() and not full()) {
            throw error::NotLinearizedNotFull{ "Reading the data will lead to undefined behavior" };
        }
        return { m_buffer.data(), size() };
    }

    template <CircularBufferElement T>
    std::span<const T> CircularBuffer<T>::data() const
    {
        if (not linearized() and not full()) {
            throw error::NotLinearizedNotFull{ "Reading the data will lead to undefined behavior" };
        }
        return { m_buffer.data(), size() };
    }

    template <CircularBufferElement T>
    auto& CircularBuffer<T>::at(std::size_t pos)
    {
        if (pos >= size()) {
            throw error::OutOfRange{ "Can't access element outside of the range", pos, size() };
        }

        auto realpos = (m_head + pos) % capacity();
        return m_buffer.at(realpos);
    }

    template <CircularBufferElement T>
    const auto& CircularBuffer<T>::at(std::size_t pos) const
    {
        if (pos >= size()) {
            throw error::OutOfRange{ "Can't access element outside of the range", pos, size() };
        }

        auto realpos = (m_head + pos) % capacity();
        return m_buffer.at(realpos);
    }

    template <CircularBufferElement T>
    auto& CircularBuffer<T>::front()
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }
        return at(0);
    }

    template <CircularBufferElement T>
    const auto& CircularBuffer<T>::front() const
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }
        return at(0);
    }

    template <CircularBufferElement T>
    auto& CircularBuffer<T>::back()
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }
        return at(size() - 1);
    }

    template <CircularBufferElement T>
    const auto& CircularBuffer<T>::back() const
    {
        if (empty()) {
            throw error::BufferEmpty{ capacity() };
        }
        return at(size() - 1);
    }

    template <CircularBufferElement T>
    std::size_t CircularBuffer<T>::increment(std::size_t& index)
    {
        if (++index == capacity()) {
            index = 0;
        }
        return index;
    }

    template <CircularBufferElement T>
    std::size_t CircularBuffer<T>::decrement(std::size_t& index)
    {
        if (index-- == 0) {
            index = capacity() - 1;
        }
        return index;
    }

    template <CircularBufferElement T>
    template <bool IsConst>
    class CircularBuffer<T>::Iterator
    {
    public:
        // STL compatibility/compliance [breaking my style, big sad...]
        using iterator          = Iterator<false>;
        using const_iterator    = Iterator<true>;
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const value_type*, value_type*>;
        using reference         = std::conditional_t<IsConst, const value_type&, value_type&>;

        using BufferPtr = std::conditional_t<IsConst, const CircularBuffer*, CircularBuffer*>;

        Iterator() noexcept                      = default;
        Iterator(const Iterator&)                = default;
        Iterator& operator=(const Iterator&)     = default;
        Iterator(Iterator&&) noexcept            = default;
        Iterator& operator=(Iterator&&) noexcept = default;

        Iterator(BufferPtr buffer, std::size_t current) noexcept
            : m_buffer{ buffer }
            , m_index{ current }
            , m_size{ buffer->size() }
        {
        }

        // for const iterator construction from iterator
        Iterator(Iterator<false>& other)
            : m_buffer{ other.m_buffer }
            , m_index{ other.m_index }
            , m_size{ other.m_size }
        {
        }

        // just a pointer comparison
        auto operator<=>(const Iterator&) const = default;
        bool operator==(const Iterator&) const  = default;

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
            if (m_buffer == nullptr or m_index == CircularBuffer::npos) {
                throw error::OutOfRange{ "Iterator is out of range", m_index, m_size };
            }
            return m_buffer->at(m_index);
        };

        pointer operator->() const
        {
            if (m_buffer == nullptr or m_index == CircularBuffer::npos) {
                throw error::OutOfRange{ "Iterator is out of range", m_index, m_size };
            }

            return &m_buffer->at(m_index);
        };

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
}

#endif /* end of include guard: CIRCBUF_CIRCULAR_BUFFER_HPP */
