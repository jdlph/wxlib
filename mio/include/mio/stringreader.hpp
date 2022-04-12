/* Copyright 2020-2022 Wuping Xin
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _MIO_STRING_READER_H_
#define _MIO_STRING_READER_H_

#include <mio/mio.hpp>

#include <functional>
#include <future>
#include <numeric>

#ifdef __GNUC__
#define semi_branch_expect(x, y) __builtin_expect(x, y)
#else
#define semi_branch_expect(x, y) x
#endif

namespace mio {

/**
   A fast line reader based on memory mapped file. It is about ~4x to ~6x faster than
   std::getline in line-by-line loading.

   Example:

     std::filesystem::path file_path = std::filesystem::current_path()/"test.txt";
     assert(std::filesystem::exists(file_path));
     mio::StringReader reader(file_path.string());

     if (reader.is_mapped()) {
       while (!reader.eof()) {
         auto line = reader.getline();
         // ... do something about the line just read.
       }
     }
 */
class StringReader
{
public:
  /**
    Fires when a new line has been read.
  */
  using OnGetline = std::function<void(const std::string_view)>;

  /**
     Constructs a reader to read from a disk file line by line. If the
     specified file does not exist, std::system_error will be thrown with
     error code describing the nature of the error.

     Precondition - the file to load must exist.

     \param   a_file  The file to read. It must exist.
   */
  [[maybe_unused]] explicit StringReader(const std::string &a_file) : m_mmap{a_file}
  {
    m_begin = m_mmap.begin();
  }

  explicit StringReader(const std::string &&a_file) : m_mmap{a_file}
  {
    m_begin = m_mmap.begin();
  }

  StringReader() = delete;
  StringReader(const StringReader &) = delete;
  StringReader(StringReader &&) = delete;
  StringReader &operator=(StringReader &) = delete;
  StringReader &operator=(StringReader &&) = delete;

  ~StringReader()
  {
    m_mmap.unmap();
  }

  /**
     Checks whether the reader has reached end of file.

     \returns True if end of line, false otherwise.
   */
  [[maybe_unused]] [[nodiscard]] bool eof() const noexcept
  {
    return (m_begin == nullptr);
  }

  /**
   Checks whether the reader has successfully mapped the underlying file.
   Only on mapped file can getline be called.

   \returns True if mapped, false otherwise.
 */
  [[nodiscard]] inline bool is_mapped() const noexcept
  {
    return m_mmap.is_mapped();
  }

  /**
     Reads a new line into a string view.
     Precondition - StringReader::is_mapped() must be true.

     \returns A std::string_view, {nullptr, 0} will be returned if the reader has reached end of file.
   */
  std::string_view getline() noexcept
  {
    const char *l_begin = m_begin;
    const char *l_find = std::find(l_begin, m_mmap.end(), '\n');

    // l_find == m_mmap.end() happens only once at end of file. The majority of the
    // processing will be for l_find != m_mmap.end(). Give this hint to the compiler
    // for better branch prediction.
    if (semi_branch_expect((l_find != m_mmap.end()), true))
      m_begin = std::next(l_find);
    else
      m_begin = (l_begin = nullptr), (l_find = nullptr); // Set BOTH l_begin AND l_find nullptr if end of file.

    return {l_begin, static_cast<size_t>(l_find - l_begin)};
  }

  size_t getline(const OnGetline &a_on_getline)
  {
    size_t l_numlines{0};

    // Fall back to synchronous line by line processing.
    while (!this->eof()) {
      a_on_getline(this->getline());
      l_numlines++;
    }
    return l_numlines;
  }

  /**
   Reads a new line asynchronously.
   Precondition - StringReader::is_mapped() must be true.

   \param a_on_getline Event handler when a new line is read.
   \tparam NumThreads Number of threads to run parallel.
   \tparam MinFileByteSize Minimum byte size of the file required for async processing to kick in.

   \returns Total number of lines read.
 */
  template<uint8_t NumThreads = 2, size_t MinFileByteSize = 1024 * 1024>
  requires (NumThreads >= 2) and (NumThreads <= 8) and (MinFileByteSize >= 1024 * 1024)
  [[maybe_unused]] size_t getline_async(const OnGetline &a_on_getline)
  {
    // Fallback to default sequential getline.
    if (m_mmap.size() < MinFileByteSize)
      return getline(a_on_getline);

    // Spawn a couple of futures, and do async processing.
    std::array<std::unique_ptr<std::future<size_t>>, NumThreads> l_futures;

    const auto l_adv = m_mmap.size() / NumThreads;
    const char *l_begin = m_begin;
    const char *l_end = find_end(l_begin, l_adv);

    for (int i = 0; i < l_futures.size(); i++) {
      *l_futures[i] = std::async(do_getline_async, l_begin, l_end, a_on_getline);

      l_begin = l_end;
      l_end = find_end(l_begin, l_adv);

      if (std::distance(l_end, m_mmap.end()) < l_adv)
        l_end = m_mmap.end();
    }

    // Collect the total number of lines read.
    return std::accumulate(l_futures.begin(), l_futures.end(), 0, [](auto &a, size_t b) { return a->get() + b; });
  }

private:
  static size_t do_getline_async(const char *a_begin, const char *a_end, const OnGetline &a_on_getline)
  {
    const char *l_begin = a_begin;
    const char *l_find = std::find(l_begin, a_end, '\n');

    size_t l_counter{0};

    while (l_find != a_end) {
      a_on_getline({l_begin, static_cast<size_t>(l_find - l_begin)}), l_counter++;
      l_begin = std::next(l_find);
      l_find = std::find(l_begin, a_end, '\n');
    }

    return l_counter;
  }

  static const char *find_end(const char *a_begin, size_t a_advance)
  {
    using iter_diff_t = typename std::iterator_traits<const char *>::difference_type;

    auto l_find = std::find(
        std::reverse_iterator(std::next(a_begin, static_cast<iter_diff_t>(a_advance))),
        std::reverse_iterator(a_begin),
        '\n').base();

    return std::next(l_find);
  }

private:
  mmap_source m_mmap;
  const char *m_begin;
};

}

#endif