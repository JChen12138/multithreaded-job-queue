#include <catch2/catch_all.hpp>
#include "JobQueue.hpp"
#include "ThreadPool.hpp"

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}

TEST_CASE("try_pop returns false when queue is empty") {
    JobQueue queue(10);
    JobQueue::Job job;

    REQUIRE(queue.try_pop(job) == false);
}

TEST_CASE("try_pop retrieves pushed job") {
    JobQueue queue(10);

    JobMetadata meta(1, "test");
    bool executed = false;

    REQUIRE(queue.push({std::move(meta), [&] { executed = true; }}) == true);

    JobQueue::Job job;
    REQUIRE(queue.try_pop(job) == true);

    job.task();
    REQUIRE(executed == true);
}


TEST_CASE("bounded queue blocks when full and resumes after pop") {
    JobQueue queue(1);

    JobMetadata meta1(1, "first");
    JobMetadata meta2(2, "second");

    REQUIRE(queue.push({std::move(meta1), []{}}) == true);

    std::atomic<bool> second_pushed = false;

    std::thread producer([&] {
        second_pushed = queue.push({std::move(meta2), []{}});
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(second_pushed == false); // should still be blocked

    JobQueue::Job job;
    REQUIRE(queue.try_pop(job) == true);

    producer.join();
    REQUIRE(second_pushed == true);
}

TEST_CASE("push after shutdown does not add job") {
    JobQueue queue(10);
    queue.shutdown();

    JobMetadata meta(1, "test");
    REQUIRE(queue.push({std::move(meta), []{}}) == false);

    REQUIRE(queue.empty());
}


TEST_CASE("pop returns dummy job after shutdown and empty") {
    JobQueue queue(10);
    queue.shutdown();

    auto job = queue.pop();

    REQUIRE(job.metadata.id == -1);
    REQUIRE(queue.empty());
}

TEST_CASE("threadpool completes submitted jobs before shutdown") {
    ThreadPool pool(2, 10);

    std::atomic<int> counter = 0;

    for (int i = 0; i < 5; ++i) {
        pool.submit(JobMetadata(i, "test"), [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            counter++;
        });
    }

    pool.shutdown(5);

    REQUIRE(counter == 5);
}

TEST_CASE("shutdown is idempotent") {
    ThreadPool pool(2, 10);

    pool.shutdown();
    pool.shutdown();
    pool.shutdown();

    REQUIRE_NOTHROW(pool.shutdown());
}

TEST_CASE("submit after shutdown throws") {
    ThreadPool pool(2, 10);
    pool.shutdown();

    REQUIRE_THROWS_AS(
        pool.submit(JobMetadata(1, "late"), []{}),
        std::runtime_error
    );
}

TEST_CASE("submit with future returns correct value") {
    ThreadPool pool(2, 10);

    auto future = pool.submit(
        JobMetadata(1, "compute"),
        [] { return 42; }
    );

    REQUIRE(future.get() == 42);

    pool.shutdown();
}

TEST_CASE("submit with future void works") {
    ThreadPool pool(2, 10);

    auto future = pool.submit(
        JobMetadata(1, "void"),
        [] {}
    );

    REQUIRE_NOTHROW(future.get());

    pool.shutdown();
}

TEST_CASE("job retries until success") {
    ThreadPool pool(1, 10);

    std::atomic<int> attempts = 0;

    JobMetadata meta(1, "retry");
    meta.allow_retry = true;
    meta.max_retries = 3;

    pool.submit(std::move(meta), [&] {
        attempts++;
        if (attempts < 3) {
            throw std::runtime_error("fail");
        }
    });

    pool.shutdown(5);

    REQUIRE(attempts == 3);
}

TEST_CASE("running job is allowed to finish even if timeout budget is smaller") {
    ThreadPool pool(1, 10);

    JobMetadata meta(1, "timeout");
    meta.timeout = std::chrono::milliseconds(50);

    pool.submit(std::move(meta), [] {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });

    pool.shutdown(2);

    SUCCEED();  // just ensure no crash or deadlock
}

TEST_CASE("expired job is skipped before execution") {
    ThreadPool pool(1, 10);

    std::atomic<bool> blocker_started = false;
    std::atomic<bool> allow_blocker_exit = false;
    std::atomic<bool> expired_executed = false;

    pool.submit(JobMetadata(1, "blocker"), [&] {
        blocker_started = true;
        while (!allow_blocker_exit.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    while (!blocker_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    JobMetadata expiring(2, "expires");
    expiring.timeout = std::chrono::milliseconds(20);

    pool.submit(std::move(expiring), [&] {
        expired_executed = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    allow_blocker_exit = true;

    pool.shutdown(2);

    REQUIRE(expired_executed == false);
}

TEST_CASE("expired future job completes with exception") {
    ThreadPool pool(1, 10);

    std::atomic<bool> blocker_started = false;
    std::atomic<bool> allow_blocker_exit = false;

    pool.submit(JobMetadata(1, "blocker"), [&] {
        blocker_started = true;
        while (!allow_blocker_exit.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    while (!blocker_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    JobMetadata expiring(2, "expires_future");
    expiring.timeout = std::chrono::milliseconds(20);

    auto future = pool.submit(std::move(expiring), []() -> int {
        return 42;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    allow_blocker_exit = true;

    pool.shutdown(2);

    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
}


