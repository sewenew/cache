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

#include <list>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace sw::cache {

// The implementation of LFU cache is base on
// http://dhruvbird.com/lfu.pdf
template <typename Key, typename Value>
class LfuCache {
public:
    explicit LfuCache(std::size_t capacity) : _capacity(capacity) {}

    void set(const Key &key, Value value) {
        if (auto iter = _key_map.find(key); iter == _key_map.end()) {
            // New item.
            LfuItem item;
            item.key = key;
            item.value = std::move(value);

            if (!_frequency_list.empty() && _frequency_list.front().count == 1) {
                // Frequency node alreday exists.
                auto &head_node = _frequency_list.front();
                head_node.items.push_back(std::move(item));
                head_node.items.back().frequency_node = _frequency_list.begin();
                _key_map.emplace(key, std::prev(head_node.items.end()));
            } else {
                // Need to add a new frequency node at the head.
                _add_node(_frequency_list.begin());
                _frequency_list.front().items.push_back(std::move(item));

                auto &head_node = _frequency_list.front();
                head_node.items.back().frequency_node = _frequency_list.begin();
                _key_map.emplace(key, std::prev(head_node.items.end()));
            }
        } else {
            // Item already exists.
            auto &item_iter = iter->second;
            auto cur_frequency_node = item_iter->frequency_node;
            auto next_frequency_node = std::next(cur_frequency_node);
            if (next_frequency_node != _frequency_list.end() &&
                    next_frequency_node->count == cur_frequency_node->count + 1) {
                // Node already exists. Move item to next node.
                _move_item(next_frequency_node, cur_frequency_node, item_iter);
            } else {
                // Add a new node after current node.
                auto dest = _add_node(next_frequency_node);
                _move_item(dest, cur_frequency_node, item_iter);
            }
            item_iter->value = std::move(value);
        }
    }

    std::optional<Value> get(const Key &key) {
        if (auto iter = _key_map.find(key); iter == _key_map.end()) {
            return std::nullopt;
        } else {
            auto item_iter = iter->second;
            auto node_iter = item_iter->frequency_node;
            auto next_node_iter = std::next(node_iter);
            if (next_node_iter != _frequency_list.end() &&
                    next_node_iter->count == node_iter->count + 1) {
                // Node already exists.
                _move_item(next_node_iter, node_iter, item_iter);

                return item_iter->value;
            } else {
                // Add a new node.
                auto iter = _add_node(next_node_iter);
                _move_item(iter, node_iter, item_iter);

                return item_iter->value;
            }
        }
    }

    void del(const Key &key) {
        if (auto iter = _key_map.find(key); iter != _key_map.end()) {
            auto item_iter = iter->second;
            auto node = item_iter->frequency_node;
            node->items.erase(item_iter);
            if (node->items.empty()) {
                _frequency_list.erase(node);
            }
            _key_map.erase(iter);
        }
    }

private:
    FrequencyList::iterator _add_node(FrequencyList::iterator pos) {
        FrequencyNode node;
        node.count = 1;

        return _frequency_list.insert(pos, std::move(node));
    }

    void _move_item(FrequencyList::iterator dest,
            FrequencyList::iterator src,
            ItemList::iterator &item) {
        auto &src_list = src->items;
        auto &dest_list = dest->items;
        dest_list.splice(dest_list.end(), src_list, item);
        // TODO: since splice won't invalidate iterators, we don't need to reset item
        item = std::prev(dest_list.end());
        item->frequency_node = dest;

        if (src_list.empty()) {
            // Source node's item list is empty, remove it.
            _frequency_list.erase(src);
        }
    }

    class LfuItem;

    using ItemList = std::list<LfuItem>;

    struct FrequencyNode {
        std::size_t count = 0;
        ItemList items;
    };

    using FrequencyList = std::list<FrequencyNode>;

    struct LfuItem {
        Key key;
        Value value;
        typename FrequencyList::iterator frequency_node;
    };

    using Map = std::unordered_map<Key, typename ItemList::iterator>;

    FrequencyList _frequency_list;

    Map _key_map;

    std::size_t _capacity;
};

}

#endif // end SEWENEW_CACHE_LFU_CACHE_H
