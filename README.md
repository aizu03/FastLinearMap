LinearMap is a fast, and bare bones, open-addressing hash map implementation that uses
linear probing rather than the traditional "bucket + linked list" approach
you find in many standard hash maps, like std::unordered_map

Why it's faster
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
- When the load factor approaches the threshold (~0.7), performance may degrade;
  resizing restores O(1) average lookup and insertion.

Examining the assembly shows, for a simple <int> map,
it only uses 2 comparisons during the search loop, inside the "Get" function. Blazing fast!

In short, LinearMap trades a bit more memory for substantially better
real-world performance on modern hardware.

All benchmarking was done on a modern mid-range CPU (Ryzen 7 3700x).  
With clang++20 and AVX2 optimizations enabled, and inlining disabled

---

### Benchmark Results (1,000,000 elements)

| Operation | LinearMap (ms) | unordered_map (ms) | Speedup |
|-----------|----------------|------------------|---------|
| Emplace   | 42.3977        | 253.53           | 5.97981x |
| Contains  | 24.0756        | 159.539          | 6.62658x |
| Get       | 22.4944        | 348.085          | 15.4743x |

Usage
-----------
All you have to do is incude the "LinearMap.h" file, and you're read to go. 

```
LinearMap<int> map;
map[1] = 1;
map.Emplace(key, value);
ma.Get(...);
ma.Cotains(...);
ma.IsValid(...);
// and much more
```

You find the full examples inside the "examples.h" file.
