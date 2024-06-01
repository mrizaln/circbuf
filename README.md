# circular-buffer

A simple C++ circular buffer written in C++20

## Usage

> `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(
  circular-buffer
  GIT_REPOSITORY https://github.com/mrizaln/circular-buffer
  GIT_TAG main)
FetchContent_MakeAvailable(circular-buffer)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE circular-buffer)
```

> `main.cpp`

```cpp
#include <circular_buffer.hpp>

#include <iostream>
#include <string>
#include <ranges>

void print(std::string_view prelude, std::ranges::range auto&& range)
{
    std::cout << prelude << "[";
    for (bool first = true; const auto& e : range) {
        if (!first) std::cout << ", ";
        std::cout << (e.empty() ? "<-->" : e);
        first = false;
    }
    std::cout << "]\n";
}

int main()
{
    CircularBuffer<std::string> queues{ 12 };       // you specify the capacity of the buffer at construction (can be resized later)

    for (auto i : std::views::iota(0, 217)) {
        queues.push(std::to_string(i));
    }
    std::optional value = queues.pop();             // pop() returns an optional; pop()-ed value will be in a moved-from state
    if (value.has_value()) {                        // the optional will be null when there's nothing to pop from buffer
        std::cout << "pop: " << *value << '\n';
    }

    // pop again...
    std::ignore = queues.pop();                     // there will be a warning when pop()-ed value is not assigned to a variable

    print("iter      : ", queues);                  // can be iterated (forward only) that iterates from the oldest element to the newest
    print("underlying: ", queues.buf());            // you can see the underlying buffer if you want

    // etc... (see the header for all of the available functionalities)
}
```
