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

#include "sw/cache/lirs_cache_test.h"
#include <string>
#include "sw/cache/utils.h"

namespace sw::cache::test {

void LirsCacheTest::run() {
    sw::cache::LirsCache<std::string, int> cache(3, 0.34);

    cache.set("B", 1); // lir: B(lir)
    cache.set("A", 1); // lir: B(lir), A(lir)
    cache.set("D", 1); // lir: B(lir), A(lir), D(hir); hir: D
    cache.del("D"); // lir: B(lir), A(lir), D(hir_nr); hir:
    auto v = cache.get("D");
    CACHE_ASSERT(!v, "failed to do lirs test");

    cache.del("A"); // lir: B(lir), D(hir_nr); hir:
    v = cache.get("A");
    CACHE_ASSERT(!v, "failed to do lirs test");

    cache.set("A", 1); // lir: B(lir), D(hir_nr), A(lir); hir:
    cache.set("E", 1); // lir: B(lir), D(hir_nr), A(lir), E(hir); hir: E

    cache.set("D", 2); // lir: A(lir), E(hir_nr), D(lir); hir: B
    v = cache.get("D");
    CACHE_ASSERT(v && *v == 2, "failed to do lirs test");

    v = cache.get("E");
    CACHE_ASSERT(!v, "failed to do lirs test");

    v = cache.get("A"); // lir: D(lir), A(lir); hir: B
    CACHE_ASSERT(v && *v == 1, "failed to do lirs test");
}

}
