// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
// BUILD_TESTS-only child process. Never opens official network/operator data.
#include <test/cybou_service_test_fixture.h>
#include <cybou/local_application_service.h>
#include <cybou/network_sync_service.h>
#include <cybou/chunk_blob_store.h>
#include <iostream>
#include <thread>

namespace {
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error{message};
}
constexpr auto PASSWORD = "isolated crash fixture password";
std::uint64_t FinalizedNonce(cybou::CybouNodeRuntime& runtime, const cybou::AccountId& account)
{
    const auto snapshot = runtime.GetStore().GetStateSnapshot();
    const auto* record = snapshot && snapshot.state ? snapshot.state->identities.Find(account) : nullptr;
    Require(record != nullptr, "fixture account missing");
    return record->nonce;
}
}

int main(int argc, char** argv)
{
    try {
        Require(argc == 4, "usage: child <absolute isolated directory> <checkpoint> <write|recover>");
        const std::filesystem::path root{argv[1]};
        const std::string point{argv[2]}, action{argv[3]};
        Require(root.is_absolute() && std::filesystem::is_regular_file(root / "crash-fixture"), "isolated fixture marker required");
        const bool rotation = point == "rotation-prepared" || point == "rotation-finalized" || point == "rotation-promoted";
        Require(point == "stage" || point == "commit" || point == "submit" || rotation, "invalid checkpoint");
        Require(action == "write" || action == "recover", "invalid action");
        auto genesis = cybou::CreateTestGenesisState();
        auto definition = cybou::CreateTestNetworkGenesis(genesis,
            cybou::TestPoaFinalizerPublicKey(0x72), cybou::TestNetworkPublicKey(0x72));
        definition = cybou::WithTestGenesisParameters(definition, [](auto& p) { p.account_creation_work_bits = 0; });
        std::array<unsigned char, 32> signer{};
        signer[0] = 0x72;
        cybou::CybouNodeRuntime runtime{cybou::NodeRuntimeConfig{
            .network_genesis = definition, .data_dir = root / "runtime",
            .poa_finalizer_recovery_entropy = signer, .memory_only = false, .wipe_data = false,
            .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0}};
        Require(runtime.InitializeGenesis(genesis), "cannot open fixture chain");
        cybou::CybouIdentityService identity{runtime, root / "fixture.vault"};
        if (action == "write") {
            Require(!std::filesystem::exists(root / "fixture.vault"), "write phase cannot overwrite an existing Identity");
            Require(identity.PrepareNewIdentity().has_value(), "cannot prepare fixture Identity");
            Require(identity.CreateIdentitySync(PASSWORD).success, "cannot finalize fixture Identity");
        } else Require(identity.LoadVault(PASSWORD), "cannot reopen fixture Identity");
        cybou::PrivateApplicationStore db{identity.GetKeyStore(), root / "application", "local.db", true};
        cybou::PrivateApplicationStore app{identity.GetKeyStore(), root / "application", "app.db", true};
        cybou::LocalApplicationService local{db};
        cybou::LocalContentStager stager{runtime.GetChunkBlobStore(), runtime.GetChunkRetention(),
            runtime.GetNetworkBinding(), db.Account(), &db};
        cybou::PublicationService publication{runtime, identity.GetKeyStore(), app,
            runtime.GetIdentityOperationCoordinator(identity.GetKeyStore()), &db};
        cybou::RecoveryEntropy rotation_entropy{};
        rotation_entropy.fill(77); // synthetic fixture only
        if (action == "write") {
            cybou::MailMessage mail;
            mail.message_id = *cybou::NewPrivateItemId();
            mail.recipient_account_id = db.Account();
            mail.subject = "Durable crash checkpoint";
            mail.body = "Preserve exact local content across forced process termination";
            mail.client_timestamp_ms = 1;
            Require(local.SaveDraft({.draft_id = "crash-draft", .body = "acknowledged draft"}), "cannot commit draft");
            std::vector<cybou::NewContent> children;
            const auto content = stager.Prepare("crash-job", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
                return cybou::EncodePrivateApplicationDocument(mail);
            });
            Require(content.has_value(), "cannot stage local content");
            std::vector<unsigned char> leaves;
            for (const auto& leaf : content->leaves) leaves.insert(leaves.end(), leaf.begin(), leaf.end());
            Require(db.Put("crash/expected-leaves", leaves), "cannot save fixture expectation");
            if (point != "stage") Require(local.QueuePublication("crash-job", *content, mail), "cannot commit local Outbox");
            if (point == "submit") {
                const auto current_nonce = FinalizedNonce(runtime, db.Account());
                std::array<unsigned char, 8> nonce{};
                for (unsigned i = 0; i < 8; ++i) nonce[i] = static_cast<unsigned char>(current_nonce >> (8 * i));
                Require(db.Put("crash/expected-nonce", nonce), "cannot save expected nonce");
                Require(app.Put("publication/leaves/crash-job", leaves), "cannot save publication leaves");
                const auto result = publication.SubmitPrepared("crash-job", content->bundle);
                Require(!result.operation_id.IsNull(), "submission has no OperationID");
                Require(result.phase == cybou::PublicationJobPhase::WAITING_FINALITY ||
                    result.phase == cybou::PublicationJobPhase::SECURING, "submission failed");
                Require(db.Put("crash/expected-operation", std::span{result.operation_id.begin(), 32}), "cannot save expected ID");
                Require(local.Outbox().front().status.operation_id.IsNull(), "local status was unexpectedly advanced");
            }
            if (rotation) {
                Require(app.Put("crash/network-index", std::vector<unsigned char>{7, 8, 9}), "cannot save application sentinel");
                Require(db.PrepareKeyRotation(rotation_entropy), "cannot wrap local data key");
                Require(app.PrepareKeyRotation(rotation_entropy), "cannot wrap network data key");
                if (point != "rotation-prepared") {
                    const auto result = identity.RotateIdentitySync(cybou::EncodeRecoveryWords(rotation_entropy), PASSWORD);
                    Require(result.phase == cybou::IdentityOperationPhase::ACCEPTED ||
                        result.phase == cybou::IdentityOperationPhase::UNCERTAIN, "cannot submit fixture rotation");
                    Require(runtime.ProduceBlock().has_value(), "cannot finalize fixture rotation");
                    if (point == "rotation-promoted")
                        Require(identity.ResumeIdentityRotationSync(PASSWORD).phase == cybou::IdentityOperationPhase::FINALIZED,
                            "cannot promote fixture vault");
                }
            }
            std::cout << "READY " << point << std::endl;
            // Supervisor terminates this process. No stack unwinding, DB close,
            // worker join or normal shutdown can contribute to the result.
            for (;;) std::this_thread::sleep_for(std::chrono::seconds{1});
        }
        Require(local.ListDrafts().size() == 1 && local.ListDrafts().front().body == "acknowledged draft", "acknowledged draft lost");
        if (rotation) {
            Require(app.Get("crash/network-index") == std::optional<std::vector<unsigned char>>{{7, 8, 9}}, "network application data lost");
            const auto expected_key = cybou::DeriveIdentityPublicKey(rotation_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
            Require(expected_key.has_value(), "fixture rotation key unavailable");
            Require((identity.GetKeyStore().GetRecoveryPublicKey() == expected_key) == (point == "rotation-promoted"),
                "vault was promoted at the wrong checkpoint");
            if (point == "rotation-prepared") {
                const auto result = identity.RotateIdentitySync(cybou::EncodeRecoveryWords(rotation_entropy), PASSWORD);
                Require(result.phase == cybou::IdentityOperationPhase::ACCEPTED ||
                    result.phase == cybou::IdentityOperationPhase::UNCERTAIN, "prepared wrappers did not permit rotation");
                Require(runtime.ProduceBlock().has_value(), "cannot finalize restarted rotation");
            }
            Require(identity.ResumeIdentityRotationSync(PASSWORD).phase == cybou::IdentityOperationPhase::FINALIZED,
                "cannot reconcile rotation after process restart");
            Require(identity.GetKeyStore().GetRecoveryPublicKey() == expected_key, "rotation did not load promoted keys");
            Require(local.ListDrafts().size() == 1 && local.ListDrafts().front().body == "acknowledged draft", "local data inaccessible with promoted keys");
            Require(app.Get("crash/network-index") == std::optional<std::vector<unsigned char>>{{7, 8, 9}}, "network data inaccessible with promoted keys");
        }
        const auto leaves = db.Get("crash/expected-leaves");
        Require(leaves && !leaves->empty() && leaves->size() % 32 == 0, "fixture leaves lost");
        for (std::size_t i = 0; i < leaves->size(); i += 32) {
            cybou::ChunkId id{};
            std::copy_n(leaves->begin() + i, 32, id.begin());
            Require(runtime.GetChunkBlobStore().Has(id), "encrypted content lost");
            Require(runtime.GetChunkRetention().IsPinned(id) == (point != "stage"), "staging ownership was not recovered");
        }
        if (point == "stage") Require(local.Outbox().empty() && local.ListMail().empty(), "unaccepted content became sent Mail");
        else {
            Require(local.Outbox().size() == 1 && local.ListMail().size() == 1, "accepted Mail/Outbox lost");
            if (point == "submit") {
                const auto expected = db.Get("crash/expected-operation");
                Require(expected && expected->size() == 32, "expected operation lost");
                // Also exercise reconstruction of app.db's job after a real crash.
                Require(app.Erase("publication/job/crash-job"), "cannot simulate missing application job");
                cybou::NetworkSyncService::ProcessOutbox(local, publication, app);
                auto pending = local.Outbox().front();
                Require(std::equal(expected->begin(), expected->end(), pending.status.operation_id.begin()), "OperationID replaced after restart");
                Require(runtime.ProduceBlock().has_value(), "cannot finalize recovered candidate");
                cybou::NetworkSyncService::ProcessOutbox(local, publication, app);
                pending = local.Outbox().front();
                Require(pending.status.phase == cybou::PublicationJobPhase::SECURING, "recovered operation did not finalize");
                Require(std::equal(expected->begin(), expected->end(), pending.status.operation_id.begin()), "finalized OperationID changed");
                const auto encoded_nonce = db.Get("crash/expected-nonce");
                Require(encoded_nonce && encoded_nonce->size() == 8, "expected nonce lost");
                std::uint64_t nonce{0};
                for (unsigned i = 0; i < 8; ++i) nonce |= std::uint64_t{(*encoded_nonce)[i]} << (8 * i);
                Require(FinalizedNonce(runtime, db.Account()) == nonce + 1, "publication executed more than once");
                cybou::NetworkSyncService::ProcessOutbox(local, publication, app);
                (void)runtime.ProduceBlock();
                Require(FinalizedNonce(runtime, db.Account()) == nonce + 1, "retry created a duplicate canonical operation");
            }
        }
        std::cout << "PASS " << point << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << std::endl;
        return 1;
    }
}
