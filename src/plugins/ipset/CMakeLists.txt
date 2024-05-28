
# Copyright (c) <current-year> <your-organization>
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
vpp_find_path(LIBIPSET_MNL_INCLUDE_DIR NAMES libipset/mnl.h)
if (NOT LIBIPSET_MNL_INCLUDE_DIR)
  message(WARNING "-- libipset headers not found - ipset plugin disabled")
  return()
endif()

vpp_plugin_find_library(ipset LIBIPSET_LIB libipset.so)

include_directories(${LIBIPSET_MNL_INCLUDE_DIR}/libipset)

add_vpp_plugin(ipset
  SOURCES
  ipset.c
  node.c
  ipset_periodic.c
  ipset.h

  LINK_LIBRARIES
  ${LIBIPSET_LIB}

  API_FILES
  ipset.api

  # API_TEST_SOURCES
  # ipset_test.c
)
