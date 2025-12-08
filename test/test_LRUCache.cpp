#include <catch2/catch_all.hpp>
#include "LRUCache.hpp"
#include <string>

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}

TEST_CASE("LRUCache Basic Functionality", "[LRUCache]") {
    LRUCache<std::string, int> cache(2);
    cache.put("a", 1);
    cache.put("b", 2);

    int val;
    REQUIRE(cache.get("a", val));
    REQUIRE(val == 1);

    cache.put("c", 3); // Evicts "b"
    REQUIRE_FALSE(cache.get("b", val));
    REQUIRE(cache.get("c", val));
    REQUIRE(val == 3);
}

TEST_CASE("LRUCache Overwrite and Access", "[LRUCache]") {
    LRUCache<std::string, int> cache(2);
    cache.put("x", 10);
    cache.put("y", 20);
    int val;

    cache.put("x", 100); // Overwrite value
    REQUIRE(cache.get("x", val));
    REQUIRE(val == 100);

    cache.put("z", 30); // Evicts "y"
    REQUIRE_FALSE(cache.get("y", val));
}
