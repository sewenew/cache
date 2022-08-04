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

#include <iostream>
#include "sw/cache/lru_cache_test.h"
#include "sw/cache/slru_cache_test.h"
#include "sw/cache/lfu_cache_test.h"

int main() {
    try {
        sw::cache::test::LruCacheTest lru_test;
        lru_test.run();
        std::cout << "Pass LruCache test" << std::endl;

        sw::cache::test::SlruCacheTest slru_test;
        slru_test.run();
        std::cout << "Pass SlruCache test" << std::endl;

        sw::cache::test::LfuCacheTest lfu_test;
        lfu_test.run();
        std::cout << "Pass LfuCache test" << std::endl;
    } catch (const sw::cache::Error &err) {
        std::cerr << "failed to run cache test: " << err.what() << std::endl;
    }
    return 0;
}
