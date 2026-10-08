# Copyright (c) 2026 CYBOU contributors
# SPDX-License-Identifier: Apache-2.0

# CYBOU native unit and integration test suites.
add_executable(cybou-core-test
  cybou_main.cpp
  cybou_account_creation_tests.cpp
  cybou_identity_tests.cpp
  cybou_identity_authorization_tests.cpp
  cybou_identity_crypto_tests.cpp
  cybou_identity_kem_tests.cpp
  cybou_hex_tests.cpp
  cybou_loadgen_metrics_tests.cpp
  cybou_identity_registry_tests.cpp
  cybou_identity_material_tests.cpp
  cybou_identity_service_tests.cpp
  cybou_network_genesis_tests.cpp
  cybou_poa_tests.cpp
  cybou_p2p_peer_manager_tests.cpp
  cybou_peer_admission_tests.cpp
  cybou_geo_database_updater_tests.cpp
  cybou_node_service_tests.cpp
  cybou_node_runtime_tests.cpp
  cybou_identity_operation_coordinator_tests.cpp
  cybou_wallet_service_tests.cpp
  cybou_identity_vault_tests.cpp
  cybou_recovery_phrase_tests.cpp
  cybou_protocol_operation_tests.cpp
  cybou_signing_tests.cpp
  cybou_state_tests.cpp
  cybou_resource_limits_tests.cpp
  cybou_binary_codec_tests.cpp
  cybou_observation_report_tests.cpp
  cybou_observation_cache_tests.cpp
  cybou_chunk_id_tests.cpp
  cybou_chunk_authorization_tests.cpp
  cybou_encrypted_chunk_tests.cpp
  cybou_encrypted_chunk_tree_tests.cpp
  cybou_chunk_blob_store_tests.cpp
  cybou_chunk_retention_tests.cpp
  cybou_finalized_chunk_store_tests.cpp
  cybou_kv_store_tests.cpp
  cybou_private_application_store_tests.cpp
  cybou_private_application_schema_tests.cpp
  cybou_publication_service_tests.cpp
  cybou_storage_economy_tests.cpp
  cybou_storage_placement_simulation_tests.cpp
  cybou_storage_service_tests.cpp
  cybou_application_service_tests.cpp
  cybou_root_publication_tests.cpp
  cybou_root_publication_capsule_tests.cpp
  cybou_crypto_tests.cpp
)
if(CYBOU_ENABLE_TEST_HOOKS)
  target_compile_definitions(cybou-core-test PRIVATE CYBOU_ENABLE_TEST_HOOKS=1)
endif()

cybou_windows_manifest(cybou-core-test)

target_link_libraries(cybou-core-test
  cybou_core
  cybou_node
  core_interface
  Boost::headers
)

# Multi-process PoA + storage provider smoke and soak clients.
add_executable(cybou-storage-smoke cybou_storage_smoke.cpp)
target_link_libraries(cybou-storage-smoke
  cybou_node
  core_interface
  Boost::headers
)

add_executable(cybou-storage-soak cybou_storage_soak.cpp)
target_link_libraries(cybou-storage-soak
  cybou_node
  core_interface
  Boost::headers
)

function(add_boost_test test_target source_file)
  if(NOT EXISTS ${source_file})
    return()
  endif()

  file(READ "${source_file}" source_file_content)
  string(REGEX
    MATCHALL "(BOOST_FIXTURE_TEST_SUITE|BOOST_AUTO_TEST_SUITE)\\(([A-Za-z0-9_]+)"
    test_suite_macro "${source_file_content}"
  )
  list(TRANSFORM test_suite_macro
    REPLACE "(BOOST_FIXTURE_TEST_SUITE|BOOST_AUTO_TEST_SUITE)\\(" ""
  )
  foreach(test_suite_name IN LISTS test_suite_macro)
    add_test(NAME ${test_suite_name}
      COMMAND ${test_target} --run_test=${test_suite_name} --catch_system_error=no --log_level=test_suite -- DEBUG_LOG_OUT
    )
  endforeach()
endfunction()

function(add_all_test_targets test_target)
  get_target_property(test_source_dir ${test_target} SOURCE_DIR)
  get_target_property(test_sources ${test_target} SOURCES)
  foreach(test_source ${test_sources})
    cmake_path(IS_RELATIVE test_source result)
    if(result)
      cmake_path(APPEND test_source_dir ${test_source} OUTPUT_VARIABLE test_source)
    endif()
    add_boost_test(${test_target} ${test_source})
  endforeach()
endfunction()

add_all_test_targets(cybou-core-test)
cybou_install(cybou-core-test INTERNAL)
