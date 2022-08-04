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

#include "sw/cache/lru_cache_test.h"
#include "sw/cache/utils.h"

namespace sw::cache::test {

void LruCacheTest::run() {
    sw::cache::LruCache<int, int> cache(2);

    cache.set(1, 1);
    cache.set(2, 2);
    auto v = cache.get(1);
    CACHE_ASSERT(v && *v == 1, "failed to do lru test");
    cache.set(3, 3);
    v = cache.get(2);
    CACHE_ASSERT(!v, "failed to do lru test");
    cache.set(4, 4);
    v = cache.get(1);
    CACHE_ASSERT(!v, "failed to do lru test");
    v = cache.get(3);
    CACHE_ASSERT(v && *v == 3, "failed to do lru test");
    v = cache.get(4);
    CACHE_ASSERT(v && *v == 4, "failed to do lru test");
}

}
