#pragma once
#include <concepts>
#include <functional>
#include <optional>
#include <string.h>

// SizeType should be an unsigned integral type
template <class SizeType>
requires std::unsigned_integral<SizeType>
struct AsyncIOReadBuffer
{
  typedef std::function<void(const SizeType&)> ReadResultHandler;
  typedef std::function<void(char *, const SizeType&, const ReadResultHandler&)> IOInterface;
  enum class LastOperation
  {
    COPY,
    PASTE,
    NONE
  };

  /**
   *  Constructor
   *  @param size Size of the Buffer
   *              If 0 is given as size, size is deemed to be 1
   **/
  AsyncIOReadBuffer(const SizeType &size) : m_readBuff(reinterpret_cast<char *>(!size ? malloc(1) : malloc(size))),
                                            m_tail(0),
                                            m_head(0),
                                            m_size(size),
                                            m_lastOperation(LastOperation::NONE)
  {
  }

  /**
   * Read some bytes from the provided IOInterface
   * @param out         The memory to read the bytes into
   * @param len         The max no. of bytes to read
   * @param ioInterface The asysnchronous IOInterface to read bytes from,
   *                    it's an std::function<SizeType(char *, const SizeType &)>
   *
   * @return            No. of bytes actually read from the IOInterface
   * @remarks           a) The "resHandler" callback should only be called after
   *                       read method is called
   *                       and vice-versa, they should never be called in parallel
   *                    b) As a corallary of point 1, a call to read method should not
   *                       be made until previous read is finished.
   *                       Or in ither words a call to read should me made only when the
   *                       resultHandler for previous read has been invoked.
   *                       Infact it might be a good idea in most situation to call the read
   *                       method in the 'resHandler' to generate an "asnchronous read loop"
   *                       and exit that loop when the "reshandler" is invoked with 0 bytes,
   *                       indicating that the IOInterface can no longer provide any data
   **/
  void read(char *const &out,
            const SizeType &len,
            const IOInterface &ioInterface,
            const ReadResultHandler& resHandler)
  {
    SizeType toCopy = std::min(occupiedBytes(), len);
    copy(out, toCopy);
    if (toCopy == len)
    {
      resHandler(len);
    }
    else
    {
      SizeType lengthTillEnd = m_size - m_head;
      SizeType toRead = std::min(freeBytes(), lengthTillEnd);
      ioInterface(m_readBuff + m_head,
                  toRead,
                  [this, out, toCopy, resHandler, ioInterface, len]
                  (const SizeType &readLen)
                  { onReadFromInterface(out,
                                        len,
                                        toCopy,
                                        readLen,
                                        ioInterface,
                                        resHandler);
                  });
    }
  }

  bool empty()
  {
    return occupiedBytes() == 0;
  }

  bool full()
  {
    return freeBytes() == 0;
  }

  SizeType size()
  {
    return occupiedBytes();
  }

  SizeType capacity()
  {
    return m_size;
  }

  SizeType vacancy()
  {
    return freeBytes();
  }

  ~AsyncIOReadBuffer()
  {
    free(m_readBuff);
  }

  // Non copyable-assignable, Non moveable-move assinable for the reasons of
  // Simplicity
  AsyncIOReadBuffer(const AsyncIOReadBuffer &) = delete;
  AsyncIOReadBuffer &operator=(const AsyncIOReadBuffer &) = delete;
  AsyncIOReadBuffer(AsyncIOReadBuffer &&) = delete;
  AsyncIOReadBuffer &operator=(AsyncIOReadBuffer &&) = delete;

private:
  /**
   * This is the callback that is called whenever some bytes are yielded by the externally provided
   * IOInterface. This method checks whether the no. of bytes requested in the original 'read'
   * request have been read into the externally provided buffer.
   * If the totalReadBytes are < totalRequired bytes, then it attempts to call the IOINterface again
   * till the totalRead bytes are < totalRequired bytes
   * Hence, creating an asynchronous loop.
   * @param out               The original pointer that was provided to read method
   * @param totalRequired     The total no. of bytes that were requested to the read method
   * @param totalRead         The total bytes read into the 'out' pointer since the
   *                          read method was called
   * @param bytesInThisIOCall No. of bytes yielded by the IOInterface in last read attempt
   * @param ioInterface       The externally provided IOInterface
   * @param resHandler        The original callback provided to the read method
   *
   **/
  void onReadFromInterface(char *const &out,
                           const SizeType& totalRequired,
                           const SizeType& totalRead,
                           const SizeType& bytesInThisIOCall,
                           const IOInterface& ioInterface,
                           const ReadResultHandler& resHandler)
  {
    // The IOINterface can no longer give any data, close the async read loop here
    if (!bytesInThisIOCall)
    {
      resHandler(totalRead);
    }
    else
    {
      m_head = (m_head + bytesInThisIOCall) % m_size;
      m_lastOperation = LastOperation::PASTE;
      SizeType totalLeftToRead = totalRequired - totalRead;
      SizeType toCopy = std::min(totalLeftToRead, occupiedBytes());
      copy(out + totalRead, toCopy);
      totalLeftToRead -= toCopy;

      // If all requested bytes have been read, then close the async loop and
      // notify the externally provided callback
      if (!totalLeftToRead)
      {
        resHandler(totalRequired);
      }
      else
      {
        SizeType lengthTillEnd = m_size - m_head;
        // The memory provided to the external interface should be contiguous
        // So even if our buffer has a lot of memory, but it's fragmented,
        // we have to read into the part that spans from m_head to the end of buffer
        SizeType toRead = std::min(lengthTillEnd, freeBytes());

        ioInterface(m_readBuff + m_head,
                    toRead,
                    [this, out, totalRequired, totalRead, toCopy, ioInterface, resHandler](const SizeType &readLen)
                    {
                      onReadFromInterface(out,
                                          totalRequired,
                                          totalRead +  toCopy,
                                          readLen,
                                          ioInterface,
                                          resHandler);
                    });
      }
    }
  }

// Assumes that len <= occupiedBytes, so the caller of this function has to
// take care of that
void
  copy(char *const &out, const SizeType &len)
  {
    if (!len)
    {
      return;
    }

    // Case 1: m_tail < m_head:
    // Before:
    //                    len
    //                 <-------->
    // m_readBuff |.........................................|
    //                 ↑                  ↑
    //                 m_tail             m_head
    //
    // After:
    //                    len
    //                 <-------->
    // m_readBuff |.........................................|
    //                           ↑        ↑
    //                           m_tail   m_head
    //
    // Case 2: m_tail > m_head but len <= m_size - m_tail:
    // Before:
    //                                        len
    //                                    <-------->
    // m_readBuff |....................................|
    //                 ↑                  ↑
    //                 m_head             m_tail
    //
    // After:
    //                                      len
    //                                    <-------->
    // m_readBuff |....................................|
    //                           ↑                 ↑
    //                           m_head            m_tail
    //
    // Case 3: m_tail > m_head and len > m_size - m_tail:
    // Before:
    //                                             len
    //             ---->                          <----
    // m_readBuff |....................................|
    //                            ↑               ↑
    //                         m_head             m_tail
    //
    // After:
    //                                             len
    //             ---->                          <----
    // m_readBuff |....................................|
    //                  ↑         ↑
    //                  m_tail    m_head

    if (m_tail < m_head ||        //  Case 1
        len <= (m_size - m_tail)) //  Case 2
    {
      memcpy(out, m_readBuff + m_tail, len);
      m_tail = (m_tail + len) % m_size;
    }
    else // case 3
    {
      const SizeType l1 = m_size - m_tail;
      const SizeType l2 = len - l1;
      memcpy(out, m_readBuff + m_tail, l1);
      memcpy(out + l1, m_readBuff, l2);
      m_tail = l2;
    }

    m_lastOperation = LastOperation::COPY;
    if (!occupiedBytes())
    {
      m_head = m_tail = 0;
    }
  }

  SizeType occupiedBytes()
  {
    if (m_tail == m_head)
    {
      // In this case m_lastOperation == LastOperation::PASTE means that the
      // buffer is completely onoccupied, otherwise it's completely free
      return m_lastOperation == LastOperation::PASTE ? m_size : 0;
    }
    else if (m_tail < m_head)
    {
      return m_head - m_tail;
    }
    else
    {
      return m_size - (m_tail - m_head);
    }
  }

  SizeType freeBytes()
  {
    return m_size - occupiedBytes();
  }

  LastOperation m_lastOperation;
  SizeType m_tail;
  SizeType m_head;
  const SizeType m_size;
  char *const m_readBuff;
};