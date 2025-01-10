# circbuf

A simple C++ circular buffer written in C++20

## Usage

> `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(
  circbuf
  GIT_REPOSITORY https://github.com/mrizaln/circbuf
  GIT_TAG main)
FetchContent_MakeAvailable(circbuf)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE circbuf)
```

> `main.cpp`

```cpp
#include <circbuf/circbuf.hpp>

#include <iostream>
#include <ranges>
#include <string>

using circbuf::CircBuf;

int main()
{
    // you specify the capacity of the buffer at construction (can be resized later)
    auto queue = CircBuf<std::string>{ 12 };

    // pushing from the back
    for (auto i : std::views::iota(0, 256)) {
        queue.push_back(std::format("{0:0b}|{0}", i));
    }

    // pushing from the front
    queue.push_front("hello");
    queue.push_front("world");

    // you can iterate the CircBuf from head to tail
    for (const auto& value : queue | std::views::reverse) {    // i reverse the iterator here
        std::cout << value << '\n';
    }

    // CircBuf implements a random access iterator, so you should be able to access it like an array
    auto mid = queue.remove(6);
    std::cout << ">>> mid: " << mid << '\n';

    std::cout << "------------\n";
    for (const auto& value : queue) {
        std::cout << value << '\n';
    }
}
```

### Buffer policy

You can set the behavior of the circular buffer between two options

- `ReplaceOnFull`: replace the head with new value if `push_back` is called and replace the tail with new value if `push_front` is called (default).
- `ThrowOnFull`: this will throw an error when a push/insert is done into a full buffer, you must `pop_front`/`pop_back` first before adding new item.

```cpp
auto buf = CircBuf<int>{ 42 };                              // use ReplaceOnFull by default
buf.policy() = BufferPolicy::ThrowOnFull;                   // change the policy after construction

auto buf2 = CircBuf<int>{ 42, BufferPolicy::ThrowOnFull }   // set to other policy on construction


```

### Accessing underlying buffer

`circbuf::CircBuf` is an array under the hood, so you should be able to see its underlying array. The caveat is that you should only access the underlying buffer if the buffer itself is said to be **_full_** and/or **_linearized_**.

The underlying buffer can be accessed using the member function `data()`, which will return an `std::span`.

> - If you managed to access the non-linearized underlying data or read past the last element of the buffer while the buffer itself is not full, you will read an **uninitialized** memory.
> - In my testing, there will be no assertion error or core dump.
> - That's why I provided a check in the `data()` function to avoid this scenario.
> - The behavior when the check failed is an exception of type `std::logic_error`.

- Full: the number of elements inside the buffer is equal to its capacity

  ```cpp
  int main() {
      auto queue = CircBuf<int>{ 12 };

      for (auto i : std::views::iota(0, 2373)) {
          queue.push_back(i);
      }
      assert(queue.full());

      // if you want the data to be whatever in order in the underlying buffer, then just skip linearize
      auto raw = queue.data();

      // you need to linearize your buffer if you want to access it from head to tail
      auto buf = queue.linearize().data();
  }
  ```

- Linearized: when the head of the buffer is at the start of the buffer, it is said that the buffer is linearized

  ```cpp
  int main() {
      auto queue = CircBuf<int>{ 12 };

      for (auto i : std::ranges::iota(0, 14)) {
          queue.push_back(i);
      }
      assert(not queue.empty());

      auto span = queue.linearize().data();
      assert(queue.linearized());

      queue.pop_front();
      queue.pop_front();

      // linearize() function is in-place, use the copy ctor to make a copy instead (policy inherited)
      auto copy = queue;
      assert(copy.linearized());

      // or linearize_copy() to make a copy instead, you can change the policy here
      auto copy2 = queue.linearize_copy(circbuf::BufferPolicy::ThrowOnFull);
      assert(copy2.linearized());

      assert(copy.policy() == queue.policy());
      assert(copy.policy() != copy2.policy());
  }
  ```
