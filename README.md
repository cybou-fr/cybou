# CYBOU

CYBOU dÃ©veloppe une messagerie privÃ©e et un stockage de fichiers distribuÃ©s,
sans dÃ©pendre d'un service cloud centralisÃ©.
Ce dÃ©pÃ´t contient le code du client, du nÅ“ud, du protocole et de leurs tests.
CYBOU n'est pas un service public de messagerie ou de stockage cloud.

**Une identitÃ©. Des Ã©changes privÃ©s. Vos donnÃ©es sous votre contrÃ´le.**

## Produit

Le client de bureau relie une identitÃ© CYBOU aux fonctions Wallet, Mail et
Files. Le code applicatif prend en charge la rÃ©daction et l'envoi de messages,
les piÃ¨ces jointes, ainsi que le dÃ©pÃ´t, le tÃ©lÃ©chargement et l'organisation de
fichiers et de dossiers.

Pour les entreprises, l'offre visÃ©e est la crÃ©ation d'un rÃ©seau CYBOU privÃ©
distinct, gouvernÃ© par l'organisation cliente : genÃ¨se et identifiant de rÃ©seau
propres, clÃ©s d'autoritÃ© dÃ©tenues par l'entreprise, nÅ“uds bootstrap et rÃ¨gles
d'accÃ¨s par IP dÃ©finis pour cette instance. Le client choisit l'infrastructure
et la connectivitÃ© ; Internet ou intranet ne sont pas des produits diffÃ©rents.

Cette offre dÃ©crit l'objectif du produit, pas une fonction dÃ©jÃ  disponible de
bout en bout. Le VPS DEV exÃ©cute dÃ©sormais un Full Node ordinaire sur DEVNET
sur `51.255.46.58:29461` ; les anciens services finalizer et fournisseurs
sont inactifs. Le client vÃ©rifie la genÃ¨se signÃ©e compilÃ©e et utilise le bootstrap compilÃ©
avec son pin TLS ; le VPS utilise le mÃªme protocole CYBOU P2P.
L'exÃ©cutable unique `cybou` dÃ©marre uniquement le DEVNET compilÃ© et propose, sans
interface, `node run` avec la clÃ© PoA optionnelle, mais le
client ne possÃ¨de pas encore de parcours gÃ©nÃ©ral de crÃ©ation et de mise en
service d'un rÃ©seau privÃ© d'entreprise.

## Fonctionnement technique

- **IdentitÃ© et clÃ©s.** Le coffre local protÃ¨ge le matÃ©riel de rÃ©cupÃ©ration
  d'une identitÃ© au niveau du compte. Le code sÃ©pare les rÃ´les de clÃ©s et
  combine Ed25519 avec ML-DSA pour les signatures d'identitÃ©.
- **Contenus privÃ©s.** Mail et Files sont convertis en contenu chiffrÃ© cÃ´tÃ©
  client, dÃ©coupÃ© en blocs ROOT/INDEX/DATA. `RootPublication` est l'opÃ©ration
  protocolaire qui autorise une publication ; les schÃ©mas Mail et Files sont
  des donnÃ©es applicatives privÃ©es.
- **Stockage vÃ©rifiable.** La blockchain agit comme un registre notariÃ©
  dÃ©terministe : elle consigne les mÃ©tadonnÃ©es de `RootPublication`, les racines Merkle
  et les capsules de destinataires. Le stockage rÃ©seau se paie par un bail
  (`StorageLease`) depuis le System Balance vers un sÃ©questre, versÃ© aux nÅ“uds qui
  prouvent un service de stockage via un rÃ¨glement quotidien signÃ© par le PoA
  (aucune dÃ©claration locale ni voucher hors-chaÃ®ne ne confÃ¨re d'autoritÃ©). Chaque
  nÅ“ud choisit une capacitÃ© locale d'au moins 15 Gio, dont au plus 2/3 pour autrui.
  Les contrÃ´les opÃ©rationnels relisent les fragments via GET et vÃ©rifient BLAKE3 ;
  la rÃ©paration est tentÃ©e depuis une copie valide vers un fournisseur disponible. Le mÃ©canisme
  cryptographique d'audit de stockage (`StorageAuditChallenge`, `CreateStorageAuditProof`,
  `VerifyStorageAuditProof`) est implÃ©mentÃ© ; le protocole rÃ©seau d'audit pÃ©riodique
  et sa notarisation ne sont pas implÃ©mentÃ©s. Lors de la rÃ©vocation finalisÃ©e
  d'un objet (`RevokePublication`), les nÅ“uds conformes tentent de purger les fragments
  qu'aucune autre publication n'autorise. Cela ne prouve ni la disparition de copies
  cachÃ©es ni la destruction des clÃ©s rÃ©cupÃ©rables via les capsules historiques.
  100% des commissions de protocole reviennent Ã 
  l'opÃ©rateur pour la maintenance et le dÃ©veloppement du rÃ©seau.
- **Transport P2P.** CYBOU P2P utilise TLS 1.3 avec l'Ã©change hybride
  `X25519MLKEM768`. Chaque nÅ“ud complet implÃ©mente le mÃªme protocole. Lâ€™identitÃ© de stockage
  est prouvÃ©e Ã  la demande ; les certificats de blocs prouvent seuls lâ€™autoritÃ© PoA.
- **FinalitÃ©.** Les commandes opÃ©rateur incluent un finalizer PoA. C'est une
  finalitÃ© Ã  opÃ©rateur unique, pas un consensus BFT ; chaque nÅ“ud vÃ©rifie les
  blocs et les transitions d'Ã©tat. L'autoritÃ© racine du rÃ©seau est la clÃ©
  publique de rÃ©seau (NetworkID = Network Public Key), dont la clÃ© privÃ©e
  reste strictement hors-ligne pour signer la genÃ¨se immuable une seule fois.
  Les donnÃ©es publiques du rÃ©seau officiel (clÃ© publique, genÃ¨se signÃ©e, Ã©tat
  initial et adresses bootstrap) doivent Ãªtre compilÃ©es dans le client, sans
  fichier rÃ©seau officiel chargÃ© Ã  l'exÃ©cution. La clÃ© PoA autorisÃ©e par la
  genÃ¨se est un rÃ´le distinct de l'identitÃ© ordinaire `cybou.cybou` et signe les blocs.
  Chaque nÅ“ud complet exÃ©cute lui-mÃªme chaque opÃ©ration candidate. Il n'existe
  ni AUTH ni Validation : le spam est payÃ© en CYBOU (frais, loyer de stockage)
  et par une preuve de travail de relais unique. Le PoA rÃ©-exÃ©cute tout et reste
  seul Ã  finaliser.
- **Admission rÃ©seau.** Les commandes rÃ©seau de `cybou` exigent une
  politique d'admission explicite. Le mode `france` utilise une base GeoIP
  validÃ©e ; le dÃ©veloppement utilise DEVNET avec les mÃªmes rÃ¨gles dâ€™admission.

Les vues Mail et Files sont conservÃ©es dans une base applicative chiffrÃ©e
propre Ã  chaque identitÃ© et peuvent Ãªtre reconstruites Ã  partir des publications
finalisÃ©es et des fournisseurs. Une copie locale en cache n'est pas comptÃ©e
comme rÃ©plique distante.
Le code distingue les fournisseurs par leur StorageId prouvÃ© : des clÃ©s distinctes
ne prouvent pas des disques, machines ou opÃ©rateurs indÃ©pendants. La cible Beta
de deux rÃ©pliques distantes indÃ©pendantes reste Ã  vÃ©rifier au-delÃ  de ce comptage.

## Ã‰tat du code

Le dÃ©pÃ´t contient des implÃ©mentations natives pour le coffre et la rÃ©cupÃ©ration
d'identitÃ©, les opÃ©rations d'identitÃ©, les noms `.cybou`, Wallet, Mail, Files,
les publications chiffrÃ©es, le stockage et le transport P2P. Les tests natifs
couvrent notamment ces composants, l'admission des pairs et le cycle de vie du
stockage ; le shell Qt dispose Ã©galement de tests d'interface.

Le client de bureau dÃ©marre actuellement avec les constantes publiques DEVNET compilÃ©es et
n'active pas encore le finalizer dans le parcours utilisateur ; le finalizer
PoA s'exÃ©cute sÃ©parÃ©ment via la commande opÃ©rateur `node run --poa-key-file`. Le CLI fournit aussi des outils de
diagnostic, de synchronisation et de vÃ©rification du stockage. Les
fonctions de dÃ©ploiement d'un rÃ©seau d'entreprise doivent encore Ãªtre reliÃ©es
Ã  un parcours opÃ©rateur complet avant de pouvoir Ãªtre prÃ©sentÃ©es comme une
fonction utilisable depuis le bureau.

DEVNET est le profil officiel visÃ© avec le bootstrap `51.255.46.58:29461`.
MAINNET n'est pas encore provisionnÃ© et doit rester dÃ©sactivÃ© dans l'interface.
Les fichiers rÃ©seau officiels externes ont Ã©tÃ© supprimÃ©s.

Ce projet est en dÃ©veloppement actif. Le code et ses tests ne constituent ni
une certification, ni un audit de sÃ©curitÃ© indÃ©pendant, ni une garantie
d'aptitude Ã  la production.

## Compilation et tests

Le projet utilise C++20, CMake et OpenSSL 3.5 ou ultÃ©rieur. Qt 6 est requis
pour construire le client de bureau. Les dÃ©pendances natives et les Ã©tapes de
compilation par plateforme sont dÃ©taillÃ©es dans [INSTALL.md](INSTALL.md).

La cible de tests protocole est `cybou-core-test`. Elle regroupe les suites
natives sur l'identitÃ©, les clÃ©s, PoA, les publications, le stockage et le
transport. Les tests Qt sont activÃ©s avec les options de compilation GUI et
tests correspondantes. Les presets et cibles sont dÃ©finis dans
[`CMakePresets.json`](CMakePresets.json), [`CMakeLists.txt`](CMakeLists.txt) et
[`src/test/CMakeLists.txt`](src/test/CMakeLists.txt).

## Parcours du code

- [Client de bureau et dÃ©marrage rÃ©seau](src/qt/cyboudesktopcontroller.cpp)
- [Adaptateur applicatif Mail et Files](src/qt/cyboucoreapplicationadapter.cpp)
- [Runtime du nÅ“ud](src/cybou/node_runtime.cpp)
- [Commandes de `cybou`](src/cybou/cli/cybou_cli.cpp)
- [Tests natifs](src/test/CMakeLists.txt)
- [Tests du shell Qt](src/qt/test/cyboushelltests.cpp)

## Contribution et sÃ©curitÃ©

- [Contribuer au projet](CONTRIBUTING.md)
- [Politique de sÃ©curitÃ©](SECURITY.md)
- [Licence Apache-2.0](LICENSE) · [Notices](NOTICE.md) · [Bitcoin Core ancestry (MIT)](COPYING)

Ne rÃ©utilisez pas les clÃ©s ou donnÃ©es de dÃ©veloppement comme actifs de
production. Signalez les problÃ¨mes de sÃ©curitÃ© selon la procÃ©dure dÃ©crite dans
`SECURITY.md`.

Â© 2026 Stanislav Saveliev. ConÃ§u en France.

Offline network tooling is built explicitly with `-DBUILD_PROVISION_TOOL=ON`.
`cybou-provision verify-devnet private/devnet` checks existing material without
signing; `create-devnet NEW_PRIVATE_DIR NEW_CONSTANTS_HEADER` only accepts fresh
outputs. Production Full Nodes do not provision networks. Omitted `--capacity`
allocates storage automatically while retaining a free-space reserve.
