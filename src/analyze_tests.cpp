#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <sstream>
#include <iostream>
#include <iomanip>

#include "analyze.hpp"

TEST_CASE("Reading a sample") {
    std::istringstream is("3\n2\n1\n");
    auto sample = read_sample(is);
    REQUIRE(sample == sample_t({ 3, 2, 1 }));
}

TEST_CASE("Arithmetic") {
    REQUIRE(log2_bucket_size(1) == 0);
    REQUIRE(log2_bucket_size(2) == 1);
    REQUIRE(log2_bucket_size(3) == 2);
    REQUIRE(log2_bucket_size(4) == 2);
    REQUIRE(log2_bucket_size(5) == 3);
    REQUIRE(log2_bucket_size(8) == 3);

    REQUIRE(lowest_bit(sample_t()) == -1);
    REQUIRE(lowest_bit(sample_t({ 4, 6 })) == 1);
    REQUIRE(lowest_bit(sample_t({ 4, 12 })) == 2);
}

TEST_CASE("Hash") {
    REQUIRE(hash_t(3, 2).mult == 3);
    REQUIRE(hash_t(3, 2).shift == (sizeof(address_t) * 8 - 2));
}

TEST_CASE("Enumerate") {
    sample_t sample = { 1, 2, 3 };
    auto gen1 = enumerate(sample);
    auto gen2 = enumerate(sample);
    REQUIRE(gen1() == 1);
    REQUIRE(gen1() == 2);
    REQUIRE(gen2() == 1);
    REQUIRE(gen1() == 3);
    REQUIRE(gen2() == 2);

    bool throws = false;

    try {
        gen1();
    } catch (std::runtime_error) {
        throws = true;
    }

    REQUIRE(throws);
}

TEST_CASE("Test") {
    const auto bits = std::numeric_limits<address_t>::digits;
    {
        sample_t sample = { 0ul, 1ul << (bits - 4), 2ul << (bits - 4), 3ul << (bits - 4) };
        test_t test;
        REQUIRE(test(sample, hash_t(4, 2)) == 0);
        REQUIRE(test(sample, hash_t(4, 2)) == 0);
        REQUIRE(test(sample, hash_t(3, 2)) != 0);
        REQUIRE(test(sample, hash_t(4, 2)) == 0);
    }

    {
        sample_t sample = { 0ul, 0ul, 2ul << (bits - 4), 3ul << (bits - 4) };
        test_t test;
        REQUIRE(test(sample, hash_t(4, 2)) == 2);
    }
}
