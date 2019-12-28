#include "analyze.hpp"

#include <iostream>
#include <iomanip>

#include <future>
#include <mutex>

#include <deque>
#include <string>
#include <vector>

#include <algorithm>
#include <functional>

#include <random>

#include <stdexcept>

#include <cstring>

// re-generate random numbers
// 361.60user 2.60system 2:07.55elapsed 285%CPU (0avgtext+0avgdata 785952maxresident)k
// 0inputs+0outputs (0major+197210minor)pagefaults 0swaps
// vs re-use
// 278.93user 2.23system 1:45.08elapsed 267%CPU (0avgtext+0avgdata 785396maxresident)k
// 0inputs+0outputs (0major+197064minor)pagefaults 0swaps

// async
// 270.71user 0.60system 0:39.88elapsed 680%CPU (0avgtext+0avgdata 85296maxresident)k
// 0inputs+0outputs (0major+22030minor)pagefaults 0swaps
// vs deferred
// 147.26user 0.21system 2:28.45elapsed 99%CPU (0avgtext+0avgdata 80184maxresident)k
// 0inputs+0outputs (0major+20145minor)pagefaults 0swaps
// vs thread pool
// 252.21user 0.43system 0:43.57elapsed 579%CPU (0avgtext+0avgdata 80412maxresident)k
// 0inputs+0outputs (0major+20229minor)pagefaults 0swaps


using std::cout;
using std::ostream;
using std::flush;
using std::left;
using std::right;
using std::setw;

using std::future;

using std::deque;
using std::function;
using std::vector;

using std::pair;
using std::make_pair;

std::mutex cout_mx;

#if 0
#define LOG(what) { \
        std::lock_guard<std::mutex> lock(cout_mx);                            \
    cout << what << "\n";                                                     \
    } while (false)

#define LOG_SAMPLE(what) \
    LOG("S(" << sample.size() << "/" << report.buckets << ')' << ": " << what)
#else
#define LOG_SAMPLE(_)
#endif

struct report_t {
    size_t attempts = 0;
    size_t buckets = 0;
    size_t collisions = 0;
    hash_t hash;
};

const size_t MAX_ATTEMPTS = 10 * 1000 * 1000;
sample_t candidates(MAX_ATTEMPTS);

auto study(const sample_t& sample, int extra) {
    const auto l2n = extra + log2_bucket_size(sample.size());
    report_t report;
    report.buckets = 1 << l2n;
    test_t test;
    auto threshold = 1000;
    auto threshold_exp = 3;

    for (int i = 0; i < MAX_ATTEMPTS; ++i) {
        ++report.attempts;
        if (report.attempts % threshold == 0) {
            LOG_SAMPLE("1e" << threshold_exp << " attempts...");
            threshold *= 10;
            ++threshold_exp;
        }

        hash_t hash(candidates[i], l2n);
        auto collisions = test(sample, hash);

        if (!collisions) {
            report.hash = hash;
            LOG_SAMPLE("SUCCESS");
            return report;
        }

        report.collisions += collisions;
    }

    LOG_SAMPLE("FAILURE");
    return report;
}

auto study_async(const sample_t& sample, int extra) {
    return std::async(
        std::launch::async,
        [&sample, extra]() { return study(sample, extra); });
}

struct col_iter_t;

struct col_defs_t {
    struct col {
        const char* title;
        size_t width;
    };

    using col_vector = std::vector<col>;
    col_vector cols;

    col_defs_t& add(const char* title, size_t width = 0) {
        cols.push_back({title, std::max(1 + std::strlen(title), width)});
        return *this;
    }

    const col_defs_t& repeat(size_t n) {
        cols.push_back({nullptr, n});
        return *this;
    }

    std::ostream& header(std::ostream& os, size_t repeat = 0) const;

    col_iter_t operator ()() const;
};

struct col_iter_t {
    col_defs_t::col_vector::const_iterator iter, last;
    explicit col_iter_t(
        col_defs_t::col_vector::const_iterator iter,
        col_defs_t::col_vector::const_iterator last)
    : iter(iter), last(last) {}
};

inline col_iter_t col_defs_t::operator ()() const {
    return col_iter_t(cols.begin(), cols.end());
}

std::ostream& operator<<(std::ostream& os, col_iter_t& iter) {
    if (iter.iter == iter.last) {
        throw std::runtime_error("fields exhuasted");
    }

    if (iter.iter->title == nullptr) {
        iter.iter -= iter.iter->width;
    }

    return os << setw(iter.iter++->width);
}

std::ostream& col_defs_t::header(std::ostream& os, size_t repeat) const {
    auto col = (*this)();

    for (int i = 0; i < cols.size(); ++i) {
        if (cols[i].title == nullptr) {
            if (!repeat--)
                break;
            i -= cols[i].width;
        }

        os << col << cols[i].title;
    }

    return os;
}

ostream& operator <<(
    ostream& os,
    const std::pair<col_iter_t*, report_t>  col_report
    ) {
    auto& col = *col_report.first;
    auto& report = col_report.second;
    return os << col << report.buckets
              << col << "NY"[report.hash.is_valid()]
              << col << report.attempts
              << col << report.collisions
        ;

}

long delta_pc(size_t a, size_t b) {
    return 100 * (double(a) - b) / b;
}

//#define USE_FUTURES

#ifdef USE_FUTURES
#define WITH_FUTURES(x) x
#define WITH_THREADS(x)
#else
#define WITH_FUTURES(x)
#define WITH_THREADS(x) x
#endif

class multi_threaded_executor {
  public:
    multi_threaded_executor()
    : batches(std::thread::hardware_concurrency()) {
    }

    void add(function<void()> job) {
        batches[batch++ % batches.size()].push_back(job);
    }

    void execute() {
        vector<std::thread> threads(std::thread::hardware_concurrency());
        std::transform(
            batches.begin(), batches.end(),
            threads.begin(),
            [](auto& batch) {
                return std::thread(
                    [&batch]() {
                        for (auto job : batch) {
                            job();
                        }});
            });
        for (auto& t : threads) {
            t.join();
        }
    }

  private:
    vector< vector< function<void()> > > batches;
    int batch = 0;
};

int main(int argc, const char** argv) {

    cout << "hardware concurrency: " << std::thread::hardware_concurrency() << "\n";
    std::deque< sample_t > samples, sorted_samples;

    std::default_random_engine rnd{13081963};
    std::uniform_int_distribution<std::uintptr_t> uniform_dist;

    for (auto& candidate : candidates) {
        candidate = uniform_dist(rnd);
    }

    //argc = 5;
    const auto fn_first = argv + 1;

    int report_last = 0;
    const int
        extra_0 = report_last++,
        extra_1 = report_last++,
        extra_2 = report_last++,
        extra_3 = report_last++,
        extra_2_sorted = report_last++;

    deque< deque<report_t> > results;

    WITH_FUTURES(vector< vector< future<report_t> > > future_results);
    WITH_THREADS(multi_threaded_executor executor);

    for (auto fn_iter = fn_first, fn_last = fn_iter + argc - 1; fn_iter != fn_last; ++fn_iter) {
        samples.push_back(read_sample(*fn_iter));
        auto& sample = samples.back();

        sorted_samples.emplace_back(sample);
        auto& sorted_sample = sorted_samples.back();
        std::sort(sorted_sample.begin(), sorted_sample.end());

#ifdef USE_FUTURES
        future_results.emplace_back();
        auto& futures = future_results.back();
        futures.emplace_back(study_async(sample, 0));
        futures.emplace_back(study_async(sample, 1));
        futures.emplace_back(study_async(sample, 2));
        futures.emplace_back(study_async(sample, 3));
        futures.emplace_back(study_async(sorted_samples.back(), 2));
#else
        results.emplace_back();
        auto& reports = results.back();
        reports.resize(report_last);
        auto iter = reports.begin();

        executor.add(
            [report_iter = iter++, &sample]() {
                *report_iter = study(sample, 0);
            });

        executor.add(
            [report_iter = iter++, &sample]() {
                *report_iter = study(sample, 1);
            });

        executor.add(
            [report_iter = iter++, &sample]() {
                *report_iter = study(sample, 2);
            });

        executor.add(
            [report_iter = iter++, &sample]() {
                *report_iter = study(sample, 3);
            });

        executor.add(
            [report_iter = iter++, &sorted_sample]() {
                *report_iter = study(sorted_sample, 2);
            });
#endif
    }

    WITH_FUTURES(
        std::transform(
            future_results.begin(), future_results.end(),
            std::back_inserter(results),
            [](auto& vec) {
                deque<report_t> reports;
                std::transform(
                    vec.begin(), vec.end(),
                    std::back_inserter(reports),
                    std::mem_fn(&future<report_t>::get)
                    );
                return reports;
            }));

    WITH_THREADS(executor.execute());

    {
        auto sample_iter = samples.begin();
        auto fn_iter = fn_first;

        col_defs_t cols;
        cols.add("sample")
            .add("size", 5)
            .add("#buck", 6)
            .add("?", 2)
            .add("#try", 12)
            .add("#coll", 12)
            .repeat(4)
            .header(cout, 3)
            << "\n";

        for (auto& result : results) {
            auto col = cols();
            cout << col << std::string(*fn_iter, *fn_iter + 6) << col << sample_iter->size();
            cout << make_pair(&col, result[0]);
            cout << make_pair(&col, result[1]);
            cout << make_pair(&col, result[2]);
            cout << make_pair(&col, result[3]);

            cout << "\n";
            ++sample_iter;
            ++fn_iter;
        }
    }

    {
        cout << "\nDoes sorting reduce the number of collisions?\n";
        auto sample_iter = samples.begin();
        auto fn_iter = fn_first;

        col_defs_t cols;
        cols.add("sample")
            .add("size", 5)
            .add("#buck", 6)
            .add("?", 2)
            .add("%coll1", 12)
            .add("%coll2", 12)
            .add("%delta", 14)
            .header(cout)
            << "\n";

        for (auto& result : results) {
            auto col = cols();
            cout << col << std::string(*fn_iter, *fn_iter + 6) << col << sample_iter->size();
            auto& res1 = result[extra_2];
            auto& res2 = result[extra_2_sorted];
            cout << col << res1.buckets
                 << col << "NY"[res1.hash.is_valid()]
                 << col << res1.collisions
                 << col << res2.collisions
                 << col;

            if (res1.collisions)
                cout << delta_pc(res2.collisions, res1.collisions);
            else
                cout << "n/a";

            cout << "\n";
            ++sample_iter;
            ++fn_iter;
        }
    }

    return 0;
}
