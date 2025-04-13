#pragma once
#include <functional>
#include <optional>
#include <string.h>

// SizeType should be an unsigned integral type
template <class SizeType>
struct AsyncIOReadBuffer
{
  typedef std::function<void(const SizeType&)> ReadResultHandler;
  typedef std::function<SizeType(char *, const SizeType&, const ReadResultHandler&)> IOInterface;
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
   **/
  void read(char *const &out,
            const SizeType &len,
            const IOInterface &ioInterface,
            const ReadResultHandler& resHandler)
  {
    SizeType occBytes = occupiedBytes();
    SizeType toCopy = std::min(occBytes, len);
    copy(out, toCopy);
    if (toCopy <= occBytes)
    {
      resHandler(len);
    }
    else
    {
      SizeType lengthTillEnd = m_size - m_head;
      SizeType toRead = std::min(freeBytes(), lengthTillEnd)
      ioInterface(m_readBuff + m_tail,
                  toRead,
                  [this, out, resHandler, ioInterface, len]
                  (const SizeType &readLen)
                  { onReadFromInterface(out + toCopy,
                                        len - toCopy,
                                        len,
                                        ioInterface,
                                        [resHandler, toCopy](const SizeType &callbackLen)
                                        {
                                          resHandler(callbackLen + toCopy);
                                        });
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
  SyncIOReadBuffer(const SyncIOReadBuffer &) = delete;
  SyncIOReadBuffer &operator=(const SyncIOReadBuffer &) = delete;
  SyncIOReadBuffer(SyncIOReadBuffer &&) = delete;
  SyncIOReadBuffer &operator=(SyncIOReadBuffer &&) = delete;

private:
  void onReadFromInterface(char *const &out,
                           const SizeType &requiredLen,
                           const SizeType &totalReadFromInterface,
                           const IOInterface &ioInterface,
                           const ReadResultHandler &resHandler)
  {
    if (!totalReadFromInterface)
    {
      resHandler(0);
    }
    else
    {
      m_head = (m_head + totalReadFromInterface) % m_size;
      m_lastOperation = LastOperation::PASTE;
      SizeType occBytes = occupiedBytes();
      SizeType toCopy = std::min(requiredLen, occBytes);
      copy(out, toCopy);

      if (toCopy <= requiredLen)
      {
        resHandler(toCopy);
      }
      if (toCopy < requiredLen)
      {
        read(out + toCopy, requiredLen - toCopy, ioInterface, resHandler);
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