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

