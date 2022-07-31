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

#ifndef SEWENEW_CACHE_SLRU_H
#define SEWENEW_CACHE_SLRU_H

#include "lru.h"

namespace sw::cache {

template <typename Key, typename Value>
class SlruCache {
    struct KeyValue {
        KeyValue(const Key &k, Value v) : key(k), value(std::move(v)) {}

        Key key;
        Value value;
    };

    using List = std::list<KeyValue>;
    using Map = std::unordered_map<Key, typename List::iterator>;

    struct LruCache {
        bool should_cap() const {
            return key_map.size() > capacity;
        }

        void add(const Key &key, Value value) {
            kv_list.emplace_front(key, std::move(value));
            key_map.emplace(key, kv_list.begin());
        }

        void cap() {
            key_map.erase(kv_list.back().key);
            kv_list.pop_back();
        }

        void touch(typename List::iterator &iter) {
            kv_list.splice(kv_list.begin(), kv_list, iter);
            iter = kv_list.begin();
        }

        // @return true if some item deleted. false, if key does not exist
        bool del(const Key &key) {
            auto iter = key_map.find(key);
            if (iter != key_map.end()) {
                kv_list.erase(iter->second);
                key_map.erase(iter);

                return true;
            }

            return false;
        }

        List kv_list;
        Map key_map;
        std::size_t capacity;
    };

public:
    SlruCache(std::size_t capacity, double probation_ratio = 0.2) {
        std::tie(_probation_seg.capacity, _protected_seg.capacity) = _calc_size(capacity, probation_ratio);
    }

    void set(const Key &key, Value value) {
        auto iter = _probation_seg.key_map.find(key);
        if (iter == _probation_seg.key_map.end()) {
            iter = _protected_seg.key_map.find(key);
            if (iter == _protected_seg.key_map.end()) {
                // New item. Put it into probation seg.
                _probation_seg.add(key, std::move(value));
                if (_probation_seg.should_cap()) {
                    _probation_seg.cap();
                }
            } else {
                // Update item in protected seg.
                iter->second->value = std::move(value);

                _protected_seg.touch(iter->second);
            }
        } else {
            // Double hit an item in probation seg.
            _move_item_to_protected_seg(iter);

            // Don't forget update value.
            _protected_seg.kv_list.front().value = std::move(value);

            if (_protected_seg.should_cap()) {
                // Protected seg is full.
                _move_lru_item_to_probation_seg();
            }
        }
    }

    std::optional<Value> get(const Key &key) {
        auto iter = _probation_seg.key_map.find(key);
        if (iter == _probation_seg.key_map.end()) {
            iter = _protected_seg.key_map.find(key);
            if (iter == _protected_seg.key_map.end()) {
                // Not found in both seg.
                return std::nullopt;
            } else {
                // Found in protected seg.
                _protected_seg.touch(iter->second);
                return std::make_optional(_protected_seg.kv_list.begin()->value);
            }
        } else {
            // Double hit an item in probation seg.
            _move_item_to_protected_seg(iter);

            if (_protected_seg.should_cap()) {
                // Protected seg is full.
                _move_lru_item_to_probation_seg();
            }

            return std::make_optional(_protected_seg.kv_list.front().value);
        }
    }

    void del(const Key &key) {
        if (!_probation_seg.del(key)) {
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

    void _move_lru_item_to_probation_seg() {
        _probation_seg.kv_list.splice(_probation_seg.kv_list.begin(),
                _protected_seg.kv_list,
                std::prev(_protected_seg.kv_list.end()));

        const auto &key = _probation_seg.kv_list.front().key;
        _probation_seg.key_map.emplace(key, _probation_seg.kv_list.begin());

        _protected_seg.key_map.erase(key);
    }

    void _move_item_to_protected_seg(typename Map::iterator iter) {
        _protected_seg.kv_list.splice(_protected_seg.kv_list.begin(), _probation_seg.kv_list, iter->second);

        _protected_seg.key_map.emplace(_protected_seg.kv_list.front().key, _protected_seg.kv_list.begin());

        _probation_seg.key_map.erase(iter);
    }

    LruCache _probation_seg;

    LruCache _protected_seg;
};

}

#endif // end SEWENEW_CACHE_SLRU_H
