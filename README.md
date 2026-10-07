# CYBOU

**Infrastructure souveraine de cybersécurité, de communication et de données, conçue en France autour d'un cœur P2P open source.**

CYBOU fournit une infrastructure de confiance distribuée conçue au niveau du protocole pour les communications privées, l'identité cryptographique et les données sensibles des organisations et des utilisateurs, sans dépendre d'un plan de contrôle cloud étranger.

Ce dépôt héberge le cœur technologique open source CYBOU : nœud complet natif C++20, protocole P2P, moteur d'état vérifiable, cryptographie hybride post-quantique, stockage distribué chiffré et client de bureau (Mail, Files, Identity).

## Architecture et déclinaisons produit

Le cœur technologique commun sert de fondation à deux verticales produit :

- **CYBOU Public** : le réseau public de référence, ouvert et fondé sur le cœur open source, avec une politique d'admission P2P restreinte aux adresses IP publiques françaises, une identité réseau publique et une autorité PoA de référence.
- **CYBOU Enterprise** : l'offre B2B permettant aux organisations et entreprises de déployer des réseaux CYBOU privés indépendants (leur propre NetworkID, leur propre genèse signée, leur propre autorité PoA, leur infrastructure et leurs politiques d'accès réseau comme VPN ou LAN), complétée par des outillages de gestion, d'intégration et de support.

> *La restriction géographique aux adresses IP françaises est une politique de déploiement du réseau public CYBOU, et non une limitation du protocole CYBOU.*

## Propriétés techniques

- **Un seul type de nœud.** Chaque Full Node (y compris le bureau) relaie les opérations, vérifie chaque bloc et transition d'état, et stocke des fragments chiffrés pour le réseau (capacité locale d'au moins 15 Gio).
- **Identité cryptographique native.** Une identité au niveau du compte avec rôles séparés (Recovery, Authorization, KEM), protégée par une phrase secrète de 24 mots ; signatures hybrides classiques et post-quantiques (Ed25519 + ML-DSA).
- **Substrat applicatif chiffré (Mail et Files).** Contenus chiffrés côté client ; l'opération générique `RootPublication` scelle l'arbre de chunks BLAKE3 et les capsules des destinataires dans le registre notarié sans exposer les données en clair.
- **Stockage distribué vérifiable.** Chunks chiffrés adressés par BLAKE3-256, admission autorisée par finalité, réplication et vérification d'intégrité (cible Beta : 2 répliques distantes).
- **Finalité déterministe.** Une autorité PoA désignée par la genèse signe les blocs ; chaque nœud complet réexécute indépendamment toutes les opérations et vérifie les signatures.
- **Transport P2P durci.** TLS 1.3 avec échange hybride post-quantique `X25519MLKEM768` obligatoire (fail-closed).

## État du projet

Développement actif sur le réseau de test DEVNET. MAINNET n'est pas provisionné. Le code, les compositions cryptographiques et les tests ne constituent ni une certification de sécurité indépendante, ni une qualification réglementaire (ANSSI, HDS, RGPD, NIS2).

## Compilation

C++20, CMake, OpenSSL 3.5+, Boost, BLAKE3 ; Qt 6 pour le client de bureau.
Détails par plateforme : [INSTALL.md](INSTALL.md).

```bash
cmake -S . -B build -DBUILD_GUI=ON
cmake --build build
build/bin/cybou-core-test
```

`cybou` sans argument lance le bureau ; `cybou node run` lance un nœud sans interface.

## Documentation

- [Documentation technique](docs/cybou/README.md)
- [Politique de souveraineté](docs/cybou/38_SOVEREIGNTY_DEFINITION.md) · [Stratégie B2B et pilotes](docs/cybou/39_GO_TO_MARKET_AND_PILOTS.md)
- [Contribuer](CONTRIBUTING.md) · [Sécurité](SECURITY.md)
- [Licence Apache-2.0](LICENSE) · [Notices](NOTICE.md) · [Historique du projet](docs/legal/PROJECT_HISTORY.md)

© 2026 Stanislav Saveliev. Conçu en France.
