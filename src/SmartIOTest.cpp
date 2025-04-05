#include <iostream>
#include <string>
#include <chrono>
#include "SmartBuffer.hpp"

int main(int argc, char** argv)
{
    auto start = std::chrono::high_resolution_clock().now();
    {
      auto io_console_reader = 
      [](char* out, const uint32_t len)
      {
          std::cin.read(out, len);
          return std::cin.gcount();
      };

      auto io_console_writer = 
      [](char* out, const uint32_t len)
      {
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

      for (uint32_t i = 0; i < numTestCases; ++i)
      {
        char out[128];
        uint32_t n1, n2;
        smartReadBuffer.readUntil(input, io_console_reader, '\n');
        sscanf(input, "%u %u", &n1, &n2);
        auto len = sprintf(out, "%u\n", n1 > n2? n1 : n2);
        smartWriteBuffer.write(out, len);
      }
    }
    auto duration = std::chrono::high_resolution_clock().now() - start;

    char endingBuff[1024];
    char* currWriteHead = endingBuff;
    std::string durationStr = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()/(double)1000000000) + " s"; 
    currWriteHead += sprintf(currWriteHead, "Duration: %s", durationStr.c_str());
    std::cout.write(endingBuff, currWriteHead - endingBuff + 1);

    return 0;
}