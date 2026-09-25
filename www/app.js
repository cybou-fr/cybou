/* ==========================================================================
   CYBOU.FR — Technical Engine & Dynamic Bilingual Localization
   Engineered for transparent systems presentation & responsive state verification
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initLanguageSwitch();
  initMobileMenu();
  initFaqAccordion();
  checkUrlLanguage();
});

// --- 1. Language Toggle (French / English) ---
const translations = {
  fr: {
    navServices: "Services",
    navStatus: "État du projet",
    navArch: "Fondations",
    navMailTx: "Protocole MailTx",
    navSpecs: "Fiche technique",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Identité Souveraine • Communications Protégées • Cryptographie Post-Quantique",
    heroAccent: "Une identité unique. Des communications privées. Vos données sous votre contrôle.",
    heroSubtitle: "CYBOU réunit identité numérique protégée, messagerie confidentielle, stockage chiffré, sauvegarde et budget de service — réunis en une plateforme souveraine, indépendante des monopoles du cloud.",
    heroPillSlogan: "Pas un projet blockchain de spéculation — une identité protégée avec ses services.",

    statusCalloutTitle: "Architecture centrée sur l'identité en développement actif",
    statusCalloutBody: "CYBOU n'est pas un token de spéculation ni un service commercial public aujourd'hui. Le code comprend un nœud DEV autonome à validateur unique, la gestion d'identité par clés locales et une interface de bureau expérimentale. L'intégration de la messagerie chiffrée complète et d'un réseau BFT multi-validateur indépendant reste le travail actif en cours.",

    heroBtnServices: "Découvrir les services",
    heroBtnStatus: "Consulter l'état réel d'avancement",
    heroBtnCode: "Code source (GitHub)",

    servicesLabel: "Écosystème unifié",
    servicesTitle: "Une seule identité. Vos services essentiels.",
    servicesDesc: "Au lieu d'éparpiller vos données entre des silos tiers, CYBOU structure votre vie numérique autour d'une identité cryptographique dont vous détenez le contrôle exclusif.",

    srvIdentityTag: "Fondation",
    srvIdentityTitle: "Identité souveraine",
    srvIdentityDesc: "Noms lisibles (stanislav.cybou), racine de récupération 24 mots et coffre-fort portable CYBV2. Zéro mot de passe centralisé, zéro tiers de confiance.",
    srvIdentityStatus: "Testé dans le cœur & bureau",

    srvMailTag: "Communication",
    srvMailTitle: "Messagerie privée (Mail)",
    srvMailDesc: "Opération MailTx native avec chiffrement post-quantique, preuve d'inclusion et index de boîte aux lettres détenu par le destinataire sur son appareil.",
    srvMailStatus: "Format testé, livraison E2EE en cours",

    srvFilesTag: "Données",
    srvFilesTitle: "Stockage chiffré (Files)",
    srvFilesDesc: "Stockage d'objets distribué et chiffré de bout en bout pour documents et pièces jointes. Vos fichiers restent confidentiels et inaccessibles aux hébergeurs.",
    srvFilesStatus: "Planifié après le pilote Mail",

    srvBackupTag: "Continuité",
    srvBackupTitle: "Sauvegarde souveraine",
    srvBackupDesc: "Restauration complète de votre environnement numérique sur machine neuve via votre phrase de récupération, sans dépendre d'un cloud propriétaire.",
    srvBackupStatus: "Planifié",

    srvWalletTag: "Usage",
    srvWalletTitle: "Budget de service (Wallet)",
    srvWalletDesc: "Deux soldes étanches : SystemBalance pour financer vos services et Balance disponible. Un outil utilitaire, pas un véhicule de spéculation.",
    srvWalletStatus: "Validé dans le cœur & l'interface",

    srvDevicesTag: "Sécurité",
    srvDevicesTitle: "Contrôle des appareils",
    srvDevicesDesc: "Autorisez ordinateurs portables et téléphones par signature cryptographique liée à votre identité racine, sans jamais divulguer vos clés maîtresses.",
    srvDevicesStatus: "Vérification active dans le cœur",

    matrixLabel: "Transparence technique",
    matrixTitle: "Matrice d'implémentation.",
    matrixDesc: "Distinction stricte et vérifiable entre le code validé en tests unitaires, les modules en cours de refonte et les composants de la feuille de route.",

    col1Title: "Validé & testé",
    badgeDone: "FONCTIONNEL",
    col1Item1: "<strong>Création de compte native & sans intermédiaire (doc 70) :</strong> opération AccountCreateOpV1 avec preuve de travail anti-Sybil.",
    col1Item2: "<strong>Identifiant typé AccountId (32 octets) :</strong> gestion déterministe du solde système (<code>SystemBalance</code>).",
    col1Item3: "<strong>Bonus d'accueil atomique :</strong> transfert immédiat du bonus depuis <code>OnboardingPool</code> vers <code>SystemBalance</code>.",
    col1Item4: "<strong>État canonique LevelDB :</strong> blocs finalisés, racines d'état, filtres compacts Mail et index de hauteur persistés atomiquement.",
    col1Item5: "<strong>MailTx, preuve et découverte :</strong> opération native bornée, frais déterministes, preuve d'inclusion et filtre compact implémentés dans le cœur ; livraison Email complète encore absente.",
    col1Item6: "<strong>Nœud DEV expérimental :</strong> producteur à un validateur, soumission d'opérations et synchronisation vérifiée entre processus, avec tests automatisés.",

    col2Title: "En cours de consolidation",
    badgeWip: "EN COURS",
    col2Item1: "<strong>BFT multi-validateur :</strong> moteur et certificats de finalité testés en code ; exploitation de plusieurs validateurs indépendants et tolérance f=1 non démontrées sur un réseau public.",
    col2Item2: "<strong>Client de bureau :</strong> pages Identity, Wallet, Email, Storage et Backup présentes ; certaines actions restent désactivées tant que le service correspondant n'est pas connecté.",
    col2Item3: "<strong>Clés et exploitation :</strong> domaines Autorité, Validateur, Release et Trésorerie définis ; garde opérationnelle et déploiement sécurisé encore à consolider.",
    col2Item4: "<strong>Transition du réseau DEV :</strong> le nouveau nœud BFT à un validateur coexiste avec la chaîne de bootstrap Bitcoin héritée ; l'intégration complète reste à faire.",

    col3Title: "Feuille de route planifiée",
    badgePlanned: "PLANIFIÉ",
    col3Item1: "<strong>Architecture cible Post-Quantique HPKE :</strong> profil hybride ML-KEM-768 + X25519 (spécifié doc 49, tests crypto OpenSSL >= 3.5).",
    col3Item2: "<strong>Stockage d'objets décentralisé :</strong> couche de pré-stockage pour validateurs et stockage d'objets pour pièces jointes à grande échelle.",
    col3Item3: "<strong>Service Email de bout en bout :</strong> chiffrement MailTx, livraison, Inbox/Sent et index de lecture locaux reliés au réseau réel.",
    col3Item4: "<strong>Réseau opérationnel :</strong> admission et exploitation de 4 validateurs indépendants, stockage d'objets et déploiement public après validation de sécurité.",

    archLabel: "Fondations de conception",
    archTitle: "Rupture avec les architectures centralisées.",
    archDesc: "Les quatre piliers qui guident la conception de la souveraineté et de l'intégrité des échanges numériques.",

    bento1Title: "Architecture sans cloud tiers obligatoire",
    bento1Desc: "Le modèle cible évite les comptes et boîtes aux lettres hébergés par un fournisseur unique. Le réseau DEV actuel utilise un validateur bootstrap connu ; la résilience de plusieurs opérateurs indépendants reste à démontrer.",
    bento1Metric: "Réseau expérimental, non déployé pour le public",

    bento2Title: "Profil cible HPKE post-quantique",
    bento2Desc: "Profil hybride cible ML-KEM-768 et X25519 conçu contre les attaques par interception et déchiffrement ultérieur ; l'intégration MailTx complète et l'audit externe restent à réaliser.",
    bento2Metric: "Standard NIST post-quantique",

    bento3Title: "Clés d'identité gérées sur l'appareil",
    bento3Desc: "Le client gère les clés d'identité localement et les opérations sont autorisées par signature. Le modèle de garde des clés d'opérateur et la sécurité complète des messages restent à valider en exploitation.",
    bento3Metric: "Gestion des clés d'identité sur l'appareil",

    bento4Title: "Finalité BFT explicite & Économie déterministe",
    bento4Desc: "Le cœur CYBOU calcule la finalité et les frais avec des règles déterministes. L'offre maximale spécifiée est de 100 milliards de CYBOU (0 décimale), avec 3/4 des frais pour la Sécurité et 1/4 pour l'Accueil ; la chaîne héritée n'applique pas encore cette politique.",
    bento4Value: "100 Mrd",
    bento4Metric: "Plafond absolu d'unités CYBOU (0 décimale)",

    flowLabel: "Ingénierie du message",
    flowTitle: "Le cycle de vie d'une transaction MailTx.",
    flowDesc: "Chemin protocolaire implémenté dans le cœur et étapes restant à relier au service Email complet.",

    step1Title: "Engagement salé de contenu",
    step1Desc: "Le cœur vérifie un engagement de contenu salé et séparé par domaine (doc 69). Le client de chiffrement et d'envoi reste en développement.",

    step2Title: "Profil cible Hybride HPKE",
    step2Desc: "Profil cible de chiffrement de bout en bout pour la clé publique du destinataire. L'intégration MailTx complète et son audit restent à réaliser avant toute revendication de sécurité en production.",

    step3Title: "Opération MailTx native",
    step3Desc: "Le cœur traite MailTx comme une opération native, avec limite de taille et frais déterministes. La soumission réseau existe en DEV ; l'expérience Email complète n'est pas encore disponible.",

    step4Title: "Consensus BFT & Index Local",
    step4Desc: "Le cœur vérifie certificats de finalité, preuves d'inclusion et filtres compacts de découverte Mail. L'index Inbox/Sent et l'état de lecture relèvent du client local encore à intégrer.",

    specLabel: "Spécifications machine & humaine",
    specTitle: "Fiche technique du protocole CYBOU.",
    specDesc: "Données d'ingénierie structurées et indexables pour moteurs de recherche et assistants d'analyse IA.",
    factsheetHeading: "Paramètres canoniques de l'architecture CYBOU",

    dtConsensus: "Modèle de consensus",
    dtPoT: "Époques de confiance (PoT)",
    dtSupply: "Offre maximale (MAX_SUPPLY)",
    dtGrant: "Bonus d'intégration (Onboarding Bonus)",
    dtFees: "Régime des frais de transaction",
    dtMailTx: "Format MailTx (v1)",
    dtKeys: "Séparation des clés d'opérateur",
    dtStack: "Socle technologique",
    dtNetworks: "Réseaux DEV, Beta & Mainnet",
    ddConsensus: "DEV : finalité à un validateur (<code>f=0</code>). Cible : BFT à poids égal, 4 validateurs minimum pour revendiquer <code>f=1</code>.",
    ddPoT: "Implémentées dans le cœur : hauteur de bloc et arithmétique entière, sans horloge locale dans le consensus.",
    ddSupply: "<code>100 000 000 000</code> CYBOU (0 décimale) dans la spécification ; chaîne de bootstrap héritée non conforme à cette politique.",
    ddGrant: "Implémenté dans l'état CYBOU : <code>OnboardingPool</code> vers <code>SystemBalance</code> après AccountCreate anti-Sybil (doc 70).",
    ddFees: "Frais Mail déterministes selon la taille, sans priorité ; 3/4 Sécurité et 1/4 Accueil dans le cœur CYBOU.",
    ddMailTx: "Opération native de premier rang, 1 destinataire, texte seul, taille bornée, sans pièces jointes ; service Email non achevé.",
    ddKeys: "4 rôles définis : Autorité, Validateur, Signature de Release, Trésorerie ; garde opérationnelle en préparation.",
    ddStack: "C++20, CMake, LevelDB, OpenSSL, Qt ; tests locaux Windows MinGW pour le cœur et le client.",
    ddNetworks: "DEV est expérimental. Beta et Mainnet sont prévus avec des genèses et paramètres séparés ; aucun solde Beta ne sera reporté sur Mainnet.",

    faqLabel: "Questions fréquentes",
    faqTitle: "Comprendre CYBOU.",
    faqDesc: "Des réponses claires et concrètes sur le fonctionnement de l'identité, la protection de vos données, l'utilité du portefeuille et la sécurité post-quantique.",
    faqPqBadge: "Sécurité Post-Quantique • PQ by Design",

    faqQ1: "Qu'est-ce que CYBOU en termes simples ?",
    faqA1: "CYBOU n'est ni un simple portefeuille crypto, ni un nouveau réseau social dépendant d'un géant du cloud. C'est une plateforme souveraine de communication protégée articulée autour de votre identité personnelle. Vous disposez d'un nom unique (comme <code>stanislav.cybou</code>), et à partir de cette identité, vous envoyez des messages privés chiffrés de bout en bout, stockez vos documents, sauvegardez vos états et contrôlez vos appareils, sans qu'aucune entreprise ne puisse lire vos correspondances ni fermer votre compte.",

    faqQ2: "En quoi CYBOU diffère-t-il des services comme Gmail, Proton ou iCloud ?",
    faqA2: "Les services centralisés imposent la dépendance à leurs serveurs : ils hébergent vos boîtes aux lettres, gèrent vos accès et collectent des métadonnées (adresses IP, graphes de contacts, horodatages). Chez CYBOU, il n'y a pas de serveur central ni d'hébergeur obligatoire. Vos clés cryptographiques sont créées et stockées exclusivement sur votre appareil. L'index de votre boîte de réception vous appartient localement, et vos messages sont vérifiés par un réseau pair-à-pair sans intermédiaire commercial.",

    faqQ3: "Comment fonctionnent les noms .cybou et la récupération de compte ?",
    faqA3: "Au lieu d'adresses cryptographiques complexes, vous choisissez un nom simple (de 5 à 32 caractères, ex: <code>alice.cybou</code>) enregistré sur un registre décentralisé sans autorité commerciale. Votre identité racine est protégée par une phrase de récupération unique de 24 mots. Si vous changez d'ordinateur ou réinstallez votre système, ces 24 mots suffisent pour restaurer votre identité et vos accès sur une machine neuve, de manière autonome et sans formulaire d'assistance.",

    faqQ4: "Pourquoi y a-t-il un portefeuille (Wallet) et pourquoi n'est-ce pas un outil de spéculation ?",
    faqA4: "Dans CYBOU, le portefeuille n'est pas conçu pour le trading ou la spéculation, mais comme un moteur utilitaire de services. Il sépare deux soldes : <code>SystemBalance</code> (un budget dédié alloué dès la création du compte pour payer automatiquement les frais anti-spam de messages, l'enregistrement de noms et le stockage) et le solde disponible (<code>Balance</code>). Cela permet au réseau de fonctionner de façon pérenne et autonome, sans nécessiter d'abonnement par carte bancaire ni publicité.",

    faqQ5: "Que signifie « Post-Quantique dès la conception » (PQ by design) et pourquoi est-ce crucial aujourd'hui ?",
    faqA5: "Les chiffrements actuels (RSA, courbes elliptiques classiques) deviendront vulnérables avec l'arrivée des calculateurs quantiques. Les agences d'interception pratiquent déjà l'attaque <strong>« Harvest Now, Decrypt Later »</strong> : intercepter et stocker des données chiffrées aujourd'hui pour les décrypter demain. CYBOU intègre nativement les standards post-quantiques du NIST (ML-KEM pour le chiffrement et ML-DSA pour les signatures), garantissant que vos échanges privés d'aujourd'hui resteront protégés pour les décennies à venir.",

    faqQ6: "Comment sont gérés les appareils et que faire en cas de perte ?",
    faqA6: "Votre phrase racine de 24 mots reste confidentielle et hors ligne. Chaque appareil (ordinateur, téléphone) génère sa propre paire de clés cryptographiques, autorisée par votre racine. Si un appareil est égaré ou volé, vous pouvez révoquer son accès depuis un autre appareil approuvé ou via votre phrase racine, sans compromettre vos clés maîtresses ni devoir réinitialiser tout votre compte.",

    faqQ7: "Quel est l'état réel d'avancement et puis-je utiliser CYBOU dès aujourd'hui ?",
    faqA7: "CYBOU est en développement actif (phase R&D ouverte). Le code source comprend un cœur C++20 testé, un nœud DEV autonome avec synchronisation vérifiée, la gestion d'identité locale et un client de bureau expérimental (Qt 6). La livraison finale des emails chiffrés de bout en bout et le réseau BFT multi-validateur indépendant sont en cours d'intégration. Le projet est entièrement open source (licence MIT) avec des spécifications publiques.",

    footBrand: "Projet R&D européen pour une infrastructure de communication souveraine.<br>Conçu en France. Nœud DEV et client de bureau expérimentaux.",
    footNav: "Navigation",
    footDocs: "Spécifications",
    footGov: "Gouvernance"
  },

  en: {
    navServices: "Services",
    navStatus: "Project Status",
    navArch: "Foundations",
    navMailTx: "MailTx Protocol",
    navSpecs: "Tech Specs",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Sovereign Identity • Protected Communication • Post-Quantum Security",
    heroAccent: "One identity. Private communication. Your data under your control.",
    heroSubtitle: "CYBOU unites protected digital identity, private messaging, encrypted storage, decentralized backup, and service funding in one sovereign platform, free from cloud monopolies.",
    heroPillSlogan: "Not a blockchain with features — a protected identity with services.",

    statusCalloutTitle: "Identity-centric platform in active development",
    statusCalloutBody: "CYBOU is not a speculative token or a commercial public service today. The codebase includes an experimental single-validator DEV node, local key-managed identity, and a desktop client. Full end-to-end encrypted mail delivery and an operational multi-validator BFT network remain our active development targets.",

    heroBtnServices: "Explore Services",
    heroBtnStatus: "View Implementation Status",
    heroBtnCode: "Source Code (GitHub)",

    servicesLabel: "Unified Ecosystem",
    servicesTitle: "One identity. Your essential services.",
    servicesDesc: "Instead of scattering your data across third-party platform silos, CYBOU organizes your digital life around a cryptographic identity under your exclusive control.",

    srvIdentityTag: "Foundation",
    srvIdentityTitle: "Sovereign Identity",
    srvIdentityDesc: "Human-readable names (stanislav.cybou), 24-word recovery root, and portable CYBV2 vault. Zero centralized passwords or third-party gatekeepers.",
    srvIdentityStatus: "Tested in core & desktop",

    srvMailTag: "Communication",
    srvMailTitle: "Private Messaging (Mail)",
    srvMailDesc: "Native MailTx operation with post-quantum encryption targets, inclusion proofs, and mailbox indexes owned locally on your device.",
    srvMailStatus: "Format tested, E2EE delivery in progress",

    srvFilesTag: "Data",
    srvFilesTitle: "Encrypted Files & Storage",
    srvFilesDesc: "Distributed, end-to-end encrypted object storage for documents and attachments. Your files remain confidential and unreadable to hosts.",
    srvFilesStatus: "Planned after Mail pilot",

    srvBackupTag: "Continuity",
    srvBackupTitle: "Resilient Backup",
    srvBackupDesc: "Clean-machine restoration of your digital environment using your recovery phrase, without dependence on proprietary cloud vendors.",
    srvBackupStatus: "Planned",

    srvWalletTag: "Utility",
    srvWalletTitle: "Service Funding (Wallet)",
    srvWalletDesc: "Two dedicated balance tiers: SystemBalance to fund services frictionlessly, and spendable Balance. A protocol utility, never speculative trading.",
    srvWalletStatus: "Validated in core & UI",

    srvDevicesTag: "Security",
    srvDevicesTitle: "Device Authority",
    srvDevicesDesc: "Authorize laptops and mobile devices via cryptographic signatures from your identity root, without ever exposing master secrets.",
    srvDevicesStatus: "Core verification active",

    matrixLabel: "Technical Transparency",
    matrixTitle: "Implementation Matrix.",
    matrixDesc: "Strict, verifiable breakdown between tested unit code, active engineering work, and the long-term roadmap.",

    col1Title: "Validated & tested",
    badgeDone: "PASSING",
    col1Item1: "<strong>Permissionless Anti-Sybil Onboarding (doc 70):</strong> protocol-native AccountCreateOpV1 with proof-of-work difficulty binding.",
    col1Item2: "<strong>Typed AccountId (32 bytes):</strong> deterministic balance validation and <code>SystemBalance</code> tracking.",
    col1Item3: "<strong>Atomic Onboarding Bonus:</strong> immediate transfer from <code>OnboardingPool</code> to <code>SystemBalance</code> upon account creation.",
    col1Item4: "<strong>Canonical LevelDB state:</strong> finalized blocks, state roots, compact Mail filters, and height indexes persisted atomically.",
    col1Item5: "<strong>MailTx, evidence, and discovery:</strong> bounded native operation, deterministic fees, inclusion proof, and compact filter implemented in core; complete Email delivery remains pending.",
    col1Item6: "<strong>Experimental DEV node:</strong> single-validator producer, operation submission, and verified sync between processes, with automated tests.",

    col2Title: "In Progress & Hardening",
    badgeWip: "IN PROGRESS",
    col2Item1: "<strong>Multi-validator BFT:</strong> engine and finality certificates tested in code; operation of independent validators and f=1 fault tolerance have not been demonstrated on a public network.",
    col2Item2: "<strong>Desktop client:</strong> Identity, Wallet, Email, Storage, and Backup pages exist; actions remain disabled until their service is connected.",
    col2Item3: "<strong>Keys and operations:</strong> Authority, Validator, Release, and Treasury roles are defined; operational custody and secure deployment need further work.",
    col2Item4: "<strong>DEV network transition:</strong> the new single-validator BFT node coexists with the inherited Bitcoin bootstrap chain; full integration remains pending.",

    col3Title: "Planned Roadmap",
    badgePlanned: "PLANNED",
    col3Item1: "<strong>Target Post-Quantum HPKE Architecture:</strong> hybrid ML-KEM-768 + X25519 (specified in doc 49, crypto verification with OpenSSL >= 3.5).",
    col3Item2: "<strong>Distributed Object Storage:</strong> validator pre-store staging layer and long-term object storage for attachments at scale.",
    col3Item3: "<strong>End-to-end Email service:</strong> MailTx encryption, delivery, Inbox/Sent, and local read-state indexes connected to the live network.",
    col3Item4: "<strong>Operational network:</strong> admission and operation of four independent validators, object storage, and public deployment after security validation.",

    archLabel: "Design Foundations",
    archTitle: "Breaking with Centralized Cloud Architectures.",
    archDesc: "The four architectural pillars guiding long-term digital sovereignty and communication integrity.",

    bento1Title: "Architecture without a mandatory third-party cloud",
    bento1Desc: "The target model avoids accounts and mailboxes hosted by one provider. Today's DEV network uses a known bootstrap validator; resilience across independent operators remains to be demonstrated.",
    bento1Metric: "Experimental network, not deployed for public use",

    bento2Title: "Target Post-Quantum HPKE Profile",
    bento2Desc: "Target hybrid ML-KEM-768 and X25519 profile designed to address 'harvest-now-decrypt-later' threats; full MailTx integration and external review remain pending.",
    bento2Metric: "NIST post-quantum standard",

    bento3Title: "Identity keys managed on the device",
    bento3Desc: "The client manages identity keys locally and operations require signatures. Operator key custody and complete message security still need operational validation.",
    bento3Metric: "Identity key management on the device",

    bento4Title: "Explicit BFT Finality & Deterministic Economics",
    bento4Desc: "CYBOU core calculates finality and fees under deterministic rules. The specified maximum supply is 100 billion CYBOU (0 decimals), with 3/4 of fees for Security and 1/4 for Onboarding; the inherited chain does not yet implement this policy.",
    bento4Value: "100B",
    bento4Metric: "Strict maximum CYBOU token supply (0 decimals)",

    flowLabel: "Message Lifecycle",
    flowTitle: "The Lifecycle of a MailTx Transaction.",
    flowDesc: "Protocol steps implemented in core and steps still needed for the complete Email service.",

    step1Title: "Salted Content Commitment",
    step1Desc: "Core verifies a salted, domain-separated content commitment (doc 69). The encryption and sending client remains in development.",

    step2Title: "Target Hybrid HPKE Profile",
    step2Desc: "Target end-to-end encryption profile for the recipient's public key. Full MailTx integration and review remain required before any production security claim.",

    step3Title: "First-Class MailTx Operation",
    step3Desc: "Core handles MailTx as a native operation with a size limit and deterministic fees. DEV network submission exists; the complete Email experience is not yet available.",

    step4Title: "BFT Consensus & Local Indexing",
    step4Desc: "Core verifies finality certificates, inclusion proofs, and compact Mail discovery filters. Inbox/Sent and read state belong in the local client, which still needs integration.",

    specLabel: "Machine & Human Specifications",
    specTitle: "CYBOU Protocol Technical Factsheet.",
    specDesc: "Structured engineering parameters formatted for search engine indexing and AI knowledge models.",
    factsheetHeading: "Canonical CYBOU Architecture Parameters",

    dtConsensus: "Consensus Model",
    dtPoT: "Proof of Trust Epochs (PoT)",
    dtSupply: "Maximum Supply (MAX_SUPPLY)",
    dtGrant: "Onboarding Bonus",
    dtFees: "Transaction Fee Structure",
    dtMailTx: "MailTx Specification (v1)",
    dtKeys: "Operator Key Isolation",
    dtStack: "Core Technology Stack",
    dtNetworks: "DEV, Beta & Mainnet Networks",
    ddConsensus: "DEV: single-validator finality (<code>f=0</code>). Target: equal-weight BFT with at least four validators before claiming <code>f=1</code>.",
    ddPoT: "Implemented in core: block-height-derived epochs and integer arithmetic, without local wall-clock consensus logic.",
    ddSupply: "<code>100,000,000,000</code> CYBOU (0 decimals) in the specification; the inherited bootstrap chain does not implement this policy.",
    ddGrant: "Implemented in CYBOU state: <code>OnboardingPool</code> to <code>SystemBalance</code> after anti-Sybil AccountCreate (doc 70).",
    ddFees: "Deterministic size-aware Mail fees with no priority bidding; 3/4 Security and 1/4 Onboarding in CYBOU core.",
    ddMailTx: "First-class native operation, one recipient, text only, bounded size, no attachments; Email service is incomplete.",
    ddKeys: "Four defined roles: Authority, Validator, Release Signing, and Treasury; operational custody remains in progress.",
    ddStack: "C++20, CMake, LevelDB, OpenSSL, Qt; local Windows MinGW tests for core and client.",
    ddNetworks: "DEV is experimental. Beta and Mainnet are planned with separate genesis and economic parameters; Beta balances will not carry to Mainnet.",

    faqLabel: "Frequently Asked Questions",
    faqTitle: "Understanding CYBOU.",
    faqDesc: "Clear, practical answers on how identity works, how your data is protected, the utility role of the wallet, and post-quantum security.",
    faqPqBadge: "Post-Quantum Security • PQ by Design",

    faqQ1: "What is CYBOU in plain language?",
    faqA1: "CYBOU is neither a speculative crypto project nor another corporate platform. It is a sovereign, protected communication platform built around your personal digital identity. With a single human-readable address (like <code>stanislav.cybou</code>), you can send end-to-end encrypted messages, store files, back up your environment, and authorize devices — without any company able to read your correspondence or shut down your account.",

    faqQ2: "How does CYBOU differ from services like Gmail, Proton, or iCloud?",
    faqA2: "Traditional cloud providers create platform lock-in: they host your mailboxes, manage your accounts, and harvest metadata (IPs, contact graphs, timestamps). With CYBOU, there is no mandatory cloud provider or central server. Your cryptographic keys are generated and retained exclusively on your device. Your inbox index is stored locally, and messages are verified through a distributed peer-to-peer network without commercial surveillance.",

    faqQ3: "How do .cybou names and account recovery work?",
    faqA3: "Instead of complex cryptographic hashes, you choose a clean address (5 to 32 characters, e.g., <code>alice.cybou</code>) registered on a decentralized protocol registry with no corporate registrar. Your root identity is protected by a standard 24-word recovery phrase. If you switch computers or wipe your device, these 24 words restore your complete identity and access on a clean machine — autonomously, without customer support tickets.",

    faqQ4: "Why is there a Wallet, and why isn't it for crypto speculation?",
    faqA4: "In CYBOU, the wallet is designed as a utility service budget, not an instrument for trading or speculation. It separates two balance tiers: <code>SystemBalance</code> (a dedicated service budget funded upon anti-Sybil onboarding to cover anti-spam message fees, name registrations, and storage) and spendable <code>Balance</code>. This provides a sustainable economic foundation without requiring credit card subscriptions or advertisements.",

    faqQ5: "What does 'Post-Quantum by design' (PQ by design) mean and why does it matter today?",
    faqA5: "Current public-key cryptography (RSA, classical elliptic curves) will be broken by future quantum computers. Adversaries already practice <strong>'Harvest Now, Decrypt Later'</strong>: intercepting encrypted traffic today to decrypt it once quantum hardware matures. CYBOU is engineered from the ground up around NIST post-quantum standards (ML-KEM for encryption and ML-DSA for signatures), ensuring communications sent today remain confidential for decades to come.",

    faqQ6: "How are devices managed, and what happens if I lose a laptop or phone?",
    faqA6: "Your 24-word recovery phrase stays offline as your root authority. Each device (laptop, desktop, mobile) generates its own independent key pair, cryptographically authorized by your root. If a device is lost or stolen, you simply revoke that device's authorization from another approved device or via your 24-word phrase, without exposing master credentials or resetting your entire identity.",

    faqQ7: "What is the real status of the project and can I use CYBOU today?",
    faqA7: "CYBOU is in active development (open R&D phase). The repository includes a functional C++20 core, a standalone DEV node with verified sync, local key-managed identity, and an experimental Qt 6 desktop client. End-to-end encrypted mail delivery and an operational multi-validator BFT network remain under active development. The codebase is fully open source (MIT license) with publicly verifiable specifications.",

    footBrand: "European R&D project for sovereign communication infrastructure.<br>Designed in France. Experimental DEV node and desktop client.",
    footNav: "Navigation",
    footDocs: "Specifications",
    footGov: "Governance"
  }
};

function checkUrlLanguage() {
  const urlParams = new URLSearchParams(window.location.search);
  const lang = urlParams.get('lang');
  if (lang === 'en' || lang === 'fr') {
    setLanguage(lang, false);
  }
}

function initLanguageSwitch() {
  const btnFr = document.getElementById('lang-fr');
  const btnEn = document.getElementById('lang-en');

  if (!btnFr || !btnEn) return;

  btnFr.addEventListener('click', () => setLanguage('fr', true));
  btnEn.addEventListener('click', () => setLanguage('en', true));
}

function initMobileMenu() {
  const toggle = document.getElementById('menu-toggle');
  const menu = document.getElementById('mobile-menu');
  if (!toggle || !menu) return;

  const closeMenu = () => {
    menu.hidden = true;
    toggle.setAttribute('aria-expanded', 'false');
  };

  toggle.addEventListener('click', () => {
    const willOpen = menu.hidden;
    menu.hidden = !willOpen;
    toggle.setAttribute('aria-expanded', String(willOpen));
  });

  menu.querySelectorAll('a').forEach(link => link.addEventListener('click', closeMenu));
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape') {
      closeMenu();
      toggle.focus();
    }
  });
}

function initFaqAccordion() {
  const cards = Array.from(document.querySelectorAll('.faq-card'));
  cards.forEach(card => {
    const trigger = card.querySelector('.faq-trigger');
    if (!trigger) return;

    trigger.addEventListener('click', () => {
      const willOpen = !card.classList.contains('is-open');
      cards.forEach(otherCard => {
        otherCard.classList.remove('is-open');
        const otherTrigger = otherCard.querySelector('.faq-trigger');
        const otherAnswer = otherCard.querySelector('.faq-answer');
        if (otherTrigger) otherTrigger.setAttribute('aria-expanded', 'false');
        if (otherAnswer) otherAnswer.setAttribute('aria-hidden', 'true');
      });
      if (willOpen) {
        card.classList.add('is-open');
        trigger.setAttribute('aria-expanded', 'true');
        const answer = card.querySelector('.faq-answer');
        if (answer) answer.setAttribute('aria-hidden', 'false');
      }
    });
  });
}

function setLanguage(lang, updateUrl = false) {
  currentLang = lang;
  const btnFr = document.getElementById('lang-fr');
  const btnEn = document.getElementById('lang-en');
  if (btnFr) btnFr.classList.toggle('active', lang === 'fr');
  if (btnEn) btnEn.classList.toggle('active', lang === 'en');
  document.documentElement.lang = lang;
  const menuToggle = document.getElementById('menu-toggle');
  if (menuToggle) {
    menuToggle.setAttribute('aria-label', lang === 'fr' ? 'Ouvrir le menu' : 'Open menu');
  }

  if (updateUrl && window.history && window.history.replaceState) {
    const url = new URL(window.location);
    if (lang === 'en') {
      url.searchParams.set('lang', 'en');
    } else {
      url.searchParams.delete('lang');
    }
    window.history.replaceState({}, '', url);
  }

  const dict = translations[lang];
  for (const [key, val] of Object.entries(dict)) {
    const elements = document.querySelectorAll(`[data-i18n="${key}"]`);
    elements.forEach(el => {
      if (val.includes('<')) {
        el.innerHTML = val;
      } else {
        el.textContent = val;
      }
    });
  }
}
