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
    heroSubtitle: "CYBOU réunit messagerie privée, fichiers et identité numérique portable dans une seule application. Vos contenus sont chiffrés sur votre ordinateur avant tout envoi.",
    heroPillSlogan: "Vos clés. Votre espace. Votre identité vous accompagne.",


    heroBtnShowcase: "Découvrir l'application",
    heroBtnSecurity: "Pourquoi CYBOU ?",

    badgeOpenssl: "Primitives standard OpenSSL · Compositions documentées",
    badgeFrance: "Réseau à admission IP française",
    badgeTls: "Transport TLS 1.3 Post-Quantique (CYBOU P2P)",
    badgePhrase: "Clés Identity · Coffre local protégé",
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
    mockupIdSub3: "Récupération sur un autre ordinateur ; le coffre local est protégé par un mot de passe.",
    mockupIdHeading4: "Moteur cryptographique standard",
    mockupIdSub4: "Primitives ML-KEM / ML-DSA standardisées ; composition protocolaire documentée.",

    mockupSecHeading1: "Primitives standard OpenSSL · Compositions documentées",
    mockupSecDesc1: "CYBOU utilise les primitives standard d’OpenSSL, notamment ML-KEM et ML-DSA, dans des compositions de protocole documentées. Le transport hybride TLS et le profil applicatif X-Wing sont des constructions distinctes ; la revue de leur composition reste séparée de la standardisation des primitives.",
    mockupSecHeading2: "Transport P2P TLS 1.3 Post-Quantique",
    mockupSecDesc2: "Tunnel TLS 1.3 avec groupe hybride X25519MLKEM768. Tout repli classique non-PQ est rejeté (fail-closed).",
    mockupSecHeading3: "Souveraineté territoriale France",
    mockupSecDesc3: "Trafic P2P public restreint à l'espace IP français (fail-closed). Infrastructure P2P souveraine en France hors juridiction cloud américaine.",

    servicesLabel: "Services intégrés",
    servicesTitle: "Trois piliers pour votre indépendance numérique.",
    servicesDesc: "Chaque outil fonctionne directement sur votre machine sans dépendre des géants de la tech.",

    srvMailTag: "Messagerie",
    srvMailTitle: "CYBOU Mail — Votre boîte de réception souveraine",
    srvMailDesc: "Ne laissez plus Google ou Microsoft analyser vos correspondances privées. Vos messages et pièces jointes sont chiffrés sur votre poste avec des clés post-quantiques et transmis via TLS 1.3 post-quantique. Zéro publicité, zéro profilage.",
    srvMailStatus: "Flux intégrés — acceptation Beta en cours",

    srvFilesTag: "Stockage",
    srvFilesTitle: "CYBOU Files — Vos documents sous clé en France",
    srvFilesDesc: "Vos fichiers sont chiffrés et répliqués sur le réseau. Des confirmations de stockage signées, des contrôles sur des portions aléatoires et des téléchargements avec vérification BLAKE3 permettent de vérifier les copies et de réparer les pertes depuis une copie valide.",
    srvFilesStatus: "Flux intégrés — tests prolongés en cours",

    srvIdentityTag: "Identité",
    srvIdentityTitle: "CYBOU Identity — Une clé unique pour votre vie numérique",
    srvIdentityDesc: "Oubliez les mots de passe vulnérables et les SMS piratables. Votre compte est sécurisé par une phrase secrète de 24 mots et un nom lisible .cybou inaliénable qui vous appartient à vie.",
    srvIdentityStatus: "Identité et coffre intégrés au protocole DEVNET",

    srvContinuityTag: "Continuité",
    srvContinuityTitle: "Restauration & Continuité",
    srvContinuityDesc: "Le coffre Identity Vault portable et le mécanisme de récupération permettent de retrouver l’accès à votre Identity sur un autre ordinateur. La reconstruction des messages et fichiers nécessite des publications accessibles, des capsules de clé ouvrables et des copies chiffrées disponibles.",
    srvContinuityStatus: "Reconstruction intégrée sur DEVNET",

    srvWalletTag: "Ressources",
    srvWalletTitle: "Un budget de service lisible",
    srvWalletDesc: "CYBOU utilise des unités de service internes pour comptabiliser les ressources consommées par les opérations et le stockage. Votre budget permet de suivre l’utilisation de ces services.",
    srvStorageEconomy: "Le stockage est organisé par des leases et des contrôles de disponibilité. Le budget de service suit les ressources utilisées ; AUTH reste distinct et ne détermine pas le volume de stockage.",
    srvWalletStatus: "Opérations de base disponibles dans le client DEVNET",

    secLabel: "Sécurité & Transport",
    secTitle: "Une forteresse numérique de bout en bout.",
    secDesc: "Découvrez les protections de CYBOU et leurs limites, du transport au stockage chiffré.",

    secOpensslTag: "Standard Mondial Audité",
    secOpensslTitle: "Primitives standard OpenSSL · Compositions documentées",
    secOpensslDesc: "CYBOU utilise les primitives ML-KEM et ML-DSA fournies par OpenSSL. Le profil hybride X-Wing reste expérimental ; sa conformité exacte et les compositions exigent une vérification. Aucune certification du produit ou du module FIPS n’est revendiquée.",

    secTlsTag: "Protection Réseau",
    secTlsTitle: "Transport 100% Chiffré en TLS 1.3 Post-Quantique (CYBOU P2P)",
    secTlsDesc: "Chaque connexion P2P impose TLS 1.3 avec échange de clés hybride X25519MLKEM768 et refuse le repli classique. Ce mécanisme protège le transport ; la sécurité dépend également des postes, des clés et de la composition du protocole.",

    secE2eeTag: "Contenu privé chiffré",
    secE2eeTitle: "Chiffrement Client de bout en bout & Arbre BLAKE3",
    secE2eeDesc: "Les messages, fichiers et schémas applicatifs Mail/Files sont chiffrés sur votre ordinateur avant l’envoi. Le registre conserve les publications, racines Merkle et capsules de clé, pas le contenu en clair. Les identifiants publics et les métadonnées réseau restent distincts du contenu chiffré.",

    secFranceTag: "Admission France",
    secFranceTitle: "Souveraineté Territoriale France & UE",
    secFranceDesc: "L’admission P2P publique utilise les données Geo locales pour accepter les adresses IP françaises et échoue en l’absence de données valides. Cette politique soutient la cible d’un réseau souverain français ; elle ne constitue pas, à elle seule, une preuve de localisation physique ou de juridiction.",

    compLabel: "Comparatif objectif",
    compTitle: "CYBOU face aux géants du numérique.",
    compDesc: "Comprenez en un coup d'œil ce qui change lorsque vous passez d'un service centralisé à une suite souveraine.",
    compThCriteria: "Critère de protection",
    compThCybou: "CYBOU Desktop",
    compThGoogle: "Google (Gmail / Drive)",
    compThAppleMs: "Apple iCloud / Microsoft",
    compRow1Crit: "Chiffrement des messages et fichiers",
    compRow1Cybou: "Client-side obligatoire (Zero-Knowledge)",
    compRow1Google: "À vérifier selon l’offre, la version et la configuration",
    compRow1AppleMs: "À vérifier selon l’offre, la version et la configuration",
    compRowCryptoCrit: "Moteur et code cryptographique",
    compRowCryptoCybou: "Primitives standard OpenSSL · Compositions documentées",
    compRowCryptoGoogle: "À vérifier selon l’offre, la version et la configuration",
    compRowCryptoAppleMs: "À vérifier selon l’offre, la version et la configuration",
    compRow2Crit: "Sécurité du transport réseau",
    compRow2Cybou: "TLS 1.3 Hybride Post-Quantique (X25519 + ML-KEM-768)",
    compRow2Google: "À vérifier selon l’offre, la version et la configuration",
    compRow2AppleMs: "À vérifier selon l’offre, la version et la configuration",
    compRow3Crit: "Analyse des contenus pour pub / IA",
    compRow3Cybou: "Zéro analyse, zéro publicité, zéro entraînement IA",
    compRow3Google: "À vérifier selon l’offre, la version et la configuration",
    compRow3AppleMs: "À vérifier selon l’offre, la version et la configuration",
    compRow4Crit: "Juridiction et législation",
    compRow4Cybou: "France & Union Européenne (RGPD souverain)",
    compRow4Google: "À vérifier selon l’offre, la version et la configuration",
    compRow4AppleMs: "À vérifier selon l’offre, la version et la configuration",
    compRow5Crit: "Résistance aux attaques quantiques",
    compRow5Cybou: "NIST ML-DSA post-quantique intégré",
    compRow5Google: "À vérifier selon l’offre, la version et la configuration",
    compRow5AppleMs: "À vérifier selon l’offre, la version et la configuration",
    compRow6Crit: "Authentification et propriété",
    compRow6Cybou: "Clés Identity et phrase de récupération ; mot de passe du coffre local",
    compRow6Google: "À vérifier selon l’offre, la version et la configuration",
    compRow6AppleMs: "À vérifier selon l’offre, la version et la configuration",

    archLabel: "Sous le capot",
    archTitle: "Architecture technique & Fondations.",
    archDesc: "Pour les développeurs, auditeurs et curieux : les spécifications exactes du moteur de consensus et du réseau.",

    bento1Title: "PoA DEVNET à opérateur unique",
    bento1Desc: "CYBOU exploite l'unique signataire de finalité du réseau DEVNET. Chaque nœud complet vérifie indépendamment les blocs et l'état ; cette vérification ne transforme pas le modèle de confiance en BFT.",
    bento1Metric: "Centralisé — pas de tolérance BFT",

    bento2Title: "Profil hybride X-Wing pour DEVNET",
    bento2Desc: "Le protocole DEVNET publie une capacité Identity X-Wing basée sur le draft IETF -05. Le profil n'est pas automatiquement transféré à Beta/Mainnet, et son existence ne signifie pas que le service Mail/Files est livré ou audité.",
    bento2Metric: "Profil de développement, revue indépendante requise",

    bento3Title: "Une Identity, rôles cryptographiques séparés",
    bento3Desc: "La récupération, l'autorisation, l'accord de clés, la finalité PoA, la signature des versions et la trésorerie utilisent des rôles séparés.",
    bento3Metric: "Clés locales et rôles distincts",

    bento4Title: "Finalité et état vérifiable",
    bento4Desc: "Un signataire PoA finalise les opérations. Chaque nœud complet vérifie indépendamment les blocs et les transitions d’état.",
    bento4Value: "PoA",
    bento4Metric: "Finalisation unique, vérification indépendante",

    step1Title: "Préparer le contenu privé",
    step1Desc: "Le client transforme le contenu en arbre ROOT/INDEX/DATA chiffré et conserve localement les éléments nécessaires. Les schémas Mail/Files restent à l'intérieur du chiffrement.",

    step2Title: "Protéger la clé de contenu",
    step2Desc: "RootPublication transporte des capsules de clé pour les capacités KEM des destinataires sans publier leur AccountID. Le profil X-Wing draft-05 est réservé au réseau DEVNET.",

    step3Title: "Autoriser une RootPublication",
    step3Desc: "Identity signe une opération générique qui engage la racine, l'arbre d'inclusion et les capsules. Mail ne possède pas de type d'opération de consensus dédié.",

    step4Title: "Finaliser, admettre, retrouver",
    step4Desc: "La finalisation autorise l’admission au stockage ; elle ne suffit pas à afficher Protégé ou Envoyé. Ces statuts exigent des copies distantes confirmées. La cible Beta est deux répliques distantes indépendantes ; le cache local ne compte pas. Des StorageId distincts ne prouvent pas l’indépendance des serveurs ou opérateurs.",

    factsheetHeading: "Paramètres canoniques de l'architecture CYBOU",
    dtTransport: "Transport P2P & Chiffrement",
    ddTransport: "CYBOU P2P encapsulé dans TLS 1.3 post-quantique strict (groupe X25519MLKEM768 obligatoire, fail-closed). Moteur OpenSSL v3.5.2+ avec export de clé de session 32 octets liée aux preuves cryptographiques.",
    dtConsensus: "Finalité actuelle sur DEVNET",
    ddConsensus: "PoA hybride à un signataire exploité par CYBOU sur DEVNET. Les nœuds complets vérifient indépendamment ; pas de revendication BFT. Cible : PoA sur poste Central Authority.",
    dtBootstrap: "Découverte initiale des pairs",
    ddBootstrap: "Le premier contact se fait avec un Full Node ordinaire à une adresse connue, vérifiée par son épingle TLS. Les participants forment ensuite une connexion P2P directe entre pairs. Le bootstrap n’est pas un type de nœud distinct et ne confère aucune autorité de consensus.",
    dtAdmission: "Admission P2P France souveraine",
    ddAdmission: "Admission P2P publique restreinte à l'espace IP français (données Geo locales, fail-closed). Filtrage VPN/proxy/Tor local optionnel.",
    dtGrant: "Création de compte",
    ddGrant: "Création d’Identity avec travail anti-Sybil et budget initial pour utiliser les services du réseau.",
    dtStorageEconomy: "Gestion du stockage",
    dtFees: "Comptabilisation des ressources",
    ddFees: "Les opérations et le stockage utilisent un budget de service, avec des règles de comptabilisation déterministes.",
    dtPublication: "Publication de contenu",
    ddPublication: "<code>RootPublication</code> générique. Mail, Files et Backup sont des schémas privés chiffrés, pas des opérations de consensus distinctes.",
    dtKeys: "Rôles cryptographiques",
    ddKeys: "Les rôles Recovery, Authorization, KEM et PoA sont séparés. Les primitives OpenSSL et leurs compositions font l’objet de vérifications distinctes.",
    dtStack: "Socle technique",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL v3.5.2+, BLAKE3 et transport CYBOU P2P sur TLS 1.3.",

    matrixLabel: "Transparence technique",
    matrixTitle: "État du projet",
    matrixDesc: "Fonctions intégrées au code, réseau expérimental déployé et étapes de préparation à l’ouverture publique.",

    col1Title: "Fonctions intégrées au code",
    badgeDone: "IMPLÉMENTÉ",
    col1Item1: "<strong>Identity et noms .cybou :</strong> AccountCreate avec travail anti-Sybil, rôles de clés hybrides, coffre portable et registre de noms finalisé.",
    col1Item2: "<strong>Finalité PoA :</strong> signataire unique, journal durable et vérification indépendante des blocs par les nœuds complets.",
    col1Item3: "<strong>RootPublication :</strong> opération générique autorisée par Identity ; aucun objet Mail ou fichier permanent dans le consensus.",
    col1Item4: "<strong>Arbre de contenu chiffré :</strong> chunks ROOT/INDEX/DATA ordonnés, adressés par BLAKE3-256 et construits pour le traitement en flux.",
    col1Item5: "<strong>Admission et transport :</strong> stockage local de chunks, preuves d’inclusion liées aux publications finalisées et transport CYBOU P2P sécurisé en TLS 1.3 post-quantique.",
    col1Item6: "<strong>Ressources :</strong> budget de service, leases de stockage et règles déterministes de comptabilisation.",

    col2Title: "Déploiement expérimental",
    badgeWip: "DEVNET",
    col2Item1: "<strong>Réseau DEVNET :</strong> pair complet de découverte et transport P2P avec admission IP française.",
    col2Item2: "<strong>Bureau :</strong> intégration de Mail, Files et Identity ; publication, récupération et reconstruction des index.",
    col2Item3: "<strong>Stockage :</strong> confirmations signées, audits et contrôles GET/BLAKE3. Les versions déployées et leurs preuves sont suivies dans le registre d’implémentation.",
    col2Item4: "<strong>Périmètre :</strong> réseau de développement ; les essais techniques ne constituent pas l’ouverture d’un service public.",

    col3Title: "Prochaines étapes",
    badgePlanned: "PRÉPARATION",
    col3Item1: "<strong>Acceptation desktop :</strong> installations propres, parcours Mail/Files et récupération sur un autre ordinateur.",
    col3Item2: "<strong>Endurance et durabilité :</strong> essais prolongés, pertes de fournisseurs et deux répliques distantes indépendantes pour la cible Beta.",
    col3Item3: "<strong>Sauvegarde :</strong> application post-Beta du même graphe chiffré, avec restauration vérifiable.",
    col3Item4: "<strong>Ouverture publique :</strong> revue de sécurité, critères UX, procédures d’exploitation et support.",

    faqLabel: "Questions fréquentes",
    faqTitle: "Comprendre CYBOU.",
    faqDesc: "Ce qui fonctionne sur DEVNET, ce qui reste à construire et les garanties de sécurité.",
    faqQ1: "Qu’est-ce que CYBOU en termes simples ?",
    faqA1: "CYBOU est une suite logicielle souveraine réunissant messagerie privée (Mail), stockage de fichiers chiffré (Files) et gestionnaire d'identité (Identity). Vos données sont chiffrées sur votre ordinateur avant tout envoi et tout le transport réseau est protégé en TLS 1.3 post-quantique.",
    faqTlsBadge: "Transport Post-Quantique",
    faqQTls: "En quoi le transport TLS 1.3 de CYBOU est-il post-quantique et que protège-t-il ?",
    faqATls: "Toutes les communications entre pairs et nœuds CYBOU (protocole CYBOU P2P) utilisent un handshake TLS 1.3 avec le groupe d'échange de clés X25519MLKEM768. Cette négociation associe la cryptographie classique à ML-KEM-768 (standard NIST FIPS 203). Si un pair ne supporte pas ce mode post-quantique, la connexion est immédiatement interrompue. Cela protège vos transferts contre l'interception locale (Wi-Fi public, FAI) et contre la stratégie d'espionnage « Récolter maintenant, déchiffrer plus tard ».",
    faqOpensslBadge: "Standard Audité",
    faqQOpenssl: "Les algorithmes de CYBOU sont-ils développés en interne (« crypto maison ») ?",
    faqAOpenssl: "CYBOU utilise les primitives OpenSSL et des compositions documentées. Les primitives ML-KEM et ML-DSA sont standardisées ; le profil X-Wing reste expérimental. Les tests ne constituent pas une certification.",
    faqQ2: "En quoi CYBOU diffère-t-il de Gmail ou Google Drive ?",
    faqA2: "Contrairement à Google qui centralise vos données, possède les clés et scanne les contenus pour la publicité ou l'entraînement d'IA, CYBOU chiffre tout sur votre ordinateur. Aucun serveur ne peut lire vos emails ou fichiers, et tout le réseau public est localisé en France, à l'abri du Cloud Act américain.",
    faqQ3: "À quoi servent un nom .cybou et la phrase de récupération de 24 mots ?",
    faqA3: "Un nom .cybou est l’adresse lisible de votre Identity. Le coffre portable conserve son AccountID stable et son secret de récupération. La phrase de récupération permet de retrouver les clés correspondantes ; le coffre local est protégé par un mot de passe. Retrouver les messages et fichiers exige aussi leurs publications, des capsules de clé ouvrables et une copie disponible du contenu chiffré.",
    faqQFrance: "Pourquoi CYBOU est-il hébergé exclusivement en France et en Europe ?",
    faqAFrance: "La souveraineté numérique repose sur une indépendance d'infrastructure. En restreignant les nœuds publics au territoire français, CYBOU assure que vos flux de données et vos fragments chiffrés restent hébergés en France sur une infrastructure P2P souveraine hors juridiction cloud américaine.",
    faqPqBadge: "Sécurité Future",
    faqQ5: "Que signifie la cryptographie post-quantique (ML-DSA) ?",
    faqA5: "Les ordinateurs quantiques rendront obsolètes les signatures RSA et ECC actuelles. CYBOU utilise dès aujourd'hui les algorithmes post-quantiques récemment normalisés par le NIST (ML-DSA-44 et ML-DSA-65) couplés à Ed25519 pour protéger vos correspondances contre l'enregistrement malveillant et le déchiffrement rétroactif futur.",
    faqQ7: "Puis-je utiliser CYBOU aujourd’hui ?",
    faqA7: "Le réseau DEVNET et le code source complet sont actifs et consultables sur GitHub. L'application CYBOU Desktop est actuellement en cours de durcissement et d'intégration continue avant sa distribution publique pour la phase Beta.",

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
    compSecTitle: "Sécurité, gouvernance et références européennes",
    compSecDesc: "Chiffrement côté client, gestion des clés et politique réseau constituent des mesures techniques. Leur évaluation doit être complétée par la gouvernance et les preuves du déploiement concerné ; aucune certification du produit n’est revendiquée.",
    compCardRgpdPill: "Règlement UE 2016/679",
    compCardRgpdStatus: "Privacy by Design",
    compCardRgpdTitle: "RGPD — Protection des données dès la conception",
    compCardRgpdDesc: "Chiffrement côté client et contenu applicatif privé. La révocation retire l’autorisation active de stockage et déclenche une purge gérée chez les fournisseurs conformes. Les blocs historiques et les copies conservées par des destinataires peuvent subsister.",
    compCardNis2Pill: "Directive UE 2022/2555",
    compCardNis2Status: "Mesures Art. 21",
    compCardNis2Title: "Directive NIS 2 — Résilience & Chaîne d'Approvisionnement",
    compCardNis2Desc: "La réplication et le remplacement de copies peuvent améliorer la disponibilité. La finalisation dépend d’un seul signataire PoA. Applicabilité NIS2, continuité, incidents et chaîne logicielle restent à évaluer.",
    compCardHdsPill: "CSP Art. L.1111-8",
    compCardHdsStatus: "Secret Médical & HDS",
    compCardHdsTitle: "Secteur Médical — Secret Médical & Données de Santé",
    compCardHdsDesc: "Le chiffrement protège le contenu sous les hypothèses documentées. Il ne garantit pas à lui seul le secret professionnel ou la conformité HDS ; les obligations dépendent du traitement et des prestataires.",
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
    cArt0Sub: "Mesures techniques, responsabilités et obligations d’hébergement doivent être évaluées séparément.",
    cArt0P1: "Dans l'industrie du logiciel et de la cybersécurité, de nombreux acteurs prétendent être « certifiés RGPD » ou « certifiés HDS » par le simple fait de chiffrer des données. <strong>Cette affirmation est juridiquement trompeuse.</strong> Les règlements européens et le droit français distinguent impérativement deux niveaux de conformité :",
    cArt0L1Title: "Le niveau logiciel et protocolaire (CYBOU) :",
    cArt0L2Title: "Le niveau d'hébergement physique et d'exploitation (Centres de données) :",
    cArt0BoxTitle: "Ce que cela implique concrètement :",
    cArt0BoxDesc: "Tout usage de données de santé nécessite une analyse du traitement, des responsabilités et des prestataires. CYBOU ne fournit ni sélection automatique de fournisseurs HDS ni preuve de leur certification.",

    cRgpdStatus: "Privacy by Design Intégral",
    cRgpdTitle: "RGPD — Protection des Données Personnelles dès la Conception",
    cRgpdSub: "Comment l'architecture Zero-Knowledge et le consensus aveugle de CYBOU éliminent structurellement les risques de non-conformité.",
    cRgpdP1: "Le RGPD impose aux responsables de traitement et sous-traitants des obligations drastiques de sécurité, de minimisation et de respect des droits des personnes. L'architecture de CYBOU a été pensée dès la première ligne de code pour satisfaire ces obligations de manière native :",
    cRgpdThArt: "Article RGPD",
    cRgpdThReq: "Exigence Légale",
    cRgpdThSol: "Implémentation Technique CYBOU",
    cRgpdArt25Req: "Garantir la protection des données dès la conception des systèmes et par défaut sans action complexe de l'utilisateur.",
    cRgpdArt25Sol: "<strong>Chiffrement côté client.</strong> Les contenus Mail/Files et leurs schémas applicatifs sont chiffrés avant l’envoi. Les publications, racines, capsules et métadonnées de protocole doivent être distinguées du contenu privé.",
    cRgpdArt5Req: "Les données doivent être adéquates, pertinentes et limitées au strict nécessaire.",
    cRgpdArt5Sol: "<strong>Contenu applicatif chiffré.</strong> Les noms de fichiers et le contenu Mail restent chiffrés. Le registre conserve des identifiants de comptes, des engagements de publication et des capsules ; les pairs observent les adresses IP et le trafic. Ces métadonnées nécessitent une analyse de protection des données.",
    cRgpdArt17Req: "Permettre l'effacement définitif et irréversible des données à caractère personnel.",
    cRgpdArt17Sol: "<strong>Révocation et purge gérée.</strong> La révocation finalisée retire l’autorisation active et déclenche la suppression des fragments non partagés chez les fournisseurs conformes, avec suivi des échecs. Elle ne supprime pas les blocs historiques et ne prouve pas l’effacement des copies conservées ailleurs ni la destruction cryptographique par objet.",
    cRgpdArt20Req: "Permettre la récupération et le transfert des données dans un format ouvert et structuré.",
    cRgpdArt20Sol: "<strong>Restauration déterministe par phrase de 24 mots.</strong> L'utilisateur peut reconstruire l'ensemble de ses messages, fichiers et annuaires sur n'importe quel ordinateur grâce au standard ouvert de dérivation de clés.",
    cRgpdArt32Req: "Mettre en œuvre des mesures techniques appropriées, incluant le chiffrement et la résilience des systèmes.",
    cRgpdArt32Sol: "<strong>Mesures techniques à évaluer selon les risques.</strong> Primitives ML-KEM/ML-DSA, TLS 1.3 et réplication de chunks. La conformité RGPD exige aussi des mesures organisationnelles et des preuves dans le périmètre concerné.",
    cRgpdArt44Req: "Interdiction de transférer des données vers des pays tiers sans garanties appropriées (invalidation Privacy Shield).",
    cRgpdArt44Sol: "<strong>Souveraineté territoriale stricte (France-only).</strong> L'admission P2P publique est filtrée par géolocalisation locale (fail-closed). Aucun serveur aux USA : infrastructure P2P souveraine en France hors juridiction cloud américaine.",

    cNis2Status: "Mesures de Gestion des Risques",
    cNis2Title: "Directive NIS 2 — Résilience Opérationnelle & Chaîne d'Approvisionnement",
    cNis2Sub: "Répondre aux exigences strictes de cybersécurité pour les Entités Essentielles (EE) et Importantes (EI).",
    cNis2P1: "La directive européenne NIS 2 (en vigueur depuis 2024 et transposée dans les droits nationaux) impose des obligations strictes aux organisations dans 18 secteurs critiques (énergie, transports, santé, administration, infrastructures numériques). CYBOU s'aligne directement sur les prescriptions de l'Article 21 :",
    cNis2Item1Title: "Sécurité de la chaîne d'approvisionnement logicielle (Supply Chain Security) :",
    cNis2Item1Desc: "CYBOU utilise les primitives standard d’OpenSSL, notamment ML-KEM et ML-DSA, dans des compositions de protocole documentées. Le transport hybride TLS et le profil applicatif X-Wing sont des constructions distinctes ; la revue de leur composition reste séparée de la standardisation des primitives.",
    cNis2Item2Title: "Anticipation de la menace quantique (ENISA & ANSSI) :",
    cNis2Item2Desc: "La menace des attaques « Harvest Now, Decrypt Later » (interception massive actuelle pour déchiffrement ultérieur par ordinateur quantique) est neutralisée par le chiffrement hybride TLS 1.3 avec échange de clés X25519MLKEM768 et signatures ML-DSA-44/65.",
    cNis2Item3Title: "Résilience, continuité d'activité et gestion des crises :",
    cNis2Item3Desc: "Le stockage vise à limiter l'effet de la perte d'une réplique par des contrôles de disponibilité et des réparations depuis une copie valide. Des StorageId distincts ne prouvent pas des machines ou opérateurs indépendants. La réparation dépend de sources valides et de fournisseurs disponibles ; la finalité dépend du PoA unique.",
    cNis2Item4Title: "Notification et traitement des incidents de sécurité (SECURITY.md) :",
    cNis2Item4Desc: "Le projet maintient une politique formelle de divulgation responsable des vulnérabilités (SECURITY.md) et se conforme par avance aux exigences de signalement rapide et de traçabilité du Cyber Resilience Act (CRA).",

    cHdsStatus: "Code de la santé publique L.1111-8",
    cHdsTitle: "Secteur Médical — Secret Professionnel, Données de Santé & HDS",
    cHdsSub: "Garanties techniques pour les professionnels de santé, laboratoires, cliniques et hôpitaux.",
    cHdsP1: "Les données médicales constituent la catégorie la plus sensible de données personnelles (« données de santé » au sens de l'art. 9 du RGPD). En France, leur traitement est encadré par le Code pénal et le Code de la santé publique.",
    cHdsH1: "1. Confidentialité des données de santé",
    cHdsP2: "CYBOU chiffre les correspondances et fichiers sur le poste avant l’envoi. Pour les données de santé, ces mesures doivent être complétées par la gestion des accès, la sécurité des postes et l’évaluation des obligations applicables au traitement et aux prestataires. Le chiffrement seul ne démontre pas la conformité HDS.",
    cHdsH2: "2. Hébergement de Données de Santé (HDS — Art. L.1111-8 du Code de la santé publique)",
    cHdsP3: "L'article L.1111-8 du CSP dispose que toute personne qui héberge des données de santé à caractère personnel recueillies à l'occasion d'activités de prévention, de diagnostic ou de soins doit être certifiée HDS. Voici la déclinaison opérationnelle claire de cette obligation avec CYBOU :",
    cHdsThMode: "Scénario d'Usage",
    cHdsThCadre: "Cadre Réglementaire",
    cHdsThCybou: "Conformité CYBOU",
    cHdsMode1Cadre: "Le praticien ou l'hôpital conserve les données sur ses propres serveurs et terminaux internes.",
    cHdsMode1Cybou: "<strong>Périmètre à évaluer.</strong> L’auto-hébergement ne suffit pas à établir la conformité du traitement. Les contrôles, les flux et les obligations restent à vérifier.",
    cHdsMode2Cadre: "Les données chiffrées sont confiées à des fournisseurs de stockage distants gérés par des tiers.",
    cHdsMode2Cybou: "<strong>Prestataires à vérifier.</strong> Évaluer les obligations HDS et le périmètre certifié des services utilisés. Les Full Nodes CYBOU n’annoncent aucun statut HDS.",
    cHdsBoxTitle: "Engagement d'intégrité pour le secteur médical :",
    cHdsBoxDesc: "CYBOU ne revendique aucune certification HDS ou SecNumCloud. Le chiffrement ne remplace pas la vérification des prestataires, des contrôles opérationnels et des obligations du traitement.",

    cIsoStatus: "Annexe A — Contrôles de Sécurité",
    cIsoTitle: "ISO/IEC 27001:2022 — Matrice de Correspondance des Contrôles",
    cIsoSub: "Comment l'architecture technique de CYBOU aide les entreprises à satisfaire leurs exigences de Système de Management de la Sécurité de l'Information (SMSI).",
    cIsoP1: "La norme internationale ISO/IEC 27001 définit les bonnes pratiques pour protéger les actifs d'information. Le tableau ci-dessous explicite la contribution directe de CYBOU aux contrôles opérationnels de l'Annexe A (révision 2022) :",
    cIsoThCode: "Contrôle Annexe A",
    cIsoThName: "Intitulé de la Mesure",
    cIsoThSol: "Mise en Œuvre par CYBOU",
    cIsoA515Req: "Contrôle d'accès et authentification",
    cIsoA515Sol: "Authentification par signatures hybrides ML-DSA-44 et Ed25519, sans mot de passe de compte centralisé. Le coffre local est protégé par un mot de passe.",
    cIsoA82Req: "Gestion des droits d'accès privilégiés",
    cIsoA82Sol: "Modèle Zero-Knowledge : aucun administrateur réseau ni opérateur PoA ne dispose d'un pouvoir technique de déchiffrement des capsules utilisateur.",
    cIsoA812Req: "Prévention des fuites de données (DLP)",
    cIsoA812Sol: "Chiffrement côté client avant toute transmission. Les fragments déjà chiffrés sont identifiés et vérifiés par leur empreinte BLAKE3-256 ; BLAKE3 est une fonction de hachage, pas un algorithme de chiffrement.",
    cIsoA814Req: "Redondance des moyens de traitement",
    cIsoA814Sol: "Réplication vers des fournisseurs aux StorageId distincts, contrôles GET et BLAKE3, puis tentative de réparation. La diversité géographique et l'indépendance physique ne sont pas prouvées par ces identifiants.",
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
    cCiaIDesc: "<strong>Intégrité vérifiable :</strong> Chaque fragment chiffré est identifié par son empreinte BLAKE3-256. Le client vérifie les données récupérées ; les nœuds vérifient les publications et transitions finalisées par le PoA. Le journal de signature refuse de signer en cas de conflit détecté.",
    cCiaA: "3. Disponibilité (D)",
    cCiaADesc: "<strong>Disponibilité contrôlée :</strong> StorageService utilise des confirmations signées, des audits de portions aléatoires et des téléchargements complets vérifiés par BLAKE3. La réparation nécessite une copie valide et un fournisseur disponible. Un contrôle réussi établit la disponibilité au moment du contrôle.",

    cAnssiStatus: "Réglementation Nationale",
    cAnssiTitle: "ANSSI & Souveraineté Numérique Française",
    cAnssiSub: "Alignement avec les recommandations de l'Agence Nationale de la Sécurité des Systèmes d'Information.",
    cAnssiP1: "L'Agence Nationale de la Sécurité des Systèmes d'Information (ANSSI) édicte les règles et recommandations de sécurité pour l'État français, les Opérateurs d'Importance Vitale (OIV) et le tissu économique. CYBOU s'inscrit pleinement dans ces orientations :",
    cAnssiItem1Title: "Transition post-quantique et hybridation :",
    cAnssiItem1Desc: "L'ANSSI recommande expressément une approche d'hybridation (combiner un algorithme classique reconnu et un mécanisme post-quantique normalisé) pour éviter les régressions de sécurité. CYBOU suit rigoureusement cette préconisation en combinant Ed25519 + ML-DSA pour les signatures et X25519 + ML-KEM-768 pour l'échange de clés.",
    cAnssiItem2Title: "Régime de déclaration des moyens de cryptologie (CPCE) :",
    cAnssiItem2Desc: "Conformément aux articles L. 133-1 et suivants du Code des postes et des communications électroniques, l'utilisation de moyens de cryptologie assurant des fonctions de confidentialité et d'authentification est libre en France pour les particuliers et les entreprises.",
    cAnssiItem3Title: "Souveraineté des données et cloud de confiance (SecNumCloud) :",
    cAnssiItem3Desc: "Le choix des prestataires et des infrastructures doit être évalué dans le périmètre du déploiement. Une qualification d’hébergement éventuelle doit être vérifiée auprès du prestataire ; elle ne certifie pas automatiquement CYBOU ni l’ensemble du traitement.",
    cAnssiCtaTitle: "Besoin d'une analyse d'adéquation pour votre organisation ?",
    cAnssiCtaDesc: "Notre équipe technique et juridique répond à vos questions concernant vos déploiements on-premise, votre politique de conformité RGPD ou l'hébergement HDS.",
    cAnssiCtaBtn: "Contacter l'équipe",

    footProdEnterprise: "CYBOU Entreprise",
    footSecEnterprise: "Réseaux privés dédiés",
    srvEnterpriseTag: "Sur Mesure",
    srvEnterpriseTitle: "CYBOU Entreprise — Réseau Privé Dédié & Isolé",
    srvEnterpriseDesc: "CYBOU Entreprise vise un réseau dédié à votre organisation : genèse propre, clé PoA sous votre contrôle et stockage sur l’infrastructure choisie.",
    srvEnterpriseLink: "Explorer les solutions d'entreprise &rarr;",

    // Dedicated Enterprise Page (entreprise.html)
    eBreadHome: "Accueil",
    eBreadCurrent: "Solutions Entreprise & Réseaux Dédiés",
    eHeroBadge: "DÉPLOIEMENT DÉDIÉ & ISOLATION SOUVERAINE",
    eHeroBadgeText: "Isolation Cryptographique & Gouvernance Dédiée",
    eHeroTitle: "Créez votre Réseau Privé d'Entreprise Sécurisé & Dédié",
    eHeroDesc: "Nous construisons CYBOU Entreprise pour les organisations qui souhaitent leur propre domaine de confiance : un réseau privé dédié, une genèse propre, une clé PoA sous leur contrôle et un stockage sur l’infrastructure choisie.",

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
    eCard4Desc: "Vos fichiers et messages chiffrés sont répliqués sur vos serveurs. Confirmations signées, audits de portions aléatoires et téléchargements vérifiés par BLAKE3 contrôlent les copies. Les pertes sont réparées à partir d’une copie valide lorsqu’un fournisseur est disponible.",

    eArt2Pill: "Sous le Capot",
    eArt2Status: "Spécifications Ouvertes",
    eArt2Title: "Comment fonctionne l'isolation d'un réseau dédié ?",
    eArt2Sub: "Tous les participants exécutent le même logiciel robuste standard, mais l'espace cryptographique est strictement circonscrit à votre entreprise.",
    eArt2P1: "La direction Entreprise repose sur un réseau distinct, avec sa propre genèse et son identité réseau. L’organisation maîtrise la clé de finalité PoA et choisit son infrastructure de stockage. Le déploiement et l’outillage dédiés font partie du produit à construire :",

    eStep1Title: "Définition de Genèse",
    eStep1Desc: "Génération d'un fichier genesis.json signé par la clé privée de réseau (hors-ligne), avec un identifiant de réseau propre et les paramètres d'autorité.",
    eStep2Title: "Autorité Centrale PoA",
    eStep2Desc: "La DSI génère la clé de finalisation PoA sur un poste sécurisé (SecOps) pour sceller les blocs d'entreprise.",
    eStep3Title: "Points de contact initiaux",
    eStep3Desc: "Des Full Nodes ordinaires à des adresses connues sur votre intranet permettent la découverte initiale ; les pairs communiquent ensuite directement.",
    eStep4Title: "Grappe de Stockage",
    eStep4Desc: "Configuration de la capacité de stockage du nœud complet (cybou node run) sur vos serveurs internes pour héberger les fragments chiffrés.",

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
    eRowBootPriv: "Full Nodes ordinaires à des adresses connues sur votre LAN / Intranet / VPN",
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
    eGovItem1Desc: "Le même exécutable unifié cybou sert de nœud complet, de serveur d'amorçage (bootstrap) et de serveur de stockage (provider). Il tourne en tâche de fond sous Linux avec un service systemd standard, consomme peu de mémoire et s'intègre naturellement à vos outils de déploiement Ansible, Puppet ou Terraform.",
    eGovItem2Title: "Application de bureau intuitive pour les collaborateurs :",
    eGovItem2Desc: "Les équipes utilisent CYBOU Desktop sur leurs postes de travail (Windows, Linux, macOS). Une interface soignée unifiant messagerie chiffrée, espace de fichiers et identité souveraine sans formation technique préalable.",
    eGovItem3Title: "Élimination des mots de passe et bases d'identifiants compromises :",
    eGovItem3Desc: "Finis les annuaires LDAP ou Active Directory exposés aux attaques de type Golden Ticket ou credential stuffing. Chaque collaborateur possède sa propre phrase secrète de 24 mots et ses paires de clés asymétriques scellées localement.",
    eGovItem4Title: "Cycle de vie et rotation des clés (IdentityRotate) :",
    eGovItem4Desc: "En cas de changement d'ordinateur ou de suspicion de compromission, l'opération atomique IdentityRotate permet de révoquer et renouveler instantanément les clés de signature et de chiffrement sans perdre l'historique ni l'identifiant du collaborateur.",
    eGovItem5Title: "Sauvegarde de sinistre et PCA/PRA déterministe :",
    eGovItem5Desc: "La continuité repose sur des sauvegardes des coffres Identity, des clés de finalité et de leur historique de signature, ainsi que sur des copies de données accessibles. Restaurer l’autorité et restaurer les contenus sont deux procédures distinctes ; la clé de genèse seule ne permet pas de récupérer les fichiers.",

    eArt5Pill: "Applications Métier",
    eArt5Status: "Secteurs Sensibles",
    eArt5Title: "Cas d'usage stratégiques pour les organisations exigeantes",
    eArt5Sub: "Conçu pour protéger ce que votre organisation a de plus précieux : son savoir-faire, ses secrets et sa souveraineté d'action.",

    eUse1Pill: "Industrie de Pointe",
    eUse1Badge: "Secret des Affaires",
    eUse1Title: "R&D, Propriété Intellectuelle & Brevets",
    eUse1Desc: "Chiffrement côté client des plans, formulations, codes sources et documents stratégiques, avec contrôle des clés et copies vérifiées. La sécurité des postes et les procédures de récupération complètent ces mécanismes.",

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
    eRoiTitle: "Une infrastructure dimensionnée pour vos besoins",
    eRoiDesc: "Le dimensionnement, l’exploitation, le support et les intégrations déterminent le coût du projet. Une étude adaptée à votre environnement permet d’évaluer ces besoins et les gains attendus, sans promettre une durée d’amortissement universelle.",

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
    compCardNextTitle: "CYBOU et Nextcloud — Choisir son architecture",
    compCardNextDesc: "CYBOU associe chiffrement côté client, clés locales et stockage P2P. Pour comparer avec une installation Nextcloud, examiner sa configuration, ses options de chiffrement et ses responsabilités d’exploitation.",
    compCardDropPill: "Stockage Cloud Américain",
    compCardDropStatus: "Cloud Act & IA",
    compCardDropTitle: "CYBOU et Dropbox — Gestion des clés et du stockage",
    compCardDropDesc: "CYBOU chiffre le contenu avant son envoi et conserve les clés sous le contrôle de l’Identity. Comparer les services hébergés selon l’offre, les options de chiffrement et les conditions de traitement retenues.",
    compCardGafamPill: "Suites Centralisées US",
    compCardGafamStatus: "Émancipation",
    compCardGafamTitle: "CYBOU vs. Google Workspace & Microsoft 365",
    compCardGafamDesc: "CYBOU vise un espace privé fondé sur des clés locales et un protocole ouvert. Le choix face à Google Workspace ou Microsoft 365 dépend des usages, des intégrations et de la configuration de sécurité.",
    compCardPqPill: "Standard Mondial OpenSSL v3.5.2+",
    compCardPqStatus: "Post-Quantique",
    compCardPqTitle: "Résistance Quantique Immédiate (NIST FIPS 203/204)",
    compCardPqDesc: "CYBOU intègre ML-KEM et ML-DSA dans une architecture hybride documentée. Une comparaison post-quantique doit vérifier les versions et les mécanismes réellement déployés dans chaque solution.",
    compTeaserCtaTitle: "Consulter la matrice comparative complète et détaillée",
    compTeaserCtaDesc: "Tableau exhaustif sur 8 critères de sécurité, analyse architecturale approfondie face à Nextcloud et Dropbox, et guide de choix pour les DSI.",
    compTeaserCtaBtn: "Voir le grand comparatif",

    // Dedicated Comparison Page (comparatif.html)
    cmpBreadHome: "Accueil",
    cmpBreadCurrent: "Comparatif Objectif des Solutions",
    cmpHeroTag: "Analyse Technique & Souveraine",
    cmpHeroTitle: "CYBOU face à Nextcloud, Dropbox, Google & Microsoft",
    cmpHeroDesc: "Comprenez en profondeur ce qui différencie la suite souveraine CYBOU des clouds centralisés américains (Google, Microsoft, Dropbox) et des serveurs auto-hébergés classiques (Nextcloud). Architecture P2P, cryptographie post-quantique OpenSSL v3.5.2+, indépendance d'infrastructure et modèle de données Zero-Knowledge.",
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
    cmpR1Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR1Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR1Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR1MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR2Crit: "Moteur et code cryptographique",
    cmpR2Cybou: "Primitives standard OpenSSL · Compositions documentées",
    cmpR2Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR2Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR2Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR2MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR3Crit: "Résistance aux attaques quantiques",
    cmpR3Cybou: "Natif : ML-KEM-768 (TLS 1.3) + ML-DSA-44/65 (Identités)",
    cmpR3Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR3Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR3Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR3MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR4Crit: "Architecture & Point unique de panne",
    cmpR4Cybou: "P2P décentralisé unifié C++, auto-réparation de fragments",
    cmpR4Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR4Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR4Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR4MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR5Crit: "Juridiction & Lois extraterritoriales",
    cmpR5Cybou: "France & UE (P2P filtré France-only, hors juridiction cloud US)",
    cmpR5Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR5Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR5Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR5MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR6Crit: "Scan pour pub ou entraînement d'IA",
    cmpR6Cybou: "Zéro scan, zéro IA, zéro pub (données opaques)",
    cmpR6Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR6Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR6Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR6MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR7Crit: "Authentification & Dépendance",
    cmpR7Cybou: "Accès par clés Identity ; coffre local protégé par mot de passe",
    cmpR7Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR7Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR7Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR7MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpR8Crit: "Déploiement & Maintenance",
    cmpR8Cybou: "Binaire C++ autonome sans dépendances externes",
    cmpR8Nextcloud: "À vérifier selon l’offre, la version et la configuration",
    cmpR8Dropbox: "À vérifier selon l’offre, la version et la configuration",
    cmpR8Google: "À vérifier selon l’offre, la version et la configuration",
    cmpR8MsApple: "À vérifier selon l’offre, la version et la configuration",

    cmpNextPill: "Analyse Open Source",
    cmpNextStatus: "Architecture et contrôle",
    cmpNextTitle: "CYBOU et Nextcloud — Critères de choix",
    cmpNextSub: "Comparer les usages, les configurations et les responsabilités. Les principes CYBOU ci-dessous permettent d’évaluer les différences pertinentes.",
    cmpNextP1: "Comparer les usages, les configurations et les responsabilités. Les principes CYBOU ci-dessous permettent d’évaluer les différences pertinentes.",
    cmpNextItem1Title: "Chiffrement côté client",
    cmpNextItem1Desc: "CYBOU chiffre les contenus avant leur transmission et conserve les clés localement. Vérifier les options et la gestion des clés de chaque solution comparée.",
    cmpNextItem2Title: "Copies et récupération",
    cmpNextItem2Desc: "CYBOU contrôle les répliques par audits et vérification BLAKE3, puis répare depuis une copie valide. Comparer les mécanismes de sauvegarde, les tests de panne et les engagements de disponibilité.",
    cmpNextItem3Title: "Protocole ouvert et Identity",
    cmpNextItem3Desc: "Le code ouvert et le coffre Identity portable permettent d’examiner les mécanismes et de conserver le contrôle des clés. Comparer les exports, la récupération et les responsabilités d’exploitation.",
    cmpNextItem4Title: "Exploitation et dépendances",
    cmpNextItem4Desc: "CYBOU utilise un exécutable commun pour le bureau et le nœud complet. Le dimensionnement et la maintenance doivent être évalués dans l’environnement choisi.",

    cmpDropPill: "Stockage Cloud US",
    cmpDropStatus: "Architecture et contrôle",
    cmpDropTitle: "CYBOU et Dropbox — Critères de choix",
    cmpDropSub: "Comparer les usages, les configurations et les responsabilités. Les principes CYBOU ci-dessous permettent d’évaluer les différences pertinentes.",
    cmpDropP1: "Comparer les usages, les configurations et les responsabilités. Les principes CYBOU ci-dessous permettent d’évaluer les différences pertinentes.",
    cmpDropItem1Title: "Chiffrement côté client",
    cmpDropItem1Desc: "CYBOU chiffre les contenus avant leur transmission et conserve les clés localement. Vérifier les options et la gestion des clés de chaque solution comparée.",
    cmpDropItem2Title: "Copies et récupération",
    cmpDropItem2Desc: "CYBOU contrôle les répliques par audits et vérification BLAKE3, puis répare depuis une copie valide. Comparer les mécanismes de sauvegarde, les tests de panne et les engagements de disponibilité.",
    cmpDropItem3Title: "Protocole ouvert et Identity",
    cmpDropItem3Desc: "Le code ouvert et le coffre Identity portable permettent d’examiner les mécanismes et de conserver le contrôle des clés. Comparer les exports, la récupération et les responsabilités d’exploitation.",

    cmpGafamPill: "Monopoles Big Tech",
    cmpGafamStatus: "Architecture et contrôle",
    cmpGafamTitle: "CYBOU et Google Workspace et Microsoft 365 — Critères de choix",
    cmpGafamSub: "Comparer les usages, les configurations et les responsabilités. Les principes CYBOU ci-dessous permettent d’évaluer les différences pertinentes.",
    cmpGafamP1: "Comparer les usages, les configurations et les responsabilités. Les principes CYBOU ci-dessous permettent d’évaluer les différences pertinentes.",
    cmpGafamItem1Title: "Chiffrement côté client",
    cmpGafamItem1Desc: "CYBOU chiffre les contenus avant leur transmission et conserve les clés localement. Vérifier les options et la gestion des clés de chaque solution comparée.",
    cmpGafamItem2Title: "Copies et récupération",
    cmpGafamItem2Desc: "CYBOU contrôle les répliques par audits et vérification BLAKE3, puis répare depuis une copie valide. Comparer les mécanismes de sauvegarde, les tests de panne et les engagements de disponibilité.",
    cmpGafamItem3Title: "Protocole ouvert et Identity",
    cmpGafamItem3Desc: "Le code ouvert et le coffre Identity portable permettent d’examiner les mécanismes et de conserver le contrôle des clés. Comparer les exports, la récupération et les responsabilités d’exploitation.",

    cmpSynPill: "Guide de Choix",
    cmpSynStatus: "Synthèse Décisionnelle",
    cmpSynTitle: "Quelle solution choisir selon vos priorités ?",
    cmpSynSub: "Un récapitulatif clair pour orienter les décideurs, RSSI et utilisateurs exigeants.",
    cmpSynCol1Title: "Nextcloud",
    cmpSynCol1Desc: "Adapté aux équipes cherchant une suite collaborative riche (agendas, visio, kanban) et disposant d'administrateurs système dédiés pour maintenir une pile LAMP. Non adapté si vous exigez une étanchéité Zero-Knowledge stricte et une résistance post-quantique.",
    cmpSynCol2Title: "Dropbox / Google / Microsoft",
    cmpSynCol2Desc: "Pratiques pour un usage grand public non critique ou des documents non confidentiels. Totalement inadaptés pour les secrets d'affaires, les brevets, les données médicales HDS ou les organisations soumises à la directive NIS 2 et au RGPD souverain.",
    cmpSynCol3Title: "CYBOU Desktop & Entreprise",
    cmpSynCol3Desc: "<strong>Le choix incontournable</strong> dès lors que la confidentialité, l'hébergement souverain hors juridiction cloud américaine, la résilience sans SPOF et la préparation à l'ère post-quantique sont des impératifs non négociables.",
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
    heroSubtitle: "CYBOU brings private messaging, files and a portable digital identity together in one application. Your content is encrypted on your computer before it is sent.",
    heroPillSlogan: "Your keys. Your space. Your identity goes with you.",


    heroBtnShowcase: "Explore the Desktop App",
    heroBtnSecurity: "Why CYBOU?",

    badgeOpenssl: "Standard OpenSSL primitives · Documented compositions",
    badgeFrance: "Network with France-classified IP admission",
    badgeTls: "Post-Quantum TLS 1.3 Transport (CYBOU P2P)",
    badgePhrase: "Identity keys · Protected local vault",
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
    mockupIdSub3: "Recovery on another computer; the local vault is protected by a password.",
    mockupIdHeading4: "Standard Crypto Engine",
    mockupIdSub4: "Standardized ML-KEM / ML-DSA primitives; documented protocol composition.",

    mockupSecHeading1: "Standard OpenSSL primitives · Documented compositions",
    mockupSecDesc1: "CYBOU uses standard OpenSSL primitives, including ML-KEM and ML-DSA, within documented protocol compositions. Hybrid TLS transport and the application X-Wing profile are distinct constructions; reviewing their composition remains separate from primitive standardization.",
    mockupSecHeading2: "Post-Quantum TLS 1.3 P2P Transport",
    mockupSecDesc2: "TLS 1.3 tunnel with hybrid X25519MLKEM768 group. Any classical fallback is strictly rejected (fails closed).",
    mockupSecHeading3: "Territorial France Sovereignty",
    mockupSecDesc3: "Public P2P traffic strictly restricted to French IP space (fails closed). Sovereign French P2P infrastructure outside US cloud jurisdiction.",

    servicesLabel: "Integrated Services",
    servicesTitle: "Three Pillars for Your Digital Independence.",
    servicesDesc: "Each tool runs directly on your computer without dependence on Big Tech.",

    srvMailTag: "Messaging",
    srvMailTitle: "CYBOU Mail — Your Sovereign Inbox",
    srvMailDesc: "Never let Google or Microsoft scan your private correspondence again. Messages and attachments are encrypted on your device with post-quantum keys and transmitted over TLS 1.3. Zero ads, zero profiling.",
    srvMailStatus: "Integrated flows — Beta acceptance pending",

    srvFilesTag: "Storage",
    srvFilesTitle: "CYBOU Files — Your Documents Under Lock in France",
    srvFilesDesc: "Your files are encrypted and replicated across the network. Signed storage receipts, random-offset checks and downloads with BLAKE3 verification check replicas and allow lost copies to be repaired from a valid source.",
    srvFilesStatus: "Integrated flows — sustained soak pending",

    srvIdentityTag: "Identity",
    srvIdentityTitle: "CYBOU Identity — One Master Key for Digital Life",
    srvIdentityDesc: "Forget vulnerable passwords and hackable SMS codes. Your account is secured by a secure 24-word recovery phrase and a human-readable .cybou name you own for life.",
    srvIdentityStatus: "Identity and vault path integrated on DEVNET",

    srvContinuityTag: "Continuity",
    srvContinuityTitle: "Seamless Recovery & Continuity",
    srvContinuityDesc: "The portable Identity Vault and recovery mechanism let you regain access to your Identity on another computer. Rebuilding messages and files requires accessible publications, openable key capsules and available encrypted copies.",
    srvContinuityStatus: "Integrated reconstruction on DEVNET",

    srvWalletTag: "Resources",
    srvWalletTitle: "A clear service budget",
    srvWalletDesc: "CYBOU uses internal service units to account for resources consumed by operations and storage. Your budget lets you track service usage.",
    srvStorageEconomy: "Storage is organized through leases and availability checks. The service budget tracks resource usage; AUTH remains separate and does not determine storage volume.",
    srvWalletStatus: "Basic operations available in the DEVNET client",

    secLabel: "Security & Transport",
    secTitle: "An End-to-End Digital Fortress.",
    secDesc: "Explore CYBOU protections and their limits, from transport to encrypted storage.",

    secOpensslTag: "Audited Global Standard",
    secOpensslTitle: "Standard OpenSSL primitives · Documented compositions",
    secOpensslDesc: "CYBOU uses OpenSSL ML-KEM and ML-DSA primitives. The hybrid X-Wing profile remains experimental; exact conformance and compositions require review. No product or FIPS module certification is claimed.",

    secTlsTag: "Network Protection",
    secTlsTitle: "100% Post-Quantum TLS 1.3 Encrypted Transport (CYBOU P2P)",
    secTlsDesc: "Every P2P connection requires TLS 1.3 with hybrid X25519MLKEM768 key exchange and rejects classical fallback. This mechanism protects transport; security also depends on endpoints, keys and protocol composition.",

    secE2eeTag: "Private encrypted content",
    secE2eeTitle: "End-to-End Client Encryption & BLAKE3 Chunking",
    secE2eeDesc: "Messages, files and Mail/Files application schemas are encrypted on your computer before sending. The ledger retains publications, Merkle roots and key capsules, not plaintext content. Public identifiers and network metadata remain separate from encrypted content.",

    secFranceTag: "France admission",
    secFranceTitle: "Territorial Sovereignty France & EU",
    secFranceDesc: "Public P2P admission uses local Geo data to accept French IP addresses and fails closed without valid data. This policy supports the goal of a sovereign French network; it does not, by itself, prove physical location or jurisdiction.",

    compLabel: "Objective Comparison",
    compTitle: "CYBOU vs Big Tech.",
    compDesc: "See at a glance what changes when switching from centralized services to a sovereign suite.",
    compThCriteria: "Protection Feature",
    compThCybou: "CYBOU Desktop",
    compThGoogle: "Google (Gmail / Drive)",
    compThAppleMs: "Apple iCloud / Microsoft",
    compRow1Crit: "Message & File Encryption",
    compRow1Cybou: "Mandatory Client-side (Zero-Knowledge)",
    compRow1Google: "Verify offering, version and configuration",
    compRow1AppleMs: "Verify offering, version and configuration",
    compRowCryptoCrit: "Cryptographic Engine & Code",
    compRowCryptoCybou: "Standard OpenSSL primitives · Documented compositions",
    compRowCryptoGoogle: "Verify offering, version and configuration",
    compRowCryptoAppleMs: "Verify offering, version and configuration",
    compRow2Crit: "Network Transport Security",
    compRow2Cybou: "TLS 1.3 Hybride Post-Quantique (X25519 + ML-KEM-768)",
    compRow2Google: "Verify offering, version and configuration",
    compRow2AppleMs: "Verify offering, version and configuration",
    compRow3Crit: "Content scanning for ads / AI",
    compRow3Cybou: "Zero scanning, zero ads, zero AI training",
    compRow3Google: "Verify offering, version and configuration",
    compRow3AppleMs: "Verify offering, version and configuration",
    compRow4Crit: "Jurisdiction & Governance",
    compRow4Cybou: "France & European Union (Sovereign GDPR)",
    compRow4Google: "Verify offering, version and configuration",
    compRow4AppleMs: "Verify offering, version and configuration",
    compRow5Crit: "Quantum Computer Resistance",
    compRow5Cybou: "Integrated NIST ML-DSA post-quantum",
    compRow5Google: "Verify offering, version and configuration",
    compRow5AppleMs: "Verify offering, version and configuration",
    compRow6Crit: "Authentication & Account Ownership",
    compRow6Cybou: "Identity keys and recovery phrase; local vault password",
    compRow6Google: "Verify offering, version and configuration",
    compRow6AppleMs: "Verify offering, version and configuration",

    archLabel: "Under the Hood",
    archTitle: "Technical Architecture & Foundations.",
    archDesc: "For developers, auditors, and enthusiasts: exact consensus engine and network specifications.",

    bento1Title: "Single-operator PoA on DEVNET",
    bento1Desc: "CYBOU operates the only finality signer on the DEVNET network. Every full node independently verifies blocks and state; that verification does not make the trust model BFT.",
    bento1Metric: "Centralized — no BFT fault tolerance",

    bento2Title: "Hybrid X-Wing profile for DEVNET",
    bento2Desc: "The DEVNET protocol publishes an Identity X-Wing capability based on IETF draft -05. The profile does not automatically carry over to Beta/Mainnet, and its presence does not mean Mail/Files are shipped or audited.",
    bento2Metric: "Development profile; independent review required",

    bento3Title: "One Identity, separate cryptographic roles",
    bento3Desc: "Recovery, authorization, key agreement, PoA finality, release signing, and treasury use separate cryptographic roles.",
    bento3Metric: "Local keys and distinct roles",

    bento4Title: "Finality and verifiable state",
    bento4Desc: "One PoA signer finalizes operations. Every full node independently verifies blocks and state transitions.",
    bento4Value: "PoA",
    bento4Metric: "Single finalizer, independent verification",

    step1Title: "Prepare private content",
    step1Desc: "The client turns content into an encrypted ROOT/INDEX/DATA tree and keeps the required local material. Mail/Files schemas remain inside encryption.",

    step2Title: "Protect the content key",
    step2Desc: "RootPublication carries key capsules for recipient KEM capabilities without publishing recipient AccountIDs. X-Wing draft -05 is limited to DEVNET.",

    step3Title: "Authorize a RootPublication",
    step3Desc: "Identity signs a generic operation committing to the root, chunk-inclusion tree, and capsules. Mail has no dedicated consensus operation type.",

    step4Title: "Finalize, admit, retrieve",
    step4Desc: "Finalization authorizes storage admission; it is not enough to show Protected or Sent. Those statuses require confirmed remote copies. The Beta target is two independent remote replicas; local cache does not count. Distinct StorageIds do not prove independent servers or operators.",

    factsheetHeading: "Canonical CYBOU Architecture Parameters",
    dtTransport: "P2P Transport & Encryption",
    ddTransport: "CYBOU P2P encapsulated in strict post-quantum TLS 1.3 (mandatory X25519MLKEM768 group, fails closed). Powered by OpenSSL v3.5.2+ with 32-byte session key export bound to crypto proofs.",
    dtConsensus: "Current DEVNET finality",
    ddConsensus: "Hybrid PoA with one signer operated by CYBOU on DEVNET. Full nodes independently verify; no BFT claim. Target: Central Authority desktop finalizer.",
    dtBootstrap: "Initial peer discovery",
    ddBootstrap: "The first connection uses an ordinary Full Node at a known address, verified through its TLS pin. Participants then form direct P2P connections. Bootstrap is not a separate node type and grants no consensus authority.",
    dtAdmission: "France sovereign P2P admission",
    ddAdmission: "Public P2P admission restricted to French IP space using local Geo data (fails closed); optional local VPN/proxy/Tor filtering.",
    dtGrant: "Account creation",
    ddGrant: "Identity creation with anti-Sybil work and an initial budget for network services.",
    dtStorageEconomy: "Storage management",
    dtFees: "Resource accounting",
    ddFees: "Operations and storage use a service budget with deterministic accounting rules.",
    dtPublication: "Content publication",
    ddPublication: "Generic <code>RootPublication</code>. Mail, Files, and Backup are private encrypted schemas, not separate consensus operations.",
    dtKeys: "Cryptographic key roles",
    ddKeys: "Recovery, Authorization, KEM and PoA roles are separate. OpenSSL primitives and their compositions require separate verification.",
    dtStack: "Technology stack",
    ddStack: "C++20, CMake, Qt 6, LevelDB, OpenSSL v3.5.2+, BLAKE3, and CYBOU P2P transport over TLS 1.3.",

    matrixLabel: "Technical Transparency",
    matrixTitle: "Project status",
    matrixDesc: "Features integrated in code, deployed experimental network and preparation for public availability.",

    col1Title: "Features integrated in code",
    badgeDone: "IMPLEMENTED",
    col1Item1: "<strong>Identity and .cybou names:</strong> AccountCreate with anti-Sybil work, hybrid key roles, portable vault, and finalized name registry.",
    col1Item2: "<strong>PoA finality:</strong> single signer, durable journal and independent block verification by full nodes.",
    col1Item3: "<strong>RootPublication:</strong> generic Identity-authorized operation; no permanent Mail or file objects in consensus state.",
    col1Item4: "<strong>Encrypted content tree:</strong> ordered ROOT/INDEX/DATA chunks addressed by full BLAKE3-256 and built for streaming.",
    col1Item5: "<strong>Admission and transport:</strong> local chunk store, inclusion proofs tied to finalized publications, and CYBOU P2P transport secured with TLS 1.3.",
    col1Item6: "<strong>Resources:</strong> service budget, storage leases and deterministic accounting rules.",

    col2Title: "Experimental deployment",
    badgeWip: "DEVNET",
    col2Item1: "<strong>DEVNET:</strong> full discovery peer and P2P transport with French IP admission.",
    col2Item2: "<strong>Desktop:</strong> Mail, Files and Identity integration; publication, retrieval and index reconstruction.",
    col2Item3: "<strong>Storage:</strong> signed receipts, audits and GET/BLAKE3 checks. Deployed versions and evidence are tracked in the implementation register.",
    col2Item4: "<strong>Scope:</strong> development network; technical testing does not constitute a public service launch.",

    col3Title: "Next steps",
    badgePlanned: "PREPARATION",
    col3Item1: "<strong>Desktop acceptance:</strong> clean installations, Mail/Files workflows and recovery on another computer.",
    col3Item2: "<strong>Endurance and durability:</strong> sustained tests, provider loss and two independent remote replicas for the Beta target.",
    col3Item3: "<strong>Backup:</strong> post-Beta application of the same encrypted graph, with verifiable restore.",
    col3Item4: "<strong>Public availability:</strong> security review, UX acceptance, operating procedures and support.",

    faqLabel: "Frequently Asked Questions",
    faqTitle: "Understanding CYBOU.",
    faqDesc: "What runs on DEVNET, what still needs to be built, and security guarantees.",
    faqQ1: "What is CYBOU in simple terms?",
    faqA1: "CYBOU is a sovereign desktop privacy suite combining private messaging (Mail), encrypted cloud storage (Files), and an identity manager (Identity). Your data is encrypted on your machine before being sent, and all network transport is protected with post-quantum TLS 1.3.",
    faqTlsBadge: "Encrypted Transport",
    faqQTls: "How does post-quantum TLS 1.3 protect my connections on public Wi-Fi or with my ISP?",
    faqATls: "All communications between CYBOU peers and nodes (CYBOU P2P protocol) are sealed inside a strict post-quantum TLS 1.3 tunnel with X25519MLKEM768 key exchange. Whether you use public Wi-Fi at a train station or hotel, or your home fiber internet, no third party or ISP can intercept or read your messages or know what files you transfer. Deep Packet Inspection (DPI) and quantum recording are completely blocked.",
    faqOpensslBadge: "Audited Standard",
    faqQOpenssl: "Are CYBOU's cryptographic algorithms developed in-house (\"homemade crypto\")?",
    faqAOpenssl: "CYBOU uses OpenSSL primitives and documented compositions. ML-KEM and ML-DSA primitives are standardized; the X-Wing profile remains experimental. Tests do not constitute certification.",
    faqQ2: "How is CYBOU different from Gmail or Google Drive?",
    faqA2: "Unlike Google which centralizes your data, holds the decryption keys, and scans contents for advertising or AI model training, CYBOU encrypts everything directly on your computer. No server can read your emails or files, and the entire public network is based in France, shielded from the US Cloud Act.",
    faqQ3: "What do .cybou names and the 24-word recovery phrase do?",
    faqA3: "A .cybou name is your Identity’s readable address. The portable vault retains its stable AccountID and recovery secret. The recovery phrase lets you recover the corresponding keys; the local vault is protected by a password. Recovering messages and files also requires their publications, openable key capsules and an available copy of the encrypted content.",
    faqQFrance: "Why is CYBOU hosted exclusively in France and Europe?",
    faqAFrance: "Digital sovereignty relies on infrastructure independence. By restricting public nodes to French territory, CYBOU ensures that data streams and encrypted chunks remain hosted in France on sovereign P2P infrastructure outside US cloud jurisdiction.",
    faqPqBadge: "Future Security",
    faqQ5: "What does post-quantum cryptography (ML-DSA) mean?",
    faqA5: "Future quantum computers will render classical RSA and ECC encryption obsolete. CYBOU deploys NIST-standardized post-quantum algorithms (ML-DSA-44 and ML-DSA-65) alongside Ed25519 today to protect your correspondence against harvest-now, decrypt-later attacks.",
    faqQ7: "Can I use CYBOU today?",
    faqA7: "The DEVNET network and full open-source codebase are active and verifiable on GitHub. The CYBOU Desktop application is undergoing continuous integration and hardening ahead of public distribution for the Beta phase.",

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
    compSecTitle: "Security, governance and European references",
    compSecDesc: "Client-side encryption, key management and network policy are technical measures. Their assessment must be complemented by governance and evidence for the relevant deployment; no product certification is claimed.",
    compCardRgpdPill: "EU Regulation 2016/679",
    compCardRgpdStatus: "Privacy by Design",
    compCardRgpdTitle: "GDPR — Data protection by design",
    compCardRgpdDesc: "Client-side encryption and private application content. Revocation removes active storage authorization and initiates managed purge by compliant providers. Historical blocks and copies retained by recipients may remain.",
    compCardNis2Pill: "EU Directive 2022/2555",
    compCardNis2Status: "Art. 21 Risk Measures",
    compCardNis2Title: "NIS 2 Directive — Resilience & Supply Chain Security",
    compCardNis2Desc: "Replication and replacement copies can improve availability. Finalization depends on one PoA signer. NIS2 applicability, continuity, incidents and software supply-chain controls require assessment.",
    compCardHdsPill: "CSP Art. L.1111-8",
    compCardHdsStatus: "Medical Secret & HDS",
    compCardHdsTitle: "Healthcare Sector — Medical Secrecy & Health Data",
    compCardHdsDesc: "Encryption protects content under documented assumptions. It does not alone guarantee professional secrecy or HDS compliance; obligations depend on processing and providers.",
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
    cArt0Sub: "Technical measures, responsibilities and hosting obligations require separate assessment.",
    cArt0P1: "Across the cybersecurity and software industry, many vendors claim to be \"GDPR certified\" or \"HDS certified\" merely because they encrypt data. <strong>This claim is legally misleading.</strong> European and French regulations strictly separate two tiers of compliance:",
    cArt0L1Title: "Software and protocol tier (CYBOU):",
    cArt0L2Title: "Physical hosting and operational tier (Datacenters):",
    cArt0BoxTitle: "What this means in practice:",
    cArt0BoxDesc: "Health-data use requires assessment of processing, responsibilities and providers. CYBOU supplies neither automatic HDS-provider selection nor evidence of provider certification.",

    cRgpdStatus: "Full Privacy by Design",
    cRgpdTitle: "GDPR — Personal Data Protection by Design and by Default",
    cRgpdSub: "How CYBOU's Zero-Knowledge architecture and blind consensus structurally prevent non-compliance risks.",
    cRgpdP1: "The GDPR imposes stringent obligations of security, minimization, and data subject rights on data controllers and processors. CYBOU's architecture was engineered from day one to satisfy these requirements natively:",
    cRgpdThArt: "GDPR Article",
    cRgpdThReq: "Legal Requirement",
    cRgpdThSol: "CYBOU Technical Implementation",
    cRgpdArt25Req: "Ensure data protection by design and default without burdensome manual action.",
    cRgpdArt25Sol: "<strong>Client-side encryption.</strong> Mail/Files content and application schemas are encrypted before sending. Publications, roots, capsules and protocol metadata must be distinguished from private content.",
    cRgpdArt5Req: "Data must be adequate, relevant, and limited to what is strictly necessary.",
    cRgpdArt5Sol: "<strong>Encrypted application content.</strong> Filenames and Mail content remain encrypted. The ledger retains account identifiers, publication commitments and capsules; peers observe IP addresses and traffic. These metadata require a data-protection assessment.",
    cRgpdArt17Req: "Ensure the permanent and irreversible erasure of personal data upon request.",
    cRgpdArt17Sol: "<strong>Revocation and managed purge.</strong> Finalized revocation removes active authorization and initiates deletion of unshared chunks by compliant providers, with failure tracking. It does not remove historical blocks or prove erasure of copies retained elsewhere or per-object cryptographic destruction.",
    cRgpdArt20Req: "Enable data export and porting in an open, structured format.",
    cRgpdArt20Sol: "<strong>Deterministic 24-word recovery phrase.</strong> Users can reconstruct their entire inbox, files, and address book on any computer using open key derivation standards.",
    cRgpdArt32Req: "Implement appropriate technical security measures including encryption and system resilience.",
    cRgpdArt32Sol: "<strong>Technical measures assessed against risks.</strong> ML-KEM/ML-DSA primitives, TLS 1.3 and chunk replication. GDPR compliance also requires organizational measures and evidence within the relevant scope.",
    cRgpdArt44Req: "Prohibition of unauthorized international transfers to third countries without adequate safeguards (Schrems II).",
    cRgpdArt44Sol: "<strong>Strict territorial sovereignty (France-only).</strong> Public P2P admission is locally geo-filtered (fails closed). Zero US servers: sovereign French P2P infrastructure outside US cloud jurisdiction.",

    cNis2Status: "Risk Management Measures",
    cNis2Title: "NIS 2 Directive — Operational Resilience & Supply Chain Security",
    cNis2Sub: "Meeting cybersecurity obligations for Essential and Important Entities across critical sectors.",
    cNis2P1: "EU Directive NIS 2 imposes rigorous obligations across 18 critical sectors (healthcare, energy, transport, digital infrastructure). CYBOU aligns directly with Article 21 requirements:",
    cNis2Item1Title: "Software Supply Chain Security:",
    cNis2Item1Desc: "CYBOU uses standard OpenSSL primitives, including ML-KEM and ML-DSA, within documented protocol compositions. Hybrid TLS transport and the application X-Wing profile are distinct constructions; reviewing their composition remains separate from primitive standardization.",
    cNis2Item2Title: "Anticipation of Quantum Threats (ENISA & ANSSI):",
    cNis2Item2Desc: "The risk of \"Harvest Now, Decrypt Later\" attacks is mitigated today via hybrid TLS 1.3 handshakes (X25519MLKEM768) and quantum-resistant identity signatures (ML-DSA-44/65).",
    cNis2Item3Title: "Resilience, Business Continuity & Disaster Recovery:",
    cNis2Item3Desc: "Storage aims to limit the impact of replica loss through availability checks and repair from a valid copy. Distinct StorageIds do not prove independent machines or operators. Repair depends on valid sources and available providers; finality depends on the single PoA.",
    cNis2Item4Title: "Incident Handling & Vulnerability Disclosure (SECURITY.md):",
    cNis2Item4Desc: "A documented vulnerability disclosure policy (SECURITY.md) ensures responsible reporting, aligned with the Cyber Resilience Act (CRA) requirements.",

    cHdsStatus: "French Public Health Code L.1111-8",
    cHdsTitle: "Healthcare Sector — Professional Secrecy, Health Data & HDS",
    cHdsSub: "Technical guarantees for healthcare professionals, clinics, laboratories, and hospitals.",
    cHdsP1: "Medical data represents the most sensitive category of personal data (GDPR Article 9). In France, handling is governed by both the Penal Code and the Public Health Code.",
    cHdsH1: "1. Health-data confidentiality",
    cHdsP2: "CYBOU encrypts messages and files on the endpoint before sending. For health data, these measures must be complemented by access management, endpoint security and assessment of obligations applicable to processing and providers. Encryption alone does not establish HDS compliance.",
    cHdsH2: "2. Health Data Hosting (HDS — CSP Art. L.1111-8)",
    cHdsP3: "Under French law, hosting patient personal health data gathered during diagnosis or care requires an ANS-certified HDS provider. Here is how CYBOU operationalizes this requirement:",
    cHdsThMode: "Usage Scenario",
    cHdsThCadre: "Regulatory Framework",
    cHdsThCybou: "CYBOU Compliance",
    cHdsMode1Cadre: "The hospital or private clinic retains data on internal servers and local terminals.",
    cHdsMode1Cybou: "<strong>Scope requires assessment.</strong> Self-hosting does not establish processing compliance. Controls, data flows and obligations still require verification.",
    cHdsMode2Cadre: "Encrypted data is outsourced to remote storage providers operated by third parties.",
    cHdsMode2Cybou: "<strong>Providers require verification.</strong> Assess HDS obligations and the certified scope of services used. CYBOU Full Nodes do not announce HDS status.",
    cHdsBoxTitle: "Healthcare Integrity Commitment:",
    cHdsBoxDesc: "CYBOU claims no HDS or SecNumCloud certification. Encryption does not replace verification of providers, operational controls and processing obligations.",

    cIsoStatus: "Annex A — Security Controls",
    cIsoTitle: "ISO/IEC 27001:2022 — Control Mapping Matrix",
    cIsoSub: "How CYBOU's technical architecture helps organizations fulfill Information Security Management System (ISMS) controls.",
    cIsoP1: "ISO/IEC 27001 specifies best practices for protecting information assets. The table below details CYBOU's direct support for Annex A controls (2022 revision):",
    cIsoThCode: "Annex A Control",
    cIsoThName: "Control Objective",
    cIsoThSol: "CYBOU Implementation",
    cIsoA515Req: "Access control and authentication",
    cIsoA515Sol: "Authentication through hybrid ML-DSA-44 and Ed25519 signatures, without a centralized account password. The local vault is protected by a password.",
    cIsoA82Req: "Privileged access rights management",
    cIsoA82Sol: "Zero-Knowledge model: no network administrator or PoA operator possesses the cryptographic capability to open user data capsules.",
    cIsoA812Req: "Data leakage prevention (DLP)",
    cIsoA812Sol: "Client-side encryption before transmission. Already encrypted chunks are identified and verified by their BLAKE3-256 hash; BLAKE3 is a hash function, not an encryption algorithm.",
    cIsoA814Req: "Redundancy of processing facilities",
    cIsoA814Sol: "Replication to providers with distinct StorageIds, GET and BLAKE3 checks, followed by attempted repair. These identifiers do not prove geographical diversity or physical independence.",
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
    cCiaIDesc: "<strong>Verifiable integrity:</strong> Each encrypted chunk is identified by its BLAKE3-256 hash. The client verifies retrieved data; nodes verify publications and transitions finalized by PoA. The signing journal refuses to sign when a conflict is detected.",
    cCiaA: "3. Availability (A)",
    cCiaADesc: "<strong>Checked availability:</strong> StorageService uses signed receipts, random-offset audits and full downloads verified with BLAKE3. Repair requires a valid copy and an available provider. A successful check establishes availability at the time of the check.",

    cAnssiStatus: "National Regulatory Framework",
    cAnssiTitle: "ANSSI & French Digital Sovereignty",
    cAnssiSub: "Alignment with the recommendations of the French National Cybersecurity Agency.",
    cAnssiP1: "ANSSI defines cybersecurity guidelines for the French State, Operators of Vital Importance (OIV), and businesses. CYBOU is directly aligned with these guidelines:",
    cAnssiItem1Title: "Post-Quantum Transition & Hybridization:",
    cAnssiItem1Desc: "ANSSI explicitly recommends a hybrid approach (combining an established classical algorithm with a standardized post-quantum scheme) to prevent security regressions. CYBOU rigorously adheres to this by combining Ed25519 + ML-DSA for signatures and X25519 + ML-KEM-768 for key agreement.",
    cAnssiItem2Title: "Declaration Regime for Cryptology Means (CPCE):",
    cAnssiItem2Desc: "Pursuant to French CPCE articles L. 133-1 et seq., the use of cryptology means providing confidentiality and authentication is completely unrestricted for individuals and businesses.",
    cAnssiItem3Title: "Data Sovereignty & Trusted Cloud (SecNumCloud):",
    cAnssiItem3Desc: "Provider and infrastructure selection must be assessed within the deployment scope. Any hosting qualification must be verified with the provider; it does not automatically certify CYBOU or the entire processing activity.",
    cAnssiCtaTitle: "Need a Compliance Evaluation for Your Organization?",
    cAnssiCtaDesc: "Our technical and legal team is available to assist with on-premise deployments, GDPR compliance roadmaps, and HDS storage integration.",
    cAnssiCtaBtn: "Contact the Team",

    footProdEnterprise: "CYBOU Enterprise",
    footSecEnterprise: "Dedicated Private Networks",
    srvEnterpriseTag: "Custom",
    srvEnterpriseTitle: "CYBOU Enterprise — Dedicated & Isolated Private Network",
    srvEnterpriseDesc: "CYBOU Enterprise targets a network dedicated to your organization: its own genesis, a PoA key under your control and storage on your chosen infrastructure.",
    srvEnterpriseLink: "Explore enterprise solutions &rarr;",

    // Dedicated Enterprise Page (entreprise.html)
    eBreadHome: "Home",
    eBreadCurrent: "Enterprise Solutions & Dedicated Networks",
    eHeroBadge: "DEDICATED DEPLOYMENT & SOVEREIGN ISOLATION",
    eHeroBadgeText: "Cryptographic Isolation & Dedicated Governance",
    eHeroTitle: "Build Your Secure & Dedicated Enterprise Private Network",
    eHeroDesc: "We are building CYBOU Enterprise for organizations seeking their own trust domain: a dedicated private network, its own genesis, an organization-controlled PoA key and storage on chosen infrastructure.",

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
    eCard4Desc: "Your encrypted files and messages are replicated across your servers. Signed receipts, random-offset audits and BLAKE3-verified downloads check the copies. Losses are repaired from a valid copy when a provider is available.",

    eArt2Pill: "Under the Hood",
    eArt2Status: "Open Specifications",
    eArt2Title: "How does private network isolation work?",
    eArt2Sub: "All participants run the exact same battle-tested core software, but the cryptographic boundary is strictly confined to your enterprise.",
    eArt2P1: "The Enterprise direction is based on a distinct network with its own genesis and network identity. The organization controls the PoA finality key and chooses its storage infrastructure. Dedicated deployment procedures and tooling are part of the product we are building:",

    eStep1Title: "Genesis Definition",
    eStep1Desc: "Generation of a genesis.json file signed by the offline Network Private Key, with a custom enterprise network identifier and initial authority parameters.",
    eStep2Title: "PoA Central Authority",
    eStep2Desc: "IT SecOps generates the PoA finalization key on a secured internal machine to seal enterprise blocks.",
    eStep3Title: "Initial contact points",
    eStep3Desc: "Ordinary Full Nodes at known addresses on your intranet enable initial discovery; peers then communicate directly.",
    eStep4Title: "Private Storage Cluster",
    eStep4Desc: "Configuring the Full Node storage capacity (cybou node run) on internal servers to host encrypted chunk replicas.",

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
    eRowBootPriv: "Ordinary Full Nodes at known addresses on your LAN / Intranet / VPN",
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
    eGovItem1Desc: "The exact same cybou binary acts as a full node, bootstrap node, and storage provider. It runs headlessly under Linux as a standard systemd service with negligible memory footprint, easily deployable via Ansible, Puppet, or Terraform.",
    eGovItem2Title: "Intuitive desktop application for team members:",
    eGovItem2Desc: "Employees run CYBOU Desktop on workstations (Windows, Linux, macOS). An intuitive interface combining encrypted mail, file drive, and sovereign identity with zero employee retraining needed.",
    eGovItem3Title: "Elimination of vulnerable passwords and compromised directory stores:",
    eGovItem3Desc: "No centralized LDAP or Active Directory databases vulnerable to Golden Ticket or credential stuffing attacks. Each employee holds an inviolable 24-word recovery phrase and locally sealed asymmetric keypairs.",
    eGovItem4Title: "Key lifecycle and rotation (IdentityRotate):",
    eGovItem4Desc: "When changing hardware or upon suspected endpoint compromise, the atomic IdentityRotate transaction revokes and refreshes signature and encryption keys instantly without losing identity history.",
    eGovItem5Title: "Disaster recovery and deterministic BCP / DRP:",
    eGovItem5Desc: "Continuity relies on backups of Identity vaults, finality keys and their signing history, together with accessible data copies. Restoring the authority and restoring content are separate procedures; the genesis key alone cannot recover files.",

    eArt5Pill: "Industry Applications",
    eArt5Status: "High-Security Sectors",
    eArt5Title: "Strategic use cases for demanding organizations",
    eArt5Sub: "Engineered to protect your organization's most critical assets: proprietary knowledge, corporate secrets, and sovereignty.",

    eUse1Pill: "Advanced Manufacturing",
    eUse1Badge: "Trade Secrets",
    eUse1Title: "R&D, Intellectual Property & Patents",
    eUse1Desc: "Client-side encryption of plans, formulations, source code and strategic documents, with key control and verified copies. Endpoint security and recovery procedures complement these mechanisms.",

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
    eRoiTitle: "Infrastructure sized for your needs",
    eRoiDesc: "Sizing, operations, support and integrations determine project cost. An assessment tailored to your environment evaluates these needs and expected benefits without promising a universal payback period.",

    eArt6Pill: "Take Action",
    eArt6Status: "Pilot & Deployment",
    eArt6Title: "Deploy your first CYBOU pilot network",
    eArt6Sub: "Our architecture is fully documented, open, and verifiable. Set up an internal proof-of-concept or connect with our engineering team.",
    eContactP1: "Interested in evaluating CYBOU on DEVNET or designing a hardened enclave for executive teams? Here are your next steps:",
    eCtaTitle: "Connect with the CYBOU engineering team for a private network",
    eCtaDesc: "Assistance with custom genesis generation, infrastructure sizing recommendations, and compliance audits.",
    eCtaBtn: "Contact the Engineering Team",

    // Comparison Teaser on Homepage
    compCardNextPill: "Open Source & Self-Hosting",
    compCardNextStatus: "P2P vs. LAMP",
    compCardNextTitle: "CYBOU and Nextcloud — Choosing an architecture",
    compCardNextDesc: "CYBOU combines client-side encryption, local keys and P2P storage. To compare a Nextcloud deployment, examine its configuration, encryption options and operational responsibilities.",
    compCardDropPill: "US Cloud Storage",
    compCardDropStatus: "Cloud Act & AI Scans",
    compCardDropTitle: "CYBOU and Dropbox — Key and storage management",
    compCardDropDesc: "CYBOU encrypts content before sending and keeps keys under Identity control. Compare hosted services by offering, encryption options and selected processing terms.",
    compCardGafamPill: "Centralized US Suites",
    compCardGafamStatus: "Digital Freedom",
    compCardGafamTitle: "CYBOU vs. Google Workspace & Microsoft 365",
    compCardGafamDesc: "CYBOU targets a private workspace based on local keys and an open protocol. Selection against Google Workspace or Microsoft 365 depends on use cases, integrations and security configuration.",
    compCardPqPill: "Global Standard OpenSSL v3.5.2+",
    compCardPqStatus: "Post-Quantum",
    compCardPqTitle: "Immediate Quantum Resistance (NIST FIPS 203/204)",
    compCardPqDesc: "CYBOU integrates ML-KEM and ML-DSA in a documented hybrid architecture. A post-quantum comparison must verify versions and mechanisms actually deployed in each solution.",
    compTeaserCtaTitle: "View the Complete & Detailed Comparison Matrix",
    compTeaserCtaDesc: "Comprehensive table across 8 security criteria, deep architectural breakdown vs. Nextcloud and Dropbox, and decision guide for CISOs.",
    compTeaserCtaBtn: "Explore the Full Comparison",

    // Dedicated Comparison Page (comparatif.html)
    cmpBreadHome: "Home",
    cmpBreadCurrent: "Objective Solution Comparison",
    cmpHeroTag: "Technical & Sovereign Analysis",
    cmpHeroTitle: "CYBOU vs. Nextcloud, Dropbox, Google & Microsoft",
    cmpHeroDesc: "Deeply understand what differentiates the sovereign CYBOU suite from centralized US clouds (Google, Microsoft, Dropbox) and traditional self-hosted servers (Nextcloud). P2P architecture, OpenSSL v3.5.2+ post-quantum cryptography, infrastructure independence, and Zero-Knowledge data model.",
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
    cmpR1Nextcloud: "Verify offering, version and configuration",
    cmpR1Dropbox: "Verify offering, version and configuration",
    cmpR1Google: "Verify offering, version and configuration",
    cmpR1MsApple: "Verify offering, version and configuration",

    cmpR2Crit: "Cryptographic engine and code",
    cmpR2Cybou: "Standard OpenSSL primitives · Documented compositions",
    cmpR2Nextcloud: "Verify offering, version and configuration",
    cmpR2Dropbox: "Verify offering, version and configuration",
    cmpR2Google: "Verify offering, version and configuration",
    cmpR2MsApple: "Verify offering, version and configuration",

    cmpR3Crit: "Resistance to quantum attacks",
    cmpR3Cybou: "Native: ML-KEM-768 (TLS 1.3) + ML-DSA-44/65 (Identities)",
    cmpR3Nextcloud: "Verify offering, version and configuration",
    cmpR3Dropbox: "Verify offering, version and configuration",
    cmpR3Google: "Verify offering, version and configuration",
    cmpR3MsApple: "Verify offering, version and configuration",

    cmpR4Crit: "Architecture & Single Point of Failure",
    cmpR4Cybou: "Unified decentralized C++ P2P, self-healing chunk replicas",
    cmpR4Nextcloud: "Verify offering, version and configuration",
    cmpR4Dropbox: "Verify offering, version and configuration",
    cmpR4Google: "Verify offering, version and configuration",
    cmpR4MsApple: "Verify offering, version and configuration",

    cmpR5Crit: "Jurisdiction & Extraterritorial laws",
    cmpR5Cybou: "France & EU (France-only geo-filtered P2P, outside US cloud jurisdiction)",
    cmpR5Nextcloud: "Verify offering, version and configuration",
    cmpR5Dropbox: "Verify offering, version and configuration",
    cmpR5Google: "Verify offering, version and configuration",
    cmpR5MsApple: "Verify offering, version and configuration",

    cmpR6Crit: "Scanning for ads or AI model training",
    cmpR6Cybou: "Zero scans, zero AI, zero ads (opaque cryptographic chunks)",
    cmpR6Nextcloud: "Verify offering, version and configuration",
    cmpR6Dropbox: "Verify offering, version and configuration",
    cmpR6Google: "Verify offering, version and configuration",
    cmpR6MsApple: "Verify offering, version and configuration",

    cmpR7Crit: "Authentication & Account ownership",
    cmpR7Cybou: "Access through Identity keys; password-protected local vault",
    cmpR7Nextcloud: "Verify offering, version and configuration",
    cmpR7Dropbox: "Verify offering, version and configuration",
    cmpR7Google: "Verify offering, version and configuration",
    cmpR7MsApple: "Verify offering, version and configuration",

    cmpR8Crit: "Deployment & Maintenance overhead",
    cmpR8Cybou: "Standalone C++ binary with zero external dependencies",
    cmpR8Nextcloud: "Verify offering, version and configuration",
    cmpR8Dropbox: "Verify offering, version and configuration",
    cmpR8Google: "Verify offering, version and configuration",
    cmpR8MsApple: "Verify offering, version and configuration",

    cmpNextPill: "Open Source Analysis",
    cmpNextStatus: "Architecture and control",
    cmpNextTitle: "CYBOU and Nextcloud — Selection criteria",
    cmpNextSub: "Compare use cases, configurations and responsibilities. The CYBOU principles below help assess relevant differences.",
    cmpNextP1: "Compare use cases, configurations and responsibilities. The CYBOU principles below help assess relevant differences.",
    cmpNextItem1Title: "Client-side encryption",
    cmpNextItem1Desc: "CYBOU encrypts content before transmission and keeps keys local. Verify options and key management in each compared solution.",
    cmpNextItem2Title: "Copies and recovery",
    cmpNextItem2Desc: "CYBOU checks replicas through audits and BLAKE3 verification, then repairs from a valid copy. Compare backup mechanisms, failure tests and availability commitments.",
    cmpNextItem3Title: "Open protocol and Identity",
    cmpNextItem3Desc: "Open code and the portable Identity vault allow mechanisms to be examined and keys to remain under user control. Compare export, recovery and operational responsibilities.",
    cmpNextItem4Title: "Operations and dependencies",
    cmpNextItem4Desc: "CYBOU uses a common executable for desktop and full-node operation. Sizing and maintenance must be assessed in the chosen environment.",

    cmpDropPill: "US Cloud Storage",
    cmpDropStatus: "Architecture and control",
    cmpDropTitle: "CYBOU and Dropbox — Selection criteria",
    cmpDropSub: "Compare use cases, configurations and responsibilities. The CYBOU principles below help assess relevant differences.",
    cmpDropP1: "Compare use cases, configurations and responsibilities. The CYBOU principles below help assess relevant differences.",
    cmpDropItem1Title: "Client-side encryption",
    cmpDropItem1Desc: "CYBOU encrypts content before transmission and keeps keys local. Verify options and key management in each compared solution.",
    cmpDropItem2Title: "Copies and recovery",
    cmpDropItem2Desc: "CYBOU checks replicas through audits and BLAKE3 verification, then repairs from a valid copy. Compare backup mechanisms, failure tests and availability commitments.",
    cmpDropItem3Title: "Open protocol and Identity",
    cmpDropItem3Desc: "Open code and the portable Identity vault allow mechanisms to be examined and keys to remain under user control. Compare export, recovery and operational responsibilities.",

    cmpGafamPill: "Big Tech Monopolies",
    cmpGafamStatus: "Architecture and control",
    cmpGafamTitle: "CYBOU and Google Workspace and Microsoft 365 — Selection criteria",
    cmpGafamSub: "Compare use cases, configurations and responsibilities. The CYBOU principles below help assess relevant differences.",
    cmpGafamP1: "Compare use cases, configurations and responsibilities. The CYBOU principles below help assess relevant differences.",
    cmpGafamItem1Title: "Client-side encryption",
    cmpGafamItem1Desc: "CYBOU encrypts content before transmission and keeps keys local. Verify options and key management in each compared solution.",
    cmpGafamItem2Title: "Copies and recovery",
    cmpGafamItem2Desc: "CYBOU checks replicas through audits and BLAKE3 verification, then repairs from a valid copy. Compare backup mechanisms, failure tests and availability commitments.",
    cmpGafamItem3Title: "Open protocol and Identity",
    cmpGafamItem3Desc: "Open code and the portable Identity vault allow mechanisms to be examined and keys to remain under user control. Compare export, recovery and operational responsibilities.",

    cmpSynPill: "Decision Guide",
    cmpSynStatus: "Strategic Synthesis",
    cmpSynTitle: "Which Solution Aligns with Your Priorities?",
    cmpSynSub: "A clear summary to guide CISOs, IT directors, and privacy-conscious organizations.",
    cmpSynCol1Title: "Nextcloud",
    cmpSynCol1Desc: "Well-suited for teams seeking a broad collaborative suite (calendars, video conferencing, kanban) with dedicated sysadmins capable of maintaining a LAMP stack. Not recommended if strict Zero-Knowledge confidentiality and post-quantum resistance are required.",
    cmpSynCol2Title: "Dropbox / Google / Microsoft",
    cmpSynCol2Desc: "Convenient for non-critical consumer workflows or unclassified documents. Completely inappropriate for trade secrets, patent filings, HDS health records, or entities subject to NIS 2 and sovereign GDPR.",
    cmpSynCol3Title: "CYBOU Desktop & Enterprise",
    cmpSynCol3Desc: "<strong>The definitive choice</strong> whenever airtight confidentiality, sovereign hosting outside US cloud jurisdiction, zero-SPOF resilience, and future-proof post-quantum security are non-negotiable mandates.",
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

// --- Galactic flight background shared by all pages ---
function initHeroParticles() {
  document.getElementById('hero-particles')?.remove();
  const canvas=document.createElement('canvas');
  canvas.id='hero-particles';canvas.setAttribute('aria-hidden','true');
  Object.assign(canvas.style,{position:'fixed',inset:'0',width:'100%',height:'100%',pointerEvents:'none',zIndex:'10'});
  document.body.appendChild(canvas);
  const ctx=canvas.getContext('2d');if(!ctx)return;
  const reduced=matchMedia('(prefers-reduced-motion: reduce)');
  let width=0,height=0,stars=[],frame=0,last=0,boost=0,previousScroll=scrollY;
  let cx=.5,cy=.42,targetX=.5,targetY=.42;
  function star(randomDepth=true){
    const angle=Math.random()*Math.PI*2;
    const radius=.12+Math.sqrt(Math.random())*1.6;
    return {x:Math.cos(angle)*radius,y:Math.sin(angle)*radius,
      z:randomDepth?.25+Math.random()*3:3,size:.75+Math.random()*1.8,
      tone:Math.random()};
  }
  function draw(dt=0){
    ctx.clearRect(0,0,width,height);
    const focal=Math.max(width,height)*.55;
    const originX=width*cx,originY=height*cy;
    boost*=Math.exp(-dt*2.4);
    cx+=(targetX-cx)*(1-Math.exp(-dt*2));cy+=(targetY-cy)*(1-Math.exp(-dt*2));
    for(let i=0;i<stars.length;i++){
      let s=stars[i];const oldZ=s.z;
      s.z-=dt*(.16+boost);
      if(s.z<.12){stars[i]=star(false);continue;}
      const x=originX+s.x/s.z*focal,y=originY+s.y/s.z*focal;
      if(x<-60||x>width+60||y<-60||y>height+60){stars[i]=star(false);continue;}
      const depth=Math.min(1,1/s.z);
      const alpha=.12+depth*.28;
      const color=s.tone>.75?'95,100,150':'40,110,110';
      ctx.fillStyle=`rgba(${color},${alpha})`;
      ctx.beginPath();ctx.arc(x,y,Math.min(3.8,s.size*depth),0,Math.PI*2);ctx.fill();
      // Perspective streaks appear only on nearby stars and during acceleration.
      if(dt && (s.z<.8||boost>.25)){
        const previous=Math.min(3,oldZ+(.025+boost*.04));
        const px=originX+s.x/previous*focal,py=originY+s.y/previous*focal;
        const gradient=ctx.createLinearGradient(px,py,x,y);
        gradient.addColorStop(0,`rgba(${color},0)`);
        gradient.addColorStop(1,`rgba(${color},${alpha*.65})`);
        ctx.strokeStyle=gradient;ctx.lineWidth=1;
        ctx.beginPath();ctx.moveTo(px,py);ctx.lineTo(x,y);ctx.stroke();
      }
    }
  }
  function resize(){
    width=innerWidth;height=innerHeight;
    const dpr=Math.min(devicePixelRatio||1,2);
    canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0);
    stars=Array.from({length:Math.min(650,Math.max(220,Math.floor(width*height/2000)))},()=>star());draw();
  }
  function tick(now){
    if(!last||now-last>=32){draw(last?Math.min((now-last)/1000,.08):1/30);last=now;}
    frame=requestAnimationFrame(tick);
  }
  function sync(){
    cancelAnimationFrame(frame);last=0;
    if(!document.hidden&&!reduced.matches)frame=requestAnimationFrame(tick);else draw();
  }
  addEventListener('pointermove',event=>{
    targetX=.5+(event.clientX/width-.5)*.3;
    targetY=.42+(event.clientY/height-.5)*.25;
  },{passive:true});
  document.documentElement.addEventListener('pointerleave',()=>{targetX=.5;targetY=.42;});
  addEventListener('scroll',()=>{
    const delta=Math.abs(scrollY-previousScroll);previousScroll=scrollY;
    if(!reduced.matches)boost=Math.min(1.6,boost+delta*.004);
  },{passive:true});
  addEventListener('resize',resize);
  document.addEventListener('visibilitychange',sync);reduced.addEventListener('change',sync);
  resize();sync();
}
