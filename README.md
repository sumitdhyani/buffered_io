# Mastering I/O Efficiency in C++ with Custom Buffered Readers and Writers

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
Default I/O (std::cin/std::cout): Average runtime over 100 runs: 0.470058 seconds.
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
RAM :       8 GB DDR5
Processor:  Intel(R) Core(TM) i5-7300U CPU @ 2.60GHz   2.70 GHz, 64 bit

#### OS:
Windows:    10 Pro
Version     22H2
OS build:   19045.5608

### Conclusion
SyncIOReadBuffer and SyncIOLazyWriteBuffer are powerful tools for mastering I/O efficiency in C++. By leveraging circular buffers and lazy writing, they reduce system calls, optimize memory usage, and support high-throughput applications with ease. The performance test above—8.75x faster for 100,000 test cases with just a 1 KB buffer—proves their worth in real-world scenarios. While they require careful handling for thread safety and error management, their flexibility and speed make them a must-try for I/O-heavy tasks.

So, next time you’re tackling a performance-critical I/O problem in C++, give these classes a spin. Experiment with buffer sizes, test them against your own workloads, and share your results with the community. Happy coding!

## Scopes of improvement
1. The "read" method is recursive, it can potentially cause stack overflow when the read size is several size the buffer size
2. Doesn't handle errors, the provider of reader and writer functions has to kake care if the IOInterface throws/returns errors.
   That may lead to addtional effort to handle errors
3. The interface is not as user friendly as "<<" and ">>" operators with cin and cout, the user of the class has to conert numeric types to strings
   in order to use this class. Other alternative is to write a wrapper class with "<<" and ">>" operators, and implement the string conversion logic
   there.
