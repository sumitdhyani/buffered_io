#include <iostream>
#include <string>
#include <chrono>

int main()
{
    auto start = std::chrono::high_resolution_clock().now();
    {
      uint32_t numTestCases;
      std::cin >> numTestCases;

      for (uint32_t i = 0; i < numTestCases; ++i)
      {
        uint32_t n1, n2;
        std::cin >> n1 >> n2;
        std::cout << ((n1 > n2)? n1 : n2) << std::endl;
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