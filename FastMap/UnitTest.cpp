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

#define assert_always(expr) \
    do { \
        if (!(expr)) { \
		std::cerr << "Assertion failed: (" #expr ") in " \
                      << __FILE__ << ":" << __LINE__ << std::endl; \
            std::terminate(); \
        } \
    } while (0)

NO_OPTIMIZE_BEGIN
static void TestIntMap()
{
    LinearMap<int> map(8); // small initial size to force resize

    auto val = map.GetOrCreate(1, []
    {
	    return 99887;
    });
    assert_always(val == 99887);

    map[789] = 123456;
	auto operator_value = map[789];
	assert_always(operator_value == 123456);

    // Insert and get
    for (size_t i = 1; i <= 20; i++)
    {
		const auto key = i * 1234;
		map.Emplace(key, (int)i);
		auto val = map.Get(key);
		assert_always(val == (int)i);
    }

    map.Rehash(32);

    for (size_t i = 1; i <= 20; ++i)
    {
        assert_always(map.Get(i * 1234) == i);
    }

    map.Rehash(512);

	for (size_t i = 1; i <= 20; ++i)
    {
        assert_always(map.Get(i * 1234) == i);
    }

    map.Rehash(64);

	// Overwrite
	for (size_t i = 1; i <= 20; i++)
	{
		map.Emplace(i, (int)(i * 100));
		auto val = map.Get(i);
		assert_always(val && val == (int)(i * 100));
	}

	// Get non-existing
	assert_always(!map.Get(999));
	assert_always(!map.Contains(999));

	// GetOrCreate
	for (size_t i = 21; i <= 25; i++)
	{
		int& val = map.GetOrCreate(i, [i]() { return (int)(i * 7); });
		assert_always(val == (int)(i * 7));
		val += 1; // test reference
		auto check = map.Get(i);
		assert_always(check && check == (int)(i * 7 + 1));
	}

    map.Erase(16);
    auto e1 = map.TryEmplace(16, 123);
    auto e2 = map.TryEmplace(16, 123);
    assert_always(e2 != e1);

    std::cout << "TestIntMap passed!\n";
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void TestStructMap()
{
    LinearMap<MyVector> map;

    MyVector vec;
    vec.data.push_back(1);
    vec.data.push_back(2);
    vec.data.push_back(3);

    map.Emplace(42, std::move(vec));

    // vec should be empty after move
    assert_always(vec.data.empty());

    MyVector& v = map.Get(42);
    assert_always(v.data.size() == 3);
    assert_always(v.data[1] == 2);

    // modify retrieved value
    v.data.clear();
    assert_always(map.Get(42).data.empty());

    LinearMap<std::vector<int>> map2;
    std::vector<int> vec2;
    vec2.push_back(12);
    vec2.push_back(777);
    map2.Emplace(2012, std::move(vec2)); // should be moved
    assert_always(vec2.empty());
    auto& retrieved = map2.Get(2012);
    auto& retrieved3 = map2.Get(2013); 
    retrieved.clear();
    auto& retrieved2 = map2.Get(2012); 

    std::cout << "TestStructMap passed!\n";
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void TestRandomStress()
{
    LinearMap<int> map(16);
    std::mt19937 rng(1234);
    std::uniform_int_distribution<size_t> dist(1, 1000);

    for (int iter = 0; iter < 1000; iter++)
    {
        size_t key = dist(rng);
        map.Emplace(key, (int)key);

        auto val = map.Get(key);
        assert_always(val && val == (int)key);
    }

    std::cout << "TestRandomStress passed!\n";
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void TestIterator()
{
    LinearMap<int> map(8);
    for (int i = 1; i <= 10; i++)
        map.Emplace(i, i * 10);

    int sum = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        auto [k, v] = *it;
        sum += (int)v;
    }

    // Sum should be 10*11/2 * 10 = 550
    assert_always(sum == 550);

    std::cout << "TestIterator passed!\n";
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void TestErase()
{
    LinearMap<MyVector> map2(8);
    for (size_t i = 1; i <= 10; i++)
    {
        MyVector vec;
        vec.data.push_back((i + 1) * 4);
        vec.data.push_back((i + 2) * 4);
        map2.Emplace(i, std::move(vec));
    }


    //map2.Erase(4);
    map2.Erase(8);
    map2.Erase(9);

    //should_equal(!map2.Contains(4));
    assert_always(!map2.Contains(8));
    assert_always(!map2.Contains(9));


    LinearMap<int> map(8);
    for (size_t i = 1; i <= 9; i++)
    {
	    map.Emplace(i, i * 2);
    }

    assert_always(map.Get(2) == 4);
    map.Erase(2);
	assert_always(!map.Get(2)); // 2 and 8 share same hash index
    assert_always(map.Get(8) == 16);

    map.Clear();
	
    std::cout << "TestErase passed!\n";
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void TestEmplaceAll()
{
    LinearMap<int> map;
    std::vector<std::tuple<size_t, int>> pairs;
    pairs.emplace_back(1, 99);
    pairs.emplace_back(2, 88);
    pairs.emplace_back(4, 77);
    pairs.emplace_back(5, 66);

    map.EmplaceAll(pairs);

    assert_always(map.Get(4) == 77);
    assert_always(map.Get(5) == 66);


    LinearGenericMap<std::string, int> map2;

	std::vector<std::tuple<std::string, int>> pairs2;

    for (int i = 0; i < 1000; ++i)
    {
		auto key = "key_" + std::to_string(i);
        auto value = i * 2 + 10;
		pairs2.emplace_back(key, value);
    }

    std::vector<std::tuple<std::string, int>> copy1;
    std::vector<std::tuple<std::string, int>> copy2;
	copy1 = pairs2;
	copy2 = pairs2;

    map2.EmplaceAll(pairs2);

    for (int i = 0; i < 1000; ++i)
    {
        auto key = "key_" + std::to_string(i);
        auto expected_value = i * 2 + 10;
        auto val = map2.Get(key);
		assert_always(val && val == expected_value);
    }

    map2.Clear();

    std::vector<std::string> keys;
    std::vector<int> values;
    keys.reserve(copy1.size());
    values.reserve(copy1.size());

    for (auto& [k, v] : copy1)
    {
        keys.push_back(k);
        values.push_back(v);
    }

    map2.EmplaceAll(keys.data(), values.data(), copy1.size());

    for (int i = 0; i < 1000; ++i)
    {
        auto key = "key_" + std::to_string(i);
        auto expected_value = i * 2 + 10;
        auto val = map2.Get(key);
        assert_always(val && val == expected_value);
    }

    map2.Clear();


    std::vector<std::string> keys2;
    std::vector<int> values2;
    keys.reserve(copy2.size());
    values.reserve(copy2.size());

    for (auto& [k, v] : copy2)
    {
        keys2.push_back(k);
        values2.push_back(v);
    }

    map2.EmplaceAll(keys2, values2);

    for (int i = 0; i < 1000; ++i)
    {
        auto key = "key_" + std::to_string(i);
        auto expected_value = i * 2 + 10;
        auto val = map2.Get(key);
        assert_always(val && val == expected_value);
    }

    std::cout << "TestEmplaceAll passed!\n";
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void SafeLinearEmplace(LinearMap<int>& map, size_t key, int value)
{
    map.Emplace(key, value);
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static int SafeLinearGet(LinearMap<int>& map, const size_t key)
{
    return map.Get(key);
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static bool SafeLinearContains(LinearMap<int>& map, const size_t key)
{
    return map.Contains(key);
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static void SafeUnorderedEmplace(std::unordered_map<size_t, int>& map, size_t key, int value)
{
    map[key] = value;
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static int SafeUnorderedGet(std::unordered_map<size_t, int>& map, const size_t key)
{
    return map[key];
}
NO_OPTIMIZE_END
NO_OPTIMIZE_BEGIN
static bool SafeUnorderedContains(std::unordered_map<size_t, int>& map, const size_t key)
{
    return map.contains(key);
}
NO_OPTIMIZE_END


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
    LinearMap<int> lmap(1024);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_elements; ++i)
        SafeLinearEmplace(lmap, i, values[i]);

    auto t1 = std::chrono::high_resolution_clock::now();
    double linear_put = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    size_t found = 0;
    for (size_t i = 0; i < num_elements * 2; ++i)
        if (SafeLinearContains(lmap, i))
            found++;

    t1 = std::chrono::high_resolution_clock::now();
    double linear_contains = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    long long sum = 0;
    for (size_t i = 0; i < num_elements * 2; ++i)
    {
    	sum += SafeLinearGet(lmap, i);
    }

    t1 = std::chrono::high_resolution_clock::now();
    double linear_get = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // --- std::unordered_map ---
    std::unordered_map<size_t, int> umap;

    t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_elements; ++i)
        SafeUnorderedEmplace(umap, i, values[i]);

    t1 = std::chrono::high_resolution_clock::now();
    double unordered_put = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    found = 0;
    for (size_t i = 0; i < num_elements * 2; ++i)
        if (SafeUnorderedContains(umap, i))
            found++;

    t1 = std::chrono::high_resolution_clock::now();
    double unordered_contains = std::chrono::duration<double, std::milli>(t1 - t0).count();

    t0 = std::chrono::high_resolution_clock::now();
    sum = 0;
    for (size_t i = 0; i < num_elements * 2; ++i)
        sum += SafeUnorderedGet(umap, i);

    t1 = std::chrono::high_resolution_clock::now();
    double unordered_get = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // --- Print results neatly ---
    std::cout << "\n--- Benchmark Results (" << num_elements << " elements) ---\n\n";

    std::cout << "Operation\tLinearMap(ms)\tunordered_map(ms)\tSpeedup\n";
    std::cout << "Emplace\t\t" << linear_put << "\t\t" << unordered_put << "\t\t"
        << unordered_put / linear_put << "x\n";
    std::cout << "Contains\t" << linear_contains << "\t\t" << unordered_contains << "\t\t"
        << unordered_contains / linear_contains << "x\n";
    std::cout << "Get\t\t" << linear_get << "\t\t" << unordered_get << "\t\t"
        << unordered_get / linear_get << "x\n";
	std::cout << sum << found << "\n"; // to prevent optimization
}

static void RunAllTests()
{
    TestIntMap();
    TestStructMap();
    TestRandomStress();
    TestIterator();
    TestErase();
    TestEmplaceAll();

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
        dbg.Emplace(i, i * 2 + 31);
    }

    for (size_t i = 0; i < num; ++i)
    {
        assert_always(dbg.Get(i) == i * 2 + 31);
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
