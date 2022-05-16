# Copyright (c) 2019, Sascha Willems
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 the "License";
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_LIST_DIR} PATH)
get_filename_component(CATEGORY_NAME ${PARENT_DIR} NAME)

set (EXTRA_FILES
    hair_system.h
    hair_system.cpp
    strand_hair.h
    strand_hair.cpp
    hybrid_hair.h
    hybrid_hair.cpp
    obj_loader.h
    obj_loader.cpp
    tiny_obj_loader.h
    vertex.h
    scene.h
    scene.cpp
    render_system.h
    render_system.cpp
    model.h
    model.cpp
    hair_render.cpp
    model_render.cpp
    grid_render.cpp
    define.h
    util.h
    util.cpp
)

add_sample_with_tags(
    ID ${FOLDER_NAME}
    CATEGORY ${CATEGORY_NAME}
    AUTHOR "Yiwen Xue"
    NAME "PHysically based hair simulation"
    DESCRIPTION "Multi-pass compute dispatch Hair simulation"
    FILES ${EXTRA_FILES}
)