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

#ifndef SEWENEW_CACHE_LFU_CACHE_H
#define SEWENEW_CACHE_LFU_CACHE_H

#include <cassert>
#include <limits>
#include <list>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include "sw/cache/errors.h"

namespace sw::cache {

// The implementation of LFU cache is based on
// http://dhruvbird.com/lfu.pdf
template <typename Key, typename Value>
class LfuCache {
    class LfuItem;

    using ItemList = std::list<LfuItem>;

    struct FrequencyNode {
        explicit FrequencyNode(std::size_t cnt) : frequency(cnt) {}

        std::size_t frequency = 0;
        ItemList items;
    };

    using FrequencyList = std::list<FrequencyNode>;

    struct LfuItem {
        LfuItem(const Key &k, Value v) : key(k), value(std::move(v)) {}

        Key key;
        Value value;

        typename FrequencyList::iterator frequency_node;
    };

    using Map = std::unordered_map<Key, typename ItemList::iterator>;

public:
    explicit LfuCache(std::size_t capacity) : _capacity(capacity) {
        if (_capacity == 0) {
            throw Error("capacity should be larger than 0");
        }
    }

    void set(const Key &key, Value value) {
        if (auto iter = _key_map.find(key); iter == _key_map.end()) {
            // New item. Evict before adding a new one.
            if (_is_full()) {
                _evict();
            }

            auto node_iter = _frequency_list.begin();
            if (_frequency_list.empty() || _frequency_list.front().frequency != 1) {
                // Need to add a new frequency node at the head.
                assert(_frequency_list.empty() || _frequency_list.front().frequency > 1);

                node_iter = _add_node(_frequency_list.begin(), 1);

                assert(_frequency_list.front().frequency == 1);
            }

            assert(node_iter != _frequency_list.end());

            _add_item(node_iter, LfuItem(key, std::move(value)));
        } else {
            // Item already exists. Need to update item position.
            _touch(iter->second);

            // Update value.
            iter->second->value = std::move(value);
        }
    }

    std::optional<Value> get(const Key &key) {
        if (auto iter = _key_map.find(key); iter != _key_map.end()) {
            // Found the key. Update item position.
            _touch(iter->second);

            return iter->second->value;
        } else {
            // Not found.
            return std::nullopt;
        }
    }

    void del(const Key &key) {
        if (auto iter = _key_map.find(key); iter != _key_map.end()) {
            auto item = iter->second;
            auto node = item->frequency_node;
            node->items.erase(item);
            if (node->items.empty()) {
                _frequency_list.erase(node);
            }
            _key_map.erase(iter);
        }
    }

private:
    bool _is_full() const {
        return _key_map.size() == _capacity;
    }

    // Add a new node before `pos`.
    typename FrequencyList::iterator _add_node(typename FrequencyList::iterator pos, std::size_t frequency) {
        return _frequency_list.insert(pos, FrequencyNode(frequency));
    }

    void _add_item(typename FrequencyList::iterator node_iter, LfuItem item) {
        auto &items = node_iter->items;

        items.push_back(std::move(item));
        items.back().frequency_node = node_iter;

        _key_map.emplace(items.back().key, std::prev(items.end()));
    }

    void _move_item(typename FrequencyList::iterator dest,
            typename FrequencyList::iterator src,
            typename ItemList::iterator item) {
        auto &src_list = src->items;
        auto &dest_list = dest->items;
        dest_list.splice(dest_list.end(), src_list, item);

        item->frequency_node = dest;

        if (src_list.empty()) {
            // Source node's item list is empty, remove it.
            _frequency_list.erase(src);
        }
    }

    void _touch(typename ItemList::iterator item) {
        auto src = item->frequency_node;
        auto dest = std::next(src);
        auto frequency = _next_node_frequency(src);
        if (dest == _frequency_list.end()) {
            if (frequency > src->frequency) {
                // Need to add a new node
                dest = _add_node(dest, frequency);
            } else {
                // Reached the maximum frequency, do LRU on this node.
                dest = src;
            }
        } else {
            if (dest->frequency != frequency) {
                // Need to add a new node.
                dest = _add_node(dest, frequency);
            } // else: node with the right frequency already exists.
        }

        _move_item(dest, src, item);
    }

    std::size_t _next_node_frequency(typename FrequencyList::iterator node) const {
        assert(node != _frequency_list.end());

        auto frequency = node->frequency;
        if (frequency == std::numeric_limits<std::size_t>::max()) {
            // Has reached the max possible freqency.
            return frequency;
        } else {
            return frequency + 1;
        }
    }

    void _evict() {
        assert(!_frequency_list.empty() && !_key_map.empty());

        auto node = _frequency_list.begin();
        auto &items = node->items;

        assert(!items.empty());

        _key_map.erase(items.front().key);
        items.pop_front();

        if (items.empty()) {
            _frequency_list.erase(node);
        }
    }

    FrequencyList _frequency_list;

    Map _key_map;

    std::size_t _capacity;
};

}

#endif // end SEWENEW_CACHE_LFU_CACHE_H
