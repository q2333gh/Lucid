cmake_minimum_required(VERSION 3.20)

add_library(icp_agent_c STATIC
    src/agent.c
)
target_include_directories(icp_agent_c PUBLIC include)
target_compile_features(icp_agent_c PUBLIC c_std_11)

add_executable(icp_agent_c_build_smoke
    tests/test_build_smoke.c
)
target_link_libraries(icp_agent_c_build_smoke PRIVATE icp_agent_c)

add_test(NAME icp_agent_c_build_smoke COMMAND icp_agent_c_build_smoke)
