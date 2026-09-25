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
    faqTitle: "Architecture, Cryptographie & Gouvernance.",
    faqDesc: "Des réponses précises et transparentes sur nos choix techniques, notre feuille de route et la sécurité post-quantique.",
    faqPqBadge: "Sécurité Post-Quantique • PQ by Design",
    faqQ1: "Que signifie « Post-Quantique dès la conception » (PQ by design) pour CYBOU ?",
    faqA1P1: "La plupart des systèmes de communication historiques s'appuient sur RSA ou la cryptographie sur les courbes elliptiques classiques. Face à l'émergence des calculateurs quantiques, ils tentent de patcher a posteriori leurs protocoles, exposant toutes les communications passées aux attaques <strong>« Harvest Now, Decrypt Later » (HNDL)</strong> — où des acteurs étatiques enregistrent dès aujourd'hui les flux chiffrés pour les casser demain.",
    faqA1P2: "Dans CYBOU, la résistance post-quantique est un objectif de conception. Le profil cible <code>MailTx</code> combine <strong>ML-KEM-768</strong> et <strong>X25519</strong>, tandis qu'un vérificateur hybride <strong>Ed25519 + ML-DSA-65</strong> est validé pour les signatures d'autorité (OpenSSL >= 3.5). Le chiffrement MailTx complet, l'audit externe et les migrations de clés restent à réaliser : CYBOU ne revendique pas encore une sécurité post-quantique de production.",
    faqQ2: "Pourquoi CYBOU n'est-il pas encore téléchargeable pour le grand public ?",
    faqA2: "CYBOU dispose d'un nœud DEV à validateur unique, d'une synchronisation vérifiée et d'une interface de bureau expérimentale. Le service Email chiffré de bout en bout, le fonctionnement de plusieurs validateurs indépendants et la sécurité opérationnelle doivent être achevés avant une diffusion grand public. Le code source et les tests sont consultables.",
    faqQ3: "En quoi CYBOU diffère-t-il d'une messagerie électronique classique (SMTP/IMAP) ?",
    faqA3: "L'architecture cible CYBOU remplace le modèle SMTP/IMAP centralisé par une opération native <code>MailTx</code>, un engagement de contenu salé et un index local au destinataire. Ce flux complet dépend encore de l'intégration du moteur BFT, du chiffrement MailTx et du client ; il n'est pas présenté comme un service déployé aujourd'hui.",
    faqQ4: "Comment fonctionne le consensus BFT et la tolérance aux pannes ?",
    faqA4: "Le cœur implémente votes, certificats de finalité et transition d'ensemble de validateurs à poids égal. Le nœud DEV autonome produit aujourd'hui avec un seul validateur : <code>f=0</code>. Il faut au moins 4 validateurs indépendants pour revendiquer <code>f=1</code> ; ce déploiement n'est pas encore établi. La chaîne de bootstrap Bitcoin héritée est distincte du nouveau chemin d'état CYBOU.",
    faqQ5: "Quelle est la finalité économique du jeton CYBOU ?",
    faqA5: "La spécification CYBOU fixe une offre maximale de <strong>100 000 000 000 unités (0 décimale)</strong>. Le cœur utilise AccountCreate avec preuve anti-Sybil et crédite le bonus depuis OnboardingPool vers SystemBalance, sans invitation d'opérateur. Les frais Mail sont déterministes selon la taille et répartis à 75% pour la sécurité du réseau et 25% pour l'accueil. La chaîne de bootstrap héritée n'applique pas cette politique monétaire.",

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
    faqTitle: "Architecture, Cryptography & Governance.",
    faqDesc: "Transparent and rigorous answers regarding our technical foundation, roadmap, and post-quantum security.",
    faqPqBadge: "Post-Quantum Security • PQ by Design",
    faqQ1: "What does 'Post-Quantum by design' (PQ by design) mean for CYBOU?",
    faqA1P1: "Most legacy messaging systems rely on RSA or classical elliptic curve cryptography. Facing the rise of quantum computing, they attempt to patch protocols retroactively, leaving all historically recorded correspondence vulnerable to <strong>'Harvest Now, Decrypt Later' (HNDL)</strong> attacks — where adversaries intercept encrypted traffic today to decrypt it tomorrow.",
    faqA1P2: "Post-quantum resilience is a design objective for CYBOU. The target <code>MailTx</code> profile combines <strong>ML-KEM-768</strong> and <strong>X25519</strong>, while a hybrid <strong>Ed25519 + ML-DSA-65</strong> verifier is validated for authority signatures (OpenSSL >= 3.5). Full MailTx encryption, external review, and key migration remain pending; CYBOU does not yet claim production post-quantum security.",
    faqQ2: "Why isn't CYBOU available for public download yet?",
    faqA2: "CYBOU has a single-validator DEV node, verified sync, and an experimental desktop interface. End-to-end encrypted Email, operation of independent validators, and operational security must be completed before public release. Source code and tests are available for inspection.",
    faqQ3: "How does CYBOU differ from standard email (SMTP/IMAP)?",
    faqA3: "The target CYBOU architecture replaces centralized SMTP/IMAP infrastructure with a native <code>MailTx</code>, a salted content commitment, and recipient-owned local indexes. This complete flow still depends on BFT, MailTx encryption, and client integration and is not presented as a deployed service today.",
    faqQ4: "How do BFT consensus and fault tolerance work?",
    faqA4: "Core implements votes, finality certificates, and equal-weight validator-set transitions. The standalone DEV node currently produces with one validator: <code>f=0</code>. At least four independent validators are needed before claiming <code>f=1</code>; that deployment is not yet established. The inherited Bitcoin bootstrap chain is separate from the new CYBOU state path.",
    faqQ5: "What is the economic purpose of the CYBOU token?",
    faqA5: "The CYBOU specification sets a maximum supply of <strong>100,000,000,000 units (0 decimals)</strong>. Core uses anti-Sybil AccountCreate and credits the onboarding grant from OnboardingPool to SystemBalance without operator invitations. Mail fees are deterministic and size-aware, split 75% to network security and 25% to onboarding. The inherited bootstrap chain does not implement this monetary policy.",

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
