/*
------------------------------------------------------------------------------
LinearMap - A Simple Header-Only, Cache-Friendly Linear-Probing Hash Map
------------------------------------------------------------------------------
Author: [aizu03]
License: MIT (free to use, modify, and distribute)

Description:
------------
LinearMap is a fast, and bare bones, open-addressing hash map implementation that uses
*linear probing* rather than the traditional "bucket + linked list" approach
you find in many standard hash maps.

Why is it faster?
-----------------
1. **Cache locality:**
   All keys, values, and usage flags are stored in contiguous arrays.
   Once the CPU begins scanning a cache line, subsequent lookups and
   probes are typically already in cache, drastically reducing the number
   of random memory accesses compared to pointer-heavy bucket maps.

2. **Predictable memory layout:**
   There is no heap fragmentation or pointer indirection  just flat arrays.
   This predictable layout allows modern CPUs to prefetch data more efficiently.

3. **Branch prediction friendly:**
   Linear probing uses simple loops and tight branches, which compilers
   and CPUs optimize extremely well.

Trade-offs:
-----------
- Slightly higher memory usage due to probe slack and the `m_used[]` flags.
- When the load factor approaches the threshold (~0.6), performance may degrade;
  resizing restores O(1) average lookup and insertion.

Examining the assembly shows, for a simple <int> map,
it only uses 2 comparisons during the search loop, inside the "Get" function. Blazing fast!

In short, LinearMap trades a bit more memory for substantially better
real-world performance on modern hardware.

All benchmarking was done on a modern mid-range CPU (Ryzen 7 3700x).
With clang++20 and AVX2 optimizations enabled



--- Benchmark Results (1000000 elements) ---

Operation       LinearMap(ms)   unordered_map(ms)       Speedup
Put             50.8429         244.397         4.8069x
Contains        28.7827         92.914          3.22812x
Get             42.9095         96.8458         2.25698x

--- Benchmark Results (100000 elements) ---

Operation       LinearMap(ms)   unordered_map(ms)       Speedup
Put             5.9485          18.9987         3.19386x
Contains        1.3921          5.0767          3.64679x
Get             2.1118          7.0732          3.34937x

--- Benchmark Results (1000 elements) ---

Operation       LinearMap(ms)   unordered_map(ms)       Speedup
Put             0.0239          0.083           3.4728x
Contains        0.0086          0.0157          1.82558x
Get             0.0113          0.0152          1.34513x

------------------------------------------------------------------------------
*/

#pragma once
#include <tuple>
#include <functional>
#include <iostream>
#include <fstream>
#include <optional>
#include <bit>

#if defined(__clang__)
// Clang/LLVM
#define UNREACHABLE()  __builtin_unreachable()
#elif defined(_MSC_VER)
// Microsoft Visual C++
#define UNREACHABLE() __assume(0)
#elif defined(__GNUC__)
// GCC (optional, same as clang for most cases)
#define UNREACHABLE()  __builtin_unreachable()
#else
#define UNREACHABLE() ((void)0) // fallback
#endif

template <class Value>
class LinearMap
{
	static constexpr double max_load_factor = 0.6;

	std::unique_ptr<size_t[]> m_keys;
	std::unique_ptr<Value[]>  m_values;
	std::unique_ptr<bool[]>   m_used;

	std::unique_ptr<size_t[]> m_keys_new;
	std::unique_ptr<Value[]>  m_values_new;
	std::unique_ptr<bool[]>   m_used_new;

	size_t m_count = 0;
	size_t m_data_size = 0;

	Value default_value{};

public:

	// Default constructor
	explicit LinearMap(size_t capacity = 128)
	{
		capacity = EnsureSize(capacity);
		m_keys = std::make_unique<size_t[]>(capacity);
		m_values = std::make_unique<Value[]>(capacity);
		m_used = std::make_unique<bool[]>(capacity);
		m_data_size = capacity;
	}

	// Destructor - now defaulted because smart pointers manage memory automatically
	~LinearMap() = default;

	// Copy constructor (deep copy)
	LinearMap(const LinearMap& other)
		: m_keys(std::make_unique<size_t[]>(other.m_data_size)),
		m_values(std::make_unique<Value[]>(other.m_data_size)),
		m_used(std::make_unique<bool[]>(other.m_data_size)),
		m_count(other.m_count),
		m_data_size(other.m_data_size)
	{
		std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
		std::copy_n(other.m_values.get(), other.m_data_size, m_values.get());
		std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());
	}

	// Copy assignment (deep copy)
	LinearMap& operator=(const LinearMap& other)
	{
		if (this == &other)
			return *this;

		m_count = other.m_count;
		m_data_size = other.m_data_size;

		m_keys = std::make_unique<size_t[]>(other.m_data_size);
		m_values = std::make_unique<Value[]>(other.m_data_size);
		m_used = std::make_unique<bool[]>(other.m_data_size);

		std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
		std::copy_n(other.m_values.get(), other.m_data_size, m_values.get());
		std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());

		return *this;
	}

	// Move constructor
	LinearMap(LinearMap&& other) noexcept
		: m_keys(std::move(other.m_keys)),
		m_values(std::move(other.m_values)),
		m_used(std::move(other.m_used)),
		m_keys_new(std::move(other.m_keys_new)),
		m_values_new(std::move(other.m_values_new)),
		m_used_new(std::move(other.m_used_new)),
		m_count(other.m_count),
		m_data_size(other.m_data_size)
	{
		other.m_count = 0;
		other.m_data_size = 0;
	}

	// Move assignment
	LinearMap& operator=(LinearMap&& other) noexcept
	{
		if (this == &other)
			return *this;

		m_keys = std::move(other.m_keys);
		m_values = std::move(other.m_values);
		m_used = std::move(other.m_used);

		m_keys_new = std::move(other.m_keys_new);
		m_values_new = std::move(other.m_values_new);
		m_used_new = std::move(other.m_used_new);

		m_count = other.m_count;
		m_data_size = other.m_data_size;

		other.m_count = 0;
		other.m_data_size = 0;

		return *this;
	}

	/*
	Value* operator[](const size_t key)
	{
		return Get(key);
	}

	const Value* operator[](const size_t key) const
	{
		//return Get(key);
	}
	*/

	void Clear()
	{
		// simply reset usage flags and count, but keep allocated memory
		std::fill_n(m_used.get(), m_data_size, false);
		m_count = 0;
	}

	[[nodiscard]] size_t Size() const
	{
		return m_count;
	}

	[[nodiscard]] size_t Capacity() const
	{
		return m_data_size;
	}

	[[nodiscard]] double GetLoadFactor() const
	{
		return static_cast<double>(m_count) / static_cast<double>(m_data_size);
	}

	[[nodiscard]] bool Contains(const size_t key)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return false;

			if (m_keys[i] == key)
				return true;
		}
	}

	[[nodiscard]] Value& Get(const size_t key)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return default_value;

			if (m_keys[i] == key)
				return m_values[i];
		}
	}

	template <typename F>
	Value& GetOrCreate(const size_t key, F&& create)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				m_used[i] = true;
				Value new_value = std::invoke(std::forward<F>(create)); // get value
				Insert(key, new_value, i); // move into slot
				return m_values[i]; // return after insert
			}

			if (m_keys[i] == key)
				return m_values[i];
		}
	}

	template <typename T>
	void Put(const size_t key, T&& value)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				m_used[i] = true;
				Insert(key, std::forward<T>(value), i);
				return;
			}

			if (m_keys[i] == key)
			{
				m_values[i] = std::forward<T>(value);
				return;
			}
		}
	}

	void Erase(const size_t key)
	{
		// TODO: make this
	}

	void Rehash(size_t new_capacity) {

		new_capacity = EnsureSize(new_capacity);
		Resize(new_capacity);
	}

	class Iterator {
	public:
		Iterator(size_t* keys, Value* values, bool* used, size_t index, size_t size)
			: m_keys(keys), m_values(values), m_used(used), m_index(index), m_size(size)
		{
			advance();
		}

		Iterator& operator++() {
			++m_index;
			advance();
			return *this;
		}

		bool operator!=(const Iterator& other) const {
			return m_index != other.m_index;
		}

		std::pair<size_t, Value> operator*() const {
			return { m_keys[m_index], m_values[m_index] };
		}

	private:
		size_t* m_keys;
		Value* m_values;
		bool* m_used;
		size_t m_index;
		size_t m_size;

		void advance() {
			while (m_index < m_size && !m_used[m_index])
			{
				++m_index;
			}
		}
	};

	Iterator begin() {
		return Iterator(m_keys.get(), m_values.get(), m_used.get(), 0, m_data_size);
	}

	Iterator end() {
		return Iterator(m_keys.get(), m_values.get(), m_used.get(), m_data_size, m_data_size);
	}

private:

	static size_t Hash(const size_t key)
	{
		auto hash = key + 1; // fix for 0 keys
		hash ^= hash >> 21;
		hash ^= hash << 37;
		hash ^= hash >> 4;
		hash *= 0x165667919E3779F9ULL;
		hash ^= hash >> 32;
		return hash;
	}

	[[nodiscard]] std::tuple<size_t, size_t> GetSlot(const size_t key, const size_t data_size)
	{
		if (!m_keys || !m_values || !m_used)
			UNREACHABLE();

		const auto hash = Hash(key);
		const auto size = data_size;
		const auto last_index = size - 1;
		const auto start = hash & last_index;
		return std::make_tuple(start, last_index);
	}

	static size_t EnsureSize(size_t n)
	{
		n = std::max(n, 8ull); // minimum size
		const size_t next = 1ull << std::bit_width(n - 1); // next power of two
		const size_t prev = next >> 1;                          // previous power of two
		return (n - prev < next - n) ? prev : next;
	}

	template <typename T>
	void Insert(const size_t key, T&& new_value, size_t i)
	{
		m_keys[i] = key;
		m_values[i] = std::forward<T>(new_value);
		m_count++;

		if ((double)m_count / (double)m_data_size > max_load_factor)
			Resize(m_data_size * 2);
	}

	void PutWithNewSize(const size_t key, const Value& value, const size_t new_size)
	{
		auto [start, last_index] = GetSlot(key, new_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used_new[i])
			{
				m_used_new[i] = true;
				m_keys_new[i] = key;
				m_values_new[i] = std::move(value);
				return;
			}

			if (m_keys_new[i] == key)
			{
				m_values_new[i] = std::move(value);
				return;
			}
		}
	}

	void Resize(const size_t new_size)
	{
		// Allocate new buffers (unique_ptr handles cleanup automatically on re-assignment)
		m_keys_new = std::make_unique<size_t[]>(new_size);
		m_values_new = std::make_unique<Value[]>(new_size);
		m_used_new = std::make_unique<bool[]>(new_size);

		// Rehash and insert existing elements into the new storage
		for (size_t i = 0; i < m_data_size; ++i)
		{
			if (!m_used[i])
				continue;

			const auto& value = m_values[i];
			PutWithNewSize(m_keys[i], value, new_size);
		}

		// Move the new arrays into place
		m_keys = std::move(m_keys_new);
		m_values = std::move(m_values_new);
		m_used = std::move(m_used_new);

		// Clear temporaries (unique_ptr reset is automatic after move)
		m_data_size = new_size;
	}

	//public:
		void PrintHashDistribution() const
		{
			std::cout << m_data_size << "\n";
			std::ofstream file("F:\\map.txt", std::ios::binary);

			if (!file.is_open())
				return;

			for (auto i = 0ull; i < m_data_size; i++)
			{
				if (!m_used[i])
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