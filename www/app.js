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
    navProtocol: "Architecture du protocole",
    navSpecs: "Fiche technique",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Identité numérique • Services privés • Protocole ouvert",
    heroAccent: "Une identité unique. Des communications privées. Vos données sous votre contrôle.",
    heroSubtitle: "CYBOU construit une identité numérique portable et un socle partagé pour des services de communication et de fichiers privés. Le protocole fonctionne sur le réseau expérimental DEV ; les produits Mail et Files complets restent en développement.",
    heroPillSlogan: "Une identité protégée avec des services privés — sans promesse de service public prématurée.",

    statusCalloutTitle: "Le protocole DEV fonctionne. Le produit reste en construction.",
    statusCalloutBody: "CYBOU reste expérimental et non prêt pour la production. DEV utilise un finaliseur PoA unique ; les nœuds vérifient les blocs et l’état. Mail/Files, restauration, réplication, audit et réparation sont intégrés ; les tests prolongés et l’acceptation Beta restent à compléter.",

    heroBtnServices: "Découvrir les services",
    heroBtnStatus: "Consulter l'état réel d'avancement",
    heroBtnCode: "Code source (GitHub)",

    servicesLabel: "Écosystème unifié",
    servicesTitle: "Une seule identité. Vos services essentiels.",
    servicesDesc: "CYBOU réunit l'identité, le budget de service et un protocole de contenu chiffré. Mail et Files sont des expériences produit prévues sur ce socle, pas des services publics déjà disponibles.",

    srvIdentityTag: "Fondation",
    srvIdentityTitle: "Identité souveraine",
    srvIdentityDesc: "Identité de compte, coffre CYBV2 et noms .cybou. La phrase restaure les clés ; l’historique finalisé, les capsules et les chunks disponibles permettent de reconstruire les contenus, y compris après rotation via RecoveryBridge.",
    srvIdentityStatus: "Identité et coffre intégrés au protocole DEV",

    srvMailTag: "Communication",
    srvMailTitle: "Messagerie privée (Mail)",
    srvMailDesc: "Publications privées avec pièces jointes, découverte des capsules, récupération vérifiée et vues Inbox/Sent dans une base applicative chiffrée par Identity.",
    srvMailStatus: "Flux intégrés — acceptation Beta en cours",

    srvFilesTag: "Données",
    srvFilesTitle: "Stockage chiffré (Files)",
    srvFilesDesc: "Catalogue privé, transferts et reconstruction via le même socle que Mail. Le stockage distant est audité et réparé ; une finalisation seule ne signifie pas Protected.",
    srvFilesStatus: "Flux intégrés — tests prolongés en cours",

    srvBackupTag: "Continuité",
    srvBackupTitle: "Sauvegarde souveraine",
    srvBackupDesc: "La sauvegarde complète est une application ultérieure du même socle de contenu. Elle ne fait pas partie de la version initiale.",
    srvBackupStatus: "Prévu après Beta",

    srvWalletTag: "Usage",
    srvWalletTitle: "Budget de service (Wallet)",
    srvWalletDesc: "Balance finance les transferts ; SystemBalance est un budget de services protocolaires. Les règles économiques sont déterministes et propres à chaque réseau.",
    srvWalletStatus: "Opérations de base disponibles dans le client DEV",

    matrixLabel: "Transparence technique",
    matrixTitle: "Matrice d'implémentation.",
    matrixDesc: "Mail/Files et le stockage sont intégrés sur DEV. Les tests prolongés, installations propres, critères UX et revue de sécurité restent les étapes Beta.",

    col1Title: "Socle implémenté sur DEV",
    badgeDone: "IMPLÉMENTÉ",
    col1Item1: "<strong>Identity et noms .cybou :</strong> AccountCreate avec travail anti-Sybil, rôles de clés hybrides, coffre portable et registre de noms finalisé.",
    col1Item2: "<strong>Finalité PoA :</strong> un signataire DEV dédié, journal anti-équivocation durable, arrêt de sécurité en cas de conflit et validation indépendante par les nœuds complets.",
    col1Item3: "<strong>RootPublication :</strong> opération générique autorisée par Identity ; aucun objet Mail ou fichier permanent dans le consensus.",
    col1Item4: "<strong>Arbre de contenu chiffré :</strong> chunks ROOT/INDEX/DATA ordonnés, adressés par BLAKE3-256 et construits pour le traitement en flux.",
    col1Item5: "<strong>Admission et transport :</strong> stockage local de chunks, preuves d’inclusion liées aux publications finalisées et messages CYP2 PUT/GET.",
    col1Item6: "<strong>Économie déterministe :</strong> frais et transitions de solde validés par le state machine ; DEV, Beta et Mainnet ont des paramètres distincts.",

    col2Title: "Durcissement et préparation Beta",
    badgeWip: "EN COURS",
    col2Item1: "<strong>Mail intégré :</strong> publications, pièces jointes et reconstruction Inbox/Sent ; acceptation desktop à compléter.",
    col2Item2: "<strong>Files intégré :</strong> catalogue privé, transferts et restauration ; installations propres et UX à vérifier.",
    col2Item3: "<strong>Durabilité :</strong> réplication, audit et réparation intégrés. DEV : 1 copie distante ; Beta : 2 indépendantes. Le cache local ne compte pas.",
    col2Item4: "<strong>Preuves :</strong> tests de restauration propre et de panne multiprocessus ; soak prolongé et acceptation Beta à compléter.",

    col3Title: "Étapes ultérieures",
    badgePlanned: "PLUS TARD",
    col3Item1: "<strong>Validation optionnelle :</strong> recherche pré-finalisation, sans état canonique ni pouvoir PoA. Aucun BFT prévu dans le protocole actif.",
    col3Item2: "<strong>Montée en charge :</strong> tests prolongés des pièces jointes et du stockage partagé déjà intégrés.",
    col3Item3: "<strong>Sauvegarde :</strong> application post-Beta du même graphe chiffré, avec restauration vérifiable.",
    col3Item4: "<strong>Beta puis service public :</strong> coûts opérationnels mesurés, revue de sécurité, critères UX et exploitation documentée avant ouverture.",

    archLabel: "Fondations de conception",
    archTitle: "Rupture avec les architectures centralisées.",
    archDesc: "Le protocole sépare clairement les clés, le contenu privé, la finalité centralisée actuelle et les objectifs de résilience à venir.",

    bento1Title: "PoA DEV à opérateur unique",
    bento1Desc: "CYBOU exploite l'unique signataire de finalité du réseau DEV. Chaque nœud complet vérifie indépendamment les blocs et l'état ; cette vérification ne transforme pas le modèle de confiance en BFT.",
    bento1Metric: "Centralisé — pas de tolérance BFT",

    bento2Title: "Profil hybride X-Wing pour DEV",
    bento2Desc: "Le protocole DEV publie une capacité Identity X-Wing basée sur le draft IETF -05. Le profil n'est pas automatiquement transféré à Beta/Mainnet, et son existence ne signifie pas que le service Mail/Files est livré ou audité.",
    bento2Metric: "Profil de développement, revue indépendante requise",

    bento3Title: "Une Identity, rôles cryptographiques séparés",
    bento3Desc: "La récupération, l'autorisation, l'accord de clés, la finalité PoA, la signature des versions et la trésorerie utilisent des rôles séparés.",
    bento3Metric: "Clés locales et rôles distincts",

    bento4Title: "Finalité PoA et économie déterministe",
    bento4Desc: "DEV utilise un opérateur de finalité unique et ne revendique pas le BFT. Le plafond de l'actif est de 100 milliards d'unités sans décimales ; les frais suivent les paramètres du réseau et la règle 3 Sécurité / 1 Accueil.",
    bento4Value: "100 Mrd",
    bento4Metric: "Plafond de 100 milliards — 0 décimale",

    flowLabel: "Cycle de publication privée",
    flowTitle: "Du contenu chiffré à sa disponibilité.",
    flowDesc: "Les services applicatifs relient chiffrement, publication PoA et stockage distant. La durabilité est mesurée séparément de la finalité.",

    step1Title: "Préparer le contenu privé",
    step1Desc: "Le client transforme le contenu en arbre ROOT/INDEX/DATA chiffré et conserve localement les éléments nécessaires. Les schémas Mail/Files restent à l'intérieur du chiffrement.",

    step2Title: "Protéger la clé de contenu",
    step2Desc: "RootPublication transporte des capsules de clé pour les capacités KEM des destinataires sans publier leur AccountID. Le profil X-Wing draft-05 est réservé au réseau DEV.",

    step3Title: "Autoriser une RootPublication",
    step3Desc: "Identity signe une opération générique qui engage la racine, l'arbre d'inclusion et les capsules. Mail ne possède pas de type d'opération de consensus dédié.",

    step4Title: "Finaliser, admettre, retrouver",
    step4Desc: "PoA finalise l'opération ; les fournisseurs vérifient ensuite les preuves d'admission des chunks. Finalité ne signifie pas durabilité : le client doit mesurer la disponibilité, réessayer et reconstruire ses index locaux.",

    specLabel: "Spécifications machine & humaine",
    specTitle: "Fiche technique du protocole CYBOU.",
    specDesc: "Données d'ingénierie structurées et indexables pour moteurs de recherche et assistants d'analyse IA.",
    factsheetHeading: "Paramètres canoniques de l'architecture CYBOU",

    dtConsensus: "Finalité actuelle sur DEV",
    dtPoT: "Époques protocolaires",
    dtSupply: "Offre maximale",
    dtGrant: "Création de compte",
    dtFees: "Frais et répartition",
    dtPublication: "Publication de contenu",
    dtKeys: "Rôles cryptographiques",
    dtStack: "Socle technique",
    dtNetworks: "Réseaux DEV, Beta et Mainnet",
    ddConsensus: "PoA hybride à un signataire exploité par CYBOU. Les nœuds complets vérifient indépendamment ; pas de revendication BFT.",
    ddPoT: "Les périodes dérivées de la hauteur et les calculs entiers évitent l’horloge locale dans les transitions de consensus.",
    ddSupply: "<code>100 000 000 000</code> CYBOU, 0 décimale ; les paramètres sont liés à la définition de chaque réseau.",
    ddGrant: "<code>AccountCreate</code> permissionless avec travail anti-Sybil ; le bonus est transféré de <code>OnboardingPool</code> vers <code>SystemBalance</code>.",
    ddFees: "Frais déterministes de publication selon la taille et le nombre de chunks ; priorité désactivée ; 3 unités Sécurité et 1 Accueil par tranche de 4.",
    ddPublication: "<code>RootPublication</code> générique. Mail, Files et Backup sont des schémas privés chiffrés, pas des opérations de consensus distinctes.",
    ddKeys: "Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing et Treasury sont des rôles séparés ; aucune signature de production classique seule.",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL, BLAKE3 et transport CYP2.",
    ddNetworks: "DEV est actif avec PoA à opérateur unique. Beta et Mainnet auront des genèses et paramètres économiques séparés ; aucun solde Beta ne sera transféré.",

    faqLabel: "Questions fréquentes",
    faqTitle: "Comprendre CYBOU.",
    faqDesc: "Ce qui fonctionne sur DEV, ce qui reste à construire et les limites du modèle de confiance.",
    faqPqBadge: "Cryptographie hybride sur DEV",

    faqQ1: "Qu’est-ce que CYBOU ?",
    faqA1: "CYBOU est un projet logiciel expérimental centré sur une Identity et un socle commun pour la communication privée et les fichiers. Le protocole DEV existe ; Mail et Files ne sont pas encore des services publics utilisables.",

    faqQ2: "En quoi CYBOU vise-t-il une expérience différente de Gmail ou Google Drive ?",
    faqA2: "L'objectif produit est de proposer des parcours familiers de messagerie et de fichiers tout en chiffrant les contenus côté client et en laissant les vues Inbox/Files au client. Ces parcours ne sont pas encore intégrés de bout en bout ; CYBOU ne remplace pas aujourd'hui Gmail ni Google Drive.",

    faqQ3: "À quoi servent un nom .cybou et la phrase de récupération ?",
    faqA3: "Un nom .cybou est un alias de compte enregistré par opérations finalisées (5–32 caractères ASCII). La phrase restaure Identity ; les contenus nécessitent aussi l’historique et les chunks disponibles. La restauration et RecoveryBridge sont implémentés.",

    faqQ4: "Pourquoi un wallet dans un produit de communication ?",
    faqA4: "Le protocole distingue le solde dépensable <code>Balance</code> du budget de services <code>SystemBalance</code>. Les frais et le bonus de création sont déterministes et propres au réseau. Les soldes DEV sont expérimentaux, sans valeur de production.",

    faqQ5: "Que signifie le profil de cryptographie hybride de DEV ?",
    faqA5: "Les signatures du protocole combinent Ed25519 avec le profil ML-DSA désigné. La capacité KEM X-Wing du DEV utilise le draft IETF -05. Il s'agit d'un profil de développement, pas d'une garantie de confidentialité à long terme ni d'un audit indépendant ; Beta/Mainnet doivent faire l'objet d'une décision séparée.",

    faqQ6: "La phrase de récupération restaure-t-elle mes messages et fichiers ?",
    faqA6: "Elle restaure les clés et l'Identity si les vérifications réseau réussissent. La reconstruction de Mail et Files doit également retrouver les publications finalisées, ouvrir les capsules et récupérer les chunks. Ce parcours complet n'est pas encore livré.",

    faqQ7: "Puis-je utiliser CYBOU aujourd’hui ?",
    faqA7: "Le client et DEV restent expérimentaux. Mail/Files, réplication, audit, réparation et restauration sont intégrés ; soak prolongé, acceptation Beta et revue de sécurité restent ouverts. Consultez le statut dans le dépôt.",

    footBrand: "Projet open source en développement.<br>Protocole DEV expérimental. Mail et Files ne sont pas encore des services publics.",
    footNav: "Navigation",
    footDocs: "Spécifications",
    footGov: "Gouvernance"
  },

  en: {
    navServices: "Services",
    navStatus: "Project Status",
    navArch: "Foundations",
    navProtocol: "Protocol Architecture",
    navSpecs: "Tech Specs",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Digital identity • Private services • Open protocol",
    heroAccent: "One identity. Private communication. Your data under your control.",
    heroSubtitle: "CYBOU is building a portable digital identity and a shared foundation for private communication and files. The protocol runs on the experimental DEV network; complete Mail and Files products are still in development.",
    heroPillSlogan: "One protected identity with private services — without premature public-service claims.",

    statusCalloutTitle: "The DEV protocol runs. The product is still being built.",
    statusCalloutBody: "CYBOU remains experimental and unready for production. DEV uses one PoA finalizer; full nodes verify blocks and state. Mail/Files, restore, replication, audit and repair are integrated; sustained soak and Beta acceptance remain open.",

    heroBtnServices: "Explore Services",
    heroBtnStatus: "View Implementation Status",
    heroBtnCode: "Source Code (GitHub)",

    servicesLabel: "Unified Ecosystem",
    servicesTitle: "One identity. Your essential services.",
    servicesDesc: "CYBOU brings identity, service funding, and an encrypted-content protocol together. Mail and Files are planned product experiences on that foundation, not public services already available.",

    srvIdentityTag: "Foundation",
    srvIdentityTitle: "Sovereign Identity",
    srvIdentityDesc: "Account-level Identity, CYBV2 vault and .cybou names. The phrase restores keys; finalized history, capsules and available chunks rebuild content, including after rotation through RecoveryBridge.",
    srvIdentityStatus: "Identity and vault path integrated on DEV",

    srvMailTag: "Communication",
    srvMailTitle: "Private Messaging (Mail)",
    srvMailDesc: "Private publications with attachments, capsule discovery, verified retrieval and Inbox/Sent views in an encrypted per-Identity Application DB.",
    srvMailStatus: "Integrated flows — Beta acceptance pending",

    srvFilesTag: "Data",
    srvFilesTitle: "Encrypted Files & Storage",
    srvFilesDesc: "Private catalog, transfers and rebuild use the same substrate as Mail. Remote storage is audited and repaired; finalization alone does not mean Protected.",
    srvFilesStatus: "Integrated flows — sustained soak pending",

    srvBackupTag: "Continuity",
    srvBackupTitle: "Resilient Backup",
    srvBackupDesc: "Full backup is a later application of the same content substrate. It is outside the initial release.",
    srvBackupStatus: "Planned after Beta",

    srvWalletTag: "Utility",
    srvWalletTitle: "Service Funding (Wallet)",
    srvWalletDesc: "Balance pays for transfers; SystemBalance is a budget for protocol services. Economic rules are deterministic and set separately for each network.",
    srvWalletStatus: "Basic operations available in the DEV client",

    matrixLabel: "Technical Transparency",
    matrixTitle: "Implementation Matrix.",
    matrixDesc: "Mail/Files and storage are integrated on DEV. Sustained soak, clean installations, UX acceptance and security review remain Beta gates.",

    col1Title: "Protocol substrate on DEV",
    badgeDone: "IMPLEMENTED",
    col1Item1: "<strong>Identity and .cybou names:</strong> AccountCreate with anti-Sybil work, hybrid key roles, portable vault, and finalized name registry.",
    col1Item2: "<strong>PoA finality:</strong> dedicated DEV signer, durable anti-equivocation journal, conflict safety halt, and independent full-node validation.",
    col1Item3: "<strong>RootPublication:</strong> generic Identity-authorized operation; no permanent Mail or file objects in consensus state.",
    col1Item4: "<strong>Encrypted content tree:</strong> ordered ROOT/INDEX/DATA chunks addressed by full BLAKE3-256 and built for streaming.",
    col1Item5: "<strong>Admission and transport:</strong> local chunk store, inclusion proofs tied to finalized publications, and CYP2 PUT/GET messages.",
    col1Item6: "<strong>Deterministic economics:</strong> fee and balance transitions validated by the state machine; DEV, Beta, and Mainnet use separate parameters.",

    col2Title: "Hardening and Beta preparation",
    badgeWip: "IN PROGRESS",
    col2Item1: "<strong>Integrated Mail:</strong> publications, attachments and Inbox/Sent rebuild; desktop acceptance remains open.",
    col2Item2: "<strong>Integrated Files:</strong> private catalog, transfers and restore; clean-install and UX acceptance remain open.",
    col2Item3: "<strong>Durability:</strong> replication, audit and repair are integrated. DEV: 1 remote copy; Beta: 2 independent copies. Local cache does not count.",
    col2Item4: "<strong>Evidence:</strong> clean-restore and multi-process failure tests; sustained soak and Beta acceptance remain open.",

    col3Title: "Later stages",
    badgePlanned: "LATER",
    col3Item1: "<strong>Optional Validation:</strong> pre-finalization research with no canonical effects or PoA power. The active protocol has no BFT path.",
    col3Item2: "<strong>Scale:</strong> sustained testing of integrated attachments and shared storage.",
    col3Item3: "<strong>Backup:</strong> post-Beta application of the same encrypted graph, with verifiable restore.",
    col3Item4: "<strong>Beta, then public service:</strong> measured operating costs, security review, UX acceptance, and documented operations before opening.",

    archLabel: "Design Foundations",
    archTitle: "Breaking with Centralized Cloud Architectures.",
    archDesc: "The protocol separates key roles, private content, today’s centralized finality, and future resilience goals.",

    bento1Title: "Single-operator PoA on DEV",
    bento1Desc: "CYBOU operates the only finality signer on the DEV network. Every full node independently verifies blocks and state; that verification does not make the trust model BFT.",
    bento1Metric: "Centralized — no BFT fault tolerance",

    bento2Title: "Hybrid X-Wing profile for DEV",
    bento2Desc: "The DEV protocol publishes an Identity X-Wing capability based on IETF draft -05. The profile does not automatically carry over to Beta/Mainnet, and its presence does not mean Mail/Files are shipped or audited.",
    bento2Metric: "Development profile; independent review required",

    bento3Title: "One Identity, separate cryptographic roles",
    bento3Desc: "Recovery, authorization, key agreement, PoA finality, release signing, and treasury use separate cryptographic roles.",
    bento3Metric: "Local keys and distinct roles",

    bento4Title: "PoA finality and deterministic economics",
    bento4Desc: "DEV uses one finality operator and makes no BFT claim. The asset cap is 100 billion units with no decimals; fees follow network parameters and the 3 Security / 1 Onboarding split.",
    bento4Value: "100B",
    bento4Metric: "100 billion cap — 0 decimals",

    flowLabel: "Private publication flow",
    flowTitle: "From encrypted content to availability.",
    flowDesc: "Application services connect encryption, PoA publication and remote storage. Durability is measured separately from finality.",

    step1Title: "Prepare private content",
    step1Desc: "The client turns content into an encrypted ROOT/INDEX/DATA tree and keeps the required local material. Mail/Files schemas remain inside encryption.",

    step2Title: "Protect the content key",
    step2Desc: "RootPublication carries key capsules for recipient KEM capabilities without publishing recipient AccountIDs. X-Wing draft -05 is limited to DEV.",

    step3Title: "Authorize a RootPublication",
    step3Desc: "Identity signs a generic operation committing to the root, chunk-inclusion tree, and capsules. Mail has no dedicated consensus operation type.",

    step4Title: "Finalize, admit, retrieve",
    step4Desc: "PoA finalizes the operation; providers then verify chunk-admission proofs. Finality is not durability: the client must measure availability, retry, and rebuild local indexes.",

    specLabel: "Machine & Human Specifications",
    specTitle: "CYBOU Protocol Technical Factsheet.",
    specDesc: "Structured engineering parameters formatted for search engine indexing and AI knowledge models.",
    factsheetHeading: "Canonical CYBOU Architecture Parameters",

    dtConsensus: "Current DEV finality",
    dtPoT: "Protocol epochs",
    dtSupply: "Maximum supply",
    dtGrant: "Account creation",
    dtFees: "Fees and allocation",
    dtPublication: "Content publication",
    dtKeys: "Cryptographic key roles",
    dtStack: "Technology stack",
    dtNetworks: "DEV, Beta, and Mainnet",
    ddConsensus: "Hybrid PoA with one signer operated by CYBOU. Full nodes independently verify; no BFT claim.",
    ddPoT: "Height-derived periods and integer arithmetic avoid local wall-clock input in consensus transitions.",
    ddSupply: "<code>100,000,000,000</code> CYBOU, 0 decimals; parameters are bound to each network definition.",
    ddGrant: "Permissionless <code>AccountCreate</code> with anti-Sybil work; the bonus moves from <code>OnboardingPool</code> to <code>SystemBalance</code>.",
    ddFees: "Deterministic publication fees based on size and chunk count; priority fees disabled; 3 Security and 1 Onboarding unit per 4-unit allocation.",
    ddPublication: "Generic <code>RootPublication</code>. Mail, Files, and Backup are private encrypted schemas, not separate consensus operations.",
    ddKeys: "Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing, and Treasury are separate roles; no classical-only production signature.",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL, BLAKE3, and CYP2 transport.",
    ddNetworks: "DEV runs single-operator PoA. Beta and Mainnet will have separate genesis and economic parameters; Beta balances will not carry over.",

    faqLabel: "Frequently Asked Questions",
    faqTitle: "Understanding CYBOU.",
    faqDesc: "What runs on DEV, what still needs to be built, and the limits of the current trust model.",
    faqPqBadge: "Hybrid cryptography on DEV",

    faqQ1: "What is CYBOU?",
    faqA1: "CYBOU is an experimental software project centered on one Identity and a shared foundation for private communication and files. The DEV protocol exists; Mail and Files are not yet usable public services.",

    faqQ2: "How is CYBOU aiming for an experience like Gmail or Google Drive?",
    faqA2: "The product goal is familiar mail and file workflows with client-side content encryption and client-owned Inbox/Files views. Those workflows are not yet integrated end to end; CYBOU does not replace Gmail or Google Drive today.",

    faqQ3: "What do .cybou names and the recovery phrase do?",
    faqA3: "A .cybou name is an account alias registered through finalized operations (5–32 ASCII characters). The phrase restores Identity; content also needs history and available chunks. Restore and RecoveryBridge are implemented.",

    faqQ4: "Why does a communication product have a wallet?",
    faqA4: "The protocol separates spendable <code>Balance</code> from the service budget <code>SystemBalance</code>. Fees and the account-creation bonus are deterministic and network-specific. DEV balances are experimental and have no production value.",

    faqQ5: "What does DEV’s hybrid cryptography profile mean?",
    faqA5: "Protocol signatures combine Ed25519 with the designated ML-DSA profile. DEV’s X-Wing KEM capability uses IETF draft -05. This is a development profile, not a long-term confidentiality guarantee or independent audit; Beta/Mainnet require a separate decision.",

    faqQ6: "Does the recovery phrase restore my messages and files?",
    faqA6: "It restores Identity keys if network checks pass. Rebuilding Mail and Files also requires finding finalized publications, opening capsules, and retrieving chunks. That complete flow has not shipped yet.",

    faqQ7: "Can I use CYBOU today?",
    faqA7: "The desktop and DEV remain experimental. Mail/Files, replication, audit, repair and restore are integrated; sustained soak, Beta acceptance and security review remain open. See the repository status.",

    footBrand: "Open-source project in active development.<br>Experimental DEV protocol. Mail and Files are not yet public services.",
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
