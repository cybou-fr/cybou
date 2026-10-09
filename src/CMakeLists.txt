# Copyright (c) 2026 CYBOU contributors
# SPDX-License-Identifier: Apache-2.0

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
# Every CYBOU library and executable gets the platform definitions, hardening
# and warning flags of core_interface.
link_libraries(core_interface)

# Set top-level target output locations.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()

# CYBOU-owned dependency boundary for consensus-neutral hash-value types.
# The native Hash256 implementation uses only standard-library code.
add_library(cybou_base STATIC
  cybou/hash256.cpp
)

# CYBOU consensus and identity implementation.
add_library(cybou_core STATIC
  cybou/account_creation.cpp
  cybou/block.cpp
  cybou/chunk_id.cpp
  cybou/chunk_authorization.cpp
  cybou/storage_audit.cpp
  cybou/storage_economy.cpp
  cybou/economics_quote.cpp
  cybou/storage_lease.cpp
  cybou/encrypted_chunk.cpp
  cybou/encrypted_chunk_tree.cpp
  cybou/root_publication.cpp
  cybou/root_recipient_capsule.cpp
  cybou/crypto/chacha20_poly1305.cpp
  cybou/crypto/hkdf_sha256.cpp
  cybou/crypto/sha256.cpp
  cybou/hex.cpp
  cybou/identity_authorization.cpp
  cybou/identity_crypto.cpp
  cybou/identity_kem.cpp
  cybou/identity_material.cpp
  cybou/identity_registry.cpp
  cybou/identity_registry_codec.cpp
  cybou/identity_vault.cpp
  cybou/keystore.cpp
  cybou/private_application_schema.cpp
  cybou/payment.cpp
  cybou/protocol_operation.cpp
  cybou/poa_finality.cpp
  cybou/recovery_phrase.cpp
  cybou/signing_openssl.cpp
  cybou/state.cpp
  cybou/network_genesis.cpp
  cybou/official_networks.cpp
)
target_link_libraries(cybou_core
  PUBLIC
    cybou_base
  PRIVATE
    Boost::headers
    OpenSSL::Crypto
    BLAKE3::blake3
)
if(BUILD_TESTS)
  target_compile_definitions(cybou_core PRIVATE CYBOU_OFFLINE_PROVISIONING=1)
endif()
if(CYBOU_ENABLE_TEST_HOOKS)
  target_compile_definitions(cybou_core PRIVATE CYBOU_ENABLE_TEST_HOOKS=1)
endif()

target_precompile_headers(cybou_core PRIVATE
  <openssl/evp.h>
  <openssl/sha.h>
  <vector>
  <string>
  <memory>
  <cstdint>
)



# CYBOU node runtime and P2P library.
add_library(cybou_node STATIC
  cybou/kv_store.cpp
  cybou/private_application_store.cpp
  cybou/chunk_blob_store.cpp
  cybou/chunk_retention.cpp
  cybou/finalized_chunk_store.cpp
  cybou/publication_service.cpp
  cybou/storage_service.cpp
  cybou/storage_service_repair.cpp
  cybou/storage_io_scheduler.cpp
  cybou/storage_placement_repository.cpp
  cybou/storage_evidence_ledger.cpp
  cybou/storage_replica_verifier.cpp
  cybou/storage_provider_selector.cpp
  cybou/storage_assignment.cpp
  cybou/application_service.cpp
  cybou/local_application_service.cpp
  cybou/local_content_stager.cpp
  cybou/network_sync_service.cpp
  cybou/secret_file.cpp
  cybou/poa_conflict_detector.cpp
  cybou/poa_signing_journal.cpp
  cybou/state_store.cpp
  cybou/finalized_event_index.cpp
  cybou/block_executor.cpp
  cybou/poa_finalizer.cpp
  cybou/operation_pool.cpp
  cybou/operation_work.cpp
  cybou/identity_operation_coordinator.cpp
  cybou/wallet_service.cpp
  cybou/node_runtime.cpp
  cybou/node_runtime_chain.cpp
  cybou/finalization_meter.cpp
  cybou/process_memory.cpp
  cybou/process_cpu.cpp
  cybou/node_runtime_network.cpp
  cybou/node_runtime_provider.cpp
  cybou/node_service.h
  cybou/node_service.cpp
  cybou/operation_relay.cpp
  cybou/event_record.cpp
  cybou/identity_service.cpp
  cybou/name_service.cpp
  cybou/p2p/session.cpp
  cybou/p2p/geo_database_updater.cpp
  cybou/p2p/peer_admission.cpp
  cybou/p2p/inbound_server.cpp
  cybou/p2p/peer_manager.cpp
  cybou/p2p/storage_session_pool.cpp
)
if(CYBOU_ENABLE_TEST_HOOKS)
  target_compile_definitions(cybou_node PUBLIC CYBOU_ENABLE_TEST_HOOKS=1)
endif()

target_link_libraries(cybou_node
  PUBLIC
    cybou_core
    Boost::headers
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:mswsock>
  PRIVATE
    leveldb
    OpenSSL::Crypto
    OpenSSL::SSL
    ZLIB::ZLIB
    $<$<PLATFORM_ID:Windows>:crypt32>
    $<$<PLATFORM_ID:Windows>:psapi>
)
target_precompile_headers(cybou_node PRIVATE
  <openssl/evp.h>
  <openssl/sha.h>
  <vector>
  <string>
  <memory>
  <cstdint>
)

# One production executable `cybou`: desktop and ordinary Full Node commands.
add_library(cybou_cli STATIC cybou/cli/cybou_cli.cpp)
target_link_libraries(cybou_cli PUBLIC cybou_node)
option(BUILD_PROVISION_TOOL "Build the offline network provisioning and verification tool" OFF)
if(BUILD_PROVISION_TOOL)
  add_executable(cybou-provision cybou_provision.cpp cybou/provision.cpp cybou/identity_crypto.cpp)
  target_compile_definitions(cybou-provision PRIVATE CYBOU_OFFLINE_PROVISIONING=1)
  target_link_libraries(cybou-provision PRIVATE cybou_node)
endif()
if(NOT BUILD_GUI)
  add_executable(cybou cybou_main.cpp)
  target_link_libraries(cybou PRIVATE cybou_cli)
  cybou_install(cybou)
  cybou_windows_manifest(cybou)
endif()
# Developer/test load generator; never part of a production build.
if(BUILD_TESTS)
  add_executable(cybou-loadgen cybou_loadgen.cpp)
  target_link_libraries(cybou-loadgen PRIVATE cybou_node)
  cybou_windows_manifest(cybou-loadgen)
endif()

if(BUILD_GUI)
  add_subdirectory(qt)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif()
