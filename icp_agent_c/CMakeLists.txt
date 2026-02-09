cmake_minimum_required(VERSION 3.20)

find_package(OpenSSL REQUIRED)

add_library(icp_agent_c STATIC
    src/agent.c
    src/request_id.c
    src/identity_anonymous.c
    src/identity_ed25519_openssl.c
    src/envelope_cbor.c
)
target_include_directories(icp_agent_c PUBLIC include)
target_compile_features(icp_agent_c PUBLIC c_std_11)
target_link_libraries(icp_agent_c PUBLIC candid_runtime OpenSSL::Crypto)

add_executable(icp_agent_c_build_smoke
    tests/test_build_smoke.c
)
target_link_libraries(icp_agent_c_build_smoke PRIVATE icp_agent_c)

add_test(NAME icp_agent_c_build_smoke COMMAND icp_agent_c_build_smoke)

add_executable(icp_agent_c_request_id
    tests/test_request_id_vectors.c
)
target_link_libraries(icp_agent_c_request_id PRIVATE icp_agent_c)

add_test(NAME icp_agent_c_request_id COMMAND icp_agent_c_request_id)

add_executable(icp_agent_c_identity
    tests/test_identity_sign.c
)
target_link_libraries(icp_agent_c_identity PRIVATE icp_agent_c OpenSSL::Crypto)

add_test(NAME icp_agent_c_identity COMMAND icp_agent_c_identity)

add_executable(icp_agent_c_envelope
    tests/test_envelope_cbor.c
)
target_link_libraries(icp_agent_c_envelope PRIVATE icp_agent_c)

add_test(NAME icp_agent_c_envelope COMMAND icp_agent_c_envelope)
