#include "analyze.hpp"

#include <algorithm>
#include <numeric>

#include <iomanip>
#include <fstream>

sample_t read_sample(std::istream& is) {
    sample_t sample;
    while (true) {
        address_t address;
        is >> std::hex >> address;
        if (not is)
            break;
        sample.push_back(address);
    }
    return sample;
}

sample_t read_sample(const char* filename) {
    std::ifstream is(filename);
    return read_sample(is);
}

size_t log2_bucket_size(address_t values) {
    size_t bits = 0;
    address_t top = 1;
    while (top < values) {
        ++bits;
        top <<= 1;
    }
    return bits;
}

size_t test_t::operator ()(const sample_t& sample, const hash_t& hash) {
    const auto n = hash.max();

    if (buckets.empty()) {
        buckets.resize(n);
    } else {
        buckets.resize(n);
        std::fill(buckets.begin(), buckets.end(), 0);
    }

    for (int i = 0; i < sample.size(); ++i) {
        if (buckets[hash(sample[i])]++ > 0) {
            return i + 1;
        }
    }

    return 0;
}

int lowest_bit(const sample_t& addresses) {
    address_t ored = std::accumulate(
        addresses.begin(), addresses.end(), address_t(),
        [](auto a, auto b) { return a | b; });
    for (int i = 0; i < std::numeric_limits<address_t>::digits; ++i) {
        if (ored & (1 << i)) {
            return i;
        }
    }

    return -1;
}
