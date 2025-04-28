#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "AsyncSmartBuffer.hpp"

template <class T>
class FifoConsumerThread
{
protected:
  typedef std::vector<T> ConsumerQueue;
  typedef std::thread stdThread;
  typedef std::mutex stdMutex;
  typedef std::unique_lock<stdMutex> stdUniqueLock;
  typedef std::condition_variable ConditionVariable;

private:
  ConsumerQueue m_queue;
  stdMutex m_mutex;
  ConditionVariable m_cond;
  std::atomic<bool> m_terminate;
  bool m_consumerBusy; // Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
  stdThread m_thread;
  std::function<void(T)> m_processor;

  void run()
  {
    while (!m_terminate)
    {
      ConsumerQueue local;

      {
        stdUniqueLock lock(m_mutex);
        if (m_queue.empty())
        {
          m_consumerBusy = false;
          m_cond.wait(lock);
        }

        local.swap(m_queue);
        m_consumerBusy = true;
      }

      for (auto const &task : local)
        m_processor(task);
    }

    // If the consumer is killed or destroyed, it should exit only after completing the pending tasks
    // so as not to leave the client code in a state of uncertainty regarding which tasks will be executed and which won't
    // This leaves a clear cut behavior, i.e, all the items pushed before killing or destroying the consumer will be processed
    {
      stdUniqueLock lock(m_mutex);
      if (!m_queue.empty())
      {
        ConsumerQueue local;
        m_queue.swap(local);
        lock.unlock();
        for (auto const &task : local)
          m_processor(task);
      }
    }
  }

public:
  FifoConsumerThread(std::function<void(T)> predicate)
      : m_processor(predicate)
  {
    m_terminate = false;
    m_consumerBusy = false;
    m_thread = stdThread(&FifoConsumerThread::run, this);
  }

  void push(const T &item)
  {
    {
      stdUniqueLock lock(m_mutex);
      if (m_terminate)
      {
        throw std::runtime_error("The consumer has been killed and is no longer in a state to process new items");
      }
      m_queue.push_back(item);

      if (!m_consumerBusy)
      {
        lock.unlock();
        m_cond.notify_one();
      }
    }
  }

  // returns number of pending items
  size_t size()
  {
    stdUniqueLock lock(m_mutex);
    return m_queue.size();
  }

  void kill()
  {
    stdUniqueLock lock(m_mutex);
    if (!m_terminate)
    {
      m_terminate = true;
      if (!m_consumerBusy)
      {
        lock.unlock();
        m_cond.notify_one();
      }
      m_thread.join();
    }
  }

  ~FifoConsumerThread()
  {
    kill();
  }
};

// Test fixture for common setup
class AsyncBufferTest : public ::testing::Test
{
protected:
  using Task = std::function<void()>;
  using WorkerThread = FifoConsumerThread<Task>;
  using ResHandler = AsyncIOReadBuffer<uint32_t>::ReadResultHandler;

  void TearDown() override
  {
    readPos = 0;
  }

  std::string mockInput;
  std::string defaultOutput;
  std::string smartOutput;
  WorkerThread w1;
  WorkerThread w2;

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

  AsyncBufferTest() :
    w1([](const Task &task) { task(); }),
    w2([](const Task &task) { task(); })
  {}
};

TEST_F(AsyncBufferTest, SearialReads)
{

  mockInput = "10HelloWorld08ByeWorld09HaleLujah10JaiShriRam";
  auto buffer = std::make_shared<AsyncIOReadBuffer<uint32_t>>(200);
  std::vector<std::string> msgs;
  uint32_t totalIOCalls = 0;
  char* buff = new char[1024]{0};

  std::function<void()> readHeader = [](){};

  auto ioInterface =
  [this, &totalIOCalls](char *out, const uint32_t &len, const ResHandler &resHandler)
  {
    w2.push(
        [this, out, resHandler, len, &totalIOCalls]()
        {
          auto readLen = mockReader(out, len);
          ++totalIOCalls;
          w1.push(
              [resHandler, readLen]()
              {
                resHandler(readLen);
              });
        });
  };

  auto onMsgRead =
  [&](const char* msg, const uint32_t& msgLen)
  {
    msgs.emplace_back(msg, msgLen);
    w1.push(readHeader);
  };

  auto onHeaderRead =
  [&](const uint32_t &msgLen)
  {
    buffer->read(buff,
                 msgLen,
                 ioInterface,
                 [&](const uint32_t &len)
                 {
                   onMsgRead(buff, len);
                 });
  };

  readHeader =
  [&]()
  {
    buff[2] = '\0';
    buffer->read(buff,
                 2,
                 ioInterface,
                 [&](const uint32_t &len)
                 {
                   if (!len)
                     return;

                   auto msgLen = atoi(buff);
                   onHeaderRead(msgLen);
                 });
  };
  
  w1.push(readHeader);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  EXPECT_EQ(msgs.size(), 4);
  EXPECT_EQ(msgs[0], std::string("HelloWorld"));
  EXPECT_EQ(msgs[1], std::string("ByeWorld"));
  EXPECT_EQ(msgs[2], std::string("HaleLujah"));
  EXPECT_EQ(msgs[3], std::string("JaiShriRam"));
  EXPECT_EQ(totalIOCalls, 2);
  delete[] buff;
}

TEST_F(AsyncBufferTest, ReadSizeGreaterThanBufferSize)
{
  
  mockInput = "HelloWorld";
  uint32_t totalLenRead = 0;
  uint32_t totalIOCalls = 0;
  auto buffer = std::make_shared<AsyncIOReadBuffer<uint32_t>>(2);
  char* output = new char[10];

  auto ioInterface =
  [this, &totalIOCalls](char *out, const uint32_t &len, const ResHandler &resHandler)
  {
    w2.push(
        [this, out, resHandler, len, &totalIOCalls]()
        {
          auto readLen = mockReader(out, len);
          ++totalIOCalls;
          w1.push(
              [resHandler, readLen]()
              {
                resHandler(readLen);
              });
        });
  };

  w1.push([buffer, output, ioInterface, &totalLenRead]()
  {
    buffer->read(output, 10, ioInterface, [output, &totalLenRead](const uint32_t& len)
    {
        totalLenRead = len;  
    });
  });

  std::this_thread::sleep_for(std::chrono::seconds(1));
  EXPECT_EQ(totalLenRead, 10);
  EXPECT_EQ(memcmp(output, mockInput.c_str(), mockInput.length()), 0);
  EXPECT_EQ(totalIOCalls, 5);
  delete[] output;
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}