/*
------------------------------------------------------------------------------
LinearMap - A Simple Header-Only, Cache-Friendly Linear-Probing Hash Map
------------------------------------------------------------------------------
Author: [aizu03]
License: MIT (free to use, modify, and distribute)

This file includes the following classes:

LinearMap<T>          - A hash map with size_t keys and T values.
LinearGenericMap<K,V> - A generic hash map with K keys and V values.


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

------------------------------------------------------------------------------

--- Benchmark Results (1000000 elements) ---

Operation       LinearMap(ms)   unordered_map(ms)       Speedup
Put             43.8725         221.236         5.04269x
Contains        35.175          164.799         4.68512x
Get             34.1008         346.833         10.1708x

------------------------------------------------------------------------------
*/


#pragma once
#include <tuple>
#include <functional>
#include <iostream>
#include <fstream>
#include <optional>
#include <bit>
#include <algorithm>
#include <iomanip>


#if defined(__clang__) or defined(__GNUC__)
#define unreachable()  __builtin_unreachable()
#elif defined(_MSC_VER)
#define unreachable() __assume(0)
#else
#define unreachable() ((void)0) // fallback
#endif

template <class K, class V>
class AzHashBase
{
protected:

	size_t(*m_hash)(const K&) = nullptr;

	[[nodiscard]] std::tuple<size_t, size_t> GetSlot(const K& key, const size_t data_size)
	{
		if (!m_hash)
			unreachable();

		const size_t hash = HashImpl(m_hash(key), data_size);
		const auto size = data_size;

#if defined(__clang__) or defined(__GNUC__)
		__builtin_assume(size != 0 && (size & (size - 1)) == 0); // always power of two
#endif

		const auto last_index = size - 1;
		const auto start = hash & last_index;
		return std::make_tuple(start, last_index);
	}

	static size_t HashImpl(const size_t n, const size_t data_size)
	{
		auto x = n + 1; // fix for 0 keys
		constexpr int hash_id = 3;

		if (hash_id == 0) // Custom
		{
			x ^= x >> 21;
			x ^= x << 37;
			x ^= x >> 4;
			x *= 0x165667919E3779F9ULL;
			x ^= x >> 32;
		}
		else if (hash_id == 1)  // Splitmix64
		{
			x += 0x9e3779b97f4a7c15;
			x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
			x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
			x ^= (x >> 31);
		}
		else if (hash_id == 2) // Wyhash Final
		{
			x ^= x >> 32;
			x *= 0xd6e8feb86659fd93;
			x ^= x >> 32;
			x *= 0xd6e8feb86659fd93;
			x ^= x >> 32;
		}
		else if (hash_id == 3) // Golden ratio
		{
			constexpr uint64_t golden_ratio = 11400714819323198485ULL;
			return (x * golden_ratio) & (data_size - 1);
		}

		return x;
	}

	static size_t FormatCapacity(size_t n)
	{
		n = std::max(n, 8ull);                     // minimum size
		size_t next = 1ull << std::bit_width(n - 1); // next power of two
		return next;
	}
};

template <class K, class V>
class LinearGenericMap : public AzHashBase<K, V> // linear probing hash map
{
	static constexpr double max_load_factor = 0.6;

protected:

	std::unique_ptr<K[]> m_keys;
	std::unique_ptr<V[]>  m_values;
	std::unique_ptr<bool[]>   m_used;

	std::unique_ptr<K[]> m_keys_new;
	std::unique_ptr<V[]>  m_values_new;
	std::unique_ptr<bool[]>   m_used_new;

	size_t m_count = 0;
	size_t m_data_size = 0;

	K m_default_key{}; // never modify this
	V m_default_value{}; // never modify this

public:

	explicit LinearGenericMap(size_t capacity, size_t(*hash_func)(const K&))
	{
		Init(this->FormatCapacity(capacity));
		this->m_hash = hash_func;
	}

	explicit LinearGenericMap(size_t(*hash_func)(const K&))
	{
		Init();
		this->m_hash = hash_func;
	}

	explicit LinearGenericMap()
	{
		Init();
		this->m_hash = [](const K& key)
			{
				return std::hash<K>{}(key);
			};
	}

	~LinearGenericMap() = default; // Destructor - now defaulted because smart pointers manage memory automatically

	LinearGenericMap(const LinearGenericMap& other) // Copy constructor (deep copy)
		: m_keys(std::make_unique<K[]>(other.m_data_size)),
		m_values(std::make_unique<V[]>(other.m_data_size)),
		m_used(std::make_unique<bool[]>(other.m_data_size)),
		m_count(other.m_count),
		m_data_size(other.m_data_size)
	{
		std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
		std::copy_n(other.m_values.get(), other.m_data_size, m_values.get());
		std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());
	}

	LinearGenericMap& operator=(const LinearGenericMap& other) // Copy assignment (deep copy)
	{
		if (this == &other)
			return *this;

		m_count = other.m_count;
		m_data_size = other.m_data_size;

		m_keys = std::make_unique<K[]>(other.m_data_size);
		m_values = std::make_unique<V[]>(other.m_data_size);
		m_used = std::make_unique<bool[]>(other.m_data_size);

		std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
		std::copy_n(other.m_values.get(), other.m_data_size, m_values.get());
		std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());

		return *this;
	}

	LinearGenericMap(LinearGenericMap&& other) noexcept // Move constructor
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

	LinearGenericMap& operator=(LinearGenericMap&& other) noexcept // Move assignment
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

	V& operator[](const K& key)
	{
		return GetOrCreate(key, V{});
	}

	const V& operator[](const K& key) const {

		return Get(key); 
	}

	void Clear()
	{
		// empty out, and keep allocated memory
		std::fill_n(m_used.get(), m_data_size, false);
		std::fill_n(m_keys.get(), m_data_size, 0);
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

	[[nodiscard]] double LoadFactor() const
	{
		return static_cast<double>(m_count) / static_cast<double>(m_data_size);
	}

	[[nodiscard]] bool Contains(const K& key)
	{
		auto [start, last_index] = this->GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return false;

			if (m_keys[i] == key)
				return true;
		}
	}

	[[nodiscard]] V& Get(const K& key)
	{
		auto [start, last_index] = this->GetSlot(key, m_data_size);
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return m_default_value;

			if (m_keys[i] == key)
				return m_values[i];
		}
	}

	template <typename F> requires std::is_invocable_v<F>
	V& GetOrCreate(const K& key, F&& create_func)
	{
		return GetOrCreateImpl(key, std::invoke(std::forward<F>(create_func)));
	}

	V& GetOrCreate(const K& key, const V& v)
	{
		return GetOrCreateImpl(key, v);
	}

	V& GetOrCreate(const K& key, V&& v)
	{
		return GetOrCreateImpl(key, std::move(v));
	}

	template <typename KeyType, typename ValType>
	void Emplace(KeyType&& key, ValType&& value)
	{
		auto [start, last_index] = this->GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				Insert(std::forward<KeyType>(key), std::forward<ValType>(value), i);
				return;
			}

			if (m_keys[i] == key)
			{
				m_values[i] = std::forward<ValType>(value); // update
				return;
			}
		}
	}

	template <std::ranges::input_range KeyRange, std::ranges::input_range ValueRange>
		requires std::convertible_to<std::ranges::range_reference_t<KeyRange>, K>&&
			std::convertible_to<std::ranges::range_reference_t<ValueRange>, V>
		void EmplaceAll(KeyRange& keys, ValueRange& values)
	{
		auto key_it = std::begin(keys);
		auto key_end = std::end(keys);
		auto val_it = std::begin(values);

		EnsureCapacity(std::distance(key_it, key_end));

		while (key_it != key_end)
		{
			EmplaceNoGrow(std::ranges::iter_move(key_it), std::ranges::iter_move(val_it));
			
			++key_it;
			++val_it;
		}
	}

	template <std::ranges::input_range PairRange>
		requires requires(const std::ranges::range_value_t<PairRange>& p) {
			{ std::get<0>(p) } -> std::convertible_to<K>;
			{ std::get<1>(p) } -> std::convertible_to<V>;
	}
	void EmplaceAll(PairRange& pairs)
	{
		EnsureCapacity(std::ranges::distance(pairs));

		for (auto&& p : pairs)
		{
			K&& key = std::get<0>(std::move(p));
			V&& value = std::get<1>(std::move(p));
			EmplaceNoGrow(std::move(key), std::move(value));
		}
	}

	void EmplaceAll(K* keys, V* values, const size_t count)
	{
		if (!keys || !values || count == 0)
			return;

		EnsureCapacity(count);

		for (size_t idx = 0; idx < count; ++idx)
		{
			K& key = keys[idx];
			V& value = values[idx];
			EmplaceNoGrow(std::move(key), std::move(value));
		}
	}

	template <typename KeyVal, typename... Args>
	bool TryEmplace(KeyVal&& key, Args&&... args)
	{
		auto [start, last_index] = this->GetSlot(key, m_data_size);
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				Insert(std::forward<KeyVal>(key), V(std::forward<Args>(args)...), i);
				return true; // inserted successfully
			}

			if (m_keys[i] == key)
				return false; // key already present
		}
	}

	bool Erase(const K& key)
	{
		auto print_array = [this]<typename T>(const bool before, std::unique_ptr<T>&arr) -> void
		{
			std::cout << (before ? "Before: " : "After:  ");
			for (size_t i = 0; i < m_data_size; i++)
				std::cout << std::setfill('0') << std::setw(2) << arr[i] << ' ';

			std::cout << "\n";
		};
		auto print_arrays = [this, print_array](const bool before) -> void
			{
				print_array(before, m_used);
				print_array(before, m_keys);
				//print_array(before, m_values);

			// [0,0,0,0,0,0,0,0,0,0]
			// [0,0,0,4,5,6,7,8,9,0]
			// [0,0,0,D,E,F,G,H,I,J]

			};

		auto [start, last_index] = this->GetSlot(key, m_data_size);
		auto inc = 0; // offset to start index
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return false; // key not found

			if (m_keys[i] == key)
				break;
			++inc;
		}

		start += inc; // adjust start index
		auto keys_above = 0; // number of keys above start index, with same start index (same bucket)
		for (auto i = (start + 1);; i = (i + 1) & last_index)
		{
			const auto used = m_used[i];
			auto [start_other, _] = this->GetSlot(m_keys[i], m_data_size);
			start_other += (0xFFFFFFFFFFFFFFFF - m_data_size) * (1 - used); // if not used, break loop
			if (start_other != start)
				break;

			++keys_above;
		}

		if (keys_above == 0)
		{
			// Example with index 4 as home
			// Case: 0 keys above
			//
			// [0,0,0,1,0,0,0,0,0,0]
			// [0,0,0,4,5,6,7,8,9,0]
			// [0,0,0,D,E,F,G,H,I,J]
			//
			//        v
			// [0,0,0,0,0,0,0,0,0,0] just empty out index 4
			// [0,0,0,0,5,6,7,8,9,0]
			// [0,0,0,0,E,F,G,H,I,J]

			m_used[start] = false;
			m_keys[start] = m_default_key;
			m_values[start] = m_default_value;
			m_count--;
			return true;
		}

		// Example with index 4 as origin
		// Case: 2 keys above
		//
		// [0,0,0,1,1,1,0,0,0,0]
		// [0,0,0,4,5,6,7,8,9,0]
		// [0,0,0,D,E,F,G,H,I,J]
		//
		//        v v < 
		// [0,0,0,1,1,1,0,0,0,0] // move index 5 and 6, one to the left 
		// [0,0,0,5,6,6,7,8,9,0]
		// [0,0,0,E,F,F,G,H,I,J]

		int count = 0;

		//print_arrays(true);

		for (auto i = start;; i = (i + 1) & last_index)
		{
			auto next = (i + 1) & last_index;
			m_used[i] = m_used[next];
			m_keys[i] = std::move(m_keys[next]);
			m_values[i] = std::move(m_values[next]);

			++count;
			if (count >= keys_above)
				break;
		}

		//            v
		// [0,0,0,1,1,0,0,0,0,0] // delete last entry
		// [0,0,0,5,6,0,7,8,9,0]
		// [0,0,0,E,F,0,G,H,I,J]

		const auto last_entry = (start + keys_above) & last_index;
		m_used[last_entry] = false;
		m_keys[last_entry] = m_default_key;
		m_values[last_entry] = m_default_value;

		//print_arrays(false);

		m_count--;
		return true;
	}

	void Rehash(size_t new_capacity) {

		new_capacity = this->FormatCapacity(new_capacity);

		if (new_capacity < m_count)
			throw std::out_of_range("New capacity is smaller than the current size of the map");

		Resize(new_capacity);
	}

	class Iterator {
	public:
		Iterator(size_t* keys, V* values, bool* used, size_t index, size_t size)
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

		std::pair<size_t, V> operator*() const {
			return { m_keys[m_index], m_values[m_index] };
		}

	private:
		size_t* m_keys;
		V* m_values;
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

	void Init(size_t capacity = 128)
	{
		m_keys = std::make_unique<K[]>(capacity);
		m_values = std::make_unique<V[]>(capacity);
		m_used = std::make_unique<bool[]>(capacity);
		m_data_size = capacity;
	}

	template <typename A, typename B>
	V& GetOrCreateImpl(A&& key, B&& new_value)
	{
		auto [start, last_index] = this->GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				Insert(std::forward<A>(key), std::forward<B>(new_value), i);
				return m_values[i]; // return after insert
			}

			if (m_keys[i] == key)
				return m_values[i];
		}
	}

	template <typename A, typename B>
	void EmplaceNoGrow(A&& key, B&& value)
	{
		auto [start, last_index] = this->GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				InsertNoGrow(std::forward<A>(key), std::forward<B>(value), i);
				break;
			}

			if (m_keys[i] == key)
			{
				m_values[i] = std::forward<B>(value);
				break;
			}
		}
	}

	template <typename A, typename B>
	void Insert(A&& key, B&& new_value, size_t i)
	{
		m_used[i] = true;
		m_keys[i] = std::forward<A>(key);
		m_values[i] = std::forward<B>(new_value);
		m_count++;

		if ((double)m_count / (double)m_data_size > max_load_factor)
			Resize(m_data_size * 2);
	}

	template <typename A, typename B>
	void InsertNoGrow(A&& key, B&& new_value, size_t i)
	{
		m_used[i] = true;
		m_keys[i] = std::forward<A>(key);
		m_values[i] = std::forward<B>(new_value);
		m_count++;
	}

	template<typename A, typename B>
	void EmplaceNewSize(A&& key, B&& value, const size_t new_size)
	{
		auto [start, last_index] = this->GetSlot(key, new_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used_new[i])
			{
				m_used_new[i] = true;
				m_keys_new[i] = std::forward<A>(key);
				m_values_new[i] = std::forward<B>(value);
				return;
			}

			if (m_keys_new[i] == key)
			{
				m_values_new[i] = std::forward<B>(value);
				return;
			}
		}
	}

	void Resize(const size_t new_size)
	{
		// Allocate new buffers (unique_ptr handles cleanup automatically on re-assignment)
		m_keys_new = std::make_unique<K[]>(new_size);
		m_values_new = std::make_unique<V[]>(new_size);
		m_used_new = std::make_unique<bool[]>(new_size);

		// Rehash and insert existing elements into the new storage
		for (size_t i = 0; i < m_data_size; ++i)
		{
			if (!m_used[i])
				continue;

			EmplaceNewSize(std::move(m_keys[i]), std::move(m_values[i]), new_size);
		}

		// Move the new arrays into place
		m_keys = std::move(m_keys_new);
		m_values = std::move(m_values_new);
		m_used = std::move(m_used_new);

		// Clear temporaries (unique_ptr reset is automatic after move)
		m_data_size = new_size;
	}

	void EnsureCapacity(const size_t count)
	{
		const auto free = m_data_size - m_count;
		const size_t free_lf = (size_t)((double)free * max_load_factor);
		if (count < free_lf)
			return; // enough space

		const auto need = count - free_lf;
		auto new_size = this->FormatCapacity(m_data_size + need);
		Resize(new_size);
	}
};

/*template <class K>
class LinearSet
{

	// TODO: Hashset version

public:

private:


};*/

template <class T>
class LinearMap : public LinearGenericMap<size_t, T>
{
public:

	explicit LinearMap(size_t capacity = 128)
		: LinearGenericMap<size_t, T>(capacity, &IntHash)
	{
	}

	using Base = LinearGenericMap<size_t, T>;

private:

	static size_t IntHash(const size_t& k)
	{
		return k;
	}
};


template <class T>
class DebugMap : public LinearMap<T>
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