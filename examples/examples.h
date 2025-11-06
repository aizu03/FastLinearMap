#pragma once
#include <cassert>
#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <format>

#include "LinearMap.h"

namespace MapExamples
{

#pragma region Util

	struct Coordinates
	{
		float x = 0;
		float y = 0;
		float z = 0;
	};

	static size_t CustomHash(const std::string& key)
	{
		size_t hash = 1;
		for (const char chr : key)
			hash += (size_t)chr * 33;

		return hash;
	}

#pragma endregion

	using namespace LinearProbing;

	static void Inserting(LinearMap<std::string>& map)
	{
		// Insert
		map.Emplace(0, "zero");
		map[1] = "one";
		map[2] = "two";

		// Get value for '3', or insert a new value using lambdas/functions
		std::string& val = map.GetOrCreate(3, [] { return "three"; });

		auto str = "Protect nature";

		// Get the value, or insert a new value
		std::string& val2 = map.GetOrCreate(3, str); // val2 will be "three"

		// Insert a tuple
		std::tuple<size_t, std::string> tuple(876, "hello world");
		map.Emplace(std::move(tuple));
	}

	static void GetValues(LinearMap<std::string>& map)
	{
		// Get
		std::string& zero = map[0];
		std::string& two = map[2];
		std::string& three = map.Get(3);
		std::string& one = map.Get(1);

		// Get and modify
		if (map.IsValid(one)) // always check, key could not exist -> modifying default reference breaks the map
			one = "uno";

		assert(map.Get(1) == "uno");
	}

	static void CheckAndTry(LinearMap<std::string>& map)
	{
		// Check if key exist
		assert(map.Contains(0));
		assert(map.Contains(1));
		assert(map.Contains(2));
		assert(!map.Contains(4));
		assert(!map.Contains(5));

		// Get and check if the return value has valid contents. Must be a reference
		const std::string& invalid = map.Get(999);
		if (map.IsValid(invalid))
		{
			// process further ...
		}

		// Try inserting. Useful for filtering
		const bool result1 = map.TryEmplace(3, "New Value"); // won't work, returns false
		const bool result2 = map.TryEmplace(12, "New Value"); // works, returns true

		assert(!result1);
		assert(result2);

		// Try inserting with a function/lambda
		map.TryEmplace(61, [] {return "lazy load string"; });

		map.Clear();

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
			std::cout << "String: " << string << "\n";
		}

		for (auto& string : filtered_strings)
			std::cout << "Unique: " << string << "\n";


		// Recommended order:
		// ---------------------------------
		// Contains -> Checks if key exists
		// TryEmplace -> Checks if key exists, inserts new value if not
		// GetOrCreate -> Checks if key exists, inserts new value if not, returns inserted/existing value
		// ---------------------------------
	}

	static void EraseAndResizing(LinearMap<std::string>& map)
	{
		// Erase by key
		map.Erase(12);
		map.Erase(2);
		assert(!map.Contains(2));

		// Resize map manually, while keeping data
		map.Rehash(16); // shrink
		map.Rehash(512); // grow

		// Clearing
		map.Clear();
		map.Reserve(32); // prepare map size for new data (existing data will be deleted)
	}

	static void BatchOperations(LinearMap<std::string>& map)
	{
		// Emplace multiple tuples
		std::vector<std::tuple<size_t, std::string>> tuples;
		tuples.emplace_back(50, "hello");
		tuples.emplace_back(52, "world");

		map.EmplaceAll(tuples);

		// Emplace multiple keys and values from arrays
		std::vector<size_t> keys = { 10, 20, 30 };
		std::vector<std::string> values = { "ten", "twenty", "thirty" };

		map.EmplaceAll(keys, values);

		// Iterate over all key-value pairs
		for (auto [key, value] : map)
		{
			std::cout << "Key: " << key << ", Value: " << value << "\n";
		}
	}

	static void GenericMap()
	{
		// string, int
		LinearCoreMap<std::string, int> map;

		map.Emplace("House", 123);
		map.Emplace("Key", 456);
		map["Dog"] = 66;
		map["Bird"] = 22;

		map.Erase("Key");

		std::cout << map["House"] << "\n";
		std::cout << map["Dog"] << "\n";
		std::cout << map["Bird"] << "\n";

		// Using a custom hash
		LinearCoreMap<std::string, uint32_t> custom(&CustomHash);

		custom["Car"] = 1;
		custom["Wash"] = 2;


		// byte, struct
		LinearCoreMap<uint8_t, Coordinates> navigation;

		navigation[8] = Coordinates{ 1,2,3 };
		navigation[10] = Coordinates{ 4,5,6 };

		auto print_location = [](Coordinates& location)
			{
				std::cout << std::format("x: {}, y: {}, z: {}\n", location.x, location.y, location.z);
			};

		Coordinates& location1 = navigation.Get(8);
		Coordinates& location2 = navigation.Get(12); // invalid, won't print

		if (navigation.IsValid(location1))
		{
			print_location(location1);
		}

		if (navigation.IsValid(location2))
		{
			print_location(location2);
		}

		Coordinates& location3 = navigation.GetOrCreate(16, []
			{
				return Coordinates{ 10,12,14 };
			});

		print_location(location3);
	}

	static void RunExamples()
	{
		LinearMap<std::string> map;

		Inserting(map);
		GetValues(map);
		CheckAndTry(map);
		EraseAndResizing(map);
		BatchOperations(map);
		GenericMap();
	}
}
