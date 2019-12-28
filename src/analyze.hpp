#ifndef ANALYZE_HPP
#define ANALYZE_HPP

#include <cstdint>
#include <limits>
#include <vector>
#include <stdexcept>

using address_t = std::uintptr_t;
using sample_t = std::vector<address_t>;

sample_t read_sample(const char* filename);
sample_t read_sample(std::istream& is);
size_t log2_bucket_size(address_t values);

struct hash_t {
    hash_t(address_t mult, size_t count)
    : mult(mult), shift(std::numeric_limits<address_t>::digits - count) { }

    explicit hash_t(size_t count)
    : mult(0), shift(std::numeric_limits<address_t>::digits - count) { }

    explicit hash_t()
    : mult(0), shift(0) { }

    size_t operator ()(address_t a) const {
        return (a * mult) >> shift;
    }

    size_t max() const {
        return 1 << (std::numeric_limits<address_t>::digits - shift);
    }

    bool is_valid() const {
        return mult != 0;
    }

    address_t mult;
    size_t shift;
};

struct test_t {
    size_t operator ()(const sample_t& sample, const hash_t& hash);
    std::vector<int> buckets;
};

int lowest_bit(const std::vector<address_t>& addresses);

inline auto enumerate(const sample_t& sample) {
    auto iter = sample.begin(), last = sample.end();
    return [iter, last]() mutable {
               if (iter == last) {
                   throw std::runtime_error("sample exhausted");
               }
               return *iter++;
           };
}

#endif
