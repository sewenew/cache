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

#ifndef SEWENEW_CACHE_LIRS_CACHE_H
#define SEWENEW_CACHE_LIRS_CACHE_H

#include <cassert>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include <variant>
#include "sw/cache/errors.h"

// The implementation of LIRS cache is based on
// http://web.cse.ohio-state.edu/hpcs/WWW/HTML/publications/papers/TR-02-6.pdf

namespace sw::cache {

namespace detail {

enum class LirsType {
    LIR = 0,
    HIR,
    HIR_NR
};

template <typename Key, typename Value>
class LirsQueue {
public:
    struct LirsItem;

    using ItemList = std::list<LirsItem>;

    using VarValue = std::variant<std::monostate,
                          Value,
                          typename ItemList::iterator>;
    struct LirsItem {
        LirsItem(const Key &k, Value val, LirsType t) :
            key(k), value(std::move(val)), type(t) {}

        LirsItem(const Key &k, typename ItemList::iterator iter, LirsType t) :
            key(k), value(iter), type(t) {}

        Key key;

        VarValue value;

        LirsType type = LirsType::LIR;
    };

    using KeyMap = std::unordered_map<Key, typename ItemList::iterator>;

    LirsQueue() = default;

    void set_capacity(std::size_t capacity) {
        _capacity = capacity;
    }

    std::size_t capacity() const {
        return _capacity;
    }

    bool empty() const {
        return _list.empty();
    }

    // This only works for list Q, i.e. HIR queue.
    bool is_full() const {
        return _list.size() >= _capacity;
    }

    typename ItemList::iterator front() {
        assert(!_list.empty());

        return _list.begin();
    }

    typename ItemList::iterator back() {
        assert(!_list.empty());

        return std::prev(_list.end());
    }

    void add(const Key &key, Value value, LirsType type) {
        _list.emplace_front(key, std::move(value), type);
        _map.emplace(key, _list.begin());
    }

    void add(const Key &key, typename ItemList::iterator iter, LirsType type) {
        _list.emplace_front(key, iter, type);
        _map.emplace(key, _list.begin());
    }

    void move_item(typename ItemList::iterator iter, LirsQueue<Key, Value> &dest) {
        assert(iter != _list.end());

        dest._list.splice(dest._list.begin(), _list, iter);

        dest._map.emplace(iter->key, dest._list.begin());

        _map.erase(iter->key);
    }

    void remove(typename KeyMap::iterator iter) {
        assert(iter != _map.end());

        _list.erase(iter->second);
        _map.erase(iter);
    }

    void remove(typename ItemList::iterator iter) {
        assert(iter != _list.end());

        _map.erase(iter->key);
        _list.erase(iter);
    }

    void remove(const Key &key) {
        auto iter = _map.find(key);
        if (iter != _map.end()) {
            remove(iter);
        }
    }

    void touch(typename ItemList::iterator iter) {
        assert(iter != _list.end());

        _list.splice(_list.begin(), _list, iter);
    }

    std::pair<typename KeyMap::iterator, bool> find(const Key &key) {
        auto iter = _map.find(key);
        return std::make_pair(iter,
                (iter == _map.end() ? false : true));
    }

private:
    ItemList _list;

    KeyMap _map;

    std::size_t _capacity = 0;
};

}

template <typename Key, typename Value>
class LirsCache {
public:
    LirsCache(std::size_t capacity, double hirs_ratio = 0.01) {
        if (capacity == 0) {
            throw Error("capacity should be larger than 0");
        }

        if (hirs_ratio <= 0 || hirs_ratio >= 1.0) {
            throw Error("hirs ratio should be larger than 0 and less than 1.0");
        }

        auto hirs_capacity = static_cast<std::size_t>(capacity * hirs_ratio);
        auto lirs_capacity = capacity - hirs_capacity;
        if (hirs_capacity == 0 || lirs_capacity == 0) {
            throw Error("invalid hirs_ratio");
        }

        _stack_s.set_capacity(lirs_capacity);
        _list_q.set_capacity(hirs_capacity);
    }

    std::optional<Value> get(const Key &key) {
        auto val = _get_from_stack_s(key);
        if (val) {
            return val;
        }

        return _get_from_list_q(key);
    }

    void set(const Key &key, Value value) {
        if (auto [iter, found] = _stack_s.find(key); found) {
            _update_item_in_stack_s(iter, std::move(value));
        } else {
            if (auto [iter, found] = _list_q.find(key); found) {
                _update_item_in_list_q(key, iter->second, std::move(value));
            } else {
                // Not found.
                _add(key, std::move(value));
            }
        }
    }

    void del(const Key &key) {
        if (_del_from_stack_s(key)) {
            return;
        }

        _del_from_list_q(key);
    }

private:
    std::optional<Value> _get_from_stack_s(const Key &key) {
        auto [iter, found] = _stack_s.find(key);
        if (!found) {
            return std::nullopt;
        }

        auto item = iter->second;
        switch (item->type) {
            case detail::LirsType::LIR:
                // Hit LIR block. Update position, i.e. move it to front.
                _stack_s.touch(item);

                break;

            case detail::LirsType::HIR:
                // Hit HIR resident block, and key exists in stack S.

                // Move HIR resident block to stack S, and change the type to LIR.
                _move_hir_to_stack_s(iter);

                if (_lirs_cnt > _stack_s.capacity()) {
                    assert(!_list_q.is_full());

                    _move_lru_lir_to_list_q();
                }

                break;

            case detail::LirsType::HIR_NR:
                return std::nullopt;

            default:
                assert(false);
                throw Error("unknow LIRS type");
                break;
        }

        _prune_stack_s();

        // When we get here, we're sure that the hitted block is in front of stack S.
        return _to_value(_stack_s.front()->value);
    }

    std::optional<Value> _get_from_list_q(const Key &key) {
        auto [iter, found] = _list_q.find(key);
        if (!found) {
            return std::nullopt;
        }

        // Hit HIR resident block, and key does not exist in stack S.

        // Create a new entry on stack S.
        _stack_s.add(key, iter->second, detail::LirsType::HIR);

        // Update its position in list Q, i.e. move it to front.
        _list_q.touch(iter->second);

        return _to_value(_list_q.front()->value);
    }

    void _move_hir_to_stack_s(typename detail::LirsQueue<Key, Value>::KeyMap::iterator iter) {
        auto q_iter = _to_iterator(iter->second->value);

        // Remove old HIR resident block in stack S.
        _stack_s.remove(iter);

        _list_q.move_item(q_iter, _stack_s);

        _stack_s.front()->type = detail::LirsType::LIR;

        ++_lirs_cnt;
    }

    void _move_lru_lir_to_list_q() {
        assert(_lirs_cnt == _stack_s.capacity() + 1);

        _prune_stack_s();

        assert(!_stack_s.empty() && _stack_s.back()->type == detail::LirsType::LIR);

        // Move lru LIR block to list Q.
        _stack_s.move_item(_stack_s.back(), _list_q);

        _list_q.front()->type = detail::LirsType::HIR;

        --_lirs_cnt;

        assert(_lirs_cnt == _stack_s.capacity());
    }

    // @return true if key exists in stack S. false, otherwise.
    bool _del_from_stack_s(const Key &key) {
        auto [iter, found] = _stack_s.find(key);
        if (!found) {
            return false;
        }

        auto item = iter->second;
        switch (item->type) {
            case detail::LirsType::LIR:
                _stack_s.remove(iter);
                --_lirs_cnt;
                break;

            case detail::LirsType::HIR:
                _list_q.remove(_to_iterator(item->value));
                _stack_s.remove(iter);
                break;

            default:
                assert(item->type == detail::LirsType::HIR_NR);
                break;
        }

        return true;
    }

    void _del_from_list_q(const Key &key) {
        if (auto [iter, found] = _list_q.find(key); found) {
            _list_q.remove(iter);
        }
    }

    auto _to_iterator(typename detail::LirsQueue<Key, Value>::VarValue &value)
        -> typename detail::LirsQueue<Key, Value>::ItemList::iterator {
        assert((std::holds_alternative<typename detail::LirsQueue<Key, Value>::ItemList::iterator>(value)));

        return std::get<typename detail::LirsQueue<Key, Value>::ItemList::iterator>(value);
    }

    Value _to_value(typename detail::LirsQueue<Key, Value>::VarValue &value) {
        assert(std::holds_alternative<Value>(value));

        return std::get<Value>(value);
    }

    void _update_item_in_stack_s(typename detail::LirsQueue<Key, Value>::KeyMap::iterator iter, Value value) {
        auto item = iter->second;
        switch (item->type) {
            case detail::LirsType::LIR:
                // Hit LIR block.
                item->value = std::move(value);
                _stack_s.touch(item);
                break;

            case detail::LirsType::HIR:
                // Hit HIR resident block, and key exists in stack S.

                // Move HIR resident block to stack S, and change the type to LIR.
                _move_hir_to_stack_s(iter);

                _stack_s.front()->value = std::move(value);

                if (_lirs_cnt > _stack_s.capacity()) {
                    assert(!_list_q.is_full());

                    _move_lru_lir_to_list_q();
                }

                break;

            case detail::LirsType::HIR_NR:
                _hir_nr_to_lir(item, std::move(value));

                if (_lirs_cnt > _stack_s.capacity()) {
                    // Evict a HIR resident block to get some space.
                    if (_list_q.is_full()) {
                        _remove_lru_hir_item();
                    }

                    assert(!_list_q.is_full());

                    _move_lru_lir_to_list_q();
                }

                break;

            default:
                assert(false);
                break;
        }

        _prune_stack_s();
    }

    void _hir_nr_to_lir(typename detail::LirsQueue<Key, Value>::ItemList::iterator iter, Value value) {
        iter->type = detail::LirsType::LIR;
        iter->value = std::move(value);

        _stack_s.touch(iter);

        ++_lirs_cnt;
    }

    void _update_item_in_list_q(const Key &key,
            typename detail::LirsQueue<Key, Value>::ItemList::iterator iter,
            Value value) {
        // Hit HIR block, and key does not exist in stack S.

        iter->value = std::move(value);

        // Create a new entry on stack S.
        _stack_s.add(key, iter, detail::LirsType::HIR);

        // Update its position in list Q, i.e. move it to front.
        _list_q.touch(iter);
    }

    void _remove_lru_hir_item() {
        assert(_list_q.is_full());

        auto lru = _list_q.back();
        if (auto [iter, found] = _stack_s.find(lru->key); found) {
            // The to-be-evicted item exists in stack S.
            // Modify type and clear value.
            auto item = iter->second;
            item->type = detail::LirsType::HIR_NR;
            item->value = std::monostate{};
        }

        _list_q.remove(lru);
    }

    void _add(const Key &key, Value value) {
        // TODO: What if _s_list is not empty, while _q_list is empty?
        // The paper does not mention how to handle this case, since it
        // does not have a `del` operation.
        if (_lirs_cnt < _stack_s.capacity()) {
            _stack_s.add(key, std::move(value), detail::LirsType::LIR);
            ++_lirs_cnt;
        } else {
            if (_list_q.is_full()) {
                _remove_lru_hir_item();
            }

            assert(!_list_q.is_full());

            _list_q.add(key, std::move(value), detail::LirsType::HIR);

            _stack_s.add(key, _list_q.front(), detail::LirsType::HIR);
        }
    }

    bool _should_prune() {
        if (_stack_s.empty()) {
            return false;
        }

        return _stack_s.back()->type != detail::LirsType::LIR;
    }

    void _prune_stack_s() {
        while (_should_prune()) {
            auto item = _stack_s.back();
            if (item->type == detail::LirsType::HIR) {
                // Remove item from list Q.
                _list_q.remove(item->key);
            }

            assert(item->type == detail::LirsType::HIR_NR);

            // Remove it from stack S.
            _stack_s.remove(item);
        }
    }

    // We call these two queues as stack and list based on the paper.
    // In fact, they are LRU-like queues.
    detail::LirsQueue<Key, Value> _stack_s;

    detail::LirsQueue<Key, Value> _list_q;

    std::size_t _lirs_cnt = 0;
};

}

#endif // end SEWENEW_CACHE_LIRS_CACHE_H
