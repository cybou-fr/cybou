# CYBOU

**Une identité. Des échanges privés. Vos données sous votre contrôle.**

CYBOU est un réseau pair-à-pair français pour la messagerie privée, le stockage
de fichiers chiffrés et les paiements, sans service cloud centralisé. Ce dépôt
contient le client de bureau, le nœud, le protocole et leurs tests.

## Fonctionnement

- **Un seul type de nœud.** Chaque nœud complet, bureau compris, relaie les
  opérations, vérifie chaque bloc et stocke des fragments chiffrés pour le
  réseau (capacité locale d'au moins 15 Gio).
- **Identité.** Une identité au niveau du compte, protégée par une phrase de
  récupération ; signatures hybrides Ed25519 + ML-DSA.
- **Mail et Files.** Le contenu est chiffré côté client ; la chaîne n'enregistre
  que les racines Merkle et les capsules des destinataires. Le stockage est payé
  par un bail et répliqué sur deux nœuds distants.
- **Finalité.** Une clé PoA fixée par la genèse signe les blocs ; chaque nœud
  réexécute tout et vérifie les signatures. Ce n'est pas un consensus BFT.
- **Réseau.** TLS 1.3 avec échange hybride `X25519MLKEM768` ; pairs admis
  uniquement depuis la France.

## État

Développement actif sur DEVNET (bootstrap `51.255.46.58:29461`). MAINNET n'est
pas encore provisionné. Le code et ses tests ne constituent ni une
certification, ni un audit de sécurité indépendant.

## Compilation

C++20, CMake, OpenSSL 3.5+, Boost, BLAKE3 ; Qt 6 pour le client de bureau.
Détails par plateforme : [INSTALL.md](INSTALL.md).

```bash
cmake -S . -B build -DBUILD_GUI=ON
cmake --build build
build/bin/cybou-core-test
```

`cybou` sans argument lance le bureau ; `cybou node run` lance un nœud sans
interface.

## Documentation

- [Documentation technique](docs/cybou/README.md)
- [Contribuer](CONTRIBUTING.md) · [Sécurité](SECURITY.md)
- [Licence Apache-2.0](LICENSE) · [Notices](NOTICE.md) · [Historique du projet](docs/legal/PROJECT_HISTORY.md)

© 2026 Stanislav Saveliev. Conçu en France.
