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

#ifndef SEWENEW_CACHE_LRU_H
#define SEWENEW_CACHE_LRU_H

#include <list>
#include <optional>
#include <unordered_map>

namespace sw::cache {

template <typename Key, typename Value>
class LruCache {
    struct KeyValue {
        Key key;
        Value value;
    };

public:
    explicit LruCache(std::size_t capacity) : _capacity(capacity) {}

    void set(const Key &key, Value value) {
        auto iter = _map.find(key);
        if (iter == _map.end()) {
            return _insert(key, std::move(value));
        }

        // update
        auto list_iter = iter->second;
        list_iter->value = std::move(value);
        _list.splice(_list.begin(), _list, list_iter);
        iter->second = _list.begin();
    }

    std::optional<Value> get(const Key &key) {
        auto iter = _map.find(key);
        if (iter == _map.end()) {
            return std::nullopt;
        }

        _list.splice(_list.begin(), _list, iter->second);
        return std::make_optional(_list.begin()->value);
    }

    void del(const Key &key) {
        auto iter = _map.find(key);
        if (iter == _map.end()) {
            return;
        }

        _list.erase(iter->second);
        _map.erase(iter);
    }

private:
    void _insert(const Key &key, Value value) {
        KeyValue kv;
        kv.key = key;
        kv.value = std::move(value);
        _list.push_front(std::move(kv));
        _map.emplace(key, _list.begin());

        // evict
        if (_map.size() > _capacity) {
            _map.erase(_list.back().key);
            _list.pop_back();
        }
    }

    std::list<KeyValue> _list;
    std::unordered_map<Key, typename std::list<KeyValue>::iterator> _map;
    std::size_t _capacity = 0;
};

}

#endif // end SEWENEW_CACHE_LRU_H
