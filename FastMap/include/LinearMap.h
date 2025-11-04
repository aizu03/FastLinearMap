/*
----------------------------------------------------------------------------------------
LinearHash - A Simple Header-Only, Cache-Friendly, Linear-Probing Hash Map Collection
----------------------------------------------------------------------------------------
Author: [aizu03]
License: MIT (free to use, modify, and distribute)

This file includes the following classes:

LinearMap<T>          - A linear probing hash map with size_t keys and T values.
LinearCoreMap<K,V>	  - A linear probing hash map with K keys and V values.
LinearSet<K>		  - A linear probing hash set with K keys.


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
Emplace         43.8725         221.236         5.04269x
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
#define OPTIMIZED
#define UNREACHABLE()  __builtin_unreachable()
#elif defined(_MSC_VER)
#define UNREACHABLE() __assume(0)
#else
#define UNREACHABLE() ((void)0) // fallback
#endif

#if defined(OPTIMIZED)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif


namespace LinearProbing
{
	template<class T>
	using HashFunction = size_t(*)(const T& key);

	namespace Internal
	{
		template <class T>
		class LinearHash
		{
		protected:

			HashFunction<T> m_hash = nullptr;
			size_t m_count = 0;
			size_t m_data_size = 0;
			static constexpr double max_load_factor = 0.7;

		public:

			virtual ~LinearHash() = default;

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

			virtual void Reserve(const size_t capacity)
			{
				throw std::runtime_error("not implemented");
			}

			virtual void Clear() {
				throw std::runtime_error("not implemented");
			}

			/// <summary>
			/// Will grow or shrink the map to 'new_capacity', while keeping existing data.
			/// </summary>
			/// <param name="new_capacity"></param>
			void Rehash(size_t new_capacity)
			{
				new_capacity = FormatCapacity(new_capacity);

				if (new_capacity < this->m_count)
					throw std::out_of_range("New capacity is smaller than the current size of the map");

				this->Resize(new_capacity);
			}

			void SetHashFunction(HashFunction<T> hash_func)
			{
				m_hash = hash_func;
			}

		protected:

			void SetDefaultHash()
			{
				m_hash = [](const T& key)
					{
						return std::hash<T>{}(key);
					};
			}

			[[nodiscard]] std::tuple<size_t, size_t> GetSlot(const T& key, const size_t data_size)
			{
				if (!m_hash)
					UNREACHABLE();

				const size_t hash = HashImpl(m_hash(key), data_size);
				const auto size = data_size;

#if defined(OPTIMIZED)
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
					constexpr size_t golden_ratio = 11400714819323198485ULL;
					return (x * golden_ratio) & (data_size - 1);
				}

				return x;
			}

			static size_t FormatCapacity(size_t n)
			{
				n = (std::max)(n, 8ull);                     // minimum size
				size_t next = 1ull << std::bit_width(n - 1); // next power of two
				return next;
			}

			virtual void Init(size_t capacity)
			{
				throw std::runtime_error("not implemented");
			}

			virtual void Resize(size_t new_size)
			{
				throw std::runtime_error("not implemented");
			}

			void EnsureCapacity(const size_t count)
			{
				const auto free = this->m_data_size - this->m_count;
				const size_t free_lf = (size_t)((double)free * max_load_factor);
				if (count < free_lf)
					return; // enough space

				const auto need = count - free_lf;
				auto new_size = FormatCapacity(this->m_data_size + need);
				Resize(new_size);
			}
		};
	}

	template <class K, class V>
	class LinearCoreMap : public Internal::LinearHash<K> // linear probing hash map
	{
	protected:

		// TODO: Test out future optimization by adding 8 empty entries for vectorization and masking

		std::unique_ptr<K[]> m_keys;
		std::unique_ptr<V[]>  m_values;
		std::unique_ptr<bool[]>   m_used;

		std::unique_ptr<K[]> m_keys_new;
		std::unique_ptr<V[]>  m_values_new;
		std::unique_ptr<bool[]>   m_used_new;

	private:

		K m_default_key{}; // never modify this
		V m_default_value{}; // never modify this

	public:

		explicit LinearCoreMap(const size_t capacity, HashFunction<K> hash_func)
		{
			LinearCoreMap::Init(capacity);
			this->m_hash = hash_func;
		}

		explicit LinearCoreMap(HashFunction<K> hash_func)
		{
			LinearCoreMap::Init();
			this->m_hash = hash_func;
		}

		explicit LinearCoreMap()
		{
			LinearCoreMap::Init();
		}

		template <std::ranges::input_range KeyRange, std::ranges::input_range ValueRange>
			requires std::convertible_to<std::ranges::range_reference_t<KeyRange>, K>&&
		std::convertible_to<std::ranges::range_reference_t<ValueRange>, V>
			explicit LinearCoreMap(KeyRange& keys, ValueRange& values)
		{
			LinearCoreMap::Init();
			EmplaceAll(keys, values);
		}

		template <std::ranges::input_range PairRange>
			requires requires(const std::ranges::range_value_t<PairRange>& p) {
				{ std::get<0>(p) } -> std::convertible_to<K>;
				{ std::get<1>(p) } -> std::convertible_to<V>;
		}
		explicit LinearCoreMap(PairRange& pairs)
		{
			LinearCoreMap::Init();
			EmplaceAll(pairs);
		}

		explicit LinearCoreMap(K* keys, V* values, const size_t count)
		{
			LinearCoreMap::Init();
			EmplaceAll(keys, values, count);
		}

		~LinearCoreMap() override = default;

		LinearCoreMap(const LinearCoreMap& other) // Copy constructor (deep copy)
			: m_keys(std::make_unique<K[]>(other.m_data_size)),
			m_values(std::make_unique<V[]>(other.m_data_size)),
			m_used(std::make_unique<bool[]>(other.m_data_size))
		{
			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
			std::copy_n(other.m_values.get(), other.m_data_size, m_values.get());
			std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());
		}

		LinearCoreMap& operator=(const LinearCoreMap& other) // Copy assignment (deep copy)
		{
			if (this == &other)
				return *this;

			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			m_keys = std::make_unique<K[]>(other.m_data_size);
			m_values = std::make_unique<V[]>(other.m_data_size);
			m_used = std::make_unique<bool[]>(other.m_data_size);

			std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
			std::copy_n(other.m_values.get(), other.m_data_size, m_values.get());
			std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());

			return *this;
		}

		LinearCoreMap(LinearCoreMap&& other) noexcept // Move constructor
			: m_keys(std::move(other.m_keys)),
			m_values(std::move(other.m_values)),
			m_used(std::move(other.m_used)),
			m_keys_new(std::move(other.m_keys_new)),
			m_values_new(std::move(other.m_values_new)),
			m_used_new(std::move(other.m_used_new))
		{
			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			other.m_count = 0;
			other.m_data_size = 0;
		}

		LinearCoreMap& operator=(LinearCoreMap&& other) noexcept // Move assignment
		{
			if (this == &other)
				return *this;

			m_keys = std::move(other.m_keys);
			m_values = std::move(other.m_values);
			m_used = std::move(other.m_used);

			m_keys_new = std::move(other.m_keys_new);
			m_values_new = std::move(other.m_values_new);
			m_used_new = std::move(other.m_used_new);

			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			other.m_count = 0;
			other.m_data_size = 0;

			return *this;
		}

		V& operator[](const K& key)
		{
			return GetOrCreate(key, [this] {return m_default_value; });
		}

		const V& operator[](const K& key) const {

			return Get(key);
		}

		template <typename T, typename = void>
		struct HasEqual : std::false_type {};

		template <typename T>
		struct HasEqual<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>> : std::true_type {};

		/// <summary>
		/// Checks if a value differs from the map's default.
		/// Works for both comparable and trivially copyable types.
		/// Returns true if uncertain.
		/// </summary>
		/// <param name="value"></param>
		/// <returns>True, if "value" differs from the default of V</returns>
		[[nodiscard]] bool HasValue(const V& value) const noexcept
		{
			if constexpr (HasEqual<V>::value)
			{
				return !(m_default_value == value);
			}
			else if constexpr (std::is_trivially_copyable_v<V> && sizeof(V) > 0)
			{
				return std::memcmp(&m_default_value, &value, sizeof(V)) != 0;
			}
			else
			{
				return true; // fallback, cannot reliably compare
			}
		}

		/// <summary>
		/// Clear all data from the map, while keeping the allocated memory.
		/// </summary>
		void Clear() override
		{
			std::fill_n(m_used.get(), this->m_data_size, false);
			std::fill_n(m_keys.get(), this->m_data_size, 0);
			this->m_count = 0;
		}

		/// <summary>
		/// Will allocate memory for at least 'capacity' elements. Existing data will be lost.
		/// Should only be used on an empty map, or to clear this map.
		/// Don't use this before using 'EmplaceAll'. Because, it will ensure capacity internally.
		/// </summary>
		/// <param name="capacity">Desired map size</param>
		void Reserve(const size_t capacity) override
		{
			auto size = this->FormatCapacity(capacity);
			m_keys = std::make_unique<K[]>(size);
			m_values = std::make_unique<V[]>(size);
			m_used = std::make_unique<bool[]>(size);
			this->m_count = 0;
			this->m_data_size = size;
		}

		[[nodiscard]] bool Contains(const K& key)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
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
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start;; i = (i + 1) & last_index)
			{
				if (!m_used[i])
					return m_default_value;

				if (m_keys[i] == key)
					return m_values[i];
			}
		}

		/// <summary>
		/// Will return the value for 'key', if found.
		/// Or emplace a new value created by 'create_func'.
		/// </summary>
		/// <typeparam name="F"></typeparam>
		/// <param name="key"></param>
		/// <param name="create_func"></param>
		/// <returns></returns>
		template <typename KeyVal, typename F> requires std::is_invocable_v<F>
		V& GetOrCreate(KeyVal&& key, F&& create_func)
		{
			return GetOrCreateImpl(
				std::forward<KeyVal>(key),
				[&]() -> V { return std::invoke(std::forward<F>(create_func)); }
			);
		}

		template <typename KeyVal, typename... Args>
		V& GetOrCreate(KeyVal&& key, Args&&... args)
		{
			return GetOrCreateImpl(
				std::forward<KeyVal>(key),
				[&]() -> V { return V(std::forward<Args>(args)...); }
			);
		}

		template <typename KeyVal, typename... Args>
		bool TryEmplace(KeyVal&& key, Args&&... args)
		{
			return TryEmplaceImpl(
				std::forward<KeyVal>(key),
				[&]() -> V { return V(std::forward<Args>(args)...); }
			);
		}

		template <typename KeyVal, typename F> requires std::is_invocable_v<F>
		bool TryEmplace(KeyVal&& key, F&& create_func)
		{
			return TryEmplaceImpl(
				std::forward<KeyVal>(key),
				[&]() -> V { return std::invoke(std::forward<F>(create_func)); }
			);
		}

		template <typename KeyType, typename ValType>
		void Emplace(KeyType&& key, ValType&& value)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
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

			this->EnsureCapacity(std::distance(key_it, key_end));

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
			this->EnsureCapacity(std::ranges::distance(pairs));

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

			this->EnsureCapacity(count);

			for (size_t idx = 0; idx < count; ++idx)
			{
				K& key = keys[idx];
				V& value = values[idx];
				EmplaceNoGrow(std::move(key), std::move(value));
			}
		}

		bool Erase(const K& key)
		{
			auto print_array = [this]<typename T>(const bool before, std::unique_ptr<T>&arr) -> void
			{
				std::cout << (before ? "Before: " : "After:  ");
				for (size_t i = 0; i < this->m_data_size; i++)
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

			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
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
				auto [start_other, _] = this->GetSlot(m_keys[i], this->m_data_size);
				start_other += (0xFFFFFFFFFFFFFFFF - this->m_data_size) * (1 - used); // if not used, break loop
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
				--this->m_count;
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

			--this->m_count;
			return true;
		}

		class Iterator {
		public:

			struct ReferenceProxy {

				const K& first;
				V& second;
				[[nodiscard]] const K& key() const { return first; }
				[[nodiscard]] V& value() const { return second; }
				explicit operator std::pair<const K&, V&>() const { return { first, second }; }
			};

			using value_type = std::pair<const K, V>;
			using reference = ReferenceProxy;
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;

			Iterator(K* keys, V* values, bool* used, const size_t index, const size_t size)
				: m_keys(keys), m_values(values), m_used(used), m_index(index), m_size(size) {
				advance();
			}

			Iterator& operator++() {
				++m_index;
				advance();
				return *this;
			}

			bool operator==(const Iterator& other) const { return m_index == other.m_index; }
			bool operator!=(const Iterator& other) const { return !(*this == other); }

			reference operator*() const {
				return ReferenceProxy{ m_keys[m_index], m_values[m_index] };
			}

		private:
			void advance() {
				while (m_index < m_size && !m_used[m_index]) ++m_index;
			}

			K* m_keys;
			V* m_values;
			bool* m_used;
			size_t m_index;
			size_t m_size;
		};

		Iterator begin() {
			return Iterator(m_keys.get(), m_values.get(), m_used.get(), 0, this->m_data_size);
		}

		Iterator end() {
			return Iterator(m_keys.get(), m_values.get(), m_used.get(), this->m_data_size, this->m_data_size);
		}

	private:

		void Init(const size_t capacity = 64) override
		{
			Reserve(capacity);
			this->SetDefaultHash();
		}

		void Resize(const size_t new_size) override
		{
			m_keys_new = std::make_unique<K[]>(new_size);
			m_values_new = std::make_unique<V[]>(new_size);
			m_used_new = std::make_unique<bool[]>(new_size);

			for (size_t i = 0; i < this->m_data_size; ++i)
			{
				if (!m_used[i])
					continue;

				EmplaceNewSize(std::move(m_keys[i]), std::move(m_values[i]), new_size);
			}

			m_keys = std::move(m_keys_new);
			m_values = std::move(m_values_new);
			m_used = std::move(m_used_new);

			this->m_data_size = new_size;
		}

		template <typename KeyVal, typename ValueCreator>
		V& GetOrCreateImpl(KeyVal&& key, ValueCreator&& make_value)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start; ; i = (i + 1) & last_index)
			{
				if (!m_used[i])
				{
					const bool will_resize = this->m_count + 1 >
						(size_t)((double)this->m_data_size * this->max_load_factor);

					if (unlikely(will_resize))
					{
						auto copy = key;
						InsertAndGrow(std::forward<KeyVal>(key), std::invoke(std::forward<ValueCreator>(make_value)), i);
						auto [start2, last_index2] = this->GetSlot(copy, this->m_data_size); // now larger size
						for (auto j = start2; ; j = (j + 1) & last_index2)
							if (m_keys[j] == copy)
								return m_values[j];
					}

					InsertNoGrow(std::forward<KeyVal>(key), std::invoke(std::forward<ValueCreator>(make_value)), i);
					return m_values[i];
				}

				if (m_keys[i] == key)
					return m_values[i];
			}
		}

		template <typename KeyVal, typename ValueCreator>
		bool TryEmplaceImpl(KeyVal&& key, ValueCreator&& make_value)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start; ; i = (i + 1) & last_index)
			{
				if (!m_used[i])
				{
					Insert(std::forward<KeyVal>(key), std::invoke(std::forward<ValueCreator>(make_value)), i);
					return true;
				}

				if (m_keys[i] == key)
					return false;
			}
		}

		template <typename A, typename B>
		void EmplaceNoGrow(A&& key, B&& value)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
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
			++this->m_count;

			const bool grow = (double)this->m_count / (double)this->m_data_size > this->max_load_factor;
			if (unlikely(grow))
				this->Resize(this->m_data_size * 2);
		}

		template <typename A, typename B>
		void InsertAndGrow(A&& key, B&& new_value, size_t i)
		{
			m_used[i] = true;
			m_keys[i] = std::forward<A>(key);
			m_values[i] = std::forward<B>(new_value);
			++this->m_count;
			this->Resize(this->m_data_size * 2);
		}

		template <typename A, typename B>
		void InsertNoGrow(A&& key, B&& new_value, size_t i)
		{
			m_used[i] = true;
			m_keys[i] = std::forward<A>(key);
			m_values[i] = std::forward<B>(new_value);
			++this->m_count;
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
	};

	template <class K>
	class LinearSet : public Internal::LinearHash<K> // linear probing hash set
	{
	protected:

		std::unique_ptr<K[]> m_keys;
		std::unique_ptr<bool[]>   m_used;
		std::unique_ptr<K[]> m_keys_new;
		std::unique_ptr<bool[]>   m_used_new;

	private:

		K m_default_key{}; // never modify this

	public:

		explicit LinearSet(const size_t capacity, HashFunction<K> hash_func)
		{
			LinearSet::Init(capacity);
			this->m_hash = hash_func;
		}

		explicit LinearSet(HashFunction<K> hash_func)
		{
			LinearSet::Init();
			this->m_hash = hash_func;
		}

		template <std::ranges::input_range KeyRange>
			requires std::convertible_to<std::ranges::range_value_t<KeyRange>, K>
		explicit LinearSet(KeyRange& keys)
		{
			LinearSet::Init();
			EmplaceAll(keys);
		}

		explicit LinearSet(K* keys, const size_t count)
		{
			LinearSet::Init();
			EmplaceAll(keys, count);
		}

		explicit LinearSet()
		{
			LinearSet::Init();
		}

		~LinearSet() override = default;

		LinearSet(const LinearSet& other) // Copy constructor (deep copy)
			: m_keys(std::make_unique<K[]>(other.m_data_size)),
			m_used(std::make_unique<bool[]>(other.m_data_size))
		{
			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
			std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());
		}

		LinearSet& operator=(const LinearSet& other) // Copy assignment (deep copy)
		{
			if (this == &other)
				return *this;

			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			m_keys = std::make_unique<K[]>(other.m_data_size);
			m_used = std::make_unique<bool[]>(other.m_data_size);

			std::copy_n(other.m_keys.get(), other.m_data_size, m_keys.get());
			std::copy_n(other.m_used.get(), other.m_data_size, m_used.get());

			return *this;
		}

		LinearSet(LinearSet&& other) noexcept // Move constructor
			: m_keys(std::move(other.m_keys)),
			m_used(std::move(other.m_used)),
			m_keys_new(std::move(other.m_keys_new)),
			m_used_new(std::move(other.m_used_new))
		{
			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;

			other.m_count = 0;
			other.m_data_size = 0;
		}

		LinearSet& operator=(LinearSet&& other) noexcept // Move assignment
		{
			if (this == &other)
				return *this;

			m_keys = std::move(other.m_keys);
			m_used = std::move(other.m_used);
			m_keys_new = std::move(other.m_keys_new);
			m_used_new = std::move(other.m_used_new);
			this->m_count = other.m_count;
			this->m_data_size = other.m_data_size;
			this->m_hash = other.m_hash;
			other.m_count = 0;
			other.m_data_size = 0;
			return *this;
		}

		/// <summary>
		/// Clear all data from the map, while keeping the allocated memory.
		/// </summary>
		void Clear() override
		{
			std::fill_n(m_used.get(), this->m_data_size, false);
			std::fill_n(m_keys.get(), this->m_data_size, 0);
			this->m_count = 0;
		}

		/// <summary>
		/// Will allocate memory for at least 'capacity' elements. Existing data will be lost.
		/// Should only be used on an empty map, or to clear this map.
		/// Don't use this before using 'EmplaceAll'. Because, it will ensure capacity internally.
		/// </summary>
		/// <param name="capacity">Desired map size</param>
		void Reserve(const size_t capacity) override
		{
			auto new_size = this->FormatCapacity(capacity);
			m_keys = std::make_unique<K[]>(new_size);
			m_used = std::make_unique<bool[]>(new_size);
			this->m_count = 0;
			this->m_data_size = new_size;
		}

		[[nodiscard]] bool Contains(const K& key)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start; ; i = (i + 1) & last_index)
			{
				if (!m_used[i])
					return false;

				if (m_keys[i] == key)
					return true;
			}
		}

		template <typename KeyType>
		void Emplace(KeyType&& key)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start; ; i = (i + 1) & last_index)
			{
				if (!m_used[i])
				{
					Insert(std::forward<KeyType>(key), i);
					return;
				}
			}
		}
		template <std::ranges::input_range KeyRange>
			requires std::convertible_to<std::ranges::range_value_t<KeyRange>, K>
		void EmplaceAll(KeyRange& keys)
		{
			auto key_it = std::begin(keys);
			auto key_end = std::end(keys);
			this->EnsureCapacity(std::distance(key_it, key_end));

			while (key_it != key_end)
			{
				EmplaceNoGrow(std::ranges::iter_move(key_it));
				++key_it;
			}
		}

		void EmplaceAll(K* keys, const size_t count)
		{
			if (!keys || count == 0)
				return;

			this->EnsureCapacity(count);

			for (size_t idx = 0; idx < count; ++idx)
			{
				K& key = keys[idx];
				EmplaceNoGrow(std::move(key));
			}
		}

		template <typename KeyVal>
		bool TryEmplace(KeyVal&& key)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start;; i = (i + 1) & last_index)
			{
				if (!m_used[i])
				{
					Insert(std::forward<KeyVal>(key), i);
					return true; // inserted successfully
				}

				if (m_keys[i] == key)
					return false; // key already present
			}
		}

		bool Erase(const K& key)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
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
				auto [start_other, _] = this->GetSlot(m_keys[i], this->m_data_size);
				start_other += (0xFFFFFFFFFFFFFFFF - this->m_data_size) * (1 - used); // if not used, break loop
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

				--this->m_count;
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

			for (auto i = start;; i = (i + 1) & last_index)
			{
				auto next = (i + 1) & last_index;
				m_used[i] = m_used[next];
				m_keys[i] = std::move(m_keys[next]);

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

			--this->m_count;
			return true;
		}

		class Iterator {
		public:
			Iterator(K* keys, bool* used, const size_t index, const size_t size)
				: m_keys(keys), m_used(used), m_index(index), m_size(size)
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

			const K& operator*() const {
				return m_keys[m_index];
			}

		private:
			K* m_keys;
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
			return Iterator(m_keys.get(), m_used.get(), 0, this->m_data_size);
		}

		Iterator end() {
			return Iterator(m_keys.get(), m_used.get(), this->m_data_size, this->m_data_size);
		}

	private:

		void Init(const size_t capacity = 64) override
		{
			Reserve(capacity);
			this->SetDefaultHash();
		}

		void Resize(const size_t new_size) override
		{
			m_keys_new = std::make_unique<K[]>(new_size);
			m_used_new = std::make_unique<bool[]>(new_size);

			for (size_t i = 0; i < this->m_data_size; ++i)
			{
				if (!m_used[i])
					continue;

				EmplaceNewSize(std::move(m_keys[i]), new_size);
			}

			m_keys = std::move(m_keys_new);
			m_used = std::move(m_used_new);

			this->m_data_size = new_size;
		}

		template <typename A>
		void EmplaceNoGrow(A&& key)
		{
			auto [start, last_index] = this->GetSlot(key, this->m_data_size);
			for (auto i = start; ; i = (i + 1) & last_index)
			{
				if (!m_used[i])
				{
					InsertNoGrow(std::forward<A>(key), i);
					break;
				}
			}
		}

		template <typename A>
		void Insert(A&& key, size_t i)
		{
			m_used[i] = true;
			m_keys[i] = std::forward<A>(key);
			++this->m_count;

			const bool grow = (double)this->m_count / (double)this->m_data_size > this->max_load_factor;
			if (unlikely(grow))
				this->Resize(this->m_data_size * 2);
		}

		template <typename A>
		void InsertNoGrow(A&& key, size_t i)
		{
			m_used[i] = true;
			m_keys[i] = std::forward<A>(key);
			++this->m_count;
		}

		template <typename A>
		void EmplaceNewSize(A&& key, const size_t new_size)
		{
			auto [start, last_index] = this->GetSlot(key, new_size);
			for (auto i = start; ; i = (i + 1) & last_index)
			{
				if (!m_used_new[i])
				{
					m_used_new[i] = true;
					m_keys_new[i] = std::forward<A>(key);
					return;
				}
			}
		}
	};

	template <class T>
	class LinearMap final : public LinearCoreMap<size_t, T>
	{
	public:

		explicit LinearMap(size_t capacity = 64) : LinearCoreMap<size_t, T>(capacity, &IntHash)
		{

		}

		template <std::ranges::input_range KeyRange, std::ranges::input_range ValueRange>
			requires std::convertible_to<std::ranges::range_reference_t<KeyRange>, size_t>&&
		std::convertible_to<std::ranges::range_reference_t<ValueRange>, T>
			explicit LinearMap(KeyRange& keys, ValueRange& values) : LinearCoreMap<size_t, T>(keys, values)
		{

		}

		template <std::ranges::input_range PairRange>
			requires requires(const std::ranges::range_value_t<PairRange>& p) {
				{ std::get<0>(p) } -> std::convertible_to<size_t>;
				{ std::get<1>(p) } -> std::convertible_to<T>;
		}
		explicit LinearMap(PairRange& pairs) : LinearCoreMap<size_t, T>(pairs)
		{

		}

		explicit LinearMap(size_t* keys, T* values, const size_t count) : LinearCoreMap<size_t, T>(keys, values, count)
		{

		}

	private:

		static size_t IntHash(const size_t& k)
		{
			return k;
		}
	};

}