#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <cstring>
#include <sstream>
#include "SmartBuffer.hpp"

// Test fixture for common setup
class BufferTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Mock input stream for performance tests
    std::ostringstream oss;
    oss << "3\n1 2\n3 4\n5 6\n"; // Small sample input
    mockInput = oss.str();
  }

  std::string mockInput;
  std::string defaultOutput;
  std::string smartOutput;

  // Mock reader for SmartIOTest
  size_t readPos = 0;
  uint32_t mockReader(char *out, uint32_t len)
  {
    uint32_t toCopy = std::min(len, static_cast<uint32_t>(mockInput.length() - readPos));
    std::memcpy(out, mockInput.c_str() + readPos, toCopy);
    readPos += toCopy;
    return toCopy;
  }

  // Mock writer for SmartIOTest
  uint32_t mockWriter(char *buf, uint32_t len)
  {
    smartOutput.append(buf, len);
    return len;
  }

  // Default I/O performance test (from DefaultIOTest.cpp)
  double runDefaultIOTest()
  {
    std::istringstream cin(mockInput);
    std::ostringstream cout;
    std::streambuf *cinBuf = std::cin.rdbuf();
    std::streambuf *coutBuf = std::cout.rdbuf();
    std::cin.rdbuf(cin.rdbuf());
    std::cout.rdbuf(cout.rdbuf());

    auto start = std::chrono::high_resolution_clock::now();
    {
      uint32_t numTestCases;
      cin >> numTestCases;

      for (uint32_t i = 0; i < numTestCases; ++i)
      {
        uint32_t n1, n2;
        cin >> n1 >> n2;
        cout << ((n1 > n2) ? n1 : n2) << std::endl;
      }
    }
    auto duration = std::chrono::high_resolution_clock::now() - start;
    defaultOutput = cout.str();

    std::cin.rdbuf(cinBuf);
    std::cout.rdbuf(coutBuf);

    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9; // Seconds
  }

  // Smart I/O performance test (from SmartIOTest.cpp)
  double runSmartIOTest(uint32_t buffSize)
  {
    readPos = 0;
    smartOutput.clear();
    auto io_console_reader = [this](char *out, const uint32_t len)
    { return mockReader(out, len); };
    auto io_console_writer = [this](char *out, const uint32_t len)
    { return mockWriter(out, len); };

    auto start = std::chrono::high_resolution_clock::now();
    {
      SyncIOReadBuffer<uint32_t> smartReadBuffer(buffSize);
      SyncIOLazyWriteBuffer<uint32_t> smartWriteBuffer(buffSize, io_console_writer);

      char input[128];
      smartReadBuffer.readUntil(input, io_console_reader, '\n');
      uint32_t numTestCases;
      sscanf(input, "%u", &numTestCases);

      for (uint32_t i = 0; i < numTestCases; ++i)
      {
        smartReadBuffer.readUntil(input, io_console_reader, '\n');
        uint32_t n1, n2;
        sscanf(input, "%u %u", &n1, &n2);
        char out[128];
        auto len = sprintf(out, "%u\n", n1 > n2 ? n1 : n2);
        smartWriteBuffer.write(out, len);
      }
    }
    auto duration = std::chrono::high_resolution_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9; // Seconds
  }
};

// Unit Tests
TEST_F(BufferTest, ReadUntilNewline)
{
  SyncIOReadBuffer<uint32_t> buffer(10);
  char output[20];
  uint32_t bytesRead = buffer.readUntil(output, [this](char *out, uint32_t len)
                                        { return mockReader(out, len); }, '\n');
  std::string result(output, bytesRead);
  EXPECT_EQ(bytesRead, 2); // "3\n" is 2 chars
  EXPECT_EQ(result, "3\n");
}

TEST_F(BufferTest, WriteAndFlush)
{
  SyncIOLazyWriteBuffer<uint32_t> buffer(10, [this](char *buf, uint32_t len)
                                         { return mockWriter(buf, len); });
  const char *data = "Hello";
  buffer.write(data, strlen(data));
  EXPECT_EQ(smartOutput, "");
  buffer.flush();
  EXPECT_EQ(smartOutput, "Hello");
}

// Performance Tests
TEST_F(BufferTest, DefaultIOPerformance)
{
  double duration = runDefaultIOTest();
  EXPECT_GT(duration, 0.0);              // Just ensure it ran
  EXPECT_EQ(defaultOutput, "2\n4\n6\n"); // Check correctness
  std::cout << "Default I/O Duration: " << duration << "s\n";
}

TEST_F(BufferTest, SmartIOPerformance)
{
  double duration = runSmartIOTest(1024);
  EXPECT_GT(duration, 0.0);
  EXPECT_EQ(smartOutput, "2\n4\n6\n"); // Check correctness
  std::cout << "Smart I/O Duration: " << duration << "s\n";
}

TEST_F(BufferTest, PerformanceComparison)
{
  double defaultDuration = runDefaultIOTest();
  double smartDuration = runSmartIOTest(1024);
  EXPECT_LT(smartDuration, defaultDuration); // Smart should be faster
  double speedup = defaultDuration / smartDuration;
  std::cout << "Speedup: " << speedup << "x\n";
  EXPECT_GT(speedup, 1.0); // Expect some speedup
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}