#ifndef NETSTACK_UTILS_H
#define NETSTACK_UTILS_H

#include <cinttypes>

#include <vector>
#include <list>
#include <queue>
#include <set>
#include <unordered_set>
#include <unordered_map>

template <typename T> using Vector = std::vector<T>;

template <typename T> using List = std::list<T>;

template <typename T> using Queue = std::queue<T>;

template <typename T, typename TCmp = std::less<T>>
using OrdSet = std::set<T, TCmp>;

template <typename T> class Hash {
  static constexpr size_t SEED = 257;

public:
  size_t operator()(const T &v) const {
    size_t r = 0;
    auto *b = (const unsigned char *)&v;
    for (size_t i = 0; i != sizeof(v); i++)
      r = r * SEED + b[i];
    return r;
  }
};

template <typename T> using HashSet = std::unordered_set<T>;

template <typename K, typename V>
using HashMap = std::unordered_map<K, V, Hash<K>>;

template <typename K, typename V>
using HashMultiMap = std::unordered_multimap<K, V, Hash<K>>;

/**
 * @brief Calculate the 16-bit checksum.
 *
 * @param data Pointer to the data to be checksummed.
 * @param len Length of the data.
 * @return The checksum of the data, in network endian.
 */
uint16_t csum16(const void *data, size_t len, uint16_t init = 0);

#endif
