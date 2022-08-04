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

#ifndef SEWENEW_CACHE_TEST_UTILS_H
#define SEWENEW_CACHE_TEST_UTILS_H

#include <string>
#include <vector>

#define CACHE_ASSERT(condition, msg) \
    sw::cache::test::cache_assert((condition), (msg), __FILE__, __LINE__)

namespace sw::cache::test {

inline void cache_assert(bool condition,
                            const std::string &msg,
                            const std::string &file,
                            int line) {
    if (!condition) {
        auto err_msg = "ASSERT: " + msg + ". " + file + ":" + std::to_string(line);
        throw Error(err_msg);
    }
}

}

#endif // end SEWENEW_CACHE_TEST_UTILS_H
