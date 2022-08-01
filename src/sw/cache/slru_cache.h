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

#ifndef SEWENEW_CACHE_SLRU_CACHE_H
#define SEWENEW_CACHE_SLRU_CACHE_H

#include <list>
#include <optional>
#include <unordered_map>
#include "sw/cache/errors.h"
#include "sw/cache/lru_cache_impl.h"

namespace sw::cache {

template <typename Key, typename Value>
class SlruCache {
public:
    SlruCache(std::size_t capacity, double probation_ratio = 0.2) {
        auto [probation_seg_capacity, protected_seg_capacity] = _calc_size(capacity, probation_ratio);
        _probation_seg.set_capacity(probation_seg_capacity);
        _protected_seg.set_capacity(protected_seg_capacity);
    }

    void set(const Key &key, Value value) {
        if (auto [iter, found] = _protected_seg.find(key); found) {
            // Update existing item in protected seg.
            _protected_seg.update(iter, std::move(value));
        } else {
            // Not in protected seg. Try probation seg.
            if (auto [iter, found] = _probation_seg.find(key); found) {
                // Double hit an item in probation seg.
                _move_from_probation_to_protected(iter);

                // Don't forget update value, and the item has been the Most Recently Used one.
                _protected_seg.mru_item()->value = std::move(value);
            } else {
                // New item. Put it into probation seg.
                _probation_seg.add(key, std::move(value));
            }
        }
    }

    std::optional<Value> get(const Key &key) {
        // Try protected seg first.
        auto value = _protected_seg.get(key);
        if (value) {
            return value;
        }

        // Try probation seg.
        if (auto [iter, found] = _probation_seg.find(key); found) {
            // Double hit an item in probation seg.
            _move_from_probation_to_protected(iter);

            // The item has been the Most Recently Used one.
            return _protected_seg.mru_item()->value;
        } else {
            // Not found.
            return std::nullopt;
        }
    }

    void del(const Key &key) {
        if (!_probation_seg.del(key)) {
            // Not in probation seg. Try proteced seg.
            _protected_seg.del(key);
        }
    }

private:
    std::pair<std::size_t, std::size_t> _calc_size(std::size_t capacity, double probation_ratio) const {
        if (capacity == 0) {
            throw Error("capacity should be larger than 0");
        }

        if (probation_ratio < 0 || probation_ratio > 1.0) {
            throw Error("probation ration should be in (0, 1)");
        }

        auto probation_size = static_cast<std::size_t>(capacity * probation_ratio);
        auto protected_size = capacity - probation_size;
        if (probation_size == 0 || protected_size == 0) {
            throw Error("invalid probation_ratio");
        }

        return std::make_pair(probation_size, protected_size);
    }

    void _move_from_probation_to_protected(typename detail::LruCacheImpl<Key, Value>::Map::iterator iter) {
        _probation_seg.move_item(iter, _protected_seg);

        if (_protected_seg.is_full()) {
            _protected_seg.move_lru_item(_probation_seg);
        }
    }

    detail::LruCacheImpl<Key, Value> _probation_seg;

    detail::LruCacheImpl<Key, Value> _protected_seg;
};

}

#endif // end SEWENEW_CACHE_SLRU_CACHE_H
