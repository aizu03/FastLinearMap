// FastMap.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "LinearMap.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <string>

#if defined(__clang__)
#   define NO_OPTIMIZE_BEGIN  __attribute__((optnone))
#   define NO_OPTIMIZE_END
#elif defined(_MSC_VER)
#   define NO_OPTIMIZE_BEGIN  __pragma(optimize("", off))
#   define NO_OPTIMIZE_END    __pragma(optimize("", on))
#elif defined(__GNUC__)
#   define NO_OPTIMIZE_BEGIN  __attribute__((optnone))
#   define NO_OPTIMIZE_END
#else
#   define NO_OPTIMIZE_BEGIN
#   define NO_OPTIMIZE_END
#endif

struct MyVector { std::vector<int> data; };
struct MyVector2
{
	std::vector<int> data;
    MyVector2(const MyVector2& other) = delete;
	//MyVector2& operator=(const MyVector2& other) = delete;
	MyVector2() {  }
};

NO_OPTIMIZE_BEGIN
static void TestIntMap()
{
    LinearMap<int> map(8); // small initial size to force resize

    // Insert and get
    for (size_t i = 1; i <= 20; i++)
    {
		const auto key = i * 1234;
		map.Put(key, (int)i);
		auto val = map.Get(key);
		assert(val == (int)i);
    }

    map.Rehash(512);

    for (size_t i = 1; i <= 20; ++i)
    {
        assert(map.Get(i * 1234) == i);
    }

	// Overwrite
	for (size_t i = 1; i <= 20; i++)
	{
		map.Put(i, (int)(i * 100));
		auto val = map.Get(i);
		assert(val && val == (int)(i * 100));
	}

	// Get non-existing
	assert(!map.Get(999));
	assert(!map.Contains(999));

	// GetOrCreate
	for (size_t i = 21; i <= 25; i++)
	{
		int& val = map.GetOrCreate(i, [i]() { return (int)(i * 7); });
		assert(val == (int)(i * 7));
		val += 1; // test reference
		auto check = map.Get(i);
		assert(check && check == (int)(i * 7 + 1));
	}

    std::cout << "TestIntMap passed!\n";
}
NO_OPTIMIZE_END

// Struct/object test
NO_OPTIMIZE_BEGIN
static void TestStructMap()
{
    LinearMap<MyVector> map;

    MyVector vec;
    vec.data.push_back(1);
    vec.data.push_back(2);
    vec.data.push_back(3);

    map.Put(42, std::move(vec));

    // vec should be empty after move
    assert(vec.data.empty());

    MyVector& v = map.Get(42);
    assert(v.data.size() == 3);
    assert(v.data[1] == 2);

    // modify retrieved value
    v.data.clear();
    assert(map.Get(42).data.empty());

    LinearMap<std::vector<int>> map2;
    std::vector<int> vec2;
    vec2.push_back(12);
    vec2.push_back(777);
    map2.Put(2012, std::move(vec2)); // should be moved
    assert(vec2.empty());
    auto& retrieved = map2.Get(2012);
    auto& retrieved3 = map2.Get(2013); 
    retrieved.clear();
    auto& retrieved2 = map2.Get(2012); 

    std::cout << "TestStructMap passed!\n";
}
NO_OPTIMIZE_END

// Stress test with random inserts
NO_OPTIMIZE_BEGIN
static void TestRandomStress()
{
    LinearMap<int> map(16);
    std::mt19937 rng(1234);
    std::uniform_int_distribution<size_t> dist(1, 1000);

    for (int iter = 0; iter < 1000; iter++)
    {
        size_t key = dist(rng);
        map.Put(key, (int)key);

        auto val = map.Get(key);
        assert(val && val == (int)key);
    }

    std::cout << "TestRandomStress passed!\n";
}
NO_OPTIMIZE_END

// Iterator test
NO_OPTIMIZE_BEGIN
static void TestIterator()
{
    LinearMap<int> map(8);
    for (int i = 1; i <= 10; i++)
        map.Put(i, i * 10);

    int sum = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        auto [k, v] = *it;
        sum += (int)v;
    }

    // Sum should be 10*11/2 * 10 = 550
    assert(sum == 550);

    std::cout << "TestIterator passed!\n";
}
NO_OPTIMIZE_END

NO_OPTIMIZE_BEGIN
void BenchmarkLinearMapVsUnorderedMap()
{
    constexpr size_t num_elements = 1'000'000;
    std::mt19937 rng(1234);
    std::uniform_int_distribution<size_t> dist(1, num_elements * 10);

    std::vector<size_t> keys(num_elements);
    for (size_t i = 0; i < num_elements; ++i)
        keys[i] = dist(rng);

    std::vector<int> values(num_elements);
    for (size_t i = 0; i < num_elements; ++i)
        values[i] = (int)i;

    // --- LinearMap ---
    LinearMap<int> lmap(1024);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_elements; ++i)
        lmap.Put(keys[i], values[i]);
    auto t1 = std::chrono::high_resolution_clock::now();
    double linear_put = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    size_t found = 0;
    for (size_t i = 0; i < num_elements; ++i)
        if (lmap.Contains(keys[i])) found++;
    t1 = std::chrono::high_resolution_clock::now();
    double linear_contains = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    long long sum = 0;
    for (size_t i = 0; i < num_elements; ++i)
    {
        auto val = lmap.Get(keys[i]);
        if (val) sum += val;
    }
    t1 = std::chrono::high_resolution_clock::now();
    double linear_get = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // sanity check
    assert(found == num_elements);

    // --- std::unordered_map ---
    std::unordered_map<size_t, int> umap;

    t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_elements; ++i)
        umap[keys[i]] = values[i];
    t1 = std::chrono::high_resolution_clock::now();
    double unordered_put = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    found = 0;
    for (size_t i = 0; i < num_elements; ++i)
        if (umap.find(keys[i]) != umap.end()) found++;
    t1 = std::chrono::high_resolution_clock::now();
    double unordered_contains = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    sum = 0;
    for (size_t i = 0; i < num_elements; ++i)
        sum += umap[keys[i]];
    t1 = std::chrono::high_resolution_clock::now();
    double unordered_get = std::chrono::duration<double, std::milli>(t1 - t0).count();

    assert(found == num_elements);

    // --- Print results neatly ---
    std::cout << "\n--- Benchmark Results (" << num_elements << " elements) ---\n\n";

    std::cout << "Operation\tLinearMap(ms)\tunordered_map(ms)\tSpeedup\n";
    std::cout << "Put\t\t" << linear_put << "\t\t" << unordered_put << "\t\t"
        << unordered_put / linear_put << "x\n";
    std::cout << "Contains\t" << linear_contains << "\t\t" << unordered_contains << "\t\t"
        << unordered_contains / linear_contains << "x\n";
    std::cout << "Get\t\t" << linear_get << "\t\t" << unordered_get << "\t\t"
        << unordered_get / linear_get << "x\n";


}
NO_OPTIMIZE_END

// Run all tests
static void RunAllTests()
{
    TestIntMap();
    TestStructMap();
    TestRandomStress();
    TestIterator();
    std::cout << "All tests passed successfully!\n";
}

NO_OPTIMIZE_BEGIN
int main()
{
	RunAllTests();
    BenchmarkLinearMapVsUnorderedMap();
}
NO_OPTIMIZE_END

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
