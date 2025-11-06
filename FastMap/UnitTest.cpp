// FastMap.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// ReSharper disable CppClangTidyMiscUseAnonymousNamespace
#include "include/LinearMap.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>

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

#define assert_always(expr) \
    do { \
        if (!(expr)) { \
		std::cerr << "Assertion failed: (" #expr ") in " \
                      << __FILE__ << ":" << __LINE__ << std::endl; \
            std::terminate(); \
        } \
    } while (0)

struct MyVector
{
	std::vector<int> data;
};

struct Coordinates
{
	float x = 0;
	float y = 0;
	float z = 0;
};

using namespace LinearProbing;


#if defined(LMAP_DEV)

template <class T>
class DebugMap : public Internal::LinearCoreMapImpl<size_t, T>
{

public:

	[[nodiscard]] unsigned CountCollisions(const size_t& key)
	{
		unsigned count = 0;
		auto [start, last_index] = this->GetSlot(key, this->m_data_size);
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!this->m_used[i])
				return count;

			if (this->m_keys[i] == key)
				return count;

			++count;
		}
	}

	void PrintHashDistribution() const
	{
		std::cout << this->m_data_size << "\n";
		std::ofstream file("F:\\map.txt", std::ios::binary);

		if (!file.is_open())
			return;

		for (auto i = 0ull; i < this->m_data_size; i++)
		{
			if (!this->m_used[i])
			{
				constexpr char val[] = { 32, 32, 48 };
				file.write(val, 3);
				continue;
			}

			constexpr char val2[] = { 32, 32, 49 };
			file.write(val2, 3);
		}

		file.close();
	}
};

#endif

NO_OPTIMIZE_BEGIN
static void TestBasic()
{
	LinearMap<int> map(8); // small initial size to force resize

	auto v = map.GetOrCreate(1, []
		{
			return 99887;
		});

	map.GetOrCreate(1, 123);
	assert_always(v == 99887);
	assert_always(map[1] == 99887);

	std::tuple<size_t, int> tuple(556644, 2323323);
	map.Emplace(std::move(tuple));

	auto& yes_exist = map.Get(1);
	auto& not_exist = map.Get(2);

	// not_exist = 666; // BAD!!

	assert_always(map.IsValid(yes_exist));
	assert_always(!map.IsValid(not_exist));

	map.Erase(1);

	map.TryEmplace(1, [] { return 123; });
	assert_always(map.Get(1) == 123);
	map.TryEmplace(1, 456); // should fail
	assert_always(map.Get(1) == 123);

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
		int& val = map.GetOrCreate(i, [i]
			{
				return (int)(i * 7);
			});

		assert_always(val == (int)(i * 7));
		val += 1; // test reference
		auto check = map.Get(i);
		assert_always(check && check == (int)(i * 7 + 1));
	}

	map.Erase(16);
	auto e1 = map.TryEmplace(16, 123);
	auto e2 = map.TryEmplace(16, 123);
	assert_always(e2 != e1);

	LinearCoreMap<std::string, int> str_map;
	str_map.Emplace("hello", 321);
	auto& v1 = str_map.Get("hello");

	for (auto [key, value] : str_map)
	{
		value = 444;
	}

	assert_always(v1 == 444);

	LinearSet<int> set;

	for (int i = 0; i < 99; ++i)
	{
		set.Emplace(i);
	}

	set.Rehash(1024);

	for (int i = 0; i < 99; ++i)
	{
		assert_always(set.Contains(i));
	}

	for (int i = 100; i < 200; ++i)
	{
		assert_always(!set.Contains(i));
	}

	std::vector<std::string> keys;
	for (int i = 0; i < 36; ++i)
	{
		keys.push_back("Key" + std::to_string(i));
	}

	LinearSet<std::string> set2(keys);
	LinearSet<std::string> set3;

	set3 = set2; // test copy assignment

	for (int i = 0; i < 36; ++i)
	{
		assert_always(set2.Contains("Key" + std::to_string(i)));
	}

	auto characters = 0;
	for (auto& str : set2)
	{
		characters += str.size();
	}

	assert_always(characters == 170);

	std::cout << "TestBasic passed!\n";
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
static void TestStructMap2()
{
	LinearMap<Coordinates> map;
	size_t key = 0;

	for (int x = 0; x < 16; ++x)
	{
		for (int y = 0; y < 16; ++y)
		{
			for (int z = 0; z < 16; ++z)
			{
				Coordinates pos;
				pos.x = (float)x;
				pos.y = (float)y;
				pos.z = (float)z;

				map.Emplace(key, std::move(pos));
				++key;
			}
		}
	}

	key = 0;

	for (int x = 0; x < 16; ++x)
	{
		for (int y = 0; y < 16; ++y)
		{
			for (int z = 0; z < 16; ++z)
			{
				Coordinates& pos = map.Get(key);

				if (x > 0)
					assert_always(map.IsValid(pos));

				assert_always((int)pos.x == x);
				assert_always((int)pos.y == y);
				assert_always((int)pos.z == z);

				++key;
			}
		}
	}

	key = 0;

	for (int x = 0; x < 16; ++x)
	{
		for (int y = 0; y < 16; ++y)
		{
			for (int z = 0; z < 16; ++z)
			{
				auto result = map.TryEmplace(key, [x, y, z]
					{
						Coordinates pos;
						pos.x = (float)x;
						pos.y = (float)y;
						pos.z = (float)z;

						return pos;
					});

				assert_always(!result);

				++key;
			}
		}
	}

	std::cout << "TestStructMap2 passed!\n";
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


	LinearCoreMap<std::string, int> map2;

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
	TestBasic();
	TestStructMap();
	TestStructMap2();
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

	for (int j = 0; j < 5; ++j)
	{
		double avg_collision = 0;

		// ===== start timer =====
		auto start = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < num; ++i)
		{
			avg_collision += (double)dbg.CountCollisions(i);
		}

		auto end = std::chrono::high_resolution_clock::now();
		// ===== end timer =====

		std::chrono::duration<double, std::milli> elapsed = end - start;
		std::cout << "Collision counting took: " << elapsed.count() << " ms\n";

		avg_collision /= (double)num;
		std::cout << "Average collisions per key: " << avg_collision << "\n";
	}
}

static void Examples()
{
	LinearMap<std::string> map;

	// Insert
	map.Emplace(0, "zero");
	map[1] = "one";
	map[2] = "two";
	map[123] = "123";

	// Insert tuple
	std::tuple<size_t, std::string> tuple(4444, "all fours");
	map.Emplace(std::move(tuple));

	// Get
	std::string& zero = map[0];
	std::string& two = map[2];

	// Get and modify
	std::string& one = map.Get(1);
	if (map.IsValid(one)) // always check, key could not exist -> modifying default reference breaks the map
		one = "uno";

	// Check if key exist
	assert(map.Contains(0));
	assert(map.Contains(1));
	assert(map.Contains(2));
	assert(!map.Contains(3));
	assert(!map.Contains(4));

	// Emplace a new value, if key does not exist
	map.GetOrCreate(3, []
		{
			return "three";
		});

	// Move an existing value, if key does not exist
	std::string str("four");
	map.GetOrCreate(4, std::move(str));

	// Erase a key
	map.Erase(2);
	assert(!map.Contains(2));

	// Get and check if the return value has valid contents
	const std::string& invalid = map.Get(999);
	if (map.IsValid(invalid))
	{
		// process further ...
	}

	// Emplace multiple keys and values from arrays
	std::vector<size_t> keys = { 10, 20, 30 };
	std::vector<std::string> values = { "ten", "twenty", "thirty" };

	map.EmplaceAll(keys, values);

	// Emplace multiple key-value pairs
	std::vector<std::tuple<size_t, std::string>> tuples;
	tuples.emplace_back(50, "hello");
	tuples.emplace_back(52, "world");

	map.EmplaceAll(tuples);

	// Try inserting. Useful for filtering
	const auto result1 = map.TryEmplace(3, "New Value"); // won't work, returns false
	const auto result2 = map.TryEmplace(12, "New Value"); // works, returns true

	assert(!result1);
	assert(result2);

	// Try inserting with a function/lambda
	map.TryEmplace(61, []
		{
			return "lazy load string";
		});

	// Filter out duplicates
	std::vector<std::string> many_strings;
	many_strings.emplace_back("The dog ate the meat");
	many_strings.emplace_back("The dog ate the meat");
	many_strings.emplace_back("Her name is Lucy");
	many_strings.emplace_back("She likes playing on the field");
	many_strings.emplace_back("She only appears once");
	many_strings.emplace_back("She only appears once");
	many_strings.emplace_back("There you go! :)");

	LinearSet<std::string> filtered_strings;

	for (auto& string : many_strings)
	{
		if (!filtered_strings.TryEmplace(string))
			continue;

		// process further
		std::cout << "String " << string << " was inserted!\n";
	}

	for (auto& string : filtered_strings)
		std::cout << "Unique: " << string << "\n";


	// Adjust map capacity manually, while keeping existing data
	map.Rehash(16); // shrink
	map.Rehash(512); // grow

	// Iterate over all key-value pairs (will not be in insertion order)
	for (auto [key, value] : map)
	{
		std::cout << "Key: " << key << ", Value: " << value << "\n";
	}

	// Clear the map
	map.Reserve(16); // delete all existing elements, pre-allocate space for 16 elements

	map.Emplace(1, "Hi!");
	map.Clear(); // keeps allocated memory for reuse


	// Recommended order:
	// ---------------------------------
	// Contains -> Checks if key exists
	// TryEmplace -> Checks if key exists, inserts new value if not
	// GetOrCreate -> Checks if key exists, inserts new value if not, returns inserted/existing value
	// ---------------------------------
}

NO_OPTIMIZE_BEGIN
int main()
{
	Examples();
	RunAllTests();

#if NDEBUG

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
