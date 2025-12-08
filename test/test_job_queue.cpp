#include <catch2/catch_all.hpp>
#include "JobQueue.hpp"

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}

TEST_CASE("Basic push and pop", "[JobQueue]") {
    JobQueue queue(10);

    JobMetadata meta(1, "TestJob", 0);
    bool executed = false;

    queue.push(JobQueue::Job{
        std::move(meta),
        [&] { executed = true; }
    });

    auto job = queue.pop();
    job.task();  // Execute the task

    REQUIRE(executed == true);
}

TEST_CASE("try_pop returns false on empty queue", "[JobQueue]") {
    JobQueue queue;

    JobQueue::Job job;
    bool got_job = queue.try_pop(job);

    REQUIRE_FALSE(got_job);
}

TEST_CASE("Shutdown prevents further pop", "[JobQueue]") {
    JobQueue queue;

    queue.shutdown();

    REQUIRE(queue.is_shutdown() == true);
}

