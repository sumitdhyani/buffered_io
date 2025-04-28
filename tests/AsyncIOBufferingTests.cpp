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

};

TEST_F(AsyncBufferTest, ReadUntilAndEnderNotFound_WithPredicate)
{
  using Task = std::function<void()>;
  using WorkerThread = FifoConsumerThread<Task>;
  using ResHandler = AsyncIOReadBuffer<uint32_t>::ReadResultHandler;
  mockInput = "HelloWorld";

  auto w1 = std::make_shared<WorkerThread>([](const Task &task)
                                           { task(); });
  auto w2 = std::make_shared<WorkerThread>([](const Task &task)
                                           { task(); });
  char* output = new char[10];
  auto buffer = std::make_shared<AsyncIOReadBuffer<uint32_t>>(2);

  auto ioInterface =
      [this, w1, w2](char *out, const uint32_t &len, const ResHandler &resHandler)
  {
    w2->push(
        [this, w1, out, resHandler, len]()
        {
          auto readLen = mockReader(out, len);
          w1->push(
              [resHandler, readLen]()
              {
                resHandler(readLen);
              });
        });
  };

  w1->push([buffer, output, ioInterface]()
  {
    buffer->read(output, 10, ioInterface, [output](const uint32_t& len){
    } );
  });

  std::this_thread::sleep_for(std::chrono::seconds(1));
  EXPECT_EQ(memcmp(output, mockInput.c_str(), mockInput.length()), 0);
  delete[] output;
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}