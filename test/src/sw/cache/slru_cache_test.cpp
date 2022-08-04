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

#include "sw/cache/slru_cache_test.h"
#include "sw/cache/utils.h"

namespace sw::cache::test {

void SlruCacheTest::run() {
    sw::cache::SlruCache<int, int> cache(10, 0.2);

    cache.set(1, 1);
    cache.set(2, 2);
    cache.set(3, 3);
    auto v = cache.get(1);
    CACHE_ASSERT(!v, "failed to do slru test");
    v = cache.get(2);
    CACHE_ASSERT(v && *v == 2, "failed to do slru test");
    cache.set(4, 4);
    v = cache.get(2);
    CACHE_ASSERT(v && *v == 2, "failed to do slru test");
    v = cache.get(3);
    CACHE_ASSERT(v && *v == 3, "failed to do slru test");
    for (auto idx = 5; idx < 11; ++idx) {
        cache.set(idx, idx);
        v = cache.get(idx);
        CACHE_ASSERT(v && *v == idx, "failed to do slru test");
    }
    cache.set(11, 11);
    cache.set(12, 12);
    v = cache.get(4);
    CACHE_ASSERT(!v, "failed to do slru test");
    v = cache.get(11);
    CACHE_ASSERT(v && *v == 11, "failed to do slru test");
    cache.set(13, 13);
    cache.set(14, 14);
    v = cache.get(2);
    CACHE_ASSERT(!v, "failed to do slru test");
    v = cache.get(3);
    CACHE_ASSERT(v && *v == 3, "failed to do slru test");
}

}
