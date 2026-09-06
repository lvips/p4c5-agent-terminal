# p4c5-agent-terminal — ESP-IDF 固件
# 最小可编译项目骨架 (M4)
cmake_minimum_required(VERSION 3.16)

# 禁用某些初始化警告
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# 精简构建：仅包含必要组件
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "0.1.0")
project(p4c5_agent_terminal)
