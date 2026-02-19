#pragma once

class DummyCounter {
public:
    void Increment() {}
};

class DummyGauge {
public:
    void Increment() {}
    void Decrement() {}
    double Value() const { return 0; }
};

class DummyHistogram {
public:
    void Observe(double) {}
};

class Metrics {
public:
    static Metrics& instance() {
        static Metrics instance;
        return instance;
    }

    void init(const std::string& = "/metrics") {}

    DummyCounter& job_submitted() { return counter_; }
    DummyCounter& job_completed() { return counter_; }
    DummyCounter& job_failed()    { return counter_; }
    DummyGauge&   active_jobs()   { return gauge_; }
    DummyHistogram& job_latency() { return histogram_; }

private:
    DummyCounter counter_;
    DummyGauge gauge_;
    DummyHistogram histogram_;
};
