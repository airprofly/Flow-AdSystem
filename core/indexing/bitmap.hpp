#pragma once

// Author: airprofly

#include <algorithm>
#include <cstdint>
#include <vector>

namespace flow_ad::indexing
{

class BitMap
{
public:
    explicit BitMap(size_t size = 0)
        : data_((size + 63) / 64, 0), size_(size)
    {}

    // 设置某一位为 1
    void set(size_t pos)
    {
        if (pos >= size_)
            return;
        data_[pos / 64] |= (1ULL << (pos % 64));
    }

    // 批量设置位为 1
    void set_range(size_t start, size_t end)
    {
        for (size_t pos = start; pos < end && pos < size_; ++pos)
        {
            set(pos);
        }
    }

    // 清除某一位（设为 0）
    void clear(size_t pos)
    {
        if (pos >= size_)
            return;
        data_[pos / 64] &= ~(1ULL << (pos % 64));
    }

    // 清空所有位
    void reset()
    {
        std::fill(data_.begin(), data_.end(), 0);
    }

    // 获取某一位的值
    bool test(size_t pos) const
    {
        if (pos >= size_)
            return false;
        return (data_[pos / 64] >> (pos % 64)) & 1ULL;
    }

    // 求交集（返回新 BitMap）
    BitMap operator&(const BitMap& other) const
    {
        BitMap result(std::max(size_, other.size_));
        size_t min_blocks = std::min(data_.size(), other.data_.size());

        for (size_t i = 0; i < min_blocks; ++i)
        {
            result.data_[i] = data_[i] & other.data_[i];
        }

        return result;
    }

    // 求并集（返回新 BitMap）
    BitMap operator|(const BitMap& other) const
    {
        BitMap result(std::max(size_, other.size_));
        size_t min_blocks = std::min(data_.size(), other.data_.size());

        for (size_t i = 0; i < min_blocks; ++i)
        {
            result.data_[i] = data_[i] | other.data_[i];
        }

        // 复制剩余部分
        if (data_.size() > other.data_.size())
        {
            for (size_t i = min_blocks; i < data_.size(); ++i)
            {
                result.data_[i] = data_[i];
            }
        }
        else if (other.data_.size() > data_.size())
        {
            for (size_t i = min_blocks; i < other.data_.size(); ++i)
            {
                result.data_[i] = other.data_[i];
            }
        }

        return result;
    }

    // 求差集（返回新 BitMap）
    BitMap operator-(const BitMap& other) const
    {
        BitMap result(std::max(size_, other.size_));
        size_t min_blocks = std::min(data_.size(), other.data_.size());

        for (size_t i = 0; i < min_blocks; ++i)
        {
            result.data_[i] = data_[i] & ~other.data_[i];
        }

        // 复制剩余部分
        if (data_.size() > other.data_.size())
        {
            for (size_t i = min_blocks; i < data_.size(); ++i)
            {
                result.data_[i] = data_[i];
            }
        }

        return result;
    }

    // 计算置位数量（popcount）
    size_t count() const
    {
        size_t total = 0;
        for (const auto& block : data_)
        {
            total += __builtin_popcountll(block);
        }
        return total;
    }

    // 获取大小
    size_t size() const
    {
        return size_;
    }

    // 调整大小
    void resize(size_t new_size)
    {
        size_t new_blocks = (new_size + 63) / 64;
        data_.resize(new_blocks, 0);
        size_ = new_size;
    }

    // 获取所有置位的索引
    std::vector<size_t> get_set_bits() const
    {
        std::vector<size_t> result;
        result.reserve(count());

        for (size_t i = 0; i < size_; ++i)
        {
            if (test(i))
            {
                result.push_back(i);
            }
        }

        return result;
    }

    // 内存占用（字节）
    size_t memory_usage_bytes() const
    {
        return data_.size() * sizeof(uint64_t);
    }

private:
    std::vector<uint64_t> data_;
    size_t size_;
};

} // namespace flow_ad::indexing
