# CYBOU

CYBOU est un projet open source qui développe un logiciel de bureau et un
réseau pair-à-pair pour l'identité, la messagerie privée et les fichiers.
Ce dépôt contient le code du client, du nœud, du protocole et de leurs tests.
CYBOU n'est pas un service public de messagerie ou de stockage cloud.

**Une identité. Des échanges privés. Vos données sous votre contrôle.**

## Produit

Le client de bureau relie une identité CYBOU aux fonctions Wallet, Mail et
Files. Le code applicatif prend en charge la rédaction et l'envoi de messages,
les pièces jointes, ainsi que le dépôt, le téléchargement et l'organisation de
fichiers et de dossiers.

Pour les entreprises, l'offre visée est la création d'un réseau CYBOU privé
distinct, gouverné par l'organisation cliente : genèse et identifiant de réseau
propres, clés d'autorité détenues par l'entreprise, nœuds bootstrap et règles
d'accès par IP définis pour cette instance. Le client choisit l'infrastructure
et la connectivité ; Internet ou intranet ne sont pas des produits différents.

Cette offre décrit l'objectif du produit, pas une fonction déjà disponible de
bout en bout. Le VPS DEV exécute désormais un Full Node ordinaire sur DEVNET v12
sur `51.255.46.58:29461` ; les anciens services finalizer et fournisseurs
sont inactifs. Le client vérifie la genèse signée compilée et utilise le bootstrap compilé
avec son pin TLS ; le VPS utilise le même protocole CYP2 v5.
L'exécutable unique `cybou` démarre uniquement le DEVNET compilé et propose, sans
interface, `node run` avec la clé PoA optionnelle, mais le
client ne possède pas encore de parcours général de création et de mise en
service d'un réseau privé d'entreprise.

## Fonctionnement technique

- **Identité et clés.** Le coffre local protège le matériel de récupération
  d'une identité au niveau du compte. Le code sépare les rôles de clés et
  combine Ed25519 avec ML-DSA pour les signatures d'identité.
- **Contenus privés.** Mail et Files sont convertis en contenu chiffré côté
  client, découpé en blocs ROOT/INDEX/DATA. `RootPublication` est l'opération
  protocolaire qui autorise une publication ; les schémas Mail et Files sont
  des données applicatives privées.
- **Stockage vérifiable.** Le nœud de stockage vérifie qu'un bloc appartient à
  une publication finalisée. `StorageService` gère les répliques distantes,
  l'audit d'intégrité et la réparation. La finalité seule ne signifie pas que
  le contenu est disponible ou durable.
- **Transport P2P.** CYP2 v5 utilise TLS 1.3 avec l'échange hybride
  `X25519MLKEM768`. Chaque nœud complet implémente le même protocole. L’identité de stockage
  est prouvée à la demande ; les certificats de blocs prouvent seuls l’autorité PoA.
- **Finalité.** Les commandes opérateur incluent un finalizer PoA. C'est une
  finalité à opérateur unique, pas un consensus BFT ; chaque nœud vérifie les
  blocs et les transitions d'état. L'autorité racine du réseau est la clé
  publique de réseau (NetworkID = Network Public Key), dont la clé privée
  reste strictement hors-ligne pour signer la genèse immuable une seule fois.
  Les données publiques du réseau officiel (clé publique, genèse signée, état
  initial et adresses bootstrap) doivent être compilées dans le client, sans
  fichier réseau officiel chargé à l'exécution. La clé PoA autorisée par la
  genèse est un rôle distinct de l'identité ordinaire `cybou.cybou` et signe les blocs.
  Chaque nœud complet exécute lui-même chaque opération candidate. Une identité
  dont l'AUTH finalisée dépasse 1 000 000 peut y ajouter une signature de
  Validation, simple preuve pré-finalisation. L'AUTH est stockée dans l'état du
  compte, non transférable et exclue des 100 milliards de CYBOU. Le PoA
  ré-exécute tout et reste seul à finaliser.
- **Admission réseau.** Les commandes réseau de `cybou` exigent une
  politique d'admission explicite. Le mode `france` utilise une base GeoIP
  validée ; le mode `lab` est réservé aux pairs de test locaux ou privés.

Les vues Mail et Files sont conservées dans une base applicative chiffrée
propre à chaque identité et peuvent être reconstruites à partir des publications
finalisées et des fournisseurs. Une copie locale en cache n'est pas comptée
comme réplique distante.

## État du code

Le dépôt contient des implémentations natives pour le coffre et la récupération
d'identité, les opérations d'identité, les noms `.cybou`, Wallet, Mail, Files,
les publications chiffrées, le stockage et le transport P2P. Les tests natifs
couvrent notamment ces composants, l'admission des pairs et le cycle de vie du
stockage ; le shell Qt dispose également de tests d'interface.

Le client de bureau démarre actuellement avec les constantes publiques DEVNET compilées et
n'active pas encore le finalizer dans le parcours utilisateur ; le finalizer
PoA s'exécute séparément via la commande opérateur `node run --poa-key-file`. Le CLI fournit aussi des outils de
diagnostic, de synchronisation et de vérification du stockage. Les
fonctions de déploiement d'un réseau d'entreprise doivent encore être reliées
à un parcours opérateur complet avant de pouvoir être présentées comme une
fonction utilisable depuis le bureau.

DEVNET est le profil officiel visé avec le bootstrap `51.255.46.58:29461`.
MAINNET n'est pas encore provisionné et doit rester désactivé dans l'interface.
Les fichiers réseau officiels externes ont été supprimés.

Ce projet est en développement actif. Le code et ses tests ne constituent ni
une certification, ni un audit de sécurité indépendant, ni une garantie
d'aptitude à la production.

## Compilation et tests

Le projet utilise C++20, CMake et OpenSSL 3.5 ou ultérieur. Qt 6 est requis
pour construire le client de bureau. Les dépendances natives et les étapes de
compilation par plateforme sont détaillées dans [INSTALL.md](INSTALL.md).

La cible de tests protocole est `cybou-core-test`. Elle regroupe les suites
natives sur l'identité, les clés, PoA, les publications, le stockage et le
transport. Les tests Qt sont activés avec les options de compilation GUI et
tests correspondantes. Les presets et cibles sont définis dans
[`CMakePresets.json`](CMakePresets.json), [`CMakeLists.txt`](CMakeLists.txt) et
[`src/test/CMakeLists.txt`](src/test/CMakeLists.txt).

## Parcours du code

- [Client de bureau et démarrage réseau](src/qt/cyboudesktopcontroller.cpp)
- [Adaptateur applicatif Mail et Files](src/qt/cyboucoreapplicationadapter.cpp)
- [Runtime du nœud](src/cybou/node_runtime.cpp)
- [Commandes de `cybou`](src/cybou/cli/cybou_cli.cpp)
- [Tests natifs](src/test/CMakeLists.txt)
- [Tests du shell Qt](src/qt/test/cyboushelltests.cpp)

## Contribution et sécurité

- [Contribuer au projet](CONTRIBUTING.md)
- [Politique de sécurité](SECURITY.md)
- [Licence](COPYING)

Ne réutilisez pas les clés ou données de développement comme actifs de
production. Signalez les problèmes de sécurité selon la procédure décrite dans
`SECURITY.md`.

© 2026 Stanislav Saveliev. Conçu en France.

Offline network tooling is built explicitly with `-DBUILD_PROVISION_TOOL=ON`.
`cybou-provision verify-devnet private/devnet` checks existing material without
signing; `create-devnet NEW_PRIVATE_DIR NEW_CONSTANTS_HEADER` only accepts fresh
outputs. Production Full Nodes do not provision networks. Omitted `--capacity`
allocates storage automatically while retaining a free-space reserve.
