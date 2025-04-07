# Buffered I/O Library in C++
A custom C++ library for optimizing I/O operations using `SyncIOReadBuffer` and `SyncIOLazyWriteBuffer` classes.

## Features
-   Circular buffer-based I/O
-   Lazy write batching
-   Configurable buffer size

## Build & Run
- **Prerequisites:**
    -   C++20+, g++/clang compiler(MsBuild), CMake 3.5+
    -   **For UnitTests:**
        -   Googletest should be built/installed on the machine
-   **Build with CMake:**
    ### Linux:
    ```bash
        mkdir build
        cd build
        cmake ..
        make
    ```
    ### Windows:
    ```bat
        mkdir build
        cd build
        cmake ..
        MsBuild.exe Project.sln /p:configuration=release
    ```
-   **Run:**
    ### Linux:
    ```bash
    ./SmartIOTest 1024 < ../input.txt > SmartIOTestResult.txt
    ./DefaultIOTest < ../input.txt > DefaultIOTestResult.txt
    ```
    ### Windows:
    ```bat
    cd Release
    SmartIOTest.exe 1024 < ../input.txt > SmartIOTestResult.txt
    DefaultIOTest.exe < ../input.txt > DefaultIOTestResult.txt
    ```

## Performance
Tested with 100k test cases (1 KB buffer):

**Default I/O:**    0.47s
**Buffered I/O:**   0.053s
**Speedup:**        8.75x
**Machine:**        Intel i5-7300U, 8GB RAM, Windows 10 Pro

## Details
Read the full article(docs/TECHNICAL_DETAILS.md)  for implementation details, examples, and limitations.