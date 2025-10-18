// FastMap.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "LinearMap.h"

#include <cassert>
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

NO_OPTIMIZE_BEGIN
static void TestIntMap()
{
    LinearMap<int> map(8); // small initial size to force resize

    // Insert and get
    for (size_t i = 1; i <= 1000; i++)
    {
		const auto key = i * 1234;
		map.Put(key, (int)i);
		auto val = map.Get(key);
		assert(val == (int)i);
    }

	// Overwrite
	for (size_t i = 1; i <= 20; i++)
	{
		map.Put(i, (int)(i * 100));
		auto val = map.Get(i);
		assert(val && *val == (int)(i * 100));
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
		assert(check && *check == (int)(i * 7 + 1));
	}

    std::cout << "TestIntMap passed!\n";
}
NO_OPTIMIZE_END

// Struct/object test
NO_OPTIMIZE_BEGIN
static void TestStructMap()
{
    LinearMap<MyVector> map(4);

    for (size_t i = 1; i <= 10; i++)
    {
        MyVector v;
        for (int j = 0; j < 5; j++)
            v.data.push_back((int)(i * 10 + j));

        map.Put(i, v);
    }

    for (size_t i = 1; i <= 10; i++)
    {
        auto val = map.Get(i);
        assert(val && val->data.size() == 5);
        for (int j = 0; j < 5; j++)
            assert(val->data[j] == (int)(i * 10 + j));
    }

    // Test GetOrCreate on struct
    MyVector& v = map.GetOrCreate(11, []() { MyVector mv; mv.data.push_back(42); return mv; });
    assert(v.data.size() == 1 && v.data[0] == 42);

    // Modify through reference
    v.data.push_back(99);
    auto val2 = map.Get(11);
    assert(val2 && val2->data.size() == 2 && val2->data[1] == 99);

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
        assert(val && *val == (int)key);
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

// Run all tests
static void RunAllTests()
{
    TestIntMap();
    TestStructMap();
    TestRandomStress();
    TestIterator();
    std::cout << "All tests passed successfully!\n";
}

int main()
{
    RunAllTests();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
