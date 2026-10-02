/* ==========================================================================
   CYBOU.FR — Sovereign Desktop Suite & Dynamic Localization Engine
   Engineered for clear user-facing product presentation & responsive state verification
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initLanguageSwitch();
  initMobileMenu();
  initFaqAccordion();
  initMockupTabs();
  initHeroParticles();
  initComparisonFilter();
  checkUrlLanguage();
});

// --- 1. Language Toggle (French / English) ---
const translations = {
  fr: {
    navProducts: "Produits",
    navShowcase: "Aperçu",
    navEnterprise: "Entreprise",
    navSecurity: "Sécurité & TLS",
    navCompliance: "Conformité",
    navComparison: "Comparatif",
    navArch: "Sous le capot",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Cryptographie standard OpenSSL v3.5.2+ • Souveraineté France • Post-Quantique NIST",
    heroAccent: "Votre espace privé souverain. Messagerie, fichiers et identité.",
    heroSubtitle: "CYBOU réunit messagerie privée, stockage de fichiers et identité souveraine dans une application de bureau protectrice. Zéro algorithme maison : toute la cryptographie repose sur OpenSSL v3.5.2+ et les standards post-quantiques du NIST.",
    heroPillSlogan: "Une suite souveraine qui protège votre vie privée — sans compromis publicitaire ni surveillance.",

    statusCalloutTitle: "Protocole DEV opérationnel. Préparation de la version publique.",
    statusCalloutBody: "Le réseau expérimental DEV fonctionne avec finalité PoA, stockage distribué et transport TLS 1.3 post-quantique. L'application de bureau CYBOU fait l'objet de tests continus de durcissement et d'intégration avant ouverture au grand public.",

    heroBtnShowcase: "Découvrir l'application",
    heroBtnSecurity: "Pourquoi CYBOU ?",

    badgeOpenssl: "Moteur OpenSSL v3.5.2+ (Zéro crypto maison)",
    badgeFrance: "Souveraineté France (Hors Cloud Act US)",
    badgeTls: "Transport TLS 1.3 Post-Quantique (CYP2)",
    badgePhrase: "Zéro Mot de passe / Clé 24 mots",
    badgeNoAds: "Zéro Collecte & Zéro Publicité",

    showcaseLabel: "Aperçu de l'application",
    showcaseTitle: "L'expérience CYBOU Desktop.",
    showcaseDesc: "Une interface unifiée et intuitive qui réunit vos communications, vos fichiers et votre identité sous une protection cryptographique continue.",
    mockupStatus: "Connecté • Réseau France (2 répliques)",
    mockupNetworkFr: "Nœud souverain France",
    mockupTabMail: "Mail",
    mockupTabFiles: "Files",
    mockupTabId: "Identité",
    mockupTabSec: "Sécurité & Réseau",
    mockupTabSecBadge: "12 pairs FR",

    mockupMailFrom1: "Pierre Martin",
    mockupMailSub1: "Documents d'audit confidentiels 2026",
    mockupMailSnip1: "Bonjour Alice, voici les documents chiffrés demandés...",
    mockupBadgeE2ee: "Chiffré E2EE • Protégé",
    mockupMailFrom2: "Cabinet Juridique",
    mockupMailSub2: "Contrat de partenariat signé",
    mockupMailSnip2: "Le document a été validé et scellé avec signature hybride...",
    mockupBadgePq: "Signature ML-DSA-44",
    mockupMailBodyTitle: "Documents d'audit confidentiels 2026",
    mockupReaderBadge1: "Chiffrement client E2EE",
    mockupReaderBadge2: "Signé ML-DSA-44 • Vérifié",
    mockupReaderBadge3: "Stocké en France (2 répliques)",
    mockupMailBodyText: "Bonjour Alice,<br><br>Voici les pièces jointes chiffrées de notre audit annuel. Tout a été scellé directement depuis mon poste avec notre clé d'identité souveraine. Les fragments sont répliqués sur les nœuds souverains français et restent inaccessibles à tout tiers.<br><br>Bien cordialement,<br>Pierre",

    mockupFilesColName: "Fichier",
    mockupFilesColSize: "Taille",
    mockupFilesColReplicas: "Réplication",
    mockupFilesColStatus: "État",
    mockupProtected: "Protégé en France",

    mockupIdHeading1: "Identité active",
    mockupIdSub1: "Compte souverain inaliénable enregistré par travail anti-Sybil.",
    mockupIdHeading2: "Clés Post-Quantiques",
    mockupIdSub2: "Signature hybride active contre attaques quantiques.",
    mockupIdHeading3: "Restauration de compte",
    mockupIdSub3: "Permet de recréer l'identité sur un nouveau PC sans mot de passe ni SMS.",
    mockupIdHeading4: "Moteur cryptographique standard",
    mockupIdSub4: "Standards officiels NIST FIPS 203/204. Zéro cryptographie maison.",

    mockupSecHeading1: "Cryptographie éprouvée OpenSSL v3.5.2+ (Zéro crypto maison)",
    mockupSecDesc1: "CYBOU applique la règle d'or de la sécurité : \"Don't roll your own crypto\". Aucun algorithme inventé : tout s'appuie sur la bibliothèque officielle OpenSSL v3.5.2+ conforme aux standards NIST FIPS 203 et FIPS 204.",
    mockupSecHeading2: "Transport P2P TLS 1.3 Post-Quantique",
    mockupSecDesc2: "Tunnel TLS 1.3 avec groupe hybride X25519MLKEM768. Tout repli classique non-PQ est rejeté (fail-closed).",
    mockupSecHeading3: "Souveraineté territoriale France",
    mockupSecDesc3: "Trafic P2P public restreint à l'espace IP français (fail-closed). Immunité contre le US Cloud Act et FISA 702.",

    servicesLabel: "Services intégrés",
    servicesTitle: "Trois piliers pour votre indépendance numérique.",
    servicesDesc: "Chaque outil fonctionne directement sur votre machine sans dépendre des géants de la tech.",

    srvMailTag: "Messagerie",
    srvMailTitle: "CYBOU Mail — Votre boîte de réception inviolable",
    srvMailDesc: "Ne laissez plus Google ou Microsoft analyser vos correspondances privées. Vos messages et pièces jointes sont chiffrés sur votre poste avec des clés post-quantiques et transmis via TLS 1.3 post-quantique. Zéro publicité, zéro profilage.",
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

    secOpensslTag: "Standard Mondial Audité",
    secOpensslTitle: "Cryptographie Standard OpenSSL v3.5.2+ (Zéro crypto maison)",
    secOpensslDesc: "La règle d'or en cybersécurité est formelle : ne jamais inventer sa propre cryptographie (« Don't roll your own crypto »). CYBOU n'utilise aucun algorithme expérimental fait maison. Tous les protocoles post-quantiques (ML-DSA-44, ML-DSA-65, ML-KEM-768) et symétriques sont fournis directement par la version officielle d'OpenSSL v3.5.2+, certifiée et conforme aux normes du NIST (FIPS 203 et FIPS 204).",

    secTlsTag: "Protection Réseau",
    secTlsTitle: "Transport 100% Chiffré en TLS 1.3 Post-Quantique (CYP2)",
    secTlsDesc: "Chaque connexion réseau impose un échange de clés hybride post-quantique combinant X25519 et le standard NIST ML-KEM-768 (Kyber). Tout repli classique sans ML-KEM est refusé net. Ni votre FAI, ni un pirate sur le Wi-Fi public, ni de futurs calculateurs quantiques ne peuvent intercepter vos échanges.",

    secE2eeTag: "Confidentialité Totale",
    secE2eeTitle: "Chiffrement Client de bout en bout & Arbre BLAKE3",
    secE2eeDesc: "Le TLS protège le transport, mais CYBOU protège aussi la donnée elle-même. Les messages et fichiers sont chiffrés sur votre processeur avec vos propres clés secrètes avant de partir. Le consensus de la chaîne n'enregistre aucun message, aucun nom de fichier, aucun destinataire : seuls des fragments chiffrés opaques existent.",

    secFranceTag: "Immunité Juridique",
    secFranceTitle: "Souveraineté Territoriale France & UE",
    secFranceDesc: "Zéro serveur aux États-Unis, zéro dépendance aux géants de la Silicon Valley. L'admission P2P publique est strictement limitée à la France avec contrôle géographique rigoureux (fail-closed). Vos données échappent totalement au US Cloud Act et à la FISA 702.",

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
    compRowCryptoCrit: "Moteur et code cryptographique",
    compRowCryptoCybou: "OpenSSL v3.5.2+ standard (Zéro crypto maison, NIST FIPS 203/204)",
    compRowCryptoGoogle: "Propriétaire / Boîte noire serveur",
    compRowCryptoAppleMs: "Propriétaire / Boîte noire serveur",
    compRow2Crit: "Sécurité du transport réseau",
    compRow2Cybou: "TLS 1.3 Hybride Post-Quantique (X25519 + ML-KEM-768)",
    compRow2Google: "TLS classique (Vulnérable à l'enregistrement et déchiffrement quantique)",
    compRow2AppleMs: "TLS classique (Algorithmes traditionnels vulnérables)",
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
    ddTransport: "CYP2 encapsulé dans TLS 1.3 post-quantique strict (groupe X25519MLKEM768 obligatoire, fail-closed). Moteur OpenSSL v3.5.2+ avec export de clé de session 32 octets liée aux preuves cryptographiques.",
    dtConsensus: "Finalité actuelle sur DEV",
    ddConsensus: "PoA hybride à un signataire exploité par CYBOU sur DEV. Les nœuds complets vérifient indépendamment ; pas de revendication BFT. Cible : PoA sur poste Central Authority.",
    dtBootstrap: "Pairs d'amorce (Bootstrap)",
    ddBootstrap: "Nœuds CYBOU ordinaires avec points de contact initiaux connus (IP:port + épingle TLS) pour la découverte initiale des pairs ; même logiciel complet, aucun rôle de finalité ni statut de consensus privilégié.",
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
    ddKeys: "Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing et Treasury sont des rôles séparés ; moteur OpenSSL v3.5.2+ certifié.",
    dtStack: "Socle technique",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL v3.5.2+, BLAKE3 et transport CYP2 sur TLS 1.3.",

    matrixLabel: "Transparence technique",
    matrixTitle: "Matrice d'implémentation.",
    matrixDesc: "Mail/Files et le stockage sont intégrés sur DEV. Les tests prolongés, installations propres, critères UX et revue de sécurité restent les étapes Beta.",

    col1Title: "Socle implémenté sur DEV",
    badgeDone: "IMPLÉMENTÉ",
    col1Item1: "<strong>Identity et noms .cybou :</strong> AccountCreate avec travail anti-Sybil, rôles de clés hybrides, coffre portable et registre de noms finalisé.",
    col1Item2: "<strong>Finalité PoA :</strong> un signataire DEV dédié, journal anti-équivocation durable, arrêt de sécurité en cas de conflit et validation indépendante par les nœuds complets.",
    col1Item3: "<strong>RootPublication :</strong> opération générique autorisée par Identity ; aucun objet Mail ou fichier permanent dans le consensus.",
    col1Item4: "<strong>Arbre de contenu chiffré :</strong> chunks ROOT/INDEX/DATA ordonnés, adressés par BLAKE3-256 et construits pour le traitement en flux.",
    col1Item5: "<strong>Admission et transport :</strong> stockage local de chunks, preuves d’inclusion liées aux publications finalisées et transport CYP2 sécurisé en TLS 1.3 post-quantique.",
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
    faqA1: "CYBOU est une suite logicielle souveraine réunissant messagerie privée (Mail), stockage de fichiers chiffré (Files) et gestionnaire d'identité (Identity). Vos données sont chiffrées sur votre ordinateur avant tout envoi et tout le transport réseau est protégé en TLS 1.3 post-quantique.",
    faqTlsBadge: "Transport Post-Quantique",
    faqQTls: "En quoi le transport TLS 1.3 de CYBOU est-il post-quantique et que protège-t-il ?",
    faqATls: "Toutes les communications entre pairs et nœuds CYBOU (protocole CYP2 v3) utilisent un handshake TLS 1.3 avec le groupe d'échange de clés X25519MLKEM768. Cette négociation associe la cryptographie classique à ML-KEM-768 (standard NIST FIPS 203). Si un pair ne supporte pas ce mode post-quantique, la connexion est immédiatement interrompue. Cela protège vos transferts contre l'interception locale (Wi-Fi public, FAI) et contre la stratégie d'espionnage « Récolter maintenant, déchiffrer plus tard ».",
    faqOpensslBadge: "Standard Audité",
    faqQOpenssl: "Les algorithmes de CYBOU sont-ils développés en interne (« crypto maison ») ?",
    faqAOpenssl: "Absolument pas. La règle d'or de la sécurité est formelle : « Don't roll your own crypto » (ne réinventez jamais la cryptographie). CYBOU s'appuie exclusivement sur la bibliothèque officielle OpenSSL v3.5.2+, internationalement auditée et éprouvée. Les primitives post-quantiques intégrées sont les standards officiels du NIST : ML-KEM-768 (FIPS 203) et ML-DSA-44/65 (FIPS 204). Vous bénéficiez ainsi d'une robustesse mathématique vérifiable, sans aucune boîte noire ni algorithme expérimental fait maison.",
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
    footSecTls: "Transport TLS 1.3 Post-Quantique",
    footSecE2ee: "Chiffrement client intégral",
    footSecFrance: "Réseau souverain France",
    footSecPq: "Résistance Post-Quantique",
    footRes: "Ressources",
    footResGithub: "Code source (GitHub)",
    footResTech: "Sous le capot (Technique)",
    footResFaq: "Foire aux questions",
    footResLicense: "Licence open source",
    footBottomBadge: "Recherche & Souveraineté Numérique Européenne",
    footSecCompliance: "Conformité RGPD / NIS 2",

    // Compliance Preview on Homepage
    compSecLabel: "Réglementation & Normes",
    compSecTitle: "Conformité rigoureuse aux standards européens et français.",
    compSecDesc: "Découvrez comment l'architecture Zero-Knowledge, la cryptographie OpenSSL v3.5.2+ et le réseau souverain France répondent point par point aux cadres légaux les plus exigeants.",
    compCardRgpdPill: "Règlement UE 2016/679",
    compCardRgpdStatus: "Privacy by Design",
    compCardRgpdTitle: "RGPD — Données personnelles sous protection absolue",
    compCardRgpdDesc: "Chiffrement client systématique (Art. 25). Zéro métadonnée personnelle dans le consensus. Droit à l'oubli cryptographique garanti et absence totale de transfert de données vers des pays tiers (Schrems II).",
    compCardNis2Pill: "Directive UE 2022/2555",
    compCardNis2Status: "Mesures Art. 21",
    compCardNis2Title: "Directive NIS 2 — Résilience & Chaîne d'Approvisionnement",
    compCardNis2Desc: "Moteur cryptographique audité OpenSSL v3.5.2+ (sécurité de la supply chain logicielle). Résilience opérationnelle sans point unique de panne, auto-réparation du stockage et politique stricte de divulgation des vulnérabilités.",
    compCardHdsPill: "CSP Art. L.1111-8",
    compCardHdsStatus: "Secret Médical & HDS",
    compCardHdsTitle: "Secteur Médical — Secret Médical & Données de Santé",
    compCardHdsDesc: "Garantie mathématique d'inviolabilité pour le secret professionnel (Art. 226-13 CP). En cas d'externalisation chez un tiers commercial, exigence stricte de conformité des centres d'hébergement à la certification HDS et SecNumCloud.",
    compCardIsoPill: "ISO/IEC 27001:2022",
    compCardIsoStatus: "DIC / CIA Triad",
    compCardIsoTitle: "ISO 27001 & Triade DIC — Sécurité de l'Information",
    compCardIsoDesc: "Couverture des contrôles clés de l'Annexe A (A.5.15, A.8.2, A.8.12, A.8.20, A.8.24). Respect sans compromis de la triade Disponibilité, Intégrité (BLAKE3 / Merkle) et Confidentialité (ML-KEM-768 / Zero-Knowledge).",
    compCtaTitle: "Consulter l'analyse technique et juridique exhaustive",
    compCtaDesc: "Articles détaillés, tableaux de correspondance ISO 27001, analyse d'impact RGPD et cadre réglementaire ANSSI.",
    compCtaBtn: "Accéder au dossier de conformité",

    // Dedicated Compliance Page (conformite.html)
    cBreadHome: "Accueil",
    cBreadCurrent: "Conformité Réglementaire & Standards",
    cHeroTag: "Rigueur Légale & Transparence Technique",
    cHeroTitle: "Dossier de Conformité Réglementaire & Normative",
    cHeroDesc: "Analyse technique et juridique de l'adéquation de l'architecture CYBOU aux cadres RGPD, Directive NIS 2, Données de Santé (HDS / Code de la santé publique), Secret Médical, ISO/IEC 27001 et recommandations de l'ANSSI.",
    cPillTransp: "Transparence & Rôle",
    cPillRgpd: "RGPD (UE 2016/679)",
    cPillNis2: "Directive NIS 2",
    cPillHds: "Santé & HDS",
    cPillIso: "ISO/IEC 27001:2022",
    cPillCia: "Triade DIC / CIA",
    cPillAnssi: "ANSSI & Souveraineté",

    cArt0Pill: "Fondement Juridique",
    cArt0Status: "Honnêteté Technique",
    cArt0Title: "Principe de Transparence : Architecture Logicielle vs. Certification d'Hébergement",
    cArt0Sub: "Une distinction honnête entre les garanties mathématiques du logiciel et les exigences légales pesant sur les infrastructures d'hébergement.",
    cArt0P1: "Dans l'industrie du logiciel et de la cybersécurité, de nombreux acteurs prétendent être « certifiés RGPD » ou « certifiés HDS » par le simple fait de chiffrer des données. <strong>Cette affirmation est juridiquement trompeuse.</strong> Les règlements européens et le droit français distinguent impérativement deux niveaux de conformité :",
    cArt0L1Title: "Le niveau logiciel et protocolaire (CYBOU) :",
    cArt0L2Title: "Le niveau d'hébergement physique et d'exploitation (Centres de données) :",
    cArt0BoxTitle: "Ce que cela implique concrètement :",
    cArt0BoxDesc: "Si un professionnel ou un établissement de santé utilise CYBOU pour stocker des dossiers patients chez un prestataire commercial tiers, les nœuds de stockage de destination (fournisseurs de stockage P2P) doivent être déployés dans des centres de données certifiés HDS. En revanche, en mode on-premise (auto-hébergement sur les serveurs internes de l'établissement ou du cabinet), le praticien conserve la pleine garde physique et bénéficie de la protection cryptographique post-quantique sans dépendance tierce.",

    cRgpdStatus: "Privacy by Design Intégral",
    cRgpdTitle: "RGPD — Protection des Données Personnelles dès la Conception",
    cRgpdSub: "Comment l'architecture Zero-Knowledge et le consensus aveugle de CYBOU éliminent structurellement les risques de non-conformité.",
    cRgpdP1: "Le RGPD impose aux responsables de traitement et sous-traitants des obligations drastiques de sécurité, de minimisation et de respect des droits des personnes. L'architecture de CYBOU a été pensée dès la première ligne de code pour satisfaire ces obligations de manière native :",
    cRgpdThArt: "Article RGPD",
    cRgpdThReq: "Exigence Légale",
    cRgpdThSol: "Implémentation Technique CYBOU",
    cRgpdArt25Req: "Garantir la protection des données dès la conception des systèmes et par défaut sans action complexe de l'utilisateur.",
    cRgpdArt25Sol: "<strong>Chiffrement client systématique.</strong> Aucun texte, fichier ou métadonnée ne quitte le poste de travail sans être préalablement chiffré par les clés secrètes locales de l'utilisateur.",
    cRgpdArt5Req: "Les données doivent être adéquates, pertinentes et limitées au strict nécessaire.",
    cRgpdArt5Sol: "<strong>Zéro donnée personnelle dans le consensus.</strong> Ni le registre PoA ni le réseau P2P ne connaissent les identités réelles, numéros de téléphone, adresses IP publiques persistantes, noms de fichiers ou objets de messages. Seuls des ChunkID opaques (BLAKE3-256) circulent.",
    cRgpdArt17Req: "Permettre l'effacement définitif et irréversible des données à caractère personnel.",
    cRgpdArt17Sol: "<strong>Destruction cryptographique (Crypto-shredding).</strong> La destruction de la clé symétrique locale contenue dans la capsule d'accès rend les fragments stockés mathématiquement indéchiffrables pour l'éternité, répondant pleinement aux critères de suppression du CEPD.",
    cRgpdArt20Req: "Permettre la récupération et le transfert des données dans un format ouvert et structuré.",
    cRgpdArt20Sol: "<strong>Restauration déterministe par phrase de 24 mots.</strong> L'utilisateur peut reconstruire l'ensemble de ses messages, fichiers et annuaires sur n'importe quel ordinateur grâce au standard ouvert de dérivation de clés.",
    cRgpdArt32Req: "Mettre en œuvre des mesures techniques appropriées, incluant le chiffrement et la résilience des systèmes.",
    cRgpdArt32Sol: "<strong>Cryptographie standard certifiée OpenSSL v3.5.2+.</strong> Algorithmes post-quantiques NIST FIPS 203 (ML-KEM-768), NIST FIPS 204 (ML-DSA-44/65), tunnels TLS 1.3 stricts et réplication de stockage multi-nœuds avec auto-réparation.",
    cRgpdArt44Req: "Interdiction de transférer des données vers des pays tiers sans garanties appropriées (invalidation Privacy Shield).",
    cRgpdArt44Sol: "<strong>Souveraineté territoriale stricte (France-only).</strong> L'admission P2P publique est filtrée par géolocalisation locale (fail-closed). Aucun serveur aux USA : immunité totale contre le US Cloud Act et la section 702 du FISA.",

    cNis2Status: "Mesures de Gestion des Risques",
    cNis2Title: "Directive NIS 2 — Résilience Opérationnelle & Chaîne d'Approvisionnement",
    cNis2Sub: "Répondre aux exigences strictes de cybersécurité pour les Entités Essentielles (EE) et Importantes (EI).",
    cNis2P1: "La directive européenne NIS 2 (en vigueur depuis 2024 et transposée dans les droits nationaux) impose des obligations strictes aux organisations dans 18 secteurs critiques (énergie, transports, santé, administration, infrastructures numériques). CYBOU s'aligne directement sur les prescriptions de l'Article 21 :",
    cNis2Item1Title: "Sécurité de la chaîne d'approvisionnement logicielle (Supply Chain Security) :",
    cNis2Item1Desc: "CYBOU applique la règle formelle « Don't roll your own crypto ». Aucun algorithme cryptographique fait maison n'est toléré. L'ensemble des couches symétriques, asymétriques et post-quantiques provient de la version officielle d'OpenSSL v3.5.2+, maintenue mondialement et soumise à des audits de sécurité continus.",
    cNis2Item2Title: "Anticipation de la menace quantique (ENISA & ANSSI) :",
    cNis2Item2Desc: "La menace des attaques « Harvest Now, Decrypt Later » (interception massive actuelle pour déchiffrement ultérieur par ordinateur quantique) est neutralisée par le chiffrement hybride TLS 1.3 avec échange de clés X25519MLKEM768 et signatures ML-DSA-44/65.",
    cNis2Item3Title: "Résilience, continuité d'activité et gestion des crises :",
    cNis2Item3Desc: "L'architecture de stockage distribué CYBOU élimine tout point individuel de panne (SPOF). En cas de sinistre ou de défaillance d'un serveur fournisseur, le protocole déclenche des réparations d'intégrité autonomes entre répliques indépendantes.",
    cNis2Item4Title: "Notification et traitement des incidents de sécurité (SECURITY.md) :",
    cNis2Item4Desc: "Le projet maintient une politique formelle de divulgation responsable des vulnérabilités (SECURITY.md) et se conforme par avance aux exigences de signalement rapide et de traçabilité du Cyber Resilience Act (CRA).",

    cHdsStatus: "Code de la santé publique L.1111-8",
    cHdsTitle: "Secteur Médical — Secret Professionnel, Données de Santé & HDS",
    cHdsSub: "Garanties techniques pour les professionnels de santé, laboratoires, cliniques et hôpitaux.",
    cHdsP1: "Les données médicales constituent la catégorie la plus sensible de données personnelles (« données de santé » au sens de l'art. 9 du RGPD). En France, leur traitement est encadré par le Code pénal et le Code de la santé publique.",
    cHdsH1: "1. Respect absolu du Secret Médical (Art. 226-13 du Code Pénal)",
    cHdsP2: "L'article 226-13 du Code pénal sanctionne la révélation d'une information à caractère secret par une personne qui en est dépositaire. En utilisant CYBOU Mail ou CYBOU Files, un médecin, un chirurgien ou un radiologue garantit qu'aucun intermédiaire technique ne peut matériellement commettre d'indiscrétion : les correspondances et imageries médicales sont chiffrées sur le poste de consultation avant tout envoi. Les clés privées ne sont jamais transmises à des tiers.",
    cHdsH2: "2. Hébergement de Données de Santé (HDS — Art. L.1111-8 du Code de la santé publique)",
    cHdsP3: "L'article L.1111-8 du CSP dispose que toute personne qui héberge des données de santé à caractère personnel recueillies à l'occasion d'activités de prévention, de diagnostic ou de soins doit être certifiée HDS. Voici la déclinaison opérationnelle claire de cette obligation avec CYBOU :",
    cHdsThMode: "Scénario d'Usage",
    cHdsThCadre: "Cadre Réglementaire",
    cHdsThCybou: "Conformité CYBOU",
    cHdsMode1Cadre: "Le praticien ou l'hôpital conserve les données sur ses propres serveurs et terminaux internes.",
    cHdsMode1Cybou: "<strong>Conforme sans certification tierce.</strong> Le responsable de traitement assure lui-même la garde des données. Chiffrement post-quantique et étanchéité totale du système d'information.",
    cHdsMode2Cadre: "Les données chiffrées sont confiées à des fournisseurs de stockage distants gérés par des tiers.",
    cHdsMode2Cybou: "<strong>Exigence d'infrastructure HDS.</strong> Les nœuds de stockage (cybou-provider) doivent être déployés dans des centres de données détenteurs du certificat HDS (délivré sous l'égide de l'ANS). Le chiffrement Zero-Knowledge de CYBOU constitue une mesure de sécurité supplémentaire de niveau supérieur.",
    cHdsBoxTitle: "Engagement d'intégrité pour le secteur médical :",
    cHdsBoxDesc: "CYBOU ne se substitue pas à la certification HDS des exploitants de datacenters. En revanche, CYBOU fournit la brique logicielle la plus sûre du marché pour immuniser les données médicales contre les cyberattaques par ransomware, l'espionnage extraterritorial et les fuites de données d'infogérance.",

    cIsoStatus: "Annexe A — Contrôles de Sécurité",
    cIsoTitle: "ISO/IEC 27001:2022 — Matrice de Correspondance des Contrôles",
    cIsoSub: "Comment l'architecture technique de CYBOU aide les entreprises à satisfaire leurs exigences de Système de Management de la Sécurité de l'Information (SMSI).",
    cIsoP1: "La norme internationale ISO/IEC 27001 définit les bonnes pratiques pour protéger les actifs d'information. Le tableau ci-dessous explicite la contribution directe de CYBOU aux contrôles opérationnels de l'Annexe A (révision 2022) :",
    cIsoThCode: "Contrôle Annexe A",
    cIsoThName: "Intitulé de la Mesure",
    cIsoThSol: "Mise en Œuvre par CYBOU",
    cIsoA515Req: "Contrôle d'accès et authentification",
    cIsoA515Sol: "Authentification forte sans mot de passe centralisé. Signature hybride ML-DSA-44 et Ed25519 pour chaque transaction d'identité souveraine.",
    cIsoA82Req: "Gestion des droits d'accès privilégiés",
    cIsoA82Sol: "Modèle Zero-Knowledge : aucun administrateur réseau ni opérateur PoA ne dispose d'un pouvoir technique de déchiffrement des capsules utilisateur.",
    cIsoA812Req: "Prévention des fuites de données (DLP)",
    cIsoA812Sol: "Chiffrement client systématique avant toute transmission réseau. Découpage en fragments BLAKE3-256 opaques interdisant la reconstruction hors clé privée.",
    cIsoA814Req: "Redondance des moyens de traitement",
    cIsoA814Sol: "Multi-réplication automatique sur des nœuds de stockage géographiquement distincts avec vérification continue de disponibilité.",
    cIsoA820Req: "Sécurité des réseaux et cloisonnement",
    cIsoA820Sol: "Protocole P2P encapsulé en TLS 1.3 avec négociation hybride X25519MLKEM768 et épinglage SPKI strict. Filtrage géographique France-only.",
    cIsoA824Req: "Utilisation de la cryptographie et gestion des clés",
    cIsoA824Sol: "Génération et rotation atomique de clés (IdentityRotate), chiffrement standard NIST FIPS 203/204 via OpenSSL v3.5.2+, phrase de récupération 24 mots protégée.",

    cCiaStatus: "DIC / CIA Triad",
    cCiaTitle: "Triade DIC (Disponibilité, Intégrité, Confidentialité)",
    cCiaSub: "L'analyse détaillée des trois piliers fondamentaux de la sécurité informatique au cœur de l'architecture CYBOU.",
    cCiaC: "1. Confidentialité (C)",
    cCiaCDesc: "<strong>Zéro-Knowledge total :</strong> Chiffrement asymétrique et symétrique de pointe (OpenSSL v3.5.2+). Capsules de déchiffrement réservées aux destinataires autorisés. Base de données locale de l'application scellée sur le disque. Protection post-quantique contre toute tentative d'interception actuelle ou future.",
    cCiaI: "2. Intégrité (I)",
    cCiaIDesc: "<strong>Inaltérabilité mathématique :</strong> Chaque fragment de fichier est identifié par son empreinte cryptographique BLAKE3-256. Les publications d'arbres de contenu sont scellées dans des preuves de Merkle vérifiées par consensus PoA avec journal anti-équivocation inviolable.",
    cCiaA: "3. Disponibilité (D)",
    cCiaADesc: "<strong>Résilience continue :</strong> Chaque fragment chiffré est répliqué sur des nœuds de stockage indépendants. Audits de santé périodiques, détection proactive de perte de réplique et réparation automatique sans interruption pour l'utilisateur.",

    cAnssiStatus: "Réglementation Nationale",
    cAnssiTitle: "ANSSI & Souveraineté Numérique Française",
    cAnssiSub: "Alignement avec les recommandations de l'Agence Nationale de la Sécurité des Systèmes d'Information.",
    cAnssiP1: "L'Agence Nationale de la Sécurité des Systèmes d'Information (ANSSI) édicte les règles et recommandations de sécurité pour l'État français, les Opérateurs d'Importance Vitale (OIV) et le tissu économique. CYBOU s'inscrit pleinement dans ces orientations :",
    cAnssiItem1Title: "Transition post-quantique et hybridation :",
    cAnssiItem1Desc: "L'ANSSI recommande expressément une approche d'hybridation (combiner un algorithme classique reconnu et un mécanisme post-quantique normalisé) pour éviter les régressions de sécurité. CYBOU suit rigoureusement cette préconisation en combinant Ed25519 + ML-DSA pour les signatures et X25519 + ML-KEM-768 pour l'échange de clés.",
    cAnssiItem2Title: "Régime de déclaration des moyens de cryptologie (CPCE) :",
    cAnssiItem2Desc: "Conformément aux articles L. 133-1 et suivants du Code des postes et des communications électroniques, l'utilisation de moyens de cryptologie assurant des fonctions de confidentialité et d'authentification est libre en France pour les particuliers et les entreprises.",
    cAnssiItem3Title: "Souveraineté des données et cloud de confiance (SecNumCloud) :",
    cAnssiItem3Desc: "Pour les déploiements destinés au secteur public et aux infrastructures sensibles, les nœuds fournisseurs de stockage CYBOU sont conçus pour être hébergés sur des infrastructures qualifiées SecNumCloud, garantissant une étanchéité totale face aux lois extraterritoriales non européennes.",
    cAnssiCtaTitle: "Besoin d'une analyse d'adéquation pour votre organisation ?",
    cAnssiCtaDesc: "Notre équipe technique et juridique répond à vos questions concernant vos déploiements on-premise, votre politique de conformité RGPD ou l'hébergement HDS.",
    cAnssiCtaBtn: "Contacter l'équipe",

    footProdEnterprise: "CYBOU Entreprise",
    footSecEnterprise: "Réseaux privés dédiés",
    srvEnterpriseTag: "Sur Mesure",
    srvEnterpriseTitle: "CYBOU Entreprise — Réseau Privé Dédié & Isolé",
    srvEnterpriseDesc: "Instanciez votre propre réseau souverain étanche : bloc de genèse dédié, clé PoA sous contrôle de votre DSI, stockage sur site (On-Premise / Air-Gap) et zéro fuite externe.",
    srvEnterpriseLink: "Explorer les solutions d'entreprise &rarr;",

    // Dedicated Enterprise Page (entreprise.html)
    eBreadHome: "Accueil",
    eBreadCurrent: "Solutions Entreprise & Réseaux Dédiés",
    eHeroBadge: "DÉPLOIEMENT DÉDIÉ & ISOLATION SOUVERAINE",
    eHeroBadgeText: "Isolation Cryptographique & Gouvernance Dédiée",
    eHeroTitle: "Créez votre Réseau Privé d'Entreprise Sécurisé & Dédié",
    eHeroDesc: "Pour organisations françaises et internationales : Déployez une appliance logicielle de réseau privé CYBOU 100% autonome, cryptographiquement étanche et administrée par votre DSI. Chiffrement post-quantique OpenSSL v3.5.2+, stockage On-Premise ou Air-Gap, et immunité juridique absolue.",

    ePillAdv: "Avantages Clés",
    ePillArch: "Isolation & Genèse",
    ePillModes: "Réseau dédié",
    ePillGov: "Gouvernance DSI",
    ePillUse: "Cas d'Usage",
    ePillPricing: "Tarifs & Budget",
    ePillContact: "Déploiement & PoC",

    eArt1Pill: "Souveraineté Maximale",
    eArt1Status: "Autonomie Totale",
    eArt1Title: "Pourquoi déployer un réseau CYBOU dédié pour votre organisation ?",
    eArt1Sub: "Les réseaux publics partagés ne conviennent pas toujours aux exigences des industries sensibles, des armées ou des groupes critiques. CYBOU permet de créer une bulle réseau totalement privée.",

    eCard1Pill: "Custom Genesis",
    eCard1Badge: "Étanche",
    eCard1Title: "Isolation Cryptographique & Clients Internationaux",
    eCard1Desc: "Contrairement au réseau public restreint à la France, votre appliance privée peut être déployée par toute entreprise française ou internationale. Avec votre propre bloc de genèse, vos clés racines et vos certificats SPKI dédiés, tout paquet étranger est rejeté mathématiquement (fail-closed).",

    eCard2Pill: "Contrôle DSI / RSSI",
    eCard2Badge: "Gouvernance",
    eCard2Title: "Clé d'Autorité Détenue par Votre Organisation",
    eCard2Desc: "La finalité PoA (Proof-of-Authority) est opérée par un poste de confiance sous le contrôle exclusif de votre direction informatique. Zéro intermédiaire externe, aucun tiers de confiance n'a le pouvoir de modifier vos règles ou valider vos blocs.",

    eCard3Pill: "Zero-Knowledge",
    eCard3Badge: "Zéro Backdoor",
    eCard3Title: "Confidentialité Maximale Entre Collaborateurs",
    eCard3Desc: "Tous les échanges sont chiffrés de bout en bout sur les terminaux des employés via OpenSSL v3.5.2+ (ML-KEM-768 et ML-DSA). Même vos administrateurs système ne peuvent pas lire les correspondances confidentielles des comités de direction sans clé de capsule légitime.",

    eCard4Pill: "Résilience P2P",
    eCard4Badge: "Zéro SPOF",
    eCard4Title: "Stockage Distribué sans Point Unique de Panne",
    eCard4Desc: "Vos fichiers et messages sont fragmentés (BLAKE3-256) et répliqués sur vos serveurs internes ou vos datacenters distants. La panne d'un serveur ou la rupture d'un lien WAN n'interrompt pas le service : le réseau s'auto-répare automatiquement.",

    eArt2Pill: "Sous le Capot",
    eArt2Status: "Spécifications Ouvertes",
    eArt2Title: "Comment fonctionne l'isolation d'un réseau dédié ?",
    eArt2Sub: "Tous les participants exécutent le même logiciel robuste standard, mais l'espace cryptographique est strictement circonscrit à votre entreprise.",
    eArt2P1: "L'architecture unifiée de CYBOU permet d'instancier un réseau d'entreprise indépendant en quelques minutes, sans modifier une seule ligne de code source. Le protocole sépare hermétiquement les réseaux par leur définition de genèse et leurs certificats de transport :",

    eStep1Title: "Définition de Genèse",
    eStep1Desc: "Génération d'un fichier genesis.json signé par la clé privée de réseau (hors-ligne), avec un identifiant de réseau propre et les paramètres d'autorité.",
    eStep2Title: "Autorité Centrale PoA",
    eStep2Desc: "La DSI génère la clé de finalisation PoA sur un poste sécurisé (SecOps) pour sceller les blocs d'entreprise.",
    eStep3Title: "Nœuds d'Amorçage Initiaux",
    eStep3Desc: "Déploiement de 1 à 4 pairs CYBOU ordinaires avec points de contact connus sur votre intranet pour l'aiguillage initial des pairs et la synchronisation.",
    eStep4Title: "Grappe de Stockage",
    eStep4Desc: "Activation du rôle stockage (cybou-node provider run) sur vos serveurs internes pour héberger les fragments chiffrés.",

    eCompTableTitle: "Tableau Comparatif : Réseau Public vs. Réseau Privé d'Entreprise",
    eThParam: "Caractéristique",
    eThPublic: "Réseau Public CYBOU",
    eThPrivate: "Réseau Privé Dédié d'Entreprise",
    eRowGen: "Bloc de Genèse",
    eRowGenPub: "Genèse publique souveraine (France)",
    eRowGenPriv: "<strong>Genèse privée dédiée</strong> (Identifiant réseau unique à l'organisation)",
    eRowPoa: "Opérateur de Finalité (PoA)",
    eRowPoaPub: "Autorité de référence CYBOU",
    eRowPoaPriv: "<strong>Poste sécurisé de votre DSI / RSSI</strong> (Contrôle souverain exclusif)",
    eRowBoot: "Nœuds d'Amorçage (Bootstrap)",
    eRowBootPub: "Nœuds publics géolocalisés en France",
    eRowBootPriv: "<strong>1 à 4 pairs d'amorce internes</strong> (Points de contact initiaux sur votre LAN / Intranet / VPN)",
    eRowStorage: "Infrastructure de Stockage",
    eRowStoragePub: "Fournisseurs de stockage qualifiés en France",
    eRowStoragePriv: "<strong>Serveurs d'entreprise sur site (On-Premise)</strong> ou cloud privé qualifié",
    eRowPerim: "Périmètre Réseau & Accès",
    eRowPerimPub: "Internet (Adresses IP françaises uniquement)",
    eRowPerimPriv: "<strong>Périmètre au choix (France ou International)</strong> : Intranet d'entreprise, VPN privé ou Enclave Air-Gap fermée",
    eRowCrypto: "Transport & Cryptographie",
    eRowCryptoPub: "TLS 1.3 hybride post-quantique OpenSSL 3.5.2+",
    eRowCryptoPriv: "<strong>Identique : TLS 1.3 PQ + SPKI Pinning d'entreprise strict</strong>",

    eArt3Pill: "Réseau privé dédié",
    eArt3Status: "Gouverné par votre organisation",
    eArt3Title: "Une instance CYBOU propre à votre entreprise",
    eArt3Sub: "Nous créons un réseau privé distinct, cryptographiquement séparé des autres réseaux CYBOU et contrôlé par l'administration de votre entreprise.",
    eArt3P1: "Le réseau utilise le logiciel CYBOU, avec sa propre définition de genèse, ses clés d'autorité et ses paramètres réseau. Votre organisation détient la clé de finalité PoA ; l'Autorité Centrale CYBOU n'administre pas et ne finalise pas votre réseau.",
    eArt3F1: "Votre administration définit les nœuds bootstrap et leurs adresses IP.",
    eArt3F2: "Les règles d'admission des pairs par IP sont configurées pour votre réseau.",
    eArt3F3: "Votre organisation choisit l'infrastructure sur laquelle les nœuds fonctionnent.",
    eArt3P2: "L'infrastructure et le mode de connectivité relèvent du choix du client ; ils ne créent pas différents types de réseau CYBOU.",

    eArt4Pill: "Gestion Opérationnelle",
    eArt4Status: "Prêt pour la Production",
    eArt4Title: "Gouvernance, Administration & Simplicité Opérationnelle",
    eArt4Sub: "Des outils industriels pour administrer votre flotte de postes et serveurs sans complexité inutile.",
    eArt4P1: "Contrairement aux solutions propriétaires lourdes nécessitant des serveurs d'annuaire fragiles et des dizaines de composants hétérogènes, CYBOU repose sur une architecture épurée :",

    eGovItem1Title: "Un binaire unique pour toute l'infrastructure :",
    eGovItem1Desc: "Le même exécutable unifié cybou-node sert de nœud complet, de serveur d'amorçage (bootstrap) et de serveur de stockage (provider). Il tourne en tâche de fond sous Linux avec un service systemd standard, consomme peu de mémoire et s'intègre naturellement à vos outils de déploiement Ansible, Puppet ou Terraform.",
    eGovItem2Title: "Application de bureau intuitive pour les collaborateurs :",
    eGovItem2Desc: "Les équipes utilisent CYBOU Desktop sur leurs postes de travail (Windows, Linux, macOS). Une interface soignée unifiant messagerie chiffrée, espace de fichiers et identité souveraine sans formation technique préalable.",
    eGovItem3Title: "Élimination des mots de passe et bases d'identifiants compromises :",
    eGovItem3Desc: "Finis les annuaires LDAP ou Active Directory exposés aux attaques de type Golden Ticket ou credential stuffing. Chaque collaborateur possède sa propre phrase secrète de 24 mots et ses paires de clés asymétriques scellées localement.",
    eGovItem4Title: "Cycle de vie et rotation des clés (IdentityRotate) :",
    eGovItem4Desc: "En cas de changement d'ordinateur ou de suspicion de compromission, l'opération atomique IdentityRotate permet de révoquer et renouveler instantanément les clés de signature et de chiffrement sans perdre l'historique ni l'identifiant du collaborateur.",
    eGovItem5Title: "Sauvegarde de sinistre et PCA/PRA déterministe :",
    eGovItem5Desc: "En cas de sinistre physique majeur détruisant vos locaux, la simple possession de la clé de genèse et des 24 mots permet de réinstancier l'autorité et de restaurer les données d'entreprise de manière mathématiquement vérifiée.",

    eArt5Pill: "Applications Métier",
    eArt5Status: "Secteurs Sensibles",
    eArt5Title: "Cas d'usage stratégiques pour les organisations exigeantes",
    eArt5Sub: "Conçu pour protéger ce que votre organisation a de plus précieux : son savoir-faire, ses secrets et sa souveraineté d'action.",

    eUse1Pill: "Industrie de Pointe",
    eUse1Badge: "Secret des Affaires",
    eUse1Title: "R&D, Propriété Intellectuelle & Brevets",
    eUse1Desc: "Protection absolue des plans de fabrication, formulations chimiques, codes sources et stratégies de mise sur le marché contre l'espionnage industriel étranger et les attaques par rançongiciel.",

    eUse2Pill: "Défense & OIV",
    eUse2Badge: "Directive NIS 2",
    eUse2Title: "Infrastructures Critiques & OSE",
    eUse2Desc: "Réseau de communication tactique et de sauvegarde décentralisé pour les Opérateurs d'Importance Vitale devant satisfaire aux normes NIS 2 et aux impératifs de résilience en situation de crise.",

    eUse3Pill: "Direction & Gouvernance",
    eUse3Badge: "Confidentiel C-Level",
    eUse3Title: "Direction Générale, M&A & Conseils",
    eUse3Desc: "Data room et échanges ultra-confidentiels pour les comités exécutifs, fusions-acquisitions, contentieux stratégiques et audits, sans risque de divulgation par des administrateurs système ou des SaaS tiers.",

    eUse4Pill: "Groupes Internationaux",
    eUse4Badge: "Présence Mondiale",
    eUse4Title: "Sièges & Filiales Internationales",
    eUse4Desc: "Canal de communication et de transfert étanche entre la maison-mère (en France ou à l'étranger) et ses bureaux internationaux, protégé contre les écoutes étatiques locales et le piratage sur les réseaux publics.",

    // Economic Model & Pricing
    eArtPricePill: "Modèle Économique & ROI",
    eArtPriceStatus: "Tarifs Clairs en Euros",
    eArtPriceTitle: "Estimation budgétaire pour votre réseau privé CYBOU",
    eArtPriceSub: "Un investissement forfaitaire transparent en euros (€), sans licence récurrente par utilisateur et amorti dès les premiers mois.",
    ePriceT1Title: "Appliance Réseau Privé Souverain",
    ePriceT1Amount: "À partir de 15 000 € <span style=\"font-size: 1rem; font-weight: 500; color: var(--text-muted);\">HT</span>",
    ePriceT1Unit: "Déploiement socle • Postes de travail illimités",
    ePriceT1Desc: "Mise en production industrielle de votre réseau privé sur vos infrastructures (serveurs On-Premise, Air-Gap ou VMs privées), sans aucune restriction ni licence par utilisateur.",
    ePriceT1F1: "Postes CYBOU Desktop illimités (zéro licence par utilisateur)",
    ePriceT1F2: "Genèse d'entreprise dédiée & Network ID cryptographiquement isolé",
    ePriceT1F3: "Station d'autorité PoA durcie sous contrôle direct de la DSI",
    ePriceT1F4: "Cluster de 4 nœuds bootstrap & stockage répliqué (zéro SPOF)",
    ePriceT1F5: "Procédures PRA/PCA déterministes documentées & formation DSI",
    ePriceT2Badge: "Accompagnement VIP & Support Intégral",
    ePriceT2Title: "Appliance Clé-en-main & Support VIP",
    ePriceT2Amount: "À partir de 35 000 € <span style=\"font-size: 1rem; font-weight: 500; color: var(--text-muted);\">HT</span>",
    ePriceT2Unit: "Déploiement complet • Support L3 Ingénierie & Maintien en Sécurité",
    ePriceT2Desc: "Solution clé-en-main intégrale pour les organisations exigeant un accompagnement sur mesure, une assistance VIP et un support direct avec les concepteurs du protocole.",
    ePriceT2F1: "Tout le socle Appliance Réseau Privé (postes illimités, PoA, cluster 4 nœuds)",
    ePriceT2F2: "Accompagnement VIP sur mesure au dimensionnement et à l'intégration",
    ePriceT2F3: "Support technique L3 direct & prioritaire avec les ingénieurs concepteurs",
    ePriceT2F4: "Veille et intégration continue des mises à jour OpenSSL v3.5.2+ & protocole",
    ePriceT2F5: "Revue annuelle d'architecture de sécurité, résilience et assistance aux audits",
    ePriceDisclaimer: "* Tarifs indicatifs hors taxes (« à partir de ») : les frais de déplacement (transports, hébergement), les développements spécifiques ou intégrations sur mesure et les prestations d'assistance étendue sur site font l'objet d'un devis préalable adapté à votre cahier des charges.",
    eRoiTitle: "0 € de coût de licence mensuel par utilisateur (Économie massive vs. SaaS US)",
    eRoiDesc: "Contrairement aux offres SaaS (Microsoft 365, Google Workspace) facturées entre 20 et 35 €/utilisateur/mois — représentant 30 000 €/an pour 100 postes et 150 000 €/an pour 500 postes —, une appliance réseau privé CYBOU est amortie dès les premiers mois. Votre organisation reste l'unique détentrice de ses données et de son code, sans risque d'inflation tarifaire unilatérale.",

    eArt6Pill: "Passez à l'Action",
    eArt6Status: "Pilote & Déploiement",
    eArt6Title: "Déployez votre premier réseau pilote CYBOU",
    eArt6Sub: "Notre architecture est entièrement documentée, transparente et vérifiable. Mettez en place un banc d'essai interne ou échangez avec l'équipe de conception.",
    eContactP1: "Vous souhaitez évaluer CYBOU dans votre environnement de test ou concevoir une enclave sécurisée pour vos équipes stratégiques ? Vous disposez de plusieurs points d'entrée :",
    eCtaTitle: "Échanger avec l'équipe CYBOU pour un réseau dédié",
    eCtaDesc: "Assistance pour la génération de genèse d'entreprise, conseils de dimensionnement infrastructure et audit de conformité.",
    eCtaBtn: "Contacter l'équipe d'ingénierie",

    // Comparison Teaser on Homepage
    compCardNextPill: "Open Source & Auto-Hébergement",
    compCardNextStatus: "P2P vs. LAMP",
    compCardNextTitle: "CYBOU vs. Nextcloud — Zéro-Knowledge vs. Serveur PHP",
    compCardNextDesc: "Nextcloud chiffre côté serveur par défaut (l'administrateur peut lire vos fichiers) et dépend d'une pile fragile (MySQL/PHP/Apache). CYBOU chiffre obligatoirement sur votre terminal et fonctionne sur un binaire C++ P2P sans base de données centrale.",
    compCardDropPill: "Stockage Cloud Américain",
    compCardDropStatus: "Cloud Act & IA",
    compCardDropTitle: "CYBOU vs. Dropbox — Immunité Légale vs. Scans d'IA",
    compCardDropDesc: "Dropbox est soumis au US Cloud Act et intègre des outils d'IA analysant vos documents. CYBOU restreint son réseau à la France (fail-closed), isole mathématiquement chaque fragment (BLAKE3-256) et interdit tout scan ou entraînement d'IA.",
    compCardGafamPill: "Suites Centralisées US",
    compCardGafamStatus: "Émancipation",
    compCardGafamTitle: "CYBOU vs. Google Workspace & Microsoft 365",
    compCardGafamDesc: "Les GAFAM détiennent vos clés de déchiffrement et imposent des numéros de téléphone et cartes bancaires. CYBOU vous redonne la pleine propriété de votre compte via une phrase secrète de 24 mots et des clés post-quantiques inaliénables.",
    compCardPqPill: "Standard Mondial OpenSSL v3.5.2+",
    compCardPqStatus: "Post-Quantique",
    compCardPqTitle: "Résistance Quantique Immédiate (NIST FIPS 203/204)",
    compCardPqDesc: "Alors que Nextcloud, Dropbox et Microsoft restent cantonnés aux algorithmes classiques (RSA/ECC) vulnérables aux interceptions futures (« Harvest Now, Decrypt Later »), CYBOU déploie dès aujourd'hui ML-KEM-768 et ML-DSA.",
    compTeaserCtaTitle: "Consulter la matrice comparative complète et détaillée",
    compTeaserCtaDesc: "Tableau exhaustif sur 8 critères de sécurité, analyse architecturale approfondie face à Nextcloud et Dropbox, et guide de choix pour les DSI.",
    compTeaserCtaBtn: "Voir le grand comparatif",

    // Dedicated Comparison Page (comparatif.html)
    cmpBreadHome: "Accueil",
    cmpBreadCurrent: "Comparatif Objectif des Solutions",
    cmpHeroTag: "Analyse Technique & Souveraine",
    cmpHeroTitle: "CYBOU face à Nextcloud, Dropbox, Google & Microsoft",
    cmpHeroDesc: "Comprenez en profondeur ce qui différencie la suite souveraine CYBOU des clouds centralisés américains (Google, Microsoft, Dropbox) et des serveurs auto-hébergés classiques (Nextcloud). Architecture P2P, cryptographie post-quantique OpenSSL v3.5.2+, immunité légale et modèle de données Zero-Knowledge.",
    cmpPillMatrix: "Matrice Complète",
    cmpPillNextcloud: "CYBOU vs. Nextcloud",
    cmpPillDropbox: "CYBOU vs. Dropbox",
    cmpPillBigTech: "CYBOU vs. GAFAM",
    cmpPillSummary: "Synthèse & Décision",

    cmpTabAll: "Vue Complète (Tous)",
    cmpTabNextcloud: "vs. Nextcloud",
    cmpTabDropbox: "vs. Dropbox",
    cmpTabGoogle: "vs. Google Workspace",
    cmpTabMsApple: "vs. Microsoft 365 / Apple",

    cmpMatPill: "Tableau Comparatif Global",
    cmpMatStatus: "8 Critères Clés",
    cmpMatTitle: "Matrice Complète des Solutions de Collaboration & Stockage",
    cmpMatSub: "Une comparaison directe et impartiale sur la sécurité, l'architecture, la cryptographie et la juridiction.",
    cmpThCrit: "Critère de Protection",
    cmpThCybou: "CYBOU Desktop",
    cmpThNextcloud: "Nextcloud",
    cmpThDropbox: "Dropbox",
    cmpThGoogle: "Google Workspace",
    cmpThMsApple: "Microsoft 365 / Apple",

    cmpR1Crit: "Chiffrement des messages et fichiers",
    cmpR1Cybou: "Client-side obligatoire (Zero-Knowledge)",
    cmpR1Nextcloud: "Serveur par défaut (admin lit tout) ; E2EE optionnel complexe",
    cmpR1Dropbox: "Chiffrement serveur (Dropbox possède les clés)",
    cmpR1Google: "Chiffrement serveur (Google possède les clés)",
    cmpR1MsApple: "Chiffrement serveur (Éditeur possède les clés)",

    cmpR2Crit: "Moteur et code cryptographique",
    cmpR2Cybou: "Standard OpenSSL v3.5.2+ (NIST FIPS 203/204, zéro code maison)",
    cmpR2Nextcloud: "PHP / OpenSSL classique (RSA / AES traditionnels)",
    cmpR2Dropbox: "Boîte noire propriétaire fermée",
    cmpR2Google: "Boîte noire propriétaire fermée",
    cmpR2MsApple: "Boîte noire propriétaire fermée",

    cmpR3Crit: "Résistance aux attaques quantiques",
    cmpR3Cybou: "Natif : ML-KEM-768 (TLS 1.3) + ML-DSA-44/65 (Identités)",
    cmpR3Nextcloud: "Aucune protection post-quantique",
    cmpR3Dropbox: "Aucune protection post-quantique",
    cmpR3Google: "Algorithmes classiques (RSA / ECC vulnérables)",
    cmpR3MsApple: "Algorithmes classiques (RSA / ECC vulnérables)",

    cmpR4Crit: "Architecture & Point unique de panne",
    cmpR4Cybou: "P2P décentralisé unifié C++, auto-réparation de fragments",
    cmpR4Nextcloud: "Client-Serveur (SPOF MySQL + Web server + Redis + PHP)",
    cmpR4Dropbox: "Cloud centralisé américain",
    cmpR4Google: "Cloud centralisé américain",
    cmpR4MsApple: "Cloud centralisé américain",

    cmpR5Crit: "Juridiction & Lois extraterritoriales",
    cmpR5Cybou: "France & UE (P2P filtré France-only, immunité Cloud Act)",
    cmpR5Nextcloud: "Dépend de l'hébergeur choisi (soumis au Cloud Act si AWS/Azure)",
    cmpR5Dropbox: "États-Unis (Soumis sans réserve au Cloud Act et FISA 702)",
    cmpR5Google: "États-Unis (Soumis au US Cloud Act et FISA 702)",
    cmpR5MsApple: "États-Unis (Soumis aux lois extraterritoriales US)",

    cmpR6Crit: "Scan pour pub ou entraînement d'IA",
    cmpR6Cybou: "Zéro scan, zéro IA, zéro pub (données opaques)",
    cmpR6Nextcloud: "Zéro scan par défaut (plugins IA optionnels)",
    cmpR6Dropbox: "Scans de contenu et intégration IA tierce (Dropbox Dash)",
    cmpR6Google: "Indexation algorithmique pour publicité et modèles IA",
    cmpR6MsApple: "Télémétrie intensive et intégration Copilot",

    cmpR7Crit: "Authentification & Dépendance",
    cmpR7Cybou: "Phrase secrète 24 mots, zéro mot de passe, zéro SMS",
    cmpR7Nextcloud: "Mots de passe stockés en base MySQL ou annuaire LDAP",
    cmpR7Dropbox: "Mots de passe serveur, compte révocable à tout moment",
    cmpR7Google: "Numéro de téléphone requis, risque de ban unilatéral",
    cmpR7MsApple: "Compte lié à carte bancaire et téléphone, verrouillage éditeur",

    cmpR8Crit: "Déploiement & Maintenance",
    cmpR8Cybou: "Binaire C++ autonome sans dépendances externes",
    cmpR8Nextcloud: "Déploiement lourd (pile LAMP, bases SQL, migrations délicates)",
    cmpR8Dropbox: "Simple utilisateur, mais enfermement propriétaire",
    cmpR8Google: "SaaS géré, mais perte totale de contrôle",
    cmpR8MsApple: "SaaS géré, mais dépendance commerciale critique",

    cmpNextPill: "Analyse Open Source",
    cmpNextStatus: "Architecture P2P vs. LAMP",
    cmpNextTitle: "CYBOU vs. Nextcloud : Pourquoi le P2P Zero-Knowledge surpasse le modèle Client-Serveur PHP",
    cmpNextSub: "Nextcloud est une excellente solution open-source de première génération, mais son architecture client-serveur traditionnelle présente des limites structurelles que CYBOU a résolues.",
    cmpNextP1: "Nextcloud a popularisé l'auto-hébergement collaboratif. Cependant, son socle technique conçu il y a plus de 15 ans repose sur une pile LAMP (Linux, Apache, MySQL, PHP) conçue pour le web classique, et non pour la résilience cryptographique décentralisée :",
    cmpNextItem1Title: "L'illusion du chiffrement côté serveur :",
    cmpNextItem1Desc: "Par défaut, Nextcloud stocke les fichiers en clair sur le serveur ou les chiffre avec des clés stockées sur le serveur lui-même. Tout administrateur système ayant un accès SSH ou root peut lire l'intégralité des données des utilisateurs. Le module de chiffrement de bout en bout (E2EE) de Nextcloud est un greffon complexe, tristement célèbre pour ses pannes de synchronisation et ses pertes de clés. Chez CYBOU, le chiffrement Zero-Knowledge est le socle obligatoire et inaltérable du protocole.",
    cmpNextItem2Title: "Résilience P2P vs. Single Point of Failure (SPOF) :",
    cmpNextItem2Desc: "Un serveur Nextcloud est un point individuel de panne : si la base de données MySQL est corrompue, si le serveur PHP tombe ou si le stockage local sature, l'organisation entière est paralysée. CYBOU élimine les bases de données SQL centralisées. Les fichiers sont morcelés en fragments cryptographiques BLAKE3-256 distribués et auto-réparés sur plusieurs serveurs indépendants.",
    cmpNextItem3Title: "Transition Post-Quantique (NIST FIPS 203/204) :",
    cmpNextItem3Desc: "Nextcloud utilise les primitives classiques RSA et ECC, désormais reconnues vulnérables aux futures attaques par calculateurs quantiques. CYBOU intègre nativement la cryptographie post-quantique certifiée OpenSSL v3.5.2+ (ML-KEM-768 et ML-DSA).",
    cmpNextItem4Title: "Maintenance et empreinte opérationnelle :",
    cmpNextItem4Desc: "Administrer Nextcloud exige de maintenir un serveur web, un interpréteur PHP, une base de données relationnelle, un cache Redis et de gérer des migrations de schémas souvent périlleuses. CYBOU se déploie via un binaire unique C++ sans aucune dépendance externe, consommant une fraction infime de mémoire vive et de CPU.",

    cmpDropPill: "Stockage Cloud US",
    cmpDropStatus: "Danger Cloud Act & IA",
    cmpDropTitle: "CYBOU vs. Dropbox : Le danger de l'extraterritorialité et de l'analyse par IA",
    cmpDropSub: "Dropbox reste l'un des services grand public et d'entreprise les plus utilisés, mais il constitue un risque majeur pour la confidentialité des affaires et la souveraineté.",
    cmpDropP1: "Sous son interface soignée, Dropbox fonctionne sur un modèle économique et juridique incompatible avec le secret professionnel européen :",
    cmpDropItem1Title: "Soumission totale au US Cloud Act et à FISA 702 :",
    cmpDropItem1Desc: "Dropbox étant une entreprise soumise au droit américain, les agences fédérales et les tribunaux des États-Unis peuvent ordonner l'accès aux données stockées, y compris lorsque celles-ci concernent des entreprises françaises ou européennes, sans notification préalable. CYBOU est ancré en France avec un filtrage géographique fail-closed garantissant une immunité totale face à ces lois extraterritoriales.",
    cmpDropItem2Title: "Exploitation et scan des données pour les outils d'IA :",
    cmpDropItem2Desc: "Dropbox a suscité une vive controverse en activant par défaut des fonctionnalités d'intelligence artificielle (Dropbox Dash / AI) transmettant des données de fichiers à des partenaires tiers (comme OpenAI). Avec CYBOU, les fragments de fichiers stockés sur le réseau sont mathématiquement indéchiffrables pour les nœuds d'hébergement : aucune indexation, aucun scan et aucun entraînement d'IA ne sont physiquement possibles.",
    cmpDropItem3Title: "Possession unilatérale des clés de déchiffrement :",
    cmpDropItem3Desc: "Dropbox chiffre vos fichiers « au repos », mais Dropbox détient la clé maîtresse. Un employé indélicat de Dropbox, une erreur de configuration ou une injonction judiciaire suffisent pour dévoiler vos fichiers. Chez CYBOU, seuls vos terminaux possèdent les clés de déchiffrement dérivées de votre phrase secrète de 24 mots.",

    cmpGafamPill: "Monopoles Big Tech",
    cmpGafamStatus: "Émancipation Numérique",
    cmpGafamTitle: "CYBOU vs. GAFAM : Reprendre le contrôle de ses communications",
    cmpGafamSub: "Pourquoi les suites de Google, Microsoft et Apple ne garantissent aucune confidentialité réelle pour les décideurs européens.",
    cmpGafamP1: "Google Workspace (Gmail, Drive) et Microsoft 365 (Outlook, OneDrive) dominent le marché des suites bureautiques. Cette hégémonie pose trois problèmes critiques résolus par CYBOU :",
    cmpGafamItem1Title: "Le piège de la fausse promesse du « chiffrement géré » :",
    cmpGafamItem1Desc: "Les géants américains affirment chiffrer vos données, mais ils gèrent eux-mêmes les modules HSM et les clés. Cette approche permet à leurs algorithmes de scanner en continu vos emails et documents pour alimenter leurs modèles d'IA, leurs moteurs publicitaires ou leurs télémétries. CYBOU applique le véritable chiffrement de bout en bout : le contenu est scellé avant de quitter votre processeur.",
    cmpGafamItem2Title: "L'enfermement propriétaire et la dépendance tarifaire :",
    cmpGafamItem2Desc: "Les hausses de tarifs unilatérales imposées aux entreprises et aux administrations publiques illustrent le risque du monopole. CYBOU est un protocole open-source, décentralisé, déployable sur site (on-premise) ou sur cloud souverain, garantissant une indépendance pérenne.",
    cmpGafamItem3Title: "Souveraineté des identités sans numéros de téléphone ni SMS :",
    cmpGafamItem3Desc: "Pour ouvrir un compte Google ou Microsoft, vous devez fournir un numéro de téléphone mobile et accepter des conditions générales modifiables à tout moment. Votre compte peut être suspendu arbitrairement sans recours. Une identité CYBOU est inaliénable : elle est protégée par votre phrase secrète de 24 mots et une preuve de travail anti-Sybil. Personne ne peut vous bannir de votre propre espace.",

    cmpSynPill: "Guide de Choix",
    cmpSynStatus: "Synthèse Décisionnelle",
    cmpSynTitle: "Quelle solution choisir selon vos priorités ?",
    cmpSynSub: "Un récapitulatif clair pour orienter les décideurs, RSSI et utilisateurs exigeants.",
    cmpSynCol1Title: "Nextcloud",
    cmpSynCol1Desc: "Adapté aux équipes cherchant une suite collaborative riche (agendas, visio, kanban) et disposant d'administrateurs système dédiés pour maintenir une pile LAMP. Non adapté si vous exigez une étanchéité Zero-Knowledge stricte et une résistance post-quantique.",
    cmpSynCol2Title: "Dropbox / Google / Microsoft",
    cmpSynCol2Desc: "Pratiques pour un usage grand public non critique ou des documents non confidentiels. Totalement inadaptés pour les secrets d'affaires, les brevets, les données médicales HDS ou les organisations soumises à la directive NIS 2 et au RGPD souverain.",
    cmpSynCol3Title: "CYBOU Desktop & Entreprise",
    cmpSynCol3Desc: "<strong>Le choix incontournable</strong> dès lors que la confidentialité, l'immunité juridique face aux lois américaines, la résilience sans SPOF et la préparation à l'ère post-quantique sont des impératifs non négociables.",
    cmpCtaTitle: "Prêt à tester l'alternative souveraine CYBOU ?",
    cmpCtaDesc: "Explorez notre code open-source sur GitHub ou découvrez comment déployer un réseau privé d'entreprise dédié.",
    cmpCtaBtnEnterprise: "Solutions Entreprise",
    cmpCtaBtnGithub: "Code source GitHub",

    cmpBtnViewNextcloud: "Voir le face-à-face complet CYBOU vs. Nextcloud ↑",
    cmpBtnViewDropbox: "Voir le face-à-face complet CYBOU vs. Dropbox ↑",
    cmpBtnViewGoogle: "Voir CYBOU vs. Google Workspace ↑",
    cmpBtnViewMsApple: "Voir CYBOU vs. Microsoft 365 / Apple ↑"
  },

  en: {
    navProducts: "Products",
    navShowcase: "Preview",
    navEnterprise: "Enterprise",
    navSecurity: "Security & TLS",
    navCompliance: "Compliance",
    navComparison: "Comparison",
    navArch: "Under the Hood",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Standard OpenSSL v3.5.2+ Cryptography • France Sovereign • NIST Post-Quantum",
    heroAccent: "Your sovereign private workspace. Mail, files, and identity.",
    heroSubtitle: "CYBOU brings private messaging, file storage, and sovereign identity together into a protective desktop application. Zero homemade crypto: all cryptography relies on official OpenSSL v3.5.2+ and NIST post-quantum standards.",
    heroPillSlogan: "A sovereign suite protecting your privacy — without ad tracking or surveillance.",

    statusCalloutTitle: "DEV protocol operational. Hardening for public release.",
    statusCalloutBody: "The experimental DEV network operates with PoA finality, distributed chunk storage, and TLS 1.3 transport. The CYBOU desktop application is undergoing continuous hardening and integration tests ahead of public release.",

    heroBtnShowcase: "Explore the Desktop App",
    heroBtnSecurity: "Why CYBOU?",

    badgeOpenssl: "OpenSSL v3.5.2+ Engine (Zero Homemade Crypto)",
    badgeFrance: "France Sovereign Network (Immune to US Cloud Act)",
    badgeTls: "Post-Quantum TLS 1.3 Transport (CYP2)",
    badgePhrase: "Zero Passwords / 24-Word Master Key",
    badgeNoAds: "Zero Tracking & Zero Ads",

    showcaseLabel: "Application Preview",
    showcaseTitle: "The CYBOU Desktop Experience.",
    showcaseDesc: "A unified and intuitive interface bringing your communications, files, and identity together under continuous cryptographic protection.",
    mockupStatus: "Connected • France Network (2 replicas)",
    mockupNetworkFr: "France sovereign node",
    mockupTabMail: "Mail",
    mockupTabFiles: "Files",
    mockupTabId: "Identity",
    mockupTabSec: "Security & Network",
    mockupTabSecBadge: "12 FR peers",

    mockupMailFrom1: "Pierre Martin",
    mockupMailSub1: "Confidential Audit Records 2026",
    mockupMailSnip1: "Hello Alice, here are the requested encrypted files...",
    mockupBadgeE2ee: "E2EE Encrypted • Protected",
    mockupMailFrom2: "Legal Advisory",
    mockupMailSub2: "Signed Partnership Agreement",
    mockupMailSnip2: "The document has been validated and sealed with hybrid signature...",
    mockupBadgePq: "ML-DSA-44 Signature",
    mockupMailBodyTitle: "Confidential Audit Records 2026",
    mockupReaderBadge1: "E2EE Client Encryption",
    mockupReaderBadge2: "Signed ML-DSA-44 • Verified",
    mockupReaderBadge3: "Stored in France (2 replicas)",
    mockupMailBodyText: "Hello Alice,<br><br>Here are the encrypted attachments for our annual audit. Everything was sealed directly from my computer using our sovereign identity key. The chunks are replicated across sovereign French nodes and remain inaccessible to any third party.<br><br>Best regards,<br>Pierre",

    mockupFilesColName: "File Name",
    mockupFilesColSize: "Size",
    mockupFilesColReplicas: "Replication",
    mockupFilesColStatus: "Status",
    mockupProtected: "Protected in France",

    mockupIdHeading1: "Active Identity",
    mockupIdSub1: "Permanent sovereign account registered with anti-Sybil proof-of-work.",
    mockupIdHeading2: "Post-Quantum Keys",
    mockupIdSub2: "Hybrid signatures active against future quantum threats.",
    mockupIdHeading3: "Account Recovery",
    mockupIdSub3: "Reconstructs your identity on a new PC without passwords or SMS codes.",
    mockupIdHeading4: "Standard Crypto Engine",
    mockupIdSub4: "Official NIST FIPS 203/204 standards. Zero homemade crypto.",

    mockupSecHeading1: "Battle-Tested OpenSSL v3.5.2+ (Zero Homemade Crypto)",
    mockupSecDesc1: "CYBOU follows the golden rule of cybersecurity: \"Don't roll your own crypto\". No custom math: all algorithms rely directly on OpenSSL v3.5.2+ compliant with NIST FIPS 203 and FIPS 204 standards.",
    mockupSecHeading2: "Post-Quantum TLS 1.3 P2P Transport",
    mockupSecDesc2: "TLS 1.3 tunnel with hybrid X25519MLKEM768 group. Any classical fallback is strictly rejected (fails closed).",
    mockupSecHeading3: "Territorial France Sovereignty",
    mockupSecDesc3: "Public P2P traffic strictly restricted to French IP space (fails closed). Immune to US Cloud Act and FISA 702.",

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

    secOpensslTag: "Audited Global Standard",
    secOpensslTitle: "Standard OpenSSL v3.5.2+ Cryptography (Zero Homemade Crypto)",
    secOpensslDesc: "The golden rule in cybersecurity is clear: never invent your own cryptography (\"Don't roll your own crypto\"). CYBOU uses zero experimental or homemade crypto code. All post-quantum algorithms (ML-DSA-44, ML-DSA-65, ML-KEM-768) and symmetric primitives are powered by official OpenSSL v3.5.2+, certified and strictly compliant with NIST FIPS 203 and FIPS 204.",

    secTlsTag: "Network Protection",
    secTlsTitle: "100% Post-Quantum TLS 1.3 Encrypted Transport (CYP2)",
    secTlsDesc: "Every network connection enforces a post-quantum hybrid key exchange combining X25519 and NIST ML-KEM-768 (Kyber). Any classical fallback without ML-KEM is rejected. Neither your ISP, nor Wi-Fi eavesdroppers, nor future quantum supercomputers can decrypt your traffic.",

    secE2eeTag: "Zero-Knowledge Privacy",
    secE2eeTitle: "End-to-End Client Encryption & BLAKE3 Chunking",
    secE2eeDesc: "TLS protects the wire, but CYBOU also protects the data itself. Messages and files are encrypted on your CPU with private keys before departure. The blockchain consensus stores zero emails, zero file names, zero recipients: only opaque encrypted chunks exist.",

    secFranceTag: "Legal Immunity",
    secFranceTitle: "Territorial Sovereignty France & EU",
    secFranceDesc: "Zero servers in the US, zero dependence on Silicon Valley giants. Public P2P admission is strictly confined to France with rigorous geolocation enforcement (fails closed). Your data is fully immune to the US Cloud Act and FISA 702.",

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
    compRowCryptoCrit: "Cryptographic Engine & Code",
    compRowCryptoCybou: "Standard OpenSSL v3.5.2+ (Zero homemade crypto, NIST FIPS 203/204)",
    compRowCryptoGoogle: "Proprietary / Server black-box",
    compRowCryptoAppleMs: "Proprietary / Server black-box",
    compRow2Crit: "Network Transport Security",
    compRow2Cybou: "TLS 1.3 Hybride Post-Quantique (X25519 + ML-KEM-768)",
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
    ddTransport: "CYP2 encapsulated in strict post-quantum TLS 1.3 (mandatory X25519MLKEM768 group, fails closed). Powered by OpenSSL v3.5.2+ with 32-byte session key export bound to crypto proofs.",
    dtConsensus: "Current DEV finality",
    ddConsensus: "Hybrid PoA with one signer operated by CYBOU on DEV. Full nodes independently verify; no BFT claim. Target: Central Authority desktop finalizer.",
    dtBootstrap: "Bootstrap peers",
    ddBootstrap: "Ordinary CYBOU full peers with known initial locators (IP:port + TLS pin) for initial discovery; no special consensus capability, no finality role, running the exact same software.",
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
    ddKeys: "Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing, and Treasury are separate roles; official OpenSSL v3.5.2+ certified engine.",
    dtStack: "Technology stack",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL v3.5.2+, BLAKE3, and CYP2 transport over TLS 1.3.",

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
    faqA1: "CYBOU is a sovereign desktop privacy suite combining private messaging (Mail), encrypted cloud storage (Files), and an identity manager (Identity). Your data is encrypted on your machine before being sent, and all network transport is protected with post-quantum TLS 1.3.",
    faqTlsBadge: "Encrypted Transport",
    faqQTls: "How does post-quantum TLS 1.3 protect my connections on public Wi-Fi or with my ISP?",
    faqATls: "All communications between CYBOU peers and nodes (CYP2 protocol v3) are sealed inside a strict post-quantum TLS 1.3 tunnel with X25519MLKEM768 key exchange. Whether you use public Wi-Fi at a train station or hotel, or your home fiber internet, no third party or ISP can intercept or read your messages or know what files you transfer. Deep Packet Inspection (DPI) and quantum recording are completely blocked.",
    faqOpensslBadge: "Audited Standard",
    faqQOpenssl: "Are CYBOU's cryptographic algorithms developed in-house (\"homemade crypto\")?",
    faqAOpenssl: "Absolutely not. The primary rule of cybersecurity is: \"Don't roll your own crypto\" (never reinvent cryptography). CYBOU relies exclusively on the official OpenSSL v3.5.2+ library, globally audited and battle-tested. The post-quantum primitives are official NIST standards: ML-KEM-768 (FIPS 203) and ML-DSA-44/65 (FIPS 204). You get proven mathematical robustness without black boxes or uncertified code.",
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
    footBottomBadge: "European Digital Sovereignty & Research",
    footSecCompliance: "GDPR / NIS 2 Compliance",

    // Compliance Preview on Homepage
    compSecLabel: "Regulations & Standards",
    compSecTitle: "Rigorous compliance with European and French standards.",
    compSecDesc: "Discover how Zero-Knowledge architecture, OpenSSL v3.5.2+ cryptography, and sovereign France networking address the most demanding legal and regulatory frameworks.",
    compCardRgpdPill: "EU Regulation 2016/679",
    compCardRgpdStatus: "Privacy by Design",
    compCardRgpdTitle: "GDPR — Personal data under absolute protection",
    compCardRgpdDesc: "Systematic client-side encryption (Art. 25). Zero personal metadata in consensus. Cryptographic right to be forgotten and strict territorial immunity against third-country transfers (Schrems II).",
    compCardNis2Pill: "EU Directive 2022/2555",
    compCardNis2Status: "Art. 21 Risk Measures",
    compCardNis2Title: "NIS 2 Directive — Resilience & Supply Chain Security",
    compCardNis2Desc: "Audited OpenSSL v3.5.2+ cryptographic engine (software supply chain security). Operational resilience with no single point of failure, automated storage self-healing, and strict vulnerability disclosure policies.",
    compCardHdsPill: "CSP Art. L.1111-8",
    compCardHdsStatus: "Medical Secret & HDS",
    compCardHdsTitle: "Healthcare Sector — Medical Secrecy & Health Data",
    compCardHdsDesc: "Mathematical guarantee of confidentiality for professional secrecy (Art. 226-13 CP). When outsourcing to third-party commercial hosts, rigorous requirement for host datacenters to hold HDS and SecNumCloud certification.",
    compCardIsoPill: "ISO/IEC 27001:2022",
    compCardIsoStatus: "CIA Triad",
    compCardIsoTitle: "ISO 27001 & CIA Triad — Information Security",
    compCardIsoDesc: "Coverage of key Annex A controls (A.5.15, A.8.2, A.8.12, A.8.20, A.8.24). Respect without compromise for Availability, Integrity (BLAKE3 / Merkle), and Confidentiality (ML-KEM-768 / Zero-Knowledge).",
    compCtaTitle: "Read the exhaustive technical & legal compliance dossier",
    compCtaDesc: "Detailed articles, ISO 27001 mapping matrix, GDPR impact analysis, and French ANSSI regulatory framework.",
    compCtaBtn: "View Compliance Dossier",

    // Dedicated Compliance Page (conformite.html)
    cBreadHome: "Home",
    cBreadCurrent: "Regulatory Compliance & Standards",
    cHeroTag: "Legal Rigor & Technical Transparency",
    cHeroTitle: "Regulatory & Normative Compliance Dossier",
    cHeroDesc: "Technical and legal evaluation of CYBOU's architecture against GDPR, NIS 2 Directive, Healthcare Data Hosting (HDS / French Public Health Code), Medical Secrecy, ISO/IEC 27001, and French ANSSI recommendations.",
    cPillTransp: "Transparency & Role",
    cPillRgpd: "GDPR (EU 2016/679)",
    cPillNis2: "NIS 2 Directive",
    cPillHds: "Healthcare & HDS",
    cPillIso: "ISO/IEC 27001:2022",
    cPillCia: "CIA Triad",
    cPillAnssi: "ANSSI & Sovereignty",

    cArt0Pill: "Legal Foundation",
    cArt0Status: "Technical Honesty",
    cArt0Title: "Principle of Transparency: Software Architecture vs. Hosting Certification",
    cArt0Sub: "An honest distinction between mathematical software guarantees and statutory obligations placed on hosting infrastructure.",
    cArt0P1: "Across the cybersecurity and software industry, many vendors claim to be \"GDPR certified\" or \"HDS certified\" merely because they encrypt data. <strong>This claim is legally misleading.</strong> European and French regulations strictly separate two tiers of compliance:",
    cArt0L1Title: "Software and protocol tier (CYBOU):",
    cArt0L2Title: "Physical hosting and operational tier (Datacenters):",
    cArt0BoxTitle: "What this means in practice:",
    cArt0BoxDesc: "If a healthcare professional or hospital uses CYBOU to store patient records with a commercial third party, destination storage provider nodes must be hosted in HDS-certified datacenters. Conversely, in on-premise deployments (self-hosted on internal servers), the healthcare practitioner maintains full physical custody and enjoys post-quantum protection without third-party reliance.",

    cRgpdStatus: "Full Privacy by Design",
    cRgpdTitle: "GDPR — Personal Data Protection by Design and by Default",
    cRgpdSub: "How CYBOU's Zero-Knowledge architecture and blind consensus structurally prevent non-compliance risks.",
    cRgpdP1: "The GDPR imposes stringent obligations of security, minimization, and data subject rights on data controllers and processors. CYBOU's architecture was engineered from day one to satisfy these requirements natively:",
    cRgpdThArt: "GDPR Article",
    cRgpdThReq: "Legal Requirement",
    cRgpdThSol: "CYBOU Technical Implementation",
    cRgpdArt25Req: "Ensure data protection by design and default without burdensome manual action.",
    cRgpdArt25Sol: "<strong>Systematic client-side encryption.</strong> No plaintext, file, or metadata ever leaves the client workstation without prior encryption via local secret keys.",
    cRgpdArt5Req: "Data must be adequate, relevant, and limited to what is strictly necessary.",
    cRgpdArt5Sol: "<strong>Zero personal data in consensus.</strong> Neither the PoA ledger nor the P2P network knows real identities, phone numbers, persistent public IPs, filenames, or email subjects. Only opaque ChunkIDs (BLAKE3-256) circulate.",
    cRgpdArt17Req: "Ensure the permanent and irreversible erasure of personal data upon request.",
    cRgpdArt17Sol: "<strong>Crypto-shredding.</strong> Destroying the local symmetric key in the access capsule renders stored remote chunks mathematically indistinguishable from random noise forever, satisfying EDPB deletion standards.",
    cRgpdArt20Req: "Enable data export and porting in an open, structured format.",
    cRgpdArt20Sol: "<strong>Deterministic 24-word recovery phrase.</strong> Users can reconstruct their entire inbox, files, and address book on any computer using open key derivation standards.",
    cRgpdArt32Req: "Implement appropriate technical security measures including encryption and system resilience.",
    cRgpdArt32Sol: "<strong>Certified standard OpenSSL v3.5.2+ cryptography.</strong> NIST FIPS 203 (ML-KEM-768) and FIPS 204 (ML-DSA-44/65) algorithms, strict TLS 1.3 tunnels, and self-healing multi-node chunk replication.",
    cRgpdArt44Req: "Prohibition of unauthorized international transfers to third countries without adequate safeguards (Schrems II).",
    cRgpdArt44Sol: "<strong>Strict territorial sovereignty (France-only).</strong> Public P2P admission is locally geo-filtered (fails closed). Zero US servers: immune to the US Cloud Act and FISA Section 702.",

    cNis2Status: "Risk Management Measures",
    cNis2Title: "NIS 2 Directive — Operational Resilience & Supply Chain Security",
    cNis2Sub: "Meeting cybersecurity obligations for Essential and Important Entities across critical sectors.",
    cNis2P1: "EU Directive NIS 2 imposes rigorous obligations across 18 critical sectors (healthcare, energy, transport, digital infrastructure). CYBOU aligns directly with Article 21 requirements:",
    cNis2Item1Title: "Software Supply Chain Security:",
    cNis2Item1Desc: "CYBOU strictly abides by the principle \"Don't roll your own crypto\". No homemade algorithms are permitted. All cryptographic primitives stem directly from official OpenSSL v3.5.2+, maintained by the global community with continuous audits.",
    cNis2Item2Title: "Anticipation of Quantum Threats (ENISA & ANSSI):",
    cNis2Item2Desc: "The risk of \"Harvest Now, Decrypt Later\" attacks is mitigated today via hybrid TLS 1.3 handshakes (X25519MLKEM768) and quantum-resistant identity signatures (ML-DSA-44/65).",
    cNis2Item3Title: "Resilience, Business Continuity & Disaster Recovery:",
    cNis2Item3Desc: "CYBOU's distributed storage architecture eliminates single points of failure (SPOF). Node outages trigger autonomous background health audits and repairs between independent replicas.",
    cNis2Item4Title: "Incident Handling & Vulnerability Disclosure (SECURITY.md):",
    cNis2Item4Desc: "A documented vulnerability disclosure policy (SECURITY.md) ensures responsible reporting, aligned with the Cyber Resilience Act (CRA) requirements.",

    cHdsStatus: "French Public Health Code L.1111-8",
    cHdsTitle: "Healthcare Sector — Professional Secrecy, Health Data & HDS",
    cHdsSub: "Technical guarantees for healthcare professionals, clinics, laboratories, and hospitals.",
    cHdsP1: "Medical data represents the most sensitive category of personal data (GDPR Article 9). In France, handling is governed by both the Penal Code and the Public Health Code.",
    cHdsH1: "1. Absolute Respect for Medical Secrecy (Penal Code Art. 226-13)",
    cHdsP2: "Article 226-13 punishes breaches of professional confidentiality. With CYBOU Mail and Files, physicians and radiologists ensure no technical intermediary can eavesdrop: records and medical imaging are encrypted locally on consultation workstations before transfer.",
    cHdsH2: "2. Health Data Hosting (HDS — CSP Art. L.1111-8)",
    cHdsP3: "Under French law, hosting patient personal health data gathered during diagnosis or care requires an ANS-certified HDS provider. Here is how CYBOU operationalizes this requirement:",
    cHdsThMode: "Usage Scenario",
    cHdsThCadre: "Regulatory Framework",
    cHdsThCybou: "CYBOU Compliance",
    cHdsMode1Cadre: "The hospital or private clinic retains data on internal servers and local terminals.",
    cHdsMode1Cybou: "<strong>Compliant without third-party certification.</strong> The data controller maintains direct custody. Maximum post-quantum encryption and airtight information isolation.",
    cHdsMode2Cadre: "Encrypted data is outsourced to remote storage providers operated by third parties.",
    cHdsMode2Cybou: "<strong>Mandatory HDS Infrastructure.</strong> Storage provider nodes (cybou-provider) must be hosted in datacenters holding HDS accreditation. CYBOU's Zero-Knowledge encryption provides an additional, superior layer of mathematical defense.",
    cHdsBoxTitle: "Healthcare Integrity Commitment:",
    cHdsBoxDesc: "CYBOU does not replace physical datacenter HDS certification. Instead, CYBOU provides the most secure software layer available to shield medical data from ransomware, foreign surveillance, and datacenter insider threats.",

    cIsoStatus: "Annex A — Security Controls",
    cIsoTitle: "ISO/IEC 27001:2022 — Control Mapping Matrix",
    cIsoSub: "How CYBOU's technical architecture helps organizations fulfill Information Security Management System (ISMS) controls.",
    cIsoP1: "ISO/IEC 27001 specifies best practices for protecting information assets. The table below details CYBOU's direct support for Annex A controls (2022 revision):",
    cIsoThCode: "Annex A Control",
    cIsoThName: "Control Objective",
    cIsoThSol: "CYBOU Implementation",
    cIsoA515Req: "Access control and authentication",
    cIsoA515Sol: "Strong passwordless authentication. Hybrid ML-DSA-44 and Ed25519 signatures for every sovereign identity operation.",
    cIsoA82Req: "Privileged access rights management",
    cIsoA82Sol: "Zero-Knowledge model: no network administrator or PoA operator possesses the cryptographic capability to open user data capsules.",
    cIsoA812Req: "Data leakage prevention (DLP)",
    cIsoA812Sol: "Systematic client-side encryption before transmission. Opaque BLAKE3-256 chunking prohibits content reconstruction without private keys.",
    cIsoA814Req: "Redundancy of processing facilities",
    cIsoA814Sol: "Automated multi-replica storage across distinct geographical provider nodes with continuous integrity verification.",
    cIsoA820Req: "Network security and boundary protection",
    cIsoA820Sol: "P2P protocol encapsulated in TLS 1.3 with hybrid X25519MLKEM768 handshake and strict SPKI pinning. France-only geo-filtering.",
    cIsoA824Req: "Use of cryptography and key management",
    cIsoA824Sol: "Atomic key generation and rotation (IdentityRotate), standard NIST FIPS 203/204 via OpenSSL v3.5.2+, protected 24-word recovery phrase.",

    cCiaStatus: "CIA Triad",
    cCiaTitle: "CIA Triad (Confidentiality, Integrity, Availability)",
    cCiaSub: "Detailed breakdown of the three foundational security pillars engineered into CYBOU.",
    cCiaC: "1. Confidentiality (C)",
    cCiaCDesc: "<strong>Total Zero-Knowledge:</strong> Leading-edge asymmetric and symmetric encryption (OpenSSL v3.5.2+). Decryption capsules strictly confined to authorized recipients. Sealed local application database. Post-quantum defense against current and future interception.",
    cCiaI: "2. Integrity (I)",
    cCiaIDesc: "<strong>Mathematical Inalterability:</strong> Every stored fragment is keyed by its BLAKE3-256 hash. Publication trees are verified via Merkle proofs and PoA consensus with fail-closed anti-equivocation journal.",
    cCiaA: "3. Availability (A)",
    cCiaADesc: "<strong>Continuous Resilience:</strong> Each encrypted chunk is replicated across independent storage providers. Proactive health checks and self-healing repair without user interruption.",

    cAnssiStatus: "National Regulatory Framework",
    cAnssiTitle: "ANSSI & French Digital Sovereignty",
    cAnssiSub: "Alignment with the recommendations of the French National Cybersecurity Agency.",
    cAnssiP1: "ANSSI defines cybersecurity guidelines for the French State, Operators of Vital Importance (OIV), and businesses. CYBOU is directly aligned with these guidelines:",
    cAnssiItem1Title: "Post-Quantum Transition & Hybridization:",
    cAnssiItem1Desc: "ANSSI explicitly recommends a hybrid approach (combining an established classical algorithm with a standardized post-quantum scheme) to prevent security regressions. CYBOU rigorously adheres to this by combining Ed25519 + ML-DSA for signatures and X25519 + ML-KEM-768 for key agreement.",
    cAnssiItem2Title: "Declaration Regime for Cryptology Means (CPCE):",
    cAnssiItem2Desc: "Pursuant to French CPCE articles L. 133-1 et seq., the use of cryptology means providing confidentiality and authentication is completely unrestricted for individuals and businesses.",
    cAnssiItem3Title: "Data Sovereignty & Trusted Cloud (SecNumCloud):",
    cAnssiItem3Desc: "For public sector and sensitive enterprise deployments, CYBOU storage nodes are designed to run on SecNumCloud-qualified cloud infrastructures, ensuring complete immunity against non-EU extraterritorial laws.",
    cAnssiCtaTitle: "Need a Compliance Evaluation for Your Organization?",
    cAnssiCtaDesc: "Our technical and legal team is available to assist with on-premise deployments, GDPR compliance roadmaps, and HDS storage integration.",
    cAnssiCtaBtn: "Contact the Team",

    footProdEnterprise: "CYBOU Enterprise",
    footSecEnterprise: "Dedicated Private Networks",
    srvEnterpriseTag: "Custom",
    srvEnterpriseTitle: "CYBOU Enterprise — Dedicated & Isolated Private Network",
    srvEnterpriseDesc: "Instantiate your own airtight sovereign network: custom genesis block, PoA key controlled by your IT/SecOps, on-premise or air-gapped storage, and zero data leakage.",
    srvEnterpriseLink: "Explore enterprise solutions &rarr;",

    // Dedicated Enterprise Page (entreprise.html)
    eBreadHome: "Home",
    eBreadCurrent: "Enterprise Solutions & Dedicated Networks",
    eHeroBadge: "DEDICATED DEPLOYMENT & SOVEREIGN ISOLATION",
    eHeroBadgeText: "Cryptographic Isolation & Dedicated Governance",
    eHeroTitle: "Build Your Secure & Dedicated Enterprise Private Network",
    eHeroDesc: "For French and international organizations: Deploy an autonomous, cryptographically airtight CYBOU private network appliance governed by your IT/SecOps. Standard OpenSSL v3.5.2+ post-quantum encryption, On-Premise or Air-Gap storage, and absolute legal immunity.",

    ePillAdv: "Key Advantages",
    ePillArch: "Isolation & Genesis",
    ePillModes: "Dedicated Network",
    ePillGov: "IT Governance",
    ePillUse: "Use Cases",
    ePillPricing: "Pricing & Budget",
    ePillContact: "Deployment & PoC",

    eArt1Pill: "Maximum Sovereignty",
    eArt1Status: "Total Autonomy",
    eArt1Title: "Why deploy a dedicated CYBOU network for your organization?",
    eArt1Sub: "Shared public networks may not always fulfill the extreme mandates of defense, critical industries, or confidential corporate governance. CYBOU enables a fully isolated private network bubble.",

    eCard1Pill: "Custom Genesis",
    eCard1Badge: "Airtight",
    eCard1Title: "Cryptographic Isolation & Global Reach",
    eCard1Desc: "Unlike the public network restricted to France, your private appliance can be deployed by any French or international organization. Operating on a custom genesis block, root keys, and dedicated SPKI fingerprints, foreign packets are mathematically rejected (fail-closed).",

    eCard2Pill: "IT / SecOps Control",
    eCard2Badge: "Governance",
    eCard2Title: "Authority Key Held Solely by Your Organization",
    eCard2Desc: "PoA (Proof-of-Authority) consensus finality is operated on a hardened workstation under the exclusive command of your IT leadership. Zero external intermediaries, zero third parties with authority over your chain.",

    eCard3Pill: "Zero-Knowledge",
    eCard3Badge: "Zero Backdoors",
    eCard3Title: "Total Confidentiality Among Employees",
    eCard3Desc: "All communications and files are encrypted end-to-end on employee endpoints via OpenSSL v3.5.2+ (ML-KEM-768 and ML-DSA). Even system administrators cannot read executive board correspondence without legitimate capsule keys.",

    eCard4Pill: "P2P Resilience",
    eCard4Badge: "Zero SPOF",
    eCard4Title: "Distributed Storage with No Single Point of Failure",
    eCard4Desc: "Files and messages are chunked (BLAKE3-256) and replicated across internal servers or remote datacenters. Hardware failures or severed WAN connections do not interrupt service: the mesh repairs itself automatically.",

    eArt2Pill: "Under the Hood",
    eArt2Status: "Open Specifications",
    eArt2Title: "How does private network isolation work?",
    eArt2Sub: "All participants run the exact same battle-tested core software, but the cryptographic boundary is strictly confined to your enterprise.",
    eArt2P1: "CYBOU's unified architecture enables spinning up an independent enterprise network in minutes without altering a single line of source code. The protocol isolates networks through their genesis definition and transport credentials:",

    eStep1Title: "Genesis Definition",
    eStep1Desc: "Generation of a genesis.json file signed by the offline Network Private Key, with a custom enterprise network identifier and initial authority parameters.",
    eStep2Title: "PoA Central Authority",
    eStep2Desc: "IT SecOps generates the PoA finalization key on a secured internal machine to seal enterprise blocks.",
    eStep3Title: "Initial Bootstrap Peers (1 to 4)",
    eStep3Desc: "Deployment of 1 to 4 ordinary CYBOU full peers with known locators across your corporate intranet for initial peer discovery and synchronization.",
    eStep4Title: "Private Storage Cluster",
    eStep4Desc: "Enabling storage provider capability (cybou-node provider run) on internal servers to host encrypted chunk replicas.",

    eCompTableTitle: "Comparison Matrix: Public Network vs. Dedicated Enterprise Network",
    eThParam: "Feature",
    eThPublic: "Public CYBOU Network",
    eThPrivate: "Dedicated Enterprise Network",
    eRowGen: "Genesis Block",
    eRowGenPub: "Public sovereign genesis (France)",
    eRowGenPriv: "<strong>Dedicated private genesis</strong> (Unique network ID per organization)",
    eRowPoa: "Consensus Operator (PoA)",
    eRowPoaPub: "CYBOU reference authority",
    eRowPoaPriv: "<strong>Hardened workstation of your IT / SecOps</strong> (Exclusive sovereignty)",
    eRowBoot: "Bootstrap Peers",
    eRowBootPub: "Public peers geo-located in France",
    eRowBootPriv: "<strong>1 to 4 internal bootstrap peers</strong> (Known initial locators on your LAN / Intranet / VPN)",
    eRowStorage: "Storage Infrastructure",
    eRowStoragePub: "Qualified storage providers in France",
    eRowStoragePriv: "<strong>On-Premise enterprise servers</strong> or qualified private cloud",
    eRowPerim: "Network Perimeter & Access",
    eRowPerimPub: "Internet (French IP space only)",
    eRowPerimPriv: "<strong>Client-defined perimeter (France or International)</strong>: Enterprise Intranet, private VPN, or closed Air-Gap enclave",
    eRowCrypto: "Transport & Cryptography",
    eRowCryptoPub: "Hybrid post-quantum TLS 1.3 via OpenSSL 3.5.2+",
    eRowCryptoPriv: "<strong>Identical: Post-quantum TLS 1.3 + strict enterprise SPKI pinning</strong>",

    eArt3Pill: "Dedicated private network",
    eArt3Status: "Governed by your organization",
    eArt3Title: "A CYBOU network dedicated to your company",
    eArt3Sub: "We create a distinct private network, cryptographically isolated from other CYBOU networks and controlled by your company's administration.",
    eArt3P1: "The network uses CYBOU software with its own genesis definition, authority keys, and network parameters. Your organization holds the PoA finalization key; CYBOU's Central Authority does not administer or finalize your network.",
    eArt3F1: "Your administration defines the bootstrap nodes and their IP addresses.",
    eArt3F2: "IP-based peer admission rules are configured for your network.",
    eArt3F3: "Your organization chooses the infrastructure on which the nodes run.",
    eArt3P2: "The infrastructure and connectivity are the client's choice; they do not create different types of CYBOU network.",

    eArt4Pill: "Operational Management",
    eArt4Status: "Production-Grade",
    eArt4Title: "Governance, Administration & Operational Simplicity",
    eArt4Sub: "Enterprise-grade tooling to govern your fleet of servers and endpoints without unnecessary complexity.",
    eArt4P1: "Unlike cumbersome legacy enterprise software requiring fragile directory servers and disparate middleware, CYBOU relies on a clean, unified architecture:",

    eGovItem1Title: "A single unified binary across the infrastructure:",
    eGovItem1Desc: "The exact same cybou-node binary acts as a full node, bootstrap node, and storage provider. It runs headlessly under Linux as a standard systemd service with negligible memory footprint, easily deployable via Ansible, Puppet, or Terraform.",
    eGovItem2Title: "Intuitive desktop application for team members:",
    eGovItem2Desc: "Employees run CYBOU Desktop on workstations (Windows, Linux, macOS). An intuitive interface combining encrypted mail, file drive, and sovereign identity with zero employee retraining needed.",
    eGovItem3Title: "Elimination of vulnerable passwords and compromised directory stores:",
    eGovItem3Desc: "No centralized LDAP or Active Directory databases vulnerable to Golden Ticket or credential stuffing attacks. Each employee holds an inviolable 24-word recovery phrase and locally sealed asymmetric keypairs.",
    eGovItem4Title: "Key lifecycle and rotation (IdentityRotate):",
    eGovItem4Desc: "When changing hardware or upon suspected endpoint compromise, the atomic IdentityRotate transaction revokes and refreshes signature and encryption keys instantly without losing identity history.",
    eGovItem5Title: "Disaster recovery and deterministic BCP / DRP:",
    eGovItem5Desc: "In the event of physical disaster destroying primary facilities, possession of the genesis key and 24-word phrase enables deterministic re-instantiation and cryptographic data reconstruction.",

    eArt5Pill: "Industry Applications",
    eArt5Status: "High-Security Sectors",
    eArt5Title: "Strategic use cases for demanding organizations",
    eArt5Sub: "Engineered to protect your organization's most critical assets: proprietary knowledge, corporate secrets, and sovereignty.",

    eUse1Pill: "Advanced Manufacturing",
    eUse1Badge: "Trade Secrets",
    eUse1Title: "R&D, Intellectual Property & Patents",
    eUse1Desc: "Absolute protection of blueprints, chemical formulations, proprietary source code, and go-to-market strategies against foreign industrial espionage and ransomware.",

    eUse2Pill: "Defense & Critical Infra",
    eUse2Badge: "NIS 2 Directive",
    eUse2Title: "Critical Infrastructure & Essential Entities",
    eUse2Desc: "Tactical communication and decentralized backup network for Operators of Vital Importance, satisfying NIS 2 resilience mandates during crisis events.",

    eUse3Pill: "Executive Governance",
    eUse3Badge: "C-Level Confidential",
    eUse3Title: "Executive Board, M&A & Legal Counsel",
    eUse3Desc: "Airtight virtual data rooms and ultra-confidential correspondence for executive boards, M&A transactions, litigation, and audits—with zero risk of exposure by sysadmins or third-party SaaS.",

    eUse4Pill: "International Groups",
    eUse4Badge: "Global Footprint",
    eUse4Title: "Headquarters & Foreign Subsidiaries",
    eUse4Desc: "Airtight communication and transfer channel between corporate HQ (in France or abroad) and international offices, protected against local state surveillance and public network eavesdropping.",

    // Economic Model & Pricing
    eArtPricePill: "Economic Model & ROI",
    eArtPriceStatus: "Transparent Pricing in Euros",
    eArtPriceTitle: "Budget Estimation for Your Private CYBOU Network",
    eArtPriceSub: "A transparent flat-rate investment in Euros (€), with zero per-user recurring license fees and rapid ROI from month one.",
    ePriceT1Title: "Sovereign Private Network Appliance",
    ePriceT1Amount: "From €15,000 <span style=\"font-size: 1rem; font-weight: 500; color: var(--text-muted);\">excl. VAT</span>",
    ePriceT1Unit: "Core deployment • Unlimited workstations",
    ePriceT1Desc: "Industrial-grade rollout of your private network on your infrastructure (On-Premise servers, Air-Gap, or private VMs), with zero per-user licensing restrictions.",
    ePriceT1F1: "Unlimited CYBOU Desktop workstations (zero recurring license per user)",
    ePriceT1F2: "Dedicated private enterprise genesis & cryptographically isolated Network ID",
    ePriceT1F3: "Hardened PoA authority station under direct IT & SecOps control",
    ePriceT1F4: "Cluster of 4 bootstrap & replicated storage nodes (zero SPOF)",
    ePriceT1F5: "Deterministic DRP/BCP procedures documentation & IT admin training",
    ePriceT2Badge: "VIP Guidance & Full Support",
    ePriceT2Title: "Turnkey Appliance & VIP Support",
    ePriceT2Amount: "From €35,000 <span style=\"font-size: 1rem; font-weight: 500; color: var(--text-muted);\">excl. VAT</span>",
    ePriceT2Unit: "Complete deployment • Level 3 Engineering Support & Maintenance",
    ePriceT2Desc: "All-inclusive turnkey solution for organizations requiring tailored integration, VIP engineering guidance, and direct support from core protocol architects.",
    ePriceT2F1: "Full core Private Appliance scope (unlimited seats, PoA station, 4-node cluster)",
    ePriceT2F2: "Tailored VIP advisory for infrastructure sizing, enclaves, and integrations",
    ePriceT2F3: "Priority Level 3 direct support channel with core protocol engineers",
    ePriceT2F4: "Continuous tracking & rollout of OpenSSL v3.5.2+ and protocol security updates",
    ePriceT2F5: "Annual cryptographic security review, resilience audits & compliance support",
    ePriceDisclaimer: "* Indicative pricing excluding VAT ('from'): travel expenses (flights, lodging), bespoke feature modifications or tailored integrations, and extended on-site engineering services are quoted separately based on your technical specifications.",
    eRoiTitle: "€0 monthly license fee per user (Massive savings vs. US SaaS)",
    eRoiDesc: "Unlike SaaS suites (Microsoft 365, Google Workspace) billing €20 to €35/user/month—costing €30,000/year for 100 seats and €150,000/year for 500 seats—a CYBOU private network appliance achieves full ROI within months. Your organization remains the sole owner of its data and infrastructure, immune to unilateral subscription price hikes.",

    eArt6Pill: "Take Action",
    eArt6Status: "Pilot & Deployment",
    eArt6Title: "Deploy your first CYBOU pilot network",
    eArt6Sub: "Our architecture is fully documented, open, and verifiable. Set up an internal proof-of-concept or connect with our engineering team.",
    eContactP1: "Interested in evaluating CYBOU in your test lab or designing a hardened enclave for executive teams? Here are your next steps:",
    eCtaTitle: "Connect with the CYBOU engineering team for a private network",
    eCtaDesc: "Assistance with custom genesis generation, infrastructure sizing recommendations, and compliance audits.",
    eCtaBtn: "Contact the Engineering Team",

    // Comparison Teaser on Homepage
    compCardNextPill: "Open Source & Self-Hosting",
    compCardNextStatus: "P2P vs. LAMP",
    compCardNextTitle: "CYBOU vs. Nextcloud — Zero-Knowledge vs. PHP Server",
    compCardNextDesc: "Nextcloud encrypts server-side by default (system administrators can inspect your files) and relies on a fragile stack (MySQL/PHP/Apache). CYBOU enforces client-side encryption on your endpoint and operates on an autonomous C++ P2P binary with no central database.",
    compCardDropPill: "US Cloud Storage",
    compCardDropStatus: "Cloud Act & AI Scans",
    compCardDropTitle: "CYBOU vs. Dropbox — Legal Immunity vs. AI Scans",
    compCardDropDesc: "Dropbox is subject to the US Cloud Act and integrates AI features analyzing your documents. CYBOU restricts its network to France (fail-closed), mathematically isolates every chunk (BLAKE3-256), and makes AI scanning or training impossible.",
    compCardGafamPill: "Centralized US Suites",
    compCardGafamStatus: "Digital Freedom",
    compCardGafamTitle: "CYBOU vs. Google Workspace & Microsoft 365",
    compCardGafamDesc: "Big Tech vendors hold your decryption keys and demand mobile phone numbers and credit cards. CYBOU returns full ownership of your account via a 24-word recovery phrase and inalienable post-quantum keys.",
    compCardPqPill: "Global Standard OpenSSL v3.5.2+",
    compCardPqStatus: "Post-Quantum",
    compCardPqTitle: "Immediate Quantum Resistance (NIST FIPS 203/204)",
    compCardPqDesc: "While Nextcloud, Dropbox, and Microsoft remain confined to legacy algorithms (RSA/ECC) vulnerable to future decryption attacks ('Harvest Now, Decrypt Later'), CYBOU natively deploys ML-KEM-768 and ML-DSA today.",
    compTeaserCtaTitle: "View the Complete & Detailed Comparison Matrix",
    compTeaserCtaDesc: "Comprehensive table across 8 security criteria, deep architectural breakdown vs. Nextcloud and Dropbox, and decision guide for CISOs.",
    compTeaserCtaBtn: "Explore the Full Comparison",

    // Dedicated Comparison Page (comparatif.html)
    cmpBreadHome: "Home",
    cmpBreadCurrent: "Objective Solution Comparison",
    cmpHeroTag: "Technical & Sovereign Analysis",
    cmpHeroTitle: "CYBOU vs. Nextcloud, Dropbox, Google & Microsoft",
    cmpHeroDesc: "Deeply understand what differentiates the sovereign CYBOU suite from centralized US clouds (Google, Microsoft, Dropbox) and traditional self-hosted servers (Nextcloud). P2P architecture, OpenSSL v3.5.2+ post-quantum cryptography, legal extraterritorial immunity, and Zero-Knowledge data model.",
    cmpPillMatrix: "Complete Matrix",
    cmpPillNextcloud: "CYBOU vs. Nextcloud",
    cmpPillDropbox: "CYBOU vs. Dropbox",
    cmpPillBigTech: "CYBOU vs. Big Tech",
    cmpPillSummary: "Synthesis & Decision",

    cmpTabAll: "Complete Matrix (All)",
    cmpTabNextcloud: "vs. Nextcloud",
    cmpTabDropbox: "vs. Dropbox",
    cmpTabGoogle: "vs. Google Workspace",
    cmpTabMsApple: "vs. Microsoft 365 / Apple",

    cmpMatPill: "Comprehensive Comparison",
    cmpMatStatus: "8 Core Criteria",
    cmpMatTitle: "Complete Matrix of Collaboration & Storage Solutions",
    cmpMatSub: "A direct and impartial comparison covering security, architecture, cryptography, and legal jurisdiction.",
    cmpThCrit: "Protection Criterion",
    cmpThCybou: "CYBOU Desktop",
    cmpThNextcloud: "Nextcloud",
    cmpThDropbox: "Dropbox",
    cmpThGoogle: "Google Workspace",
    cmpThMsApple: "Microsoft 365 / Apple",

    cmpR1Crit: "Message and file encryption",
    cmpR1Cybou: "Mandatory client-side (Zero-Knowledge)",
    cmpR1Nextcloud: "Server-side by default (admin reads all); complex optional E2EE",
    cmpR1Dropbox: "Server-side encryption (Dropbox holds master keys)",
    cmpR1Google: "Server-side encryption (Google holds master keys)",
    cmpR1MsApple: "Server-side encryption (Vendor holds master keys)",

    cmpR2Crit: "Cryptographic engine and code",
    cmpR2Cybou: "Standard OpenSSL v3.5.2+ (NIST FIPS 203/204, zero homemade crypto)",
    cmpR2Nextcloud: "PHP / legacy OpenSSL (traditional RSA / AES)",
    cmpR2Dropbox: "Closed proprietary black box",
    cmpR2Google: "Closed proprietary black box",
    cmpR2MsApple: "Closed proprietary black box",

    cmpR3Crit: "Resistance to quantum attacks",
    cmpR3Cybou: "Native: ML-KEM-768 (TLS 1.3) + ML-DSA-44/65 (Identities)",
    cmpR3Nextcloud: "No post-quantum protection",
    cmpR3Dropbox: "No post-quantum protection",
    cmpR3Google: "Legacy algorithms (vulnerable RSA / ECC)",
    cmpR3MsApple: "Legacy algorithms (vulnerable RSA / ECC)",

    cmpR4Crit: "Architecture & Single Point of Failure",
    cmpR4Cybou: "Unified decentralized C++ P2P, self-healing chunk replicas",
    cmpR4Nextcloud: "Client-Server (SPOF: MySQL + Web server + Redis + PHP)",
    cmpR4Dropbox: "Centralized US cloud",
    cmpR4Google: "Centralized US cloud",
    cmpR4MsApple: "Centralized US cloud",

    cmpR5Crit: "Jurisdiction & Extraterritorial laws",
    cmpR5Cybou: "France & EU (France-only geo-filtered P2P, Cloud Act immunity)",
    cmpR5Nextcloud: "Depends on hosting provider (subject to Cloud Act if AWS/Azure)",
    cmpR5Dropbox: "United States (Fully subject to Cloud Act and FISA 702)",
    cmpR5Google: "United States (Subject to US Cloud Act and FISA 702)",
    cmpR5MsApple: "United States (Subject to US extraterritorial statutes)",

    cmpR6Crit: "Scanning for ads or AI model training",
    cmpR6Cybou: "Zero scans, zero AI, zero ads (opaque cryptographic chunks)",
    cmpR6Nextcloud: "Zero scans by default (optional AI plugins)",
    cmpR6Dropbox: "Content scans and third-party AI integrations (Dropbox Dash)",
    cmpR6Google: "Algorithmic indexing for advertising and AI models",
    cmpR6MsApple: "Intensive telemetry and Copilot integrations",

    cmpR7Crit: "Authentication & Account ownership",
    cmpR7Cybou: "24-word recovery phrase, zero passwords, zero SMS",
    cmpR7Nextcloud: "Passwords stored in MySQL database or LDAP directory",
    cmpR7Dropbox: "Server-managed passwords, account revokable unilaterally",
    cmpR7Google: "Mandatory mobile phone number, risk of unilateral account ban",
    cmpR7MsApple: "Account tied to credit card and phone, proprietary lock-in",

    cmpR8Crit: "Deployment & Maintenance overhead",
    cmpR8Cybou: "Standalone C++ binary with zero external dependencies",
    cmpR8Nextcloud: "Heavy deployment (LAMP stack, SQL databases, tricky migrations)",
    cmpR8Dropbox: "Simple for end-users, but proprietary lock-in",
    cmpR8Google: "Managed SaaS, but total loss of operational control",
    cmpR8MsApple: "Managed SaaS, but critical commercial dependency",

    cmpNextPill: "Open Source Analysis",
    cmpNextStatus: "P2P vs. LAMP Architecture",
    cmpNextTitle: "CYBOU vs. Nextcloud: Why Zero-Knowledge P2P Outperforms PHP Client-Server",
    cmpNextSub: "Nextcloud is a remarkable first-generation open-source solution, but its traditional client-server architecture suffers from structural limitations that CYBOU has solved.",
    cmpNextP1: "Nextcloud popularized self-hosted collaborative suites. However, its technical foundation—conceived over 15 years ago—relies on a LAMP stack (Linux, Apache, MySQL, PHP) designed for traditional web apps, rather than decentralized cryptographic resilience:",
    cmpNextItem1Title: "The illusion of server-side encryption:",
    cmpNextItem1Desc: "By default, Nextcloud stores files in plaintext on the server or encrypts them with keys stored on the server itself. Any system administrator with SSH or root privileges can read all user files. Nextcloud's end-to-end encryption (E2EE) module is a complex add-on, notorious for synchronization failures and lost keys. With CYBOU, Zero-Knowledge client-side encryption is the mandatory and unalterable foundation of the protocol.",
    cmpNextItem2Title: "P2P resilience vs. Single Point of Failure (SPOF):",
    cmpNextItem2Desc: "A Nextcloud server is a single point of failure: if the MySQL database is corrupted, the PHP service crashes, or local disk space exhausts, the entire organization is paralyzed. CYBOU eliminates centralized SQL databases. Files are partitioned into BLAKE3-256 cryptographic chunks, distributed and automatically repaired across independent servers.",
    cmpNextItem3Title: "Post-Quantum Transition (NIST FIPS 203/204):",
    cmpNextItem3Desc: "Nextcloud relies on legacy RSA and ECC primitives, now known to be vulnerable to future quantum cryptanalysis. CYBOU natively integrates post-quantum cryptography via standard OpenSSL v3.5.2+ (ML-KEM-768 and ML-DSA).",
    cmpNextItem4Title: "Maintenance and operational footprint:",
    cmpNextItem4Desc: "Administering Nextcloud requires maintaining a web server, PHP runtime, relational database, Redis cache, and navigating delicate database schema migrations. CYBOU deploys as a single C++ binary with zero external dependencies, consuming minimal RAM and CPU.",

    cmpDropPill: "US Cloud Storage",
    cmpDropStatus: "Cloud Act & AI Risks",
    cmpDropTitle: "CYBOU vs. Dropbox: The Hazards of Extraterritoriality and AI Scanning",
    cmpDropSub: "Dropbox remains one of the most widely used enterprise and consumer services, yet it presents major liabilities for corporate secrecy and digital sovereignty.",
    cmpDropP1: "Behind its polished interface, Dropbox operates under an economic and legal framework fundamentally incompatible with European professional secrecy:",
    cmpDropItem1Title: "Full exposure to the US Cloud Act and FISA 702:",
    cmpDropItem1Desc: "As an entity incorporated under US law, Dropbox is compelled to grant US federal agencies and courts access to stored data—even when belonging to French or European organizations—without prior notification. CYBOU is anchored in France with fail-closed geo-filtering guaranteeing full immunity from foreign extraterritorial subpoenas.",
    cmpDropItem2Title: "Data exploitation and scanning for AI tools:",
    cmpDropItem2Desc: "Dropbox sparked significant controversy by enabling third-party AI features by default (Dropbox Dash / AI), sharing file contents with external vendors (such as OpenAI). With CYBOU, stored file chunks are mathematically indecipherable to hosting nodes: no indexing, content scanning, or AI model training is physically possible.",
    cmpDropItem3Title: "Unilateral possession of master decryption keys:",
    cmpDropItem3Desc: "Dropbox encrypts files 'at rest', but holds the master keys. A rogue employee, misconfiguration, or legal order is sufficient to expose stored records. With CYBOU, only your personal devices hold decryption keys derived from your 24-word recovery phrase.",

    cmpGafamPill: "Big Tech Monopolies",
    cmpGafamStatus: "Digital Emancipation",
    cmpGafamTitle: "CYBOU vs. Big Tech: Reclaiming Control Over Communications",
    cmpGafamSub: "Why suites from Google, Microsoft, and Apple fail to provide authentic confidentiality for European decision-makers.",
    cmpGafamP1: "Google Workspace (Gmail, Drive) and Microsoft 365 (Outlook, OneDrive) dominate enterprise office suites. This hegemony creates three critical problems solved by CYBOU:",
    cmpGafamItem1Title: "The false promise of 'managed encryption':",
    cmpGafamItem1Desc: "Tech giants claim to encrypt data, but they manage the HSM modules and keys themselves. This architecture enables automated algorithms to continuously scan emails and documents to feed AI models, ad networks, and telemetry. CYBOU enforces genuine end-to-end encryption: data is sealed before leaving your device's processor.",
    cmpGafamItem2Title: "Vendor lock-in and pricing leverage:",
    cmpGafamItem2Desc: "Unilateral price increases imposed on enterprises and public bodies highlight the danger of software monopolies. CYBOU is an open-source, decentralized protocol deployable on-premise or on sovereign clouds, ensuring long-term technological independence.",
    cmpGafamItem3Title: "Identity sovereignty without phone numbers or SMS:",
    cmpGafamItem3Desc: "Registering a Google or Microsoft account requires providing a mobile phone number and accepting arbitrary terms of service. Accounts can be terminated without right of appeal. A CYBOU identity is inalienable: secured by your 24-word master phrase and anti-Sybil proof-of-work. No one can evict you from your cryptographic workspace.",

    cmpSynPill: "Decision Guide",
    cmpSynStatus: "Strategic Synthesis",
    cmpSynTitle: "Which Solution Aligns with Your Priorities?",
    cmpSynSub: "A clear summary to guide CISOs, IT directors, and privacy-conscious organizations.",
    cmpSynCol1Title: "Nextcloud",
    cmpSynCol1Desc: "Well-suited for teams seeking a broad collaborative suite (calendars, video conferencing, kanban) with dedicated sysadmins capable of maintaining a LAMP stack. Not recommended if strict Zero-Knowledge confidentiality and post-quantum resistance are required.",
    cmpSynCol2Title: "Dropbox / Google / Microsoft",
    cmpSynCol2Desc: "Convenient for non-critical consumer workflows or unclassified documents. Completely inappropriate for trade secrets, patent filings, HDS health records, or entities subject to NIS 2 and sovereign GDPR.",
    cmpSynCol3Title: "CYBOU Desktop & Enterprise",
    cmpSynCol3Desc: "<strong>The definitive choice</strong> whenever airtight confidentiality, legal immunity against US extraterritorial jurisdiction, zero-SPOF resilience, and future-proof post-quantum security are non-negotiable mandates.",
    cmpCtaTitle: "Ready to Experience the Sovereign Alternative with CYBOU?",
    cmpCtaDesc: "Explore our open-source codebase on GitHub or discover how to deploy a dedicated enterprise private network.",
    cmpCtaBtnEnterprise: "Enterprise Solutions",
    cmpCtaBtnGithub: "GitHub Source Code",

    cmpBtnViewNextcloud: "View Head-to-Head: CYBOU vs. Nextcloud ↑",
    cmpBtnViewDropbox: "View Head-to-Head: CYBOU vs. Dropbox ↑",
    cmpBtnViewGoogle: "View Head-to-Head: CYBOU vs. Google Workspace ↑",
    cmpBtnViewMsApple: "View Head-to-Head: CYBOU vs. Microsoft 365 / Apple ↑"
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

// --- Comparison Table Head-to-Head & Global Filter ---
function initComparisonFilter() {
  const filterBtns = document.querySelectorAll('.comp-filter-btn');
  const wrapper = document.getElementById('comparison-wrapper');
  if (!filterBtns.length || !wrapper) return;

  filterBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const view = btn.getAttribute('data-view');
      setComparisonView(view);
    });
  });
}

function setComparisonView(view) {
  const filterBtns = document.querySelectorAll('.comp-filter-btn');
  const wrapper = document.getElementById('comparison-wrapper');
  if (!wrapper) return;

  filterBtns.forEach(b => {
    b.classList.toggle('active', b.getAttribute('data-view') === view);
  });

  wrapper.classList.remove(
    'filter-all',
    'filter-nextcloud',
    'filter-dropbox',
    'filter-google',
    'filter-msapple'
  );

  if (view && view !== 'all') {
    wrapper.classList.add('filter-' + view);
  }
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

// --- DeepTech Reactive Particle Canvas (Hero Network Visualizer) ---
function initHeroParticles() {
  const canvas = document.getElementById('hero-particles');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;

  const heroSection = document.getElementById('hero');
  let width = 0;
  let height = 0;
  let dpr = 1;
  let animationFrameId = null;
  let isVisible = true;

  const mouse = { x: -9999, y: -9999, maxDistance: 140 };
  const particleCount = 48;
  const particles = [];
  const maxConnectionDistance = 115;

  function resize() {
    if (!heroSection) return;
    dpr = window.devicePixelRatio || 1;
    width = heroSection.clientWidth;
    height = heroSection.clientHeight;
    canvas.width = Math.floor(width * dpr);
    canvas.height = Math.floor(height * dpr);
    canvas.style.width = width + 'px';
    canvas.style.height = height + 'px';
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  function createParticles() {
    particles.length = 0;
    for (let i = 0; i < particleCount; i++) {
      particles.push({
        x: Math.random() * width,
        y: Math.random() * height,
        vx: (Math.random() - 0.5) * 0.42,
        vy: (Math.random() - 0.5) * 0.42,
        radius: Math.random() * 1.6 + 1.2,
        baseAlpha: Math.random() * 0.28 + 0.22,
        pulseSpeed: Math.random() * 0.02 + 0.01,
        pulsePhase: Math.random() * Math.PI * 2
      });
    }
  }

  function onPointerMove(e) {
    const rect = heroSection.getBoundingClientRect();
    mouse.x = e.clientX - rect.left;
    mouse.y = e.clientY - rect.top;
  }

  function onPointerLeave() {
    mouse.x = -9999;
    mouse.y = -9999;
  }

  if (heroSection) {
    heroSection.addEventListener('pointermove', onPointerMove, { passive: true });
    heroSection.addEventListener('pointerleave', onPointerLeave, { passive: true });
  }

  function draw() {
    if (!isVisible) return;
    ctx.clearRect(0, 0, width, height);

    const time = Date.now() * 0.001;

    // 1. Draw connection lines between nearby cryptographic nodes
    for (let i = 0; i < particles.length; i++) {
      const p1 = particles[i];

      for (let j = i + 1; j < particles.length; j++) {
        const p2 = particles[j];
        const dx = p1.x - p2.x;
        const dy = p1.y - p2.y;
        const dist = Math.hypot(dx, dy);

        if (dist < maxConnectionDistance) {
          const lineAlpha = (1 - dist / maxConnectionDistance) * 0.22;
          ctx.beginPath();
          ctx.moveTo(p1.x, p1.y);
          ctx.lineTo(p2.x, p2.y);
          ctx.strokeStyle = `rgba(5, 150, 105, ${lineAlpha})`;
          ctx.lineWidth = 0.85;
          ctx.stroke();
        }
      }

      // 2. Interactive line to cursor when nearby
      const dxM = p1.x - mouse.x;
      const dyM = p1.y - mouse.y;
      const distM = Math.hypot(dxM, dyM);
      if (distM < mouse.maxDistance) {
        const mAlpha = (1 - distM / mouse.maxDistance) * 0.32;
        ctx.beginPath();
        ctx.moveTo(p1.x, p1.y);
        ctx.lineTo(mouse.x, mouse.y);
        ctx.strokeStyle = `rgba(52, 211, 153, ${mAlpha})`;
        ctx.lineWidth = 1;
        ctx.stroke();
      }
    }

    // 3. Update and draw nodes
    for (let i = 0; i < particles.length; i++) {
      const p = particles[i];

      // Gentle interactive gravity towards cursor
      const dx = mouse.x - p.x;
      const dy = mouse.y - p.y;
      const distMouse = Math.hypot(dx, dy);
      if (distMouse < mouse.maxDistance && distMouse > 1) {
        const force = (1 - distMouse / mouse.maxDistance) * 0.04;
        p.vx += (dx / distMouse) * force;
        p.vy += (dy / distMouse) * force;
      }

      // Movement
      p.x += p.vx;
      p.y += p.vy;

      // Damping
      p.vx *= 0.992;
      p.vy *= 0.992;

      // Soft bounce on canvas borders
      if (p.x < 0) { p.x = 0; p.vx *= -1; }
      else if (p.x > width) { p.x = width; p.vx *= -1; }
      if (p.y < 0) { p.y = 0; p.vy *= -1; }
      else if (p.y > height) { p.y = height; p.vy *= -1; }

      // Breathing node pulse
      const alpha = p.baseAlpha + Math.sin(time * 2 + p.pulsePhase) * 0.1;

      // Soft ambient aura
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.radius * 2.4, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(52, 211, 153, ${Math.max(0.02, alpha * 0.28)})`;
      ctx.fill();

      // Sharp central node
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.radius, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(5, 150, 105, ${Math.max(0.12, alpha)})`;
      ctx.fill();
    }

    animationFrameId = requestAnimationFrame(draw);
  }

  resize();
  createParticles();
  animationFrameId = requestAnimationFrame(draw);

  // Resize handler
  let resizeTimeout;
  window.addEventListener('resize', () => {
    clearTimeout(resizeTimeout);
    resizeTimeout = setTimeout(() => {
      resize();
      createParticles();
    }, 120);
  });

  // IntersectionObserver to conserve resources when out of view
  if ('IntersectionObserver' in window && heroSection) {
    const observer = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          if (!isVisible) {
            isVisible = true;
            animationFrameId = requestAnimationFrame(draw);
          }
        } else {
          isVisible = false;
          if (animationFrameId) cancelAnimationFrame(animationFrameId);
        }
      });
    }, { threshold: 0.05 });
    observer.observe(heroSection);
  }

  // Page visibility API
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      isVisible = false;
      if (animationFrameId) cancelAnimationFrame(animationFrameId);
    } else {
      isVisible = true;
      animationFrameId = requestAnimationFrame(draw);
    }
  });
}

