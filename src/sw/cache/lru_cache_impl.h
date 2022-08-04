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

#ifndef SEWENEW_CACHE_LRU_CACHE_IMPL_H
#define SEWENEW_CACHE_LRU_CACHE_IMPL_H

#include <cassert>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include "sw/cache/errors.h"

namespace sw::cache::detail {

template <typename Key, typename Value>
struct KeyValue {
    KeyValue(const Key &k, Value v) : key(k), value(std::move(v)) {}

    Key key;
    Value value;
};

template <typename Key, typename Value>
class LruCacheImpl {
public:
    using List = std::list<KeyValue<Key, Value>>;
    using Map = std::unordered_map<Key, typename List::iterator>;

    void set_capacity(std::size_t capacity) {
        if (capacity == 0) {
            throw Error("capacity should be larger than 0");
        }

        _capacity = capacity;
    }

    bool is_full() const {
        return _key_map.size() > _capacity;
    }

    std::optional<Value> get(const Key &key) {
        auto iter = _key_map.find(key);
        if (iter == _key_map.end()) {
            // Not found.
            return std::nullopt;
        } else {
            _touch(iter->second);

            return iter->second->value;
        }
    }

    std::pair<typename Map::iterator, bool> find(const Key &key) {
        auto iter = _key_map.find(key);
        return std::make_pair(iter,
                (iter == _key_map.end() ? false : true));
    }

    typename List::iterator mru_item() {
        assert(!_kv_list.empty());

        return _kv_list.begin();
    }

    void add(const Key &key, Value value) {
        _kv_list.emplace_front(key, std::move(value));
        _key_map.emplace(key, _kv_list.begin());

        if (_key_map.size() > _capacity) {
            _key_map.erase(_kv_list.back().key);
            _kv_list.pop_back();
        }

        assert(_key_map.size() <= _capacity && _key_map.size() == _kv_list.size());
    }

    void update(typename Map::iterator iter, Value value) {
        assert(iter != _key_map.end());

        iter->second->value = std::move(value);

        _touch(iter->second);
    }

    // @return true if some item deleted. false, if key does not exist
    bool del(const Key &key) {
        auto iter = _key_map.find(key);
        if (iter != _key_map.end()) {
            _kv_list.erase(iter->second);
            _key_map.erase(iter);

            return true;
        }

        return false;
    }

    void move_item(typename Map::iterator iter, LruCacheImpl &dest) {
        assert(iter != _key_map.end());

        dest._kv_list.splice(dest._kv_list.begin(), _kv_list, iter->second);

        dest._key_map.emplace(iter->second->key, iter->second);

        _key_map.erase(iter);
    }

    void move_lru_item(LruCacheImpl &dest) {
        auto iter = std::prev(_kv_list.end());
        auto map_iter = _key_map.find(iter->key);

        assert(map_iter != _key_map.end());

        move_item(map_iter, dest);
    }

private:
    void _touch(typename List::iterator iter) {
        assert(iter != _kv_list.end());

        _kv_list.splice(_kv_list.begin(), _kv_list, iter);
    }

    List _kv_list;
    Map _key_map;
    std::size_t _capacity;
};

}

#endif // end SEWENEW_CACHE_LRU_CACHE_IMPL_H
