# Copyright (c) 2026 CYBOU contributors
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(AddWindowsResources)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

#=============================
# Subprojects
#=============================
include(../cmake/crc32c.cmake)
include(../cmake/leveldb.cmake)

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
# The shared uint256 implementation uses only standard-library code here.
add_library(cybou_base STATIC
  uint256.cpp
  crypto/hex_base.cpp
)

# CYBOU consensus and identity implementation.
add_library(cybou_core STATIC
  cybou/account_creation.cpp
  cybou/block.cpp
  cybou/canonical_cbor.cpp
  cybou/chunk_id.cpp
  cybou/chunk_authorization.cpp
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
  cybou/poa_auth_adjustment.cpp
  cybou/validation_attestation.cpp
  cybou/validation_pool.cpp
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
if(CYBOU_ENABLE_TEST_HOOKS)
  target_compile_definitions(cybou_core PRIVATE CYBOU_ENABLE_TEST_HOOKS=1)
endif()
if(CYBOU_ENABLE_LAB_NETWORK)
  target_compile_definitions(cybou_core PUBLIC CYBOU_ENABLE_LAB_NETWORK=1)
endif()

include(InstallBinaryComponent)

# CYBOU node runtime and P2P library.
add_library(cybou_node STATIC
  cybou/kv_store.cpp
  cybou/private_application_store.cpp
  cybou/chunk_blob_store.cpp
  cybou/chunk_retention.cpp
  cybou/finalized_chunk_store.cpp
  cybou/chunk_authorization_proof_index.cpp
  cybou/publication_bundle_stager.cpp
  cybou/publication_service.cpp
  cybou/storage_service.cpp
  cybou/application_service.cpp
  cybou/secret_file.cpp
  cybou/provision.cpp
  cybou/poa_conflict_detector.cpp
  cybou/poa_finalizer.cpp
  cybou/poa_signing_journal.cpp
  cybou/state_store.cpp
  cybou/block_executor.cpp
  cybou/finalizer_node.cpp
  cybou/operation_pool.cpp
  cybou/network_definition.cpp
  cybou/protocol_operation.cpp
  cybou/identity_operation_coordinator.cpp
  cybou/wallet_service.cpp
  cybou/node_runtime.cpp
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
)
target_link_libraries(cybou_node
  PUBLIC
    cybou_core
    Boost::headers
  PRIVATE
    leveldb
    $<TARGET_NAME_IF_EXISTS:libevent::core>
    $<TARGET_NAME_IF_EXISTS:libevent::extra>
    OpenSSL::Crypto
    OpenSSL::SSL
    ZLIB::ZLIB
    $<$<PLATFORM_ID:Windows>:crypt32>
)

# One production executable `cybou`: the desktop, plus headless node roles and
# operator commands (`cybou finalizer run`, `cybou network info`, ...).
add_library(cybou_cli STATIC cybou/cli/cybou_cli.cpp)
target_link_libraries(cybou_cli PUBLIC cybou_node)
if(NOT BUILD_GUI)
  add_executable(cybou cybou_main.cpp)
  target_link_libraries(cybou PRIVATE cybou_cli)
  add_windows_application_manifest(cybou)
endif()
# Developer/test load generator; never part of a production build.
if(BUILD_TESTS)
  add_executable(cybou-loadgen cybou_loadgen.cpp)
  target_link_libraries(cybou-loadgen PRIVATE cybou_node)
  add_windows_application_manifest(cybou-loadgen)
endif()

if(BUILD_GUI)
  add_subdirectory(qt)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif()
