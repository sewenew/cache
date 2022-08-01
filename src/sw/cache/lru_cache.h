/**************************************************************************
   Copyright (c) 2022 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#ifndef SEWENEW_CACHE_LRU_CACHE_H
#define SEWENEW_CACHE_LRU_CACHE_H

#include "sw/cache/lru_cache_impl.h"

namespace sw::cache {

template <typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(std::size_t capacity) {
        _cache.set_capacity(capacity);
    }

    void set(const Key &key, Value value) {
        if (auto [iter, found] = _cache.find(key); found) {
            _cache.update(iter, std::move(value));
        } else {
            _cache.add(key, std::move(value));
        }
    }

    std::optional<Value> get(const Key &key) {
        return _cache.get(key);
    }

    void del(const Key &key) {
        _cache.del(key);
    }

private:
    detail::LruCacheImpl<Key, Value> _cache;
};

}

#endif // end SEWENEW_CACHE_LRU_CACHE_H
