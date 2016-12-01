/* Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef API_MANAGER_AUTH_LIB_BASE64_H_
#define API_MANAGER_AUTH_LIB_BASE64_H_

#include <string.h>

namespace google {
namespace api_manager {
namespace auth {

// Base64 encode a data.
// Returned buffer should be freed by esp_grpc_free.
char *esp_base64_encode(const void *data, size_t data_size, bool url_safe,
                        bool multiline, bool padding);

}  // namespace auth
}  // namespace api_manager
}  // namespace google

#endif /* API_MANAGER_AUTH_LIB_BASE64_H_ */
