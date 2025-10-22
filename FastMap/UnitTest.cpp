// FastMap.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// ReSharper disable CppClangTidyMiscUseAnonymousNamespace
#include "include/LinearMap.h"

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

static void assert2(bool expression)
{
#if _DEBUG
	assert(expression);
#else
    if (!expression)
    {
        std::cerr << "Assertion failed!\n";
        std::terminate();
	}
#endif
}

NO_OPTIMIZE_BEGIN
static void TestIntMap()
{
    UnitTest<int> map(8); // small initial size to force resize
    auto val = map.GetOrCreate(1, []
    {
	    return 99887;
    });
    assert2(val == 99887);

    // Insert and get
    for (size_t i = 1; i <= 20; i++)
    {
		const auto key = i * 1234;
		map.Put(key, (int)i);
		auto val = map.Get(key);
		assert2(val == (int)i);
    }

    map.Rehash(32);

    for (size_t i = 1; i <= 20; ++i)
    {
        assert2(map.Get(i * 1234) == i);
    }

    map.Rehash(512);

	for (size_t i = 1; i <= 20; ++i)
    {
        assert2(map.Get(i * 1234) == i);
    }

    map.Rehash(64);

	// Overwrite
	for (size_t i = 1; i <= 20; i++)
	{
		map.Put(i, (int)(i * 100));
		auto val = map.Get(i);
		assert2(val && val == (int)(i * 100));
	}

	// Get non-existing
	assert2(!map.Get(999));
	assert2(!map.Contains(999));

	// GetOrCreate
	for (size_t i = 21; i <= 25; i++)
	{
		int& val = map.GetOrCreate(i, [i]() { return (int)(i * 7); });
		assert2(val == (int)(i * 7));
		val += 1; // test reference
		auto check = map.Get(i);
		assert2(check && check == (int)(i * 7 + 1));
	}

    map.Erase(16);
    auto e1 = map.TryEmplace(16, 123);
    auto e2 = map.TryEmplace(16, 123);
    assert2(e2 != e1);

    std::cout << "TestIntMap passed!\n";
}
NO_OPTIMIZE_END

// Struct/object test
NO_OPTIMIZE_BEGIN
static void TestStructMap()
{
    UnitTest<MyVector> map;

    MyVector vec;
    vec.data.push_back(1);
    vec.data.push_back(2);
    vec.data.push_back(3);

    map.Put(42, std::move(vec));

    // vec should be empty after move
    assert2(vec.data.empty());

    MyVector& v = map.Get(42);
    assert2(v.data.size() == 3);
    assert2(v.data[1] == 2);

    // modify retrieved value
    v.data.clear();
    assert2(map.Get(42).data.empty());

    UnitTest<std::vector<int>> map2;
    std::vector<int> vec2;
    vec2.push_back(12);
    vec2.push_back(777);
    map2.Put(2012, std::move(vec2)); // should be moved
    assert2(vec2.empty());
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
    UnitTest<int> map(16);
    std::mt19937 rng(1234);
    std::uniform_int_distribution<size_t> dist(1, 1000);

    for (int iter = 0; iter < 1000; iter++)
    {
        size_t key = dist(rng);
        map.Put(key, (int)key);

        auto val = map.Get(key);
        assert2(val && val == (int)key);
    }

    std::cout << "TestRandomStress passed!\n";
}
NO_OPTIMIZE_END

// Iterator test
NO_OPTIMIZE_BEGIN
static void TestIterator()
{
    UnitTest<int> map(8);
    for (int i = 1; i <= 10; i++)
        map.Put(i, i * 10);

    int sum = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        auto [k, v] = *it;
        sum += (int)v;
    }

    // Sum should be 10*11/2 * 10 = 550
    assert2(sum == 550);

    std::cout << "TestIterator passed!\n";
}
NO_OPTIMIZE_END

// Erase test
NO_OPTIMIZE_BEGIN
static void TestErase()
{
    UnitTest<MyVector> map2(8);
    for (size_t i = 1; i <= 10; i++)
    {
        MyVector vec;
        vec.data.push_back((i + 1) * 4);
        vec.data.push_back((i + 2) * 4);
        map2.Put(i, std::move(vec));
    }


    //map2.Erase(4);
    map2.Erase(8);
    map2.Erase(9);

    //should_equal(!map2.Contains(4));
    assert2(!map2.Contains(8));
    assert2(!map2.Contains(9));


    UnitTest<int> map(8);
    for (size_t i = 1; i <= 9; i++)
    {
	    map.Put(i, i * 2);
    }

    assert2(map.Get(2) == 4);
    map.Erase(2);
	assert2(!map.Get(2)); // 2 and 8 share same hash index
    assert2(map.Get(8) == 16);

    map.Clear();
	
    std::cout << "TestErase passed!\n";
}
NO_OPTIMIZE_END

NO_OPTIMIZE_BEGIN
static void BenchmarkLinearMapVsUnorderedMap()
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
    UnitTest<int> lmap(1024);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_elements; ++i)
        lmap.Put(keys[i], values[i]);

    auto t1 = std::chrono::high_resolution_clock::now();
    double linear_put = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    size_t found = 0;
    for (size_t i = 0; i < num_elements; ++i)
        if (lmap.Contains(i))
            found++;

    t1 = std::chrono::high_resolution_clock::now();
    double linear_contains = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    long long sum = 0;
    for (size_t i = 0; i < num_elements; ++i)
    {
        auto val = lmap.Get(i);
        if (val) 
            sum += val;
    }

    t1 = std::chrono::high_resolution_clock::now();
    double linear_get = std::chrono::duration<double, std::milli>(t1 - t0).count();

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
        if (umap.contains(i))
            found++;

    t1 = std::chrono::high_resolution_clock::now();
    double unordered_contains = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    sum = 0;
    for (size_t i = 0; i < num_elements; ++i)
        sum += umap[i];

    t1 = std::chrono::high_resolution_clock::now();
    double unordered_get = std::chrono::duration<double, std::milli>(t1 - t0).count();

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
    TestErase();

    std::cout << "All tests passed successfully!\n";
}

static size_t Hasher(const double& key)
{
    return std::hash<double>{}(key);
}

static void HashTest()
{
    DebugMap<int> dbg;
    constexpr size_t num = 1'000'000;
    for (size_t i = 0; i < num; ++i)
    {
        dbg.Put(i, i * 2 + 31);
    }

    for (size_t i = 0; i < num; ++i)
    {
        assert2(dbg.Get(i) == i * 2 + 31);
    }

    for (int i = 0; i < 5; ++i)
    {
        double avg_collision = 0;
        double num_collision = 0;

        // ===== start timer =====
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < num; ++i)
        {
            auto col = dbg.CountCollisions(i);
            avg_collision += col;
            num_collision++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        // ===== end timer =====

        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "Collision counting took: " << elapsed.count() << " ms\n";

        avg_collision /= num_collision;
        std::cout << "Average collisions per key: " << avg_collision << "\n";
    }
}

NO_OPTIMIZE_BEGIN
int main()
{
    RunAllTests();
  
#if _DEBUG



#else
  
    HashTest();
    BenchmarkLinearMapVsUnorderedMap();
#endif
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
