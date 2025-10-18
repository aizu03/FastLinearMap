#pragma once
#include <tuple>
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <optional>


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

struct MyVector
{
	std::vector<int> data;


};


template <class Value>
class LinearMap
{
	static constexpr double max_load_factor = 0.6;

	size_t* m_keys;
	size_t* m_keys_new = nullptr;
	Value* m_values;
	Value* m_values_new = nullptr;
	bool* m_used = nullptr;
	bool* m_used_new = nullptr;

public:

	size_t m_count;
	size_t m_data_size;

	explicit LinearMap(const int size = 128) 
	{
		m_count = 0;
		m_data_size = size;
		m_keys = new size_t[size]();
		m_values = new Value[size]();
		m_used = new bool[size](); 
	}

	~LinearMap()
	{
		delete[] m_keys;
		delete[] m_values;
		delete[] m_used;
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

	[[nodiscard]] std::optional<Value> Get(const size_t key)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return std::nullopt;

			if (m_keys[i] == key)
				return m_values[i];
		}
	}

	[[nodiscard]] std::optional<std::reference_wrapper<Value>> GetRef(const size_t key)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start;; i = (i + 1) & last_index)
		{
			if (!m_used[i])
				return std::nullopt;

			if (m_keys[i] == key)
				return m_values[i];
		}
	}

	void Put(const size_t key, const Value& value)
	{
		auto [start, last_index] = GetSlot(key, m_data_size);
		for (auto i = start; ; i = (i + 1) & last_index)
		{
			if (!m_used[i])
			{
				m_used[i] = true;
				Insert(key, value, i);
				return;
			}

			if (m_keys[i] == key)
			{
				m_values[i] = value; // no need for std::move on const ref
				return;
			}
		}
	}

	[[nodiscard]] bool Contains(const size_t key) const
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
		return Iterator(m_keys, m_values, m_used, 0, m_data_size);
	}

	Iterator end() {
		return Iterator(m_keys, m_values, m_used, m_data_size, m_data_size);
	}


private:

	static size_t Hash(const size_t key)
	{
#if _DEBUG
		if (key == 0)
			throw std::exception("Hash key cannot be 0");
#endif

		if (key == 0)
			UNREACHABLE();

		auto hash = key;
		hash ^= hash >> 21;
		hash ^= hash << 37;
		hash ^= hash >> 4;
		hash *= 0x165667919E3779F9ULL;
		hash ^= hash >> 32;
		return hash;
	}

	[[nodiscard]] std::tuple<size_t, size_t> GetSlot(const size_t key, const size_t data_size) const
	{
		if (key == 0 || !m_keys || !m_values)
			UNREACHABLE();

		const auto hash = Hash(key);
		const auto size = data_size;
		const auto last_index = size - 1;
		const auto start = hash & last_index;
		return std::make_tuple(start, last_index);
	}


	void Insert(const size_t key, const Value& new_value, size_t i)
	{
		m_keys[i] = key;
		m_values[i] = std::move(new_value);
		m_count++;

		if ((double)m_count / (double)m_data_size > max_load_factor)
			Resize();
	}

	void PutWithNewSize(const size_t key, const Value& value, size_t new_size)
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

	void Resize()
	{
		const auto new_size = m_data_size * 2;

		m_keys_new = new size_t[new_size]();
		m_values_new = new Value[new_size]();
		m_used_new = new bool[new_size]();

		for (size_t i = 0; i < m_data_size; i++)
		{
			if (!m_used[i])
				continue;

			const auto& value = m_values[i];
			PutWithNewSize(m_keys[i], value, new_size);
		}

		delete[] m_keys;
		delete[] m_values;
		delete[] m_used;

		m_keys = m_keys_new;
		m_values = m_values_new;
		m_used = m_used_new;

		m_keys_new = nullptr;
		m_values_new = nullptr;
		m_used_new = nullptr;

		m_data_size = new_size;
	}

	void PrintHashDistribution() const
	{
		std::cout << m_data_size << "\n";

		if (m_data_size == 65536)
		{
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

				constexpr char val2[] = {32, 32, 49};
				file.write(val2, 3);
			}

			file.close();
		}
	}
};