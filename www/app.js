/* ==========================================================================
   CYBOU.FR — Sovereign Desktop Suite & Dynamic Localization Engine
   Engineered for clear user-facing product presentation & responsive state verification
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initLanguageSwitch();
  initMobileMenu();
  initFaqAccordion();
  initMockupTabs();
  checkUrlLanguage();
});

// --- 1. Language Toggle (French / English) ---
const translations = {
  fr: {
    navProducts: "Produits",
    navShowcase: "Aperçu",
    navSecurity: "Sécurité & TLS",
    navComparison: "Comparatif",
    navArch: "Sous le capot",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Transport 100% Chiffré TLS 1.3 • Souveraineté France • Post-Quantique",
    heroAccent: "Votre espace privé souverain. Messagerie, fichiers et identité.",
    heroSubtitle: "CYBOU réunit messagerie privée, stockage de fichiers et identité souveraine dans une application de bureau protectrice. Vos données sont chiffrées sur votre ordinateur avant tout envoi, et tous les flux réseau sont verrouillés en TLS 1.3.",
    heroPillSlogan: "Une suite souveraine qui protège votre vie privée — sans compromis publicitaire ni surveillance.",

    statusCalloutTitle: "Protocole DEV opérationnel. Préparation de la version publique.",
    statusCalloutBody: "Le réseau expérimental DEV fonctionne avec finalité PoA, stockage distribué et transport TLS 1.3. L'application de bureau CYBOU fait l'objet de tests continus de durcissement et d'intégration avant ouverture au grand public.",

    heroBtnShowcase: "Découvrir l'application",
    heroBtnSecurity: "Pourquoi CYBOU ?",

    badgeTls: "Transport 100% TLS 1.3 (Anti-interception Wi-Fi/FAI)",
    badgeFrance: "Souveraineté France (Hors Cloud Act US)",
    badgePhrase: "Zéro Mot de passe / Clé 24 mots",
    badgePq: "Bouclier Post-Quantique NIST",
    badgeNoAds: "Zéro Collecte & Zéro Publicité",

    showcaseLabel: "Aperçu de l'application",
    showcaseTitle: "L'expérience CYBOU Desktop.",
    showcaseDesc: "Une interface unifiée et intuitive qui réunit vos communications, vos fichiers et votre identité sous une protection cryptographique continue.",
    mockupStatusTls: "Connecté via TLS 1.3",
    mockupNetworkFr: "Nœud souverain France",
    mockupTabMail: "Mail",
    mockupTabFiles: "Files",
    mockupTabId: "Identité",
    mockupTabSec: "Sécurité & Réseau",

    mockupMailFrom1: "Pierre Martin",
    mockupMailSub1: "Documents confidentiels 2026",
    mockupMailSnip1: "Bonjour Alice, voici les documents chiffrés demandés...",
    mockupBadgeE2ee: "Chiffré client • TLS 1.3",
    mockupMailFrom2: "Cabinet Juridique",
    mockupMailSub2: "Contrat de partenariat signé",
    mockupMailSnip2: "Le document a été validé et scellé avec signature hybride...",
    mockupBadgePq: "Signature ML-DSA",
    mockupMailBodyTitle: "Documents fiscaux et comptables 2026",
    mockupBadgeTlsPill: "Transport TLS 1.3 Certifié",
    mockupMailBodyText: "Bonjour Alice,<br><br>Voici les pièces jointes chiffrées de notre bilan. Grâce au protocole CYBOU, cette transmission est passée directement par des liaisons chiffrées en TLS 1.3. Aucun serveur tiers, aucun FAI ni aucun robot publicitaire n'a pu lire un seul mot de notre échange.<br><br>Bien cordialement,<br>Pierre",

    mockupFilesColName: "Fichier",
    mockupFilesColSize: "Taille",
    mockupFilesColReplicas: "Réplication",
    mockupFilesColStatus: "État",
    mockupProtected: "Protégé en France",

    mockupIdHeading1: "Identité active",
    mockupIdHeading2: "Clés Post-Quantiques",
    mockupIdHeading3: "Restauration de compte",
    mockupIdHeading4: "Souveraineté des données",

    mockupSecHeading1: "Chiffrement du transport (CYP2)",
    mockupSecDesc1: "Tunnel TLS 1.3 strict avec négociation cryptographique moderne. Élimine tout risque d'interception sur Wi-Fi public ou inspection FAI (DPI).",
    mockupSecHeading2: "Politique d'admission réseau",
    mockupSecDesc2: "Trafic P2P public restreint exclusivement au territoire français via géolocalisation IP stricte. Failsafe fermé en cas de doute.",

    servicesLabel: "Services intégrés",
    servicesTitle: "Trois piliers pour votre indépendance numérique.",
    servicesDesc: "Chaque outil fonctionne directement sur votre machine sans dépendre des géants de la tech.",

    srvMailTag: "Messagerie",
    srvMailTitle: "CYBOU Mail — Votre boîte de réception inviolable",
    srvMailDesc: "Ne laissez plus Google ou Microsoft analyser vos correspondances privées. Vos messages et pièces jointes sont chiffrés sur votre poste avec des clés post-quantiques et transmis via TLS 1.3. Zéro publicité, zéro profilage.",
    srvMailStatus: "Flux intégrés — acceptation Beta en cours",

    srvFilesTag: "Stockage",
    srvFilesTitle: "CYBOU Files — Vos documents sous clé en France",
    srvFilesDesc: "Un cloud personnel souverain qui échappe au US Cloud Act. Vos fichiers sont découpés en fragments chiffrés (BLAKE3-256), répliqués et automatiquement audités sur des nœuds indépendants en France.",
    srvFilesStatus: "Flux intégrés — tests prolongés en cours",

    srvIdentityTag: "Identité",
    srvIdentityTitle: "CYBOU Identity — Une clé unique pour votre vie numérique",
    srvIdentityDesc: "Oubliez les mots de passe vulnérables et les SMS piratables. Votre compte est sécurisé par une phrase secrète de 24 mots et un nom lisible .cybou inaliénable qui vous appartient à vie.",
    srvIdentityStatus: "Identité et coffre intégrés au protocole DEV",

    srvContinuityTag: "Continuité",
    srvContinuityTitle: "Restauration & Continuité",
    srvContinuityDesc: "En cas de perte ou de changement d'ordinateur, votre phrase de 24 mots et les preuves finalisées permettent de reconstruire vos emails, fichiers et carnet d'adresses en toute sécurité.",
    srvContinuityStatus: "Reconstruction intégrée sur DEV",

    srvWalletTag: "Économie",
    srvWalletTitle: "Budget de service transparent",
    srvWalletDesc: "Un modèle économique déterministe et prévisible pour rémunérer le stockage et la sécurité du réseau sans abonnement caché ni exploitation de vos données.",
    srvWalletStatus: "Opérations de base disponibles dans le client DEV",

    secLabel: "Sécurité & Transport",
    secTitle: "Une forteresse numérique de bout en bout.",
    secDesc: "De votre connexion Wi-Fi jusqu'au stockage de vos fichiers, découvrez pourquoi vos données sont réellement impénétrables.",

    secTlsTag: "Protection Réseau",
    secTlsTitle: "Transport 100% Chiffré en TLS 1.3 (Protocole CYP2)",
    secTlsDesc: "Que vous soyez sur le Wi-Fi public d'une gare, d'un hôtel ou sur votre box fibre domestique, chaque connexion entre votre application et le réseau est scellée par le standard TLS 1.3 le plus strict. Ni votre FAI (Orange, SFR, Free, Bouygues), ni un pirate sur le réseau local ne peuvent voir ce que vous transférez ni modifier un seul octet.",

    secE2eeTag: "Confidentialité Totale",
    secE2eeTitle: "Chiffrement Client de bout en bout",
    secE2eeDesc: "Le TLS protège le transport, mais CYBOU protège aussi la donnée elle-même. Les messages et fichiers sont chiffrés sur votre processeur avec vos propres clés secrètes avant de partir. Même les nœuds du réseau ne voient que des données opaques et incompréhensibles.",

    secFranceTag: "Immunité Juridique",
    secFranceTitle: "Souveraineté Territoriale France & UE",
    secFranceDesc: "Zéro serveur aux États-Unis, zéro dépendance aux géants de la Silicon Valley. L'admission P2P publique est strictement limitée à la France avec contrôle géographique rigoureux (fail-closed). Vos données échappent totalement au US Cloud Act et à la FISA 702.",

    secPqTag: "Prêt pour l'Avenir",
    secPqTitle: "Bouclier Post-Quantique NIST (ML-DSA)",
    secPqDesc: "Les futurs ordinateurs quantiques menacent de casser les chiffrements traditionnels (RSA, courbes elliptiques). CYBOU déploie dès aujourd'hui les algorithmes post-quantiques normalisés par le NIST (ML-DSA-44 et ML-DSA-65) pour garantir la sécurité de vos données pour les décennies à venir.",

    compLabel: "Comparatif objectif",
    compTitle: "CYBOU face aux géants du numérique.",
    compDesc: "Comprenez en un coup d'œil ce qui change lorsque vous passez d'un service centralisé à une suite souveraine.",
    compThCriteria: "Critère de protection",
    compThCybou: "CYBOU Desktop",
    compThGoogle: "Google (Gmail / Drive)",
    compThAppleMs: "Apple iCloud / Microsoft",
    compRow1Crit: "Chiffrement des messages et fichiers",
    compRow1Cybou: "Client-side obligatoire (Zero-Knowledge)",
    compRow1Google: "Chiffré sur serveurs Google (Google possède les clés)",
    compRow1AppleMs: "Chiffrement serveur avec clés détenues par l'éditeur",
    compRow2Crit: "Sécurité du transport réseau",
    compRow2Cybou: "Tunnel strict TLS 1.3 de bout en bout (CYP2)",
    compRow2Google: "TLS vers centres de données Google",
    compRow2AppleMs: "TLS vers centres de données US",
    compRow3Crit: "Analyse des contenus pour pub / IA",
    compRow3Cybou: "Zéro analyse, zéro publicité, zéro entraînement IA",
    compRow3Google: "Indexation pour ciblage et entraînement d'algorithmes",
    compRow3AppleMs: "Scans automatisés et télémétrie",
    compRow4Crit: "Juridiction et législation",
    compRow4Cybou: "France & Union Européenne (RGPD souverain)",
    compRow4Google: "États-Unis (Soumis au Cloud Act & FISA 702)",
    compRow4AppleMs: "États-Unis (Soumis aux lois extraterritoriales US)",
    compRow5Crit: "Résistance aux attaques quantiques",
    compRow5Cybou: "NIST ML-DSA post-quantique intégré",
    compRow5Google: "Algorithmes classiques (RSA / ECC)",
    compRow5AppleMs: "Algorithmes classiques (RSA / ECC)",
    compRow6Crit: "Authentification et propriété",
    compRow6Cybou: "Phrase de 24 mots souveraine (Aucun mot de passe / SMS)",
    compRow6Google: "Numéro de téléphone requis, risque de ban arbitraire",
    compRow6AppleMs: "Identifiant lié à numéro de téléphone et carte bancaire",

    archLabel: "Sous le capot",
    archTitle: "Architecture technique & Fondations.",
    archDesc: "Pour les développeurs, auditeurs et curieux : les spécifications exactes du moteur de consensus et du réseau.",

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

    step1Title: "Préparer le contenu privé",
    step1Desc: "Le client transforme le contenu en arbre ROOT/INDEX/DATA chiffré et conserve localement les éléments nécessaires. Les schémas Mail/Files restent à l'intérieur du chiffrement.",

    step2Title: "Protéger la clé de contenu",
    step2Desc: "RootPublication transporte des capsules de clé pour les capacités KEM des destinataires sans publier leur AccountID. Le profil X-Wing draft-05 est réservé au réseau DEV.",

    step3Title: "Autoriser une RootPublication",
    step3Desc: "Identity signe une opération générique qui engage la racine, l'arbre d'inclusion et les capsules. Mail ne possède pas de type d'opération de consensus dédié.",

    step4Title: "Finaliser, admettre, retrouver",
    step4Desc: "PoA finalise l'opération ; les fournisseurs vérifient ensuite les preuves d'admission des chunks. Finalité ne signifie pas durabilité : le client doit mesurer la disponibilité, réessayer et reconstruire ses index locaux.",

    factsheetHeading: "Paramètres canoniques de l'architecture CYBOU",
    dtTransport: "Transport P2P & Chiffrement",
    ddTransport: "CYP2 encapsulé dans TLS 1.3 strict. SPKI pinning éphémère, zéro fuite de métadonnées de transport vers FAI/tiers.",
    dtConsensus: "Finalité actuelle sur DEV",
    ddConsensus: "PoA hybride à un signataire exploité par CYBOU sur DEV. Les nœuds complets vérifient indépendamment ; pas de revendication BFT. Cible : PoA sur poste Central Authority.",
    dtBootstrap: "1 à 4 identités d'amorce",
    ddBootstrap: "1 à 4 identités autorisées par la genèse (AccountID + RecoveryKeyID) pour la découverte et le relais ; aucun rôle de finalité ou de vote. Même logiciel de nœud complet.",
    dtAdmission: "Admission P2P France souveraine",
    ddAdmission: "Admission P2P publique restreinte à l'espace IP français (données Geo locales, fail-closed). Filtrage VPN/proxy/Tor local optionnel.",
    dtSupply: "Offre maximale",
    ddSupply: "<code>100 000 000 000</code> CYBOU, 0 décimale ; les paramètres sont liés à la définition de chaque réseau.",
    dtGrant: "Création de compte",
    ddGrant: "<code>AccountCreate</code> permissionless avec travail anti-Sybil ; le bonus est transféré de <code>OnboardingPool</code> vers <code>SystemBalance</code>.",
    dtFees: "Frais et répartition",
    ddFees: "Frais déterministes de publication selon la taille et le nombre de chunks ; priorité désactivée ; 3 unités Sécurité et 1 Accueil par tranche de 4.",
    dtPublication: "Publication de contenu",
    ddPublication: "<code>RootPublication</code> générique. Mail, Files et Backup sont des schémas privés chiffrés, pas des opérations de consensus distinctes.",
    dtKeys: "Rôles cryptographiques",
    ddKeys: "Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing et Treasury sont des rôles séparés ; aucune signature de production classique seule.",
    dtStack: "Socle technique",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL, BLAKE3 et transport CYP2 sur TLS 1.3.",

    matrixLabel: "Transparence technique",
    matrixTitle: "Matrice d'implémentation.",
    matrixDesc: "Mail/Files et le stockage sont intégrés sur DEV. Les tests prolongés, installations propres, critères UX et revue de sécurité restent les étapes Beta.",

    col1Title: "Socle implémenté sur DEV",
    badgeDone: "IMPLÉMENTÉ",
    col1Item1: "<strong>Identity et noms .cybou :</strong> AccountCreate avec travail anti-Sybil, rôles de clés hybrides, coffre portable et registre de noms finalisé.",
    col1Item2: "<strong>Finalité PoA :</strong> un signataire DEV dédié, journal anti-équivocation durable, arrêt de sécurité en cas de conflit et validation indépendante par les nœuds complets.",
    col1Item3: "<strong>RootPublication :</strong> opération générique autorisée par Identity ; aucun objet Mail ou fichier permanent dans le consensus.",
    col1Item4: "<strong>Arbre de contenu chiffré :</strong> chunks ROOT/INDEX/DATA ordonnés, adressés par BLAKE3-256 et construits pour le traitement en flux.",
    col1Item5: "<strong>Admission et transport :</strong> stockage local de chunks, preuves d’inclusion liées aux publications finalisées et transport CYP2 sécurisé en TLS 1.3.",
    col1Item6: "<strong>Économie déterministe :</strong> frais et transitions de solde validés par le state machine ; DEV, Beta et Mainnet ont des paramètres distincts.",

    col2Title: "Durcissement et préparation Beta",
    badgeWip: "EN COURS",
    col2Item1: "<strong>Mail intégré :</strong> publications, pièces jointes et reconstruction Inbox/Sent ; acceptation desktop à compléter.",
    col2Item2: "<strong>Files intégré :</strong> catalogue privé, transferts et restauration ; installations propres et UX à vérifier.",
    col2Item3: "<strong>Durabilité :</strong> réplication, audit et réparation intégrés. DEV : 1 copie distante ; Beta : 2 indépendantes. Le cache local ne compte pas.",
    col2Item4: "<strong>Preuves :</strong> tests de restauration propre et de panne multiprocessus ; soak prolongé et acceptation Beta à compléter.",

    col3Title: "Étapes ultérieures",
    badgePlanned: "PLUS TARD",
    col3Item1: "<strong>Validation facultative :</strong> avis signé, vérifié et évalué localement. Aucun effet canonique ni pouvoir PoA.",
    col3Item2: "<strong>Montée en charge :</strong> tests prolongés des pièces jointes et du stockage partagé déjà intégrés.",
    col3Item3: "<strong>Sauvegarde :</strong> application post-Beta du même graphe chiffré, avec restauration vérifiable.",
    col3Item4: "<strong>Beta puis service public :</strong> coûts opérationnels mesurés, revue de sécurité, critères UX et exploitation documentée avant ouverture.",

    faqLabel: "Questions fréquentes",
    faqTitle: "Comprendre CYBOU.",
    faqDesc: "Ce qui fonctionne sur DEV, ce qui reste à construire et les garanties de sécurité.",
    faqQ1: "Qu’est-ce que CYBOU en termes simples ?",
    faqA1: "CYBOU est une suite logicielle souveraine réunissant messagerie privée (Mail), stockage de fichiers chiffré (Files) et gestionnaire d'identité (Identity). Vos données sont chiffrées sur votre ordinateur avant tout envoi et tout le transport réseau est protégé en TLS 1.3.",
    faqTlsBadge: "Transport Chiffré",
    faqQTls: "En quoi le transport chiffré en TLS 1.3 protège-t-il mes connexions sur Wi-Fi public ou chez mon FAI ?",
    faqATls: "Toutes les communications entre pairs et nœuds CYBOU (protocole CYP2 v3) sont encapsulées dans un tunnel TLS 1.3 strict. Que vous utilisiez le Wi-Fi ouvert d'une gare, d'un café ou votre box internet à domicile, aucun tiers ni votre fournisseur d'accès (Orange, SFR, Free, Bouygues) ne peut intercepter, lire vos messages ou savoir quels fichiers vous échangez. L'inspection approfondie des paquets (DPI) est totalement inopérante.",
    faqQ2: "En quoi CYBOU diffère-t-il de Gmail ou Google Drive ?",
    faqA2: "Contrairement à Google qui centralise vos données, possède les clés et scanne les contenus pour la publicité ou l'entraînement d'IA, CYBOU chiffre tout sur votre ordinateur. Aucun serveur ne peut lire vos emails ou fichiers, et tout le réseau public est localisé en France, à l'abri du Cloud Act américain.",
    faqQ3: "À quoi servent un nom .cybou et la phrase de récupération de 24 mots ?",
    faqA3: "Un nom .cybou (ex: alice.cybou) est votre identifiant lisible inaliénable. La phrase de 24 mots est votre clé secrète maîtresse : elle vous permet de recréer instantanément votre environnement et vos clés sur n'importe quel ordinateur, sans jamais dépendre d'un mot de passe ou d'un numéro de mobile piratable.",
    faqQFrance: "Pourquoi CYBOU est-il hébergé exclusivement en France et en Europe ?",
    faqAFrance: "La souveraineté numérique exige une étanchéité juridique totale. En restreignant les nœuds publics au territoire français, CYBOU garantit que vos flux de données et vos fragments de stockage ne transitent jamais sous le contrôle de lois extraterritoriales étrangères comme le US Cloud Act ou la FISA 702.",
    faqPqBadge: "Sécurité Future",
    faqQ5: "Que signifie la cryptographie post-quantique (ML-DSA) ?",
    faqA5: "Les ordinateurs quantiques rendront obsolètes les signatures RSA et ECC actuelles. CYBOU utilise dès aujourd'hui les algorithmes post-quantiques récemment normalisés par le NIST (ML-DSA-44 et ML-DSA-65) couplés à Ed25519 pour protéger vos correspondances contre l'enregistrement malveillant et le déchiffrement rétroactif futur.",
    faqQ7: "Puis-je utiliser CYBOU aujourd’hui ?",
    faqA7: "Le réseau DEV et le code source complet sont actifs et consultables sur GitHub. L'application CYBOU Desktop est actuellement en cours de durcissement et d'intégration continue avant sa distribution publique pour la phase Beta.",

    footBrand: "L'alternative souveraine pour vos communications et fichiers privés.<br>Conçu et développé en France.",
    footProd: "Produit",
    footProdDesktop: "CYBOU Desktop",
    footProdMail: "CYBOU Mail",
    footProdFiles: "CYBOU Files",
    footProdIdentity: "CYBOU Identity",
    footSec: "Sécurité & Souveraineté",
    footSecTls: "Transport 100% TLS 1.3",
    footSecE2ee: "Chiffrement client intégral",
    footSecFrance: "Réseau souverain France",
    footSecPq: "Résistance Post-Quantique",
    footRes: "Ressources",
    footResGithub: "Code source (GitHub)",
    footResTech: "Sous le capot (Technique)",
    footResFaq: "Foire aux questions",
    footResLicense: "Licence open source",
    footBottomBadge: "Recherche & Souveraineté Numérique Européenne"
  },

  en: {
    navProducts: "Products",
    navShowcase: "Preview",
    navSecurity: "Security & TLS",
    navComparison: "Comparison",
    navArch: "Under the Hood",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "100% TLS 1.3 Encrypted • France Sovereign • Post-Quantum",
    heroAccent: "Your sovereign private workspace. Mail, files, and identity.",
    heroSubtitle: "CYBOU brings private messaging, file storage, and sovereign identity together into a protective desktop application. Your data is encrypted on your computer before leaving, and all network traffic is secured with TLS 1.3.",
    heroPillSlogan: "A sovereign suite protecting your privacy — without ad tracking or surveillance.",

    statusCalloutTitle: "DEV protocol operational. Hardening for public release.",
    statusCalloutBody: "The experimental DEV network operates with PoA finality, distributed chunk storage, and TLS 1.3 transport. The CYBOU desktop application is undergoing continuous hardening and integration tests ahead of public release.",

    heroBtnShowcase: "Explore the Desktop App",
    heroBtnSecurity: "Why CYBOU?",

    badgeTls: "100% TLS 1.3 Transport (Anti Wi-Fi/ISP interception)",
    badgeFrance: "France Sovereign Network (Immune to US Cloud Act)",
    badgePhrase: "Zero Passwords / 24-Word Master Key",
    badgePq: "NIST Post-Quantum Shield",
    badgeNoAds: "Zero Tracking & Zero Ads",

    showcaseLabel: "Application Preview",
    showcaseTitle: "The CYBOU Desktop Experience.",
    showcaseDesc: "A unified and intuitive interface bringing your communications, files, and identity together under continuous cryptographic protection.",
    mockupStatusTls: "Connected via TLS 1.3",
    mockupNetworkFr: "France sovereign node",
    mockupTabMail: "Mail",
    mockupTabFiles: "Files",
    mockupTabId: "Identity",
    mockupTabSec: "Security & Network",

    mockupMailFrom1: "Pierre Martin",
    mockupMailSub1: "Confidential Documents 2026",
    mockupMailSnip1: "Hello Alice, here are the encrypted files you requested...",
    mockupBadgeE2ee: "Client Encrypted • TLS 1.3",
    mockupMailFrom2: "Legal Advisory",
    mockupMailSub2: "Signed Partnership Agreement",
    mockupMailSnip2: "The document has been validated and sealed with hybrid signature...",
    mockupBadgePq: "ML-DSA Signature",
    mockupMailBodyTitle: "Tax & Accounting Records 2026",
    mockupBadgeTlsPill: "TLS 1.3 Certified Transport",
    mockupMailBodyText: "Hello Alice,<br><br>Here are the encrypted attachments for our annual report. Thanks to the CYBOU protocol, this transmission traveled directly over TLS 1.3 encrypted links. No third-party server, no ISP, and no advertising bot could read a single word of our exchange.<br><br>Best regards,<br>Pierre",

    mockupFilesColName: "File Name",
    mockupFilesColSize: "Size",
    mockupFilesColReplicas: "Replication",
    mockupFilesColStatus: "Status",
    mockupProtected: "Protected in France",

    mockupIdHeading1: "Active Identity",
    mockupIdHeading2: "Post-Quantum Keys",
    mockupIdHeading3: "Account Recovery",
    mockupIdHeading4: "Data Sovereignty",

    mockupSecHeading1: "Transport Encryption (CYP2)",
    mockupSecDesc1: "Strict TLS 1.3 tunnel with modern cipher negotiation. Eliminates any risk of public Wi-Fi snooping or ISP Deep Packet Inspection (DPI).",
    mockupSecHeading2: "Network Admission Policy",
    mockupSecDesc2: "Public P2P traffic strictly restricted to French IP space using local geolocation. Fails closed on uncertainty.",

    servicesLabel: "Integrated Services",
    servicesTitle: "Three Pillars for Your Digital Independence.",
    servicesDesc: "Each tool runs directly on your computer without dependence on Big Tech.",

    srvMailTag: "Messaging",
    srvMailTitle: "CYBOU Mail — Your Inviolable Inbox",
    srvMailDesc: "Never let Google or Microsoft scan your private correspondence again. Messages and attachments are encrypted on your device with post-quantum keys and transmitted over TLS 1.3. Zero ads, zero profiling.",
    srvMailStatus: "Integrated flows — Beta acceptance pending",

    srvFilesTag: "Storage",
    srvFilesTitle: "CYBOU Files — Your Documents Under Lock in France",
    srvFilesDesc: "A sovereign personal cloud immune to the US Cloud Act. Files are sliced into encrypted chunks (BLAKE3-256), replicated and automatically audited across independent nodes in France.",
    srvFilesStatus: "Integrated flows — sustained soak pending",

    srvIdentityTag: "Identity",
    srvIdentityTitle: "CYBOU Identity — One Master Key for Digital Life",
    srvIdentityDesc: "Forget vulnerable passwords and hackable SMS codes. Your account is secured by an inviolable 24-word recovery phrase and a human-readable .cybou name you own for life.",
    srvIdentityStatus: "Identity and vault path integrated on DEV",

    srvContinuityTag: "Continuity",
    srvContinuityTitle: "Seamless Recovery & Continuity",
    srvContinuityDesc: "If your computer is lost or replaced, your 24-word phrase and finalized proofs allow you to reconstruct your emails, files, and address book securely.",
    srvContinuityStatus: "Integrated reconstruction on DEV",

    srvWalletTag: "Economics",
    srvWalletTitle: "Transparent Service Budget",
    srvWalletDesc: "A deterministic and predictable economic model to fund network storage and security without hidden subscriptions or data monetization.",
    srvWalletStatus: "Basic operations available in the DEV client",

    secLabel: "Security & Transport",
    secTitle: "An End-to-End Digital Fortress.",
    secDesc: "From your local Wi-Fi connection down to file storage, discover why your data is truly impenetrable.",

    secTlsTag: "Network Protection",
    secTlsTitle: "100% TLS 1.3 Encrypted Transport (CYP2 Protocol)",
    secTlsDesc: "Whether you are on public Wi-Fi at a train station or hotel, or on your home fiber connection, every link between your app and the network is sealed by strict TLS 1.3. Neither your ISP nor an attacker on the local network can inspect what you transfer or alter a single byte.",

    secE2eeTag: "Zero-Knowledge Privacy",
    secE2eeTitle: "End-to-End Client Encryption",
    secE2eeDesc: "TLS protects the wire, but CYBOU also protects the data itself. Messages and files are encrypted on your CPU with your private keys before departure. Even network relay nodes see only opaque, indecipherable bytes.",

    secFranceTag: "Legal Immunity",
    secFranceTitle: "Territorial Sovereignty France & EU",
    secFranceDesc: "Zero servers in the US, zero dependence on Silicon Valley giants. Public P2P admission is strictly confined to France with rigorous geolocation enforcement (fails closed). Your data is fully immune to the US Cloud Act and FISA 702.",

    secPqTag: "Future Proof",
    secPqTitle: "NIST Post-Quantum Shield (ML-DSA)",
    secPqDesc: "Future quantum computers will break traditional encryption (RSA, elliptic curves). CYBOU integrates NIST-standardized post-quantum algorithms (ML-DSA-44 and ML-DSA-65) today to ensure your security for decades to come.",

    compLabel: "Objective Comparison",
    compTitle: "CYBOU vs Big Tech.",
    compDesc: "See at a glance what changes when switching from centralized services to a sovereign suite.",
    compThCriteria: "Protection Feature",
    compThCybou: "CYBOU Desktop",
    compThGoogle: "Google (Gmail / Drive)",
    compThAppleMs: "Apple iCloud / Microsoft",
    compRow1Crit: "Message & File Encryption",
    compRow1Cybou: "Mandatory Client-side (Zero-Knowledge)",
    compRow1Google: "Server-side encryption (Google holds keys)",
    compRow1AppleMs: "Server encryption with vendor-held keys",
    compRow2Crit: "Network Transport Security",
    compRow2Cybou: "Strict end-to-end TLS 1.3 tunnel (CYP2)",
    compRow2Google: "TLS to Google datacenters",
    compRow2AppleMs: "TLS to US datacenters",
    compRow3Crit: "Content scanning for ads / AI",
    compRow3Cybou: "Zero scanning, zero ads, zero AI training",
    compRow3Google: "Scanned for targeting and algorithmic training",
    compRow3AppleMs: "Automated scans and telemetry",
    compRow4Crit: "Jurisdiction & Governance",
    compRow4Cybou: "France & European Union (Sovereign GDPR)",
    compRow4Google: "USA (Subject to US Cloud Act & FISA 702)",
    compRow4AppleMs: "USA (Subject to extraterritorial US laws)",
    compRow5Crit: "Quantum Computer Resistance",
    compRow5Cybou: "Integrated NIST ML-DSA post-quantum",
    compRow5Google: "Classical algorithms (RSA / ECC)",
    compRow5AppleMs: "Classical algorithms (RSA / ECC)",
    compRow6Crit: "Authentication & Account Ownership",
    compRow6Cybou: "Sovereign 24 words (No passwords / SMS)",
    compRow6Google: "Phone number required, risk of arbitrary ban",
    compRow6AppleMs: "Tied to phone number and credit card",

    archLabel: "Under the Hood",
    archTitle: "Technical Architecture & Foundations.",
    archDesc: "For developers, auditors, and enthusiasts: exact consensus engine and network specifications.",

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

    step1Title: "Prepare private content",
    step1Desc: "The client turns content into an encrypted ROOT/INDEX/DATA tree and keeps the required local material. Mail/Files schemas remain inside encryption.",

    step2Title: "Protect the content key",
    step2Desc: "RootPublication carries key capsules for recipient KEM capabilities without publishing recipient AccountIDs. X-Wing draft -05 is limited to DEV.",

    step3Title: "Authorize a RootPublication",
    step3Desc: "Identity signs a generic operation committing to the root, chunk-inclusion tree, and capsules. Mail has no dedicated consensus operation type.",

    step4Title: "Finalize, admit, retrieve",
    step4Desc: "PoA finalizes the operation; providers then verify chunk-admission proofs. Finality is not durability: the client must measure availability, retry, and rebuild local indexes.",

    factsheetHeading: "Canonical CYBOU Architecture Parameters",
    dtTransport: "P2P Transport & Encryption",
    ddTransport: "CYP2 encapsulated in strict TLS 1.3. Ephemeral SPKI pinning, zero transport metadata leakage to ISPs or third parties.",
    dtConsensus: "Current DEV finality",
    ddConsensus: "Hybrid PoA with one signer operated by CYBOU on DEV. Full nodes independently verify; no BFT claim. Target: Central Authority desktop finalizer.",
    dtBootstrap: "1–4 bootstrap Identities",
    ddBootstrap: "1–4 genesis-authorized Identities (AccountID + RecoveryKeyID) for rendezvous and relay; no finality, quorum, or voting role. Unified full-node software.",
    dtAdmission: "France sovereign P2P admission",
    ddAdmission: "Public P2P admission restricted to French IP space using local Geo data (fails closed); optional local VPN/proxy/Tor filtering.",
    dtSupply: "Maximum supply",
    ddSupply: "<code>100,000,000,000</code> CYBOU, 0 decimals; parameters are bound to each network definition.",
    dtGrant: "Account creation",
    ddGrant: "Permissionless <code>AccountCreate</code> with anti-Sybil work; the bonus moves from <code>OnboardingPool</code> to <code>SystemBalance</code>.",
    dtFees: "Fees and allocation",
    ddFees: "Deterministic publication fees based on size and chunk count; priority fees disabled; 3 Security and 1 Onboarding unit per 4-unit allocation.",
    dtPublication: "Content publication",
    ddPublication: "Generic <code>RootPublication</code>. Mail, Files, and Backup are private encrypted schemas, not separate consensus operations.",
    dtKeys: "Cryptographic key roles",
    ddKeys: "Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing, and Treasury are separate roles; no classical-only production signature.",
    dtStack: "Technology stack",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL, BLAKE3, and CYP2 transport over TLS 1.3.",

    matrixLabel: "Technical Transparency",
    matrixTitle: "Implementation Matrix.",
    matrixDesc: "Mail/Files and storage are integrated on DEV. Sustained soak, clean installations, UX acceptance and security review remain Beta gates.",

    col1Title: "Protocol substrate on DEV",
    badgeDone: "IMPLEMENTED",
    col1Item1: "<strong>Identity and .cybou names:</strong> AccountCreate with anti-Sybil work, hybrid key roles, portable vault, and finalized name registry.",
    col1Item2: "<strong>PoA finality:</strong> dedicated DEV signer, durable anti-equivocation journal, conflict safety halt, and independent full-node validation.",
    col1Item3: "<strong>RootPublication:</strong> generic Identity-authorized operation; no permanent Mail or file objects in consensus state.",
    col1Item4: "<strong>Encrypted content tree:</strong> ordered ROOT/INDEX/DATA chunks addressed by full BLAKE3-256 and built for streaming.",
    col1Item5: "<strong>Admission and transport:</strong> local chunk store, inclusion proofs tied to finalized publications, and CYP2 transport secured with TLS 1.3.",
    col1Item6: "<strong>Deterministic economics:</strong> fee and balance transitions validated by the state machine; DEV, Beta, and Mainnet use separate parameters.",

    col2Title: "Hardening and Beta preparation",
    badgeWip: "IN PROGRESS",
    col2Item1: "<strong>Integrated Mail:</strong> publications, attachments and Inbox/Sent rebuild; desktop acceptance remains open.",
    col2Item2: "<strong>Integrated Files:</strong> private catalog, transfers and restore; clean-install and UX acceptance remain open.",
    col2Item3: "<strong>Durability:</strong> replication, audit and repair are integrated. DEV: 1 remote copy; Beta: 2 independent copies. Local cache does not count.",
    col2Item4: "<strong>Evidence:</strong> clean-restore and multi-process failure tests; sustained soak and Beta acceptance remain open.",

    col3Title: "Later stages",
    badgePlanned: "LATER",
    col3Item1: "<strong>Optional Validation:</strong> signed advisory evidence that each recipient verifies and assesses locally. It has no canonical effects or PoA power.",
    col3Item2: "<strong>Scale:</strong> sustained testing of integrated attachments and shared storage.",
    col3Item3: "<strong>Backup:</strong> post-Beta application of the same encrypted graph, with verifiable restore.",
    col3Item4: "<strong>Beta, then public service:</strong> measured operating costs, security review, UX acceptance, and documented operations before opening.",

    faqLabel: "Frequently Asked Questions",
    faqTitle: "Understanding CYBOU.",
    faqDesc: "What runs on DEV, what still needs to be built, and security guarantees.",
    faqQ1: "What is CYBOU in simple terms?",
    faqA1: "CYBOU is a sovereign desktop privacy suite combining private messaging (Mail), encrypted cloud storage (Files), and an identity manager (Identity). Your data is encrypted on your machine before being sent, and all network transport is protected with TLS 1.3.",
    faqTlsBadge: "Encrypted Transport",
    faqQTls: "How does TLS 1.3 transport encryption protect my connections on public Wi-Fi or with my ISP?",
    faqATls: "All communications between CYBOU peers and nodes (CYP2 protocol v3) are sealed inside a strict TLS 1.3 tunnel. Whether you use public Wi-Fi at a train station or hotel, or your home fiber internet, no third party or ISP can intercept or read your messages or know what files you transfer. Deep Packet Inspection (DPI) is completely blocked.",
    faqQ2: "How is CYBOU different from Gmail or Google Drive?",
    faqA2: "Unlike Google which centralizes your data, holds the decryption keys, and scans contents for advertising or AI model training, CYBOU encrypts everything directly on your computer. No server can read your emails or files, and the entire public network is based in France, shielded from the US Cloud Act.",
    faqQ3: "What do .cybou names and the 24-word recovery phrase do?",
    faqA3: "A .cybou name (e.g. alice.cybou) is your permanent, human-readable account alias. The 24-word recovery phrase is your master secret key: it lets you reconstruct your keys and environment on any computer without relying on vulnerable passwords or hackable SMS codes.",
    faqQFrance: "Why is CYBOU hosted exclusively in France and Europe?",
    faqAFrance: "True digital sovereignty requires strict legal immunity. By restricting public nodes to French territory, CYBOU ensures that data streams and storage chunks are never subject to foreign extraterritorial surveillance laws such as the US Cloud Act or FISA 702.",
    faqPqBadge: "Future Security",
    faqQ5: "What does post-quantum cryptography (ML-DSA) mean?",
    faqA5: "Future quantum computers will render classical RSA and ECC encryption obsolete. CYBOU deploys NIST-standardized post-quantum algorithms (ML-DSA-44 and ML-DSA-65) alongside Ed25519 today to protect your correspondence against harvest-now, decrypt-later attacks.",
    faqQ7: "Can I use CYBOU today?",
    faqA7: "The DEV network and full open-source codebase are active and verifiable on GitHub. The CYBOU Desktop application is undergoing continuous integration and hardening ahead of public distribution for the Beta phase.",

    footBrand: "The sovereign alternative for your private communications and files.<br>Designed and engineered in France.",
    footProd: "Product",
    footProdDesktop: "CYBOU Desktop",
    footProdMail: "CYBOU Mail",
    footProdFiles: "CYBOU Files",
    footProdIdentity: "CYBOU Identity",
    footSec: "Security & Sovereignty",
    footSecTls: "100% TLS 1.3 Transport",
    footSecE2ee: "Full Client Encryption",
    footSecFrance: "France Sovereign Network",
    footSecPq: "Post-Quantum Cryptography",
    footRes: "Resources",
    footResGithub: "Source Code (GitHub)",
    footResTech: "Under the Hood (Technical)",
    footResFaq: "Frequently Asked Questions",
    footResLicense: "Open Source License",
    footBottomBadge: "European Digital Sovereignty & Research"
  }
};

let currentLang = 'fr';

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

// --- Interactive Desktop App Mockup Tabs ---
function initMockupTabs() {
  const tabButtons = document.querySelectorAll('.mockup-tab-btn');
  const views = {
    mail: document.getElementById('view-mail'),
    files: document.getElementById('view-files'),
    identity: document.getElementById('view-identity'),
    security: document.getElementById('view-security')
  };

  tabButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      const targetTab = btn.getAttribute('data-mockup-tab');
      if (!targetTab || !views[targetTab]) return;

      tabButtons.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      Object.values(views).forEach(v => {
        if (v) v.classList.remove('active');
      });
      views[targetTab].classList.add('active');
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
