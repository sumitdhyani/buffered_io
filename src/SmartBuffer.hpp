#pragma once
#include <functional>
#include <string.h>

// SizeType should be an unsigned integral type
template <class SizeType>
struct SyncIOReadBuffer
{
  typedef std::function<SizeType(char *, const SizeType &)> IOInterface;
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
  SyncIOReadBuffer(const SizeType &size) : m_readBuff(reinterpret_cast<char *>(!size? malloc(1) : malloc(size))),
                                           m_tail(0),
                                           m_head(0),
                                           m_size(size),
                                           m_lastOperation(LastOperation::NONE)
  {
  }

  /**
   * Read some bytes from the provided IOInterface
   * @param out         The memory to read the bytes into
   * @param len         The max no. of b ytes to read
   * @param ioInterface The sysnchronous IOInterface to read bytes from,
   *                    it's an std::function<SizeType(char *, const SizeType &)>
   * 
   * @return            No. of bytes actually read from the IOInterface
   **/
  SizeType read(char *const &out,
                const SizeType &len,
                const IOInterface &ioInterface)
  {
    SizeType ret = 0;
    if (occupiedBytes() >= len)
    {
      copy(out, len);
      ret = len;
    }
    else if (paste(ioInterface))
    {
      SizeType occBytes = occupiedBytes();
      if (occBytes >= len)
      {
        copy(out, len);
        ret = len;
      }
      else
      {
        copy(out, occBytes);
        ret = occBytes + read(out + occBytes, len - occBytes, ioInterface);
      }
    }

    return ret;
  }

  /**
   * Read bytes from the provided IOInterface until the character provided
   * as 'ender' is met, or the ioInterface reads 0 bytes.
   * It assumes that if the ioInterface reads 0 bytes, then it won't read
   * anymore bytes thereafter
   *
   * @param out         The memory to read the bytes into
   * @param ioInterface The sysnchronous IOInterface to read bytes from,
   *                    it's an std::function<SizeType(char *, const SizeType &)>
   * @param ender       The character marking the end of this read
   * 
   * @return            No. of bytes actually read from the IOInterface
   **/
  SizeType readUntil(char *const &out,
                     const IOInterface &ioInterface,
                     const char &ender)
  {
    SizeType ret = 0;
    SizeType offset = 0;
    SizeType occBytes = occupiedBytes();
    if (!occBytes)
    {
      occBytes = paste(ioInterface);
    }

    if (occBytes)
    {
      for (;
           offset < occBytes && m_readBuff[(m_tail + offset) % m_size] != ender;
           ++offset)
        ;

      // Found ender
      if (ender == m_readBuff[(m_tail + offset) % m_size])
      {
        copy(out, offset + 1);
        ret = offset + 1;
      }
      // Didn't find the ender
      else
      {
        copy(out, occBytes);
        // Source the data from IO Interface
        if (SizeType bytesPasted = paste(ioInterface);
            bytesPasted > 0) // Non-zero no. of bytes read
        {
          ret = readUntil(out + occBytes, ioInterface, ender);
        }
        else // EOF reached, but there's still some data in the buffer
        {
          ret = occupiedBytes();
          copy(out, ret);
        }
      }
    }

    return ret;
  }

  /**
   * Read bytes from the provided IOInterface until the character satisfying
   * the 'ender' predicate is read, or the ioInterface reads 0 bytes.
   * It assumes that if the ioInterface reads 0 bytes, then it won't read
   * anymore bytes thereafter
   *
   * @param out         The memory to read the bytes into
   * @param ioInterface The sysnchronous IOInterface to read bytes from,
   *                    it's an std::function<SizeType(char *, const SizeType &)>
   * @param ender       The predicate detrmining whether a character qualifies
   *                    as end of the read
   * @return            No. of bytes actually read from the IOInterface
   **/
  SizeType readUntil(char *const &out,
                     const IOInterface &ioInterface,
                     const std::function<bool(const char &)> &ender)
  {
    SizeType ret = 0;
    SizeType offset = 0;
    SizeType occBytes = occupiedBytes();
    if (!occBytes)
    {
      occBytes = paste(ioInterface);
    }

    if (occBytes)
    {
      for (;
           offset < occBytes && !ender(m_readBuff[(m_tail + offset) % m_size]);
           ++offset)
        ;

      // Found ender
      if (ender == m_readBuff[(m_tail + offset) % m_size])
      {
        copy(out, offset + 1);
        ret = offset + 1;
      }
      // Didn't find the ender
      else
      {
        copy(out, occBytes);
        // Source the data from IO Interface
        if (SizeType bytesPasted = paste(ioInterface);
            bytesPasted) // Non-zero no. of bytes read
        {
          ret = readUntil(out + occBytes, ioInterface, ender);
        }
        else // EOF reached, but there's still some data in the buffer
        {
          ret = occupiedBytes();
          copy(out, ret);
        }
      }
    }

    return ret;
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

  ~SyncIOReadBuffer()
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

  // Assumes that len <= occupiedBytes, so the caller of this function has to
  // take care of that
  void copy(char *const &out, const SizeType &len)
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
    else  // case 3
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

  // Read from IOInterface, the max amount of bytes possible(i.e. read bytes <= freeBytes() )
  SizeType paste(const IOInterface &ioInterface)
  {
    // Case 1: m_head = 0:
    // Before:
    //
    // m_readBuff |..................................................|
    //             ↑                                       ↑
    //             m_head                                  m_tail
    //
    // After(read some bytes from the interface):
    //
    //                 bytesReadFromIOInterface
    //             <----------------------------->
    // m_readBuff |..................................................|
    //                                            ↑        ↑
    //                                            m_head   m_tail
    //
    // Case 2: m_tail < m_head:
    // Before:
    //                                                   <-------------lengthTillEnd----------->
    // m_readBuff |.............................................................................|
    //                                ↑                  ↑
    //                                m_tail             m_head
    //
    // After(bytesReadFromIOInterface = lengthTillEnd): turns into case 1
    //                                                   <------bytesReadFromIOInterface------->
    // m_readBuff |.............................................................................|
    //             ↑                  ↑
    //             m_head             m_tail
    //
    //                                  OR
    //
    // After(bytesReadFromIOInterface < lengthTillEnd): turns into case 1
    //                                                   <------bytesReadFromIOInterface----->
    // m_readBuff |.............................................................................|
    //                                ↑                                                       ↑
    //                                m_tail                                                  m_head

    SizeType bytesReadFromIOInterface = 0;
    if (auto free = freeBytes(); free)
    {
      SizeType lengthTillEnd = m_size - m_head;

      // if freeBytes() < lengthTillEnd, then free memory contiguous ans a single read
      // should be done, toRead should be freeBytes()
      // if freeBytes() > lengthTillEnd, then free memory is fragmanted,
      // 2 reads will be requires, 1st read should be "lengthTillEnd" bytes
      // after that if, the ioInterface reads "lengthTillEnd" bytes, then it will
      // turn into case 1, otherwise the algorithm stops
      SizeType toRead = std::min(lengthTillEnd, free);

      bytesReadFromIOInterface = pasteFromInterface(ioInterface, toRead);
      free -= bytesReadFromIOInterface;
      if (bytesReadFromIOInterface == toRead && free) // Case 1
      {
        bytesReadFromIOInterface += pasteFromInterface(ioInterface, free);
      }
    }

    return bytesReadFromIOInterface;
  }

  // Read from IOInterface, assumes that the provided memory is
  // contiguous
  SizeType pasteFromInterface(const IOInterface &ioInterface, const SizeType &len)
  {
    SizeType ret = 0;
    if (len &&
        (ret = ioInterface(m_readBuff + m_head, len)))
    {
        m_head = (m_head + ret) % m_size;
        m_lastOperation = LastOperation::PASTE;
    }

    return ret;
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

template <class SizeType>
struct SyncIOLazyWriteBuffer
{
  typedef std::function<SizeType(char *, const SizeType &)> DataWriter;
  enum class LastOperation
  {
    FLUSH,
    PUT,
    NONE
  };

  SyncIOLazyWriteBuffer(const SizeType &size, const DataWriter &dataWriter) : m_outBuff(reinterpret_cast<char *>(malloc(size))),
                                                                              m_tail(0),
                                                                              m_head(0),
                                                                              m_size(size),
                                                                              m_dataWriter(dataWriter),
                                                                              m_lastOperation(LastOperation::NONE)
  {}

  SizeType write(const char *out, const SizeType &len)
  {
    SizeType remainingLen = len;
    SizeType ret = 0;
    bool flushfailed = false;
    for (SizeType freeBytesBeforePut = freeBytes();
         freeBytesBeforePut < remainingLen && !flushfailed;
         flushfailed = flush(), freeBytesBeforePut = freeBytes())
    {
      put(out, freeBytesBeforePut);
      remainingLen -= freeBytesBeforePut;
      out += freeBytesBeforePut;
      ret += freeBytesBeforePut;
    }

    SizeType bytestoPut = std::min(remainingLen, freeBytes());
    put(out, bytestoPut);
    ret += bytestoPut;
    return ret;
  }

  SizeType flush()
  {
    if (!occupiedBytes())
    {
      return 0;
    }

    SizeType ret = 0;
    if (m_tail < m_head)
    {
      ret = m_dataWriter(m_outBuff + m_tail, occupiedBytes());
      m_tail += ret;
    }
    else
    {
      SizeType lengthTillEnd = m_size - m_tail;
      if (ret = m_dataWriter(m_outBuff + m_tail, lengthTillEnd) == lengthTillEnd)
      {
        m_tail = m_dataWriter(m_outBuff, m_head);
        ret += m_tail;
      }
    }

    if (ret)
    {
      if (m_tail == m_head)
      {
        m_tail = m_head = 0;
      }
      m_lastOperation = LastOperation::FLUSH;
    }

    return ret;
  }

  ~SyncIOLazyWriteBuffer()
  {
    flush();
    free(m_outBuff);
  }

  SyncIOLazyWriteBuffer(const SyncIOLazyWriteBuffer &) = delete;
  SyncIOLazyWriteBuffer &operator=(const SyncIOLazyWriteBuffer &) = delete;
  SyncIOLazyWriteBuffer(SyncIOLazyWriteBuffer &&) = delete;
  SyncIOLazyWriteBuffer &operator=(SyncIOLazyWriteBuffer &&) = delete;

private:
  // Should be Called only when freeBytes() <= len
  void put(const char *outData, const SizeType &len)
  {
    if (!len)
    {
      return;
    }

    if (m_head < m_tail ||
        len <= m_size - m_head)
    {
      memcpy(m_outBuff + m_head, outData, len);
      m_head += len;
    }
    else
    {
      const SizeType l1 = m_size - m_head;
      const SizeType l2 = len - l1;
      memcpy(m_outBuff + m_head, outData, l1);
      memcpy(m_outBuff, outData + l1, l2);
      m_head = l2;
    }

    m_lastOperation = LastOperation::PUT;
  }

  SizeType occupiedBytes()
  {
    if (m_tail == m_head)
    {
      return m_lastOperation == LastOperation::PUT ? m_size : 0;
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
  const DataWriter m_dataWriter;
  SizeType m_tail;
  SizeType m_head;
  const SizeType m_size;
  char *const m_outBuff;
};
