# Mastering I/O Efficiency in C++ with Custom Buffered Readers and Writers

## Introduction  
When it comes to handling input/output (I/O) operations in C++, efficiency is king. Whether you’re reading data from a file, streaming bytes over a network, or writing logs to disk, the way you manage I/O can make or break your application’s performance. While the C++ Standard Library offers tools like `std::ifstream` and `std::ofstream`, sometimes you need more control—or a deeper understanding of what’s happening under the hood. That’s where custom buffered I/O classes come in. In this article, we’ll explore two powerful custom classes—`SyncIOReadBuffer` and `SyncIOLazyWriteBuffer`—designed to optimize reading and writing in C++. We’ll break down how they work, why they matter, and how you can use them to level up your I/O game.

## Why Buffered I/O Matters  
I/O operations, like reading from a file or writing to a network socket, are notoriously slow compared to in-memory operations. Every direct call to the underlying system (a "system call") involves overhead—context switching, kernel interaction, and sometimes waiting for hardware. Buffering tackles this problem by reducing the number of system calls. Instead of reading or writing one byte at a time, a buffer collects data in memory and processes it in larger chunks. This simple idea can dramatically boost performance, especially for high-throughput tasks like streaming or file processing.

Enter `SyncIOReadBuffer` and `SyncIOLazyWriteBuffer`—two custom C++ classes that implement buffered I/O with a twist. They use circular buffers and lazy writing to minimize system calls, offering flexibility and efficiency for real-world applications. Let’s dive into each one.

## Meet `SyncIOReadBuffer`: The Buffered Reader  
`SyncIOReadBuffer` is a templated C++ class that reads data into a circular buffer, serving it to the caller in a synchronous manner. It’s designed to handle continuous data streams—like network packets or file contents—while keeping system calls to a minimum.

### How It Works  
- **Circular Buffer:** The class uses a fixed-size memory block (allocated with `malloc`) as a circular buffer. Data is read into this buffer from an `IOInterface` (a `std::function` callback), and the `m_tail` and `m_head` pointers track the read and write positions. When the buffer wraps around, it reuses space efficiently without reallocation.
-   **Key Methods:**
    -   `read`: Fetches a specified number of bytes from the buffer. If there’s not enough data, it calls `paste` to fetch more from the source.
    -   `readUntil`: Reads until a specific condition (e.g., a character like `'\n'` or a custom predicate) is met, perfect for parsing delimited data.
    -   `paste`: Fills the buffer from the `IOInterface`, handling fragmentation when the buffer wraps around.
- **Efficiency:** By caching data in the buffer, it reduces direct system calls. For example, reading 1KB in one go and serving 10-byte chunks from memory is far faster than 100 separate 10-byte reads.

### Code Example  
Here’s how you might use `SyncIOReadBuffer` to read a string from a mock data source:

```cpp
#include <iostream>
#include <string>
#include "SyncIOReadBuffer.h"

int main() {
    auto dataSource = [](char* buf, size_t len) {
        static std::string data = "Hello\nWorld!";
        static size_t pos = 0;
        size_t toCopy = std::min(len, data.length() - pos);
        std::copy(data.begin() + pos, data.begin() + pos + toCopy, buf);
        pos += toCopy;
        return toCopy;
    };

    SyncIOReadBuffer<size_t> buffer(10);
    char output[20];
    size_t bytesRead = buffer.readUntil(output, dataSource, '\n');
    std::string result(output, bytesRead);
    std::cout << "Read: " << result << std::endl; // Outputs: "Hello\n"
    return 0;
}
```

This example reads until a newline, caching data in the buffer and serving it efficiently.

### Why It’s Great
-   Flexibility: The IOInterface callback lets you plug in any data source—files, sockets, or even mock data for testing.
-   Customizable Enders: With an overload of readUntil accepting a std::function<bool(char)>, you can define complex stopping conditions beyond single characters.
-   Performance: The circular buffer minimizes memory allocation and system calls, making it ideal for streaming applications.

## Enter SyncIOLazyWriteBuffer: The Lazy Writer
While SyncIOReadBuffer excels at reading, SyncIOLazyWriteBuffer takes the stage for writing. This class buffers data in memory and writes it out lazily—only when the buffer is full or explicitly flushed—reducing the frequency of write operations.

### How It Works
-   Circular Buffer:
    -   Like its reader counterpart, it uses a circular buffer to store data before writing. The m_tail and m_head pointers manage the write position and flush point.
-   Lazy Writing:
    -   The write method buffers data with put until the buffer is full, then calls flush to write it out via a DataWriter callback (a std::function).
-   Key Methods:
    -   write: Adds data to the buffer, flushing if necessary when space runs out.
    -   flush: Writes all buffered data to the destination and resets the buffer.
-   **Efficiency:** By batching writes, it reduces system calls. For instance, instead of writing 100 small strings individually, it collects them and writes in one go.

### Code Example
Here’s how you might use SyncIOLazyWriteBuffer to write data to a mock sink:

```cpp
#include <iostream>
#include <string>
#include "SyncIOLazyWriteBuffer.h"

int main() {
    std::string output;
    auto dataSink = [&output](char* buf, size_t len) {
        output.append(buf, len);
    };

    SyncIOLazyWriteBuffer<size_t> buffer(10, dataSink);
    const char* data = "Hello, World!";
    buffer.write(data, strlen(data));
    std::cout << "Buffered: " << output << std::endl; // Outputs nothing yet
    buffer.flush();
    std::cout << "Flushed: " << output << std::endl; // Outputs: "Hello, World!"
    return 0;
}
```

This shows how data stays in the buffer until flushed, optimizing write operations.

## Why It’s Great
-   Lazy Efficiency: Writing only when necessary cuts down on system calls, boosting throughput for high-volume writes.
-   Customizable Sink: The DataWriter callback supports any destination—files, networks, or even in-memory strings.
-   Automatic Cleanup: The destructor calls flush, ensuring no data is lost when the object goes out of scope.

### Benefits of Buffered I/O in Practice
-   Reduced System Calls: Both classes minimize direct I/O operations, improving performance for slow devices like disks or networks.
-   Memory Efficiency: Circular buffers reuse memory without frequent allocations, keeping resource usage low.
-   Streaming Support: Perfect for continuous data flows, like video streaming or real-time logging.
-   Flexibility: Templated size types and functional callbacks make them adaptable to various use cases.
-   Limitations to Consider
-   Thread Safety: Neither class is thread-safe by default. If multiple threads access them, you’ll need to add synchronization (e.g., mutexes).
-   Latency Trade-Off: Buffering can introduce slight delays, which might matter in ultra-low-latency scenarios.
-   Error Handling: They don’t handle I/O errors natively—you’d need a wrapper or additional logic for robustness.

## Putting It All Together
Imagine a real-world scenario: streaming log data from a server to a file. `SyncIOReadBuffer` could read incoming network packets, while `SyncIOLazyWriteBuffer` batches and writes them to disk. This duo reduces system calls on both ends, ensuring smooth, efficient data flow.

But let’s dive into a practical performance test that showcases the power of these classes. Consider a sample problem:  
- **Problem Statement:** Read from the console (redirected from a file) where the first line contains a number `n` (number of test cases), followed by `n` lines, each with two numbers. For each test case, output the larger number to the console (redirected to a file).  
- **Input Example:**  
100000<br>
1 1742111530<br>
1742111529 1742111530<br>
...
- **Goal:** Compare the performance of standard `std::cin`/`std::cout` (default I/O) against `SyncIOReadBuffer` and `SyncIOLazyWriteBuffer` (buffered I/O) with a 1 KB buffer.

### Default I/O Implementation  
Here’s how you might solve this using standard I/O (`DefaultIOTest.cpp`):

```cpp
#include <iostream>
#include <string>
#include <chrono>

int main() {
  auto start = std::chrono::high_resolution_clock::now();
  {
      uint32_t numTestCases;
      std::cin >> numTestCases;

      for (uint32_t i = 0; i < numTestCases; ++i) {
          uint32_t n1, n2;
          std::cin >> n1 >> n2;
          std::cout << ((n1 > n2) ? n1 : n2) << std::endl;
      }
  }
  auto duration = std::chrono::high_resolution_clock::now() - start;

  char endingBuff[1024];
  char* currWriteHead = endingBuff;
  std::string durationStr = std::to_string(
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / (double)1000000000) + " s";
  currWriteHead += sprintf(currWriteHead, "Duration: %s", durationStr.c_str());
  std::cout.write(endingBuff, currWriteHead - endingBuff + 1);

  return 0;
}
```

### Buffered I/O Implementation
Now, here’s the same problem solved using our custom buffered classes with a configurable buffer size passed as a command-line argument (SmartIOTest.cpp):

```cpp
#include <iostream>
#include <string>
#include <chrono>
#include "SmartBuffer.hpp"

int main(int argc, char** argv) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        auto io_console_reader = [](char* out, const uint32_t len) {
            std::cin.read(out, len);
            return static_cast<uint32_t>(std::cin.gcount());
        };

        auto io_console_writer = [](char* out, const uint32_t len) {
            std::cout.write(out, len);
            return len;
        };

        uint32_t buffSize = atoll(argv[1]);
        SyncIOReadBuffer<uint32_t> smartReadBuffer(buffSize);
        SyncIOLazyWriteBuffer<uint32_t> smartWriteBuffer(buffSize, io_console_writer);

        char input[128];
        smartReadBuffer.readUntil(input, io_console_reader, '\n');
        uint32_t numTestCases;
        sscanf(input, "%u", &numTestCases);

        for (uint32_t i = 0; i < numTestCases; ++i) {
            smartReadBuffer.readUntil(input, io_console_reader, '\n');
            uint32_t n1, n2;
            sscanf(input, "%u %u", &n1, &n2);
            char out[128];
            auto len = sprintf(out, "%u\n", n1 > n2 ? n1 : n2);
            smartWriteBuffer.write(out, len);
        }
    }
    auto duration = std::chrono::high_resolution_clock::now() - start;

    char endingBuff[1024];
    char* currWriteHead = endingBuff;
    std::string durationStr = std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / (double)1000000000) + " s";
    currWriteHead += sprintf(currWriteHead, "Duration: %s", durationStr.c_str());
    std::cout.write(endingBuff, currWriteHead - endingBuff + 1);

    return 0;
}
```

### Performance Test Setup

### To measure real-world performance:

#### Input File (input.txt):
Contains 100,000 test cases, starting with 100000 followed by pairs like 1742111529 1742111530.
Command: Both programs were run with input redirected from input.txt and output to a file. For the buffered version: SmartIOTest.exe 1024 < input.txt > SmartIOTestResults.txt, setting the buffer size to 1 KB (1024 bytes) for both reader and writer.

#### Methodology:
Each implementation was executed 100 times, and the average runtime was calculated based on the duration printed at the end of each run.

#### Results:
Default I/O (std::cin/std::cout): Average runtime over 100 runs: 0.470058 seconds.<br>
Buffered I/O (SyncIOReadBuffer/SyncIOLazyWriteBuffer with 1 KB buffer): Average runtime over 100 runs: 0.0537578 seconds.

#### Speedup:
Buffered I/O is approximately 8.75x faster than default I/O for this workload!

#### Why Buffered I/O Wins:
With 100,000 test cases, default I/O makes frequent system calls for every cin >> and cout <<, leading to significant overhead.
In contrast, the buffered approach using a 1 KB buffer to batch reads (SyncIOReadBuffer), reducing the number of calls to std::cin.read.
Lazily writes output in 1 KB chunks (SyncIOLazyWriteBuffer), minimizing std::cout.write calls.
Even with a modest 1 KB buffer, this batching slashes system call overhead, delivering a massive performance boost.
Try It Yourself
Grab the input.txt with 100,000 test cases, compile both programs, and run them with redirection. For SmartIOTest.exe, pass the buffer size as an argument (e.g., 1024 for 1 KB). Experiment with different sizes—say, 512, 2048, or 4096 bytes—to see how it impacts performance on your system. The difference is night and day!

### Specs of the machine and os used to run tests:
#### Hardware:
RAM :       8 GB DDR5<br>
Processor:  Intel(R) Core(TM) i5-7300U CPU @2.60GHz 2.70 GHz, 64 bit

#### OS:
Windows:    10 Pro<br>
Version     22H2<br>
OS build:   19045.5608

### Conclusion
SyncIOReadBuffer and SyncIOLazyWriteBuffer are powerful tools for mastering I/O efficiency in C++. By leveraging circular buffers and lazy writing, they reduce system calls, optimize memory usage, and support high-throughput applications with ease. The performance test above—8.75x faster for 100,000 test cases with just a 1 KB buffer—proves their worth in real-world scenarios. While they require careful handling for thread safety and error management, their flexibility and speed make them a must-try for I/O-heavy tasks.

So, next time you’re tackling a performance-critical I/O problem in C++, give these classes a spin. Experiment with buffer sizes, test them against your own workloads, and share your results with the community. Happy coding!

## Limitations/shortfalls to consider:
-   Recursive methods:
    -   The "read" method is recursive, it can potentially cause stack overflow when the read size is several
        size the buffer size
-   Error Handling:
    -   They don’t handle I/O errors natively—you’d need a wrapper or additional logic for robustness.The client code functions has to kake care if the IOInterface throws/returns errors.
   That may lead to addtional effort to handle errors
-   Ease of use:
    -   The interface is not as user friendly as "<<" and ">>" operators with cin and cout, the user of the class has to conert numeric types to strings
   in order to use this class. Other alternative is to write a wrapper class with "<<" and ">>" operators, and implement the string conversion logic
   there.
-   Thread Safety:
    -   Neither class is thread-safe by default. If multiple threads access them, you’ll need to add synchronization (e.g., mutexes/worker threads).
-   Latency Trade-Off:
    -   Buffering can introduce slight delays, which might matter in ultra-low-latency scenarios.
                   The "lazy" write approach in the "SyncIOLazyWriteBuffer" class writes to IO-Interface only when the buffer is full, this will yield high throughput but can add to latency as a side-effect
                   This is fine fot applications like file transfer etc., but can impact real-time applications like audio/video streamimng.
                   The user of this call can mitigate this by regularly calling **flush()** methi\od when the buffer has reached certain size

## Github:
    https://github.com/sumitdhyani/buffered_io.git