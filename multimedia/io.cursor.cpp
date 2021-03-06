#include "stdafx.h"
#include "io.cursor.h"
#include "core/core.h"
#include "core/exception.hpp"
#include <numeric>
#pragma warning(disable: 4819)
extern "C" {
#include <libavutil/error.h>
#include <libavutil/common.h>
#include <libavformat/avio.h>
}

namespace media
{
    using namespace detail;

    //-- cursor
    cursor::cursor(const multi_buffer& buffer)
        : buffer_begin(boost::asio::buffer_sequence_begin(buffer.data()))
        , buffer_end(boost::asio::buffer_sequence_end(buffer.data()))
        , buffer_iterator(buffer_begin) {
        auto& buffer_sizes = core::as_mutable(this->buffer_sizes);
        std::transform(buffer_begin, buffer_end, std::back_inserter(buffer_sizes),
                       [](const_buffer_iterator::reference buffer) {
                           return boost::asio::buffer_size(buffer);
                       });
    }

    int64_t cursor::seek_sequence(int64_t seek_offset) {
        const auto size_iter = std::upper_bound(buffer_sizes.crbegin(), buffer_sizes.crend(),
                                                seek_offset, std::greater<>{});
        buffer_iterator = std::prev(buffer_end, std::distance(buffer_sizes.crbegin(), size_iter));
        const auto partial_sequence_size = size_iter != buffer_sizes.crend() ? *size_iter : 0;
        buffer_offset = std::min(seek_offset - partial_sequence_size, buffer_size());
        sequence_offset = partial_sequence_size + buffer_offset;
        return sequence_offset;
    }

    int64_t cursor::buffer_size() const {
        auto const size = buffer_sizes.at(std::distance(buffer_begin, buffer_iterator));
        return folly::to<int64_t>(size);
    }

    int64_t cursor::sequence_size() const {
        return std::accumulate(buffer_sizes.begin(), buffer_sizes.end(), 0i64);
    }

    //-- generic_cursor
    generic_cursor::generic_cursor(read_context&& rfunc, write_context&& wfunc, seek_context&& sfunc)
        : reader(std::move(rfunc))
        , writer(std::move(wfunc))
        , seeker(std::move(sfunc)) {}

    int generic_cursor::read(uint8_t* buffer, const int size) {
        return readable() ? reader(buffer, size) : std::numeric_limits<int>::min();
    }

    int generic_cursor::write(uint8_t* buffer, const int size) {
        return writable() ? writer(buffer, size) : std::numeric_limits<int>::min();
    }

    int64_t generic_cursor::seek(const int64_t offset, const int whence) {
        return seekable() ? seeker(offset, whence) : std::numeric_limits<int64_t>::min();
    }

    bool generic_cursor::readable() const {
        return reader != nullptr;
    }

    bool generic_cursor::writable() const {
        return writer != nullptr;
    }

    bool generic_cursor::seekable() const {
        return seeker != nullptr;
    }

    std::shared_ptr<generic_cursor> generic_cursor::create(
        read_context&& rfunc, write_context&& wfunc, seek_context&& sfunc) {
        return std::make_shared<generic_cursor>(std::move(rfunc), std::move(wfunc), std::move(sfunc));
    }

    //-- random_access_curser
    random_access_cursor::random_access_cursor(const multi_buffer& buffer)
        : cursor(buffer) {}

    int random_access_cursor::read(uint8_t* buffer, int expect_size) {
        if (buffer_iterator == buffer_end)
            return AVERROR_EOF;
        auto total_read_size = 0;
        fmt::print("cursor reading, expect_size {}, sequence{}/{}\n", expect_size, sequence_offset, sequence_size());
        while (buffer_iterator != buffer_end && total_read_size < expect_size) {
            auto const read_ptr = static_cast<char const*>((*buffer_iterator).data());
            auto const read_size = std::min<int64_t>(expect_size - total_read_size, buffer_size() - buffer_offset);
            assert(read_size > 0);
            std::copy_n(read_ptr + buffer_offset, read_size, buffer + total_read_size);
            buffer_offset += read_size;
            sequence_offset += read_size;
            if (buffer_offset == buffer_size()) {
                buffer_iterator.operator++();
                buffer_offset = 0;
            }
            total_read_size += folly::to<int>(read_size);
        }
        fmt::print("read_size {}, expect_size {}, sequence{}/{}\n", total_read_size, expect_size, sequence_offset, sequence_size());
        return folly::to<int>(total_read_size);
    }

    int random_access_cursor::write(uint8_t* buffer, int size) {
        core::not_implemented_error::throw_directly();
    }

    int64_t random_access_cursor::seek(int64_t seek_offset, int whence) {
        switch (whence) {
            case SEEK_SET: fmt::print("SEEK_SET OFFSET {}\n", seek_offset);
                break;
            case SEEK_END: fmt::print("SEEK_END OFFSET {}\n", seek_offset);
                seek_offset += sequence_size();
                break;
            case SEEK_CUR: fmt::print("SEEK_CUR OFFSET {}\n", seek_offset);
                seek_offset += sequence_offset;
                break;
            case AVSEEK_SIZE: fmt::print("AVSEEK_SIZE OFFSET {}\n", seek_offset);
                return sequence_size();
            default:
                core::not_reachable_error::throw_directly();

        }
        return seek_sequence(seek_offset);
    }

    bool random_access_cursor::readable() const {
        return true;
    }

    bool random_access_cursor::seekable() const {
        return true;
    }

    std::shared_ptr<random_access_cursor> random_access_cursor::create(const multi_buffer& buffer) {
        return std::make_shared<random_access_cursor>(buffer);
    }

    bool io_base::readable() const {
        return false;
    }

    bool io_base::writable() const {
        return false;
    }

    bool io_base::seekable() const {
        return false;
    }

    bool io_base::available() const {
        core::not_implemented_error::throw_directly();
    }

    int64_t io_base::consume_size() const {
        core::not_implemented_error::throw_directly();
    }

    int64_t io_base::remain_size() const {
        core::not_implemented_error::throw_directly();
    }

    //-- buffer_list_cursor
    buffer_list_cursor::buffer_list_cursor(std::list<const_buffer> bufs) {
        full_size_ = std::accumulate(
            bufs.begin(), bufs.end(), 0i64,
            [](int64_t sum, const_buffer& buffer) {
                return sum + buffer.size();
            });
        buffer_list_ = std::move(bufs);
        buffer_iterator_ = buffer_list_.begin();
    }

    int buffer_list_cursor::read(uint8_t* buffer, int expect_size) {
        if (buffer_iterator_ != buffer_list_.end()) {
            auto read_size = 0i64;
            while (buffer_iterator_ != buffer_list_.end() && read_size < expect_size) {
                auto* pointer = static_cast<const char*>(buffer_iterator_->data());
                const auto increment = std::min<int64_t>(expect_size - read_size,
                                                         buffer_iterator_->size() - offset_);
                assert(increment > 0);
                std::copy_n(pointer + offset_, increment, buffer + read_size);
                offset_ += increment;
                if (offset_ == buffer_iterator_->size()) {
                    if (buffer_iterator_.operator++() == buffer_list_.end()) {
                        eof_ = true;
                    }
                    offset_ = 0;
                }
                read_size += increment;
            }
            full_offset_ += read_size;
            full_read_size_ += read_size;
            return folly::to<int>(read_size);
        }
        return AVERROR_EOF;
    }

    int buffer_list_cursor::write(uint8_t* buffer, int size) {
        core::not_implemented_error::throw_directly();
    }

    int64_t buffer_list_cursor::seek(int64_t seek_offset, int whence) {
        switch (whence) {
            case SEEK_SET:
                break;
            case SEEK_END:
                seek_offset += full_size_;
                break;
            case SEEK_CUR:
                seek_offset += full_offset_;
                break;
            case AVSEEK_SIZE:
                return -1; //TODO: return -1 for streaming
            default: 
                core::not_reachable_error::throw_directly();
        }
        if (seek_offset >= full_size_) {
            buffer_iterator_ = buffer_list_.end();
            full_offset_ = full_size_;
            offset_ = 0;
            return full_size_;
        }
        auto partial_sum = 0i64;
        buffer_iterator_ = std::find_if(
            buffer_list_.begin(), buffer_list_.end(),
            [&partial_sum, &seek_offset](const_buffer& buffer) {
                const auto buffer_size = folly::to<int64_t>(buffer.size());
                const auto next_buffer_offset = partial_sum + buffer_size;
                if (next_buffer_offset <= seek_offset) {
                    partial_sum = next_buffer_offset;
                    return false;
                }
                return true;
            });
        full_offset_ = seek_offset;
        offset_ = seek_offset - partial_sum;
        return seek_offset;
    }

    bool buffer_list_cursor::readable() const {
        return true;
    }

    bool buffer_list_cursor::seekable() const {
        return true;
    }

    bool buffer_list_cursor::available() const {
        return buffer_iterator_ != buffer_list_.end();
    }

    int64_t buffer_list_cursor::consume_size() const {
        return full_offset_;
    }

    int64_t buffer_list_cursor::remain_size() const {
        return full_size_ - full_offset_;
    }

    std::unique_ptr<buffer_list_cursor>
    buffer_list_cursor::create(const multi_buffer& buffer) {
        return std::make_unique<buffer_list_cursor>(core::split_buffer_sequence(buffer));
    }

    std::unique_ptr<buffer_list_cursor>
    buffer_list_cursor::create(std::list<const_buffer>&& buffer_list) {
        return std::make_unique<buffer_list_cursor>(std::move(buffer_list));
    }
}
