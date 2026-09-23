/* ==========================================================================
   CYBOU.FR — Technical Engine & Dynamic Bilingual Localization
   Engineered for transparent systems presentation & responsive state verification
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initLanguageSwitch();
  initOnboardingSimulator();
  checkUrlLanguage();
});

// --- 1. Language Toggle (French / English) ---
const translations = {
  fr: {
    badgeStatus: "v0.0.1 Baseline",
    navStatus: "État du projet",
    navArch: "Piliers",
    navMailTx: "Protocole MailTx",
    navSpecs: "Fiche technique",
    navSim: "Simulateur",
    navFaq: "FAQ",
    navRepo: "Dépôt GitHub",

    heroTag: "R&D Ouverte • PQ by Design • Architecture P2P C++20 • Souveraineté Européenne",
    heroAccent: "Messagerie P2P souveraine & registre BFT.",
    heroSubtitle: "Une architecture logicielle conçue en France, émancipée des géants du cloud. Chiffrement hybride post-quantique, finalité BFT explicite et isolation stricte de l'autorité de l'appareil.",

    statusCalloutTitle: "Statut de développement : v0.0.1 (Baseline d'ingénierie & durcissement)",
    statusCalloutBody: "CYBOU n'est pas un service commercial ou une application de bureau déployée pour le grand public. Le réseau s'exécute actuellement sur une chaîne de démarrage interne (CYBOU-DEV v0.0.2). Cette plateforme présente les fondations architecturales, les transitions d'état formelles en C++ et l'état réel d'avancement du projet.",

    heroBtnStatus: "Consulter l'état réel d'avancement",
    heroBtnCode: "Code source (GitHub)",

    matrixLabel: "Transparence technique",
    matrixTitle: "Matrice d'implémentation v0.0.1.",
    matrixDesc: "Distinction stricte et vérifiable entre le code validé en tests unitaires, les modules en cours de refonte et les composants de la feuille de route.",

    col1Title: "Validé & Testé (v0.0.1)",
    badgeDone: "FONCTIONNEL",
    col1Item1: "<strong>Création de compte native & sans intermédiaire (doc 70) :</strong> opération AccountCreateOpV1 avec preuve de travail anti-Sybil.",
    col1Item2: "<strong>Identifiant typé AccountId (32 octets) :</strong> gestion déterministe du solde système (<code>SystemBalance</code>).",
    col1Item3: "<strong>Bonus d'accueil atomique :</strong> transfert immédiat du bonus depuis <code>OnboardingPool</code> vers <code>SystemBalance</code>.",
    col1Item4: "<strong>Persistance d'état LevelDB :</strong> instantanés atomiques avec nommage et isolats stricts CYBOU.",
    col1Item5: "<strong>Définition native MailTx (doc 16) :</strong> opération de premier rang, 1 destinataire, taille bornée, frais prévisibles (sans surenchère).",
    col1Item6: "<strong>Couverture de tests :</strong> 684 tests unitaires validés, élimination intégrale des dépendances et terminologies Bitcoin résiduelles.",

    col2Title: "En cours de consolidation",
    badgeWip: "EN COURS",
    col2Item1: "<strong>Moteur de consensus BFT (doc 07) :</strong> finalité explicite multi-validateurs avec admission contrôlée par l'opérateur (seuil f=1 dès 4 validateurs à poids égal).",
    col2Item2: "<strong>Époques Proof of Trust (PoT) :</strong> calcul arithmétique entier strict dérivé de la hauteur de bloc (zéro dépendance à l'horloge locale).",
    col2Item3: "<strong>Séparation stricte des clés (doc 68) :</strong> 4 domaines isolés (Autorité, Validateur, Signature de release, Trésorerie).",
    col2Item4: "<strong>Remplacement PoW :</strong> élimination définitive des reliquats de preuve de travail de la chaîne de bootstrap <code>CYBOU-DEV v0.0.2</code>.",

    col3Title: "Feuille de route planifiée",
    badgePlanned: "PLANIFIÉ",
    col3Item1: "<strong>Encapsulation Post-Quantique HPKE :</strong> intégration production du profil hybride ML-KEM-768 + X25519 (doc 49).",
    col3Item2: "<strong>Stockage d'objets décentralisé :</strong> couche de pré-stockage pour validateurs et stockage d'objets pour pièces jointes à grande échelle.",
    col3Item3: "<strong>Client graphique de bureau :</strong> interface utilisateur native (Inbox, Sent, état de lecture) avec conservation des index localement sur le poste.",
    col3Item4: "<strong>Preuves d'inclusion de correspondance :</strong> certificats de finalité vérifiables hors-chaîne par des tiers certificateurs.",

    archLabel: "Fondations de conception",
    archTitle: "Rupture avec les architectures centralisées.",
    archDesc: "Les quatre piliers de conception garantissant la souveraineté et l'intégrité pérenne des échanges numériques.",

    bento1Title: "Émancipation totale des hébergeurs de cloud tiers",
    bento1Desc: "Aucun serveur central, aucun compte sous juridiction extra-européenne, aucun point unique de censure ou de coupure. Les nœuds pairs échangent directement via un protocole P2P durci en C++20 sans passerelle obligatoire.",
    bento1Metric: "Résilience autonome native au niveau protocolaire",

    bento2Title: "Chiffrement Post-Quantique HPKE",
    bento2Desc: "Anticipation des attaques par interception et déchiffrement ultérieur grâce au standard hybride ML-KEM-768 et X25519 (RFC 9180).",
    bento2Metric: "Standard NIST post-quantique",

    bento3Title: "Autorité exclusive de l'appareil",
    bento3Desc: "Les clés privées ne transitent jamais sur le réseau. L'opérateur ne détient aucun passe-partout et ne peut modifier les soldes ou intercepter les courriers.",
    bento3Metric: "Accès dérobé ou clé maîtresse centrale",

    bento4Title: "Finalité BFT explicite & Économie déterministe",
    bento4Desc: "Validation par consensus byzantin à poids égal (poids = 1). Arithmétique entière stricte, réserve plafonnée à 100 milliards de CYBOU, 0 décimale, et répartition déterministe des frais : 3/4 pour la Sécurité et 1/4 pour l'Accueil.",
    bento4Metric: "Plafond absolu d'unités CYBOU (0 décimale)",

    flowLabel: "Ingénierie du message",
    flowTitle: "Le cycle de vie d'une transaction MailTx.",
    flowDesc: "Comment un message textuel est engagé, scellé et enregistré de manière vérifiable sur le réseau.",

    step1Title: "Engagement salé de contenu",
    step1Desc: "Le corps textuel est lié à un sel cryptographique imprédictible (doc 69). L'empreinte résultante garantit l'intégrité sans exposer le contenu en clair aux validateurs.",

    step2Title: "Chiffrement Hybride HPKE",
    step2Desc: "Chiffrement de bout en bout ciblé pour la clé publique du destinataire avec protection post-quantique. Aucun tiers ne peut accéder au message en clair.",

    step3Title: "Opération MailTx native",
    step3Desc: "Émission d'une transaction de premier rang (pas de payload arbitraire Bitcoin Script). Frais déterministes fonction de la taille, sans enchère de priorité.",

    step4Title: "Consensus BFT & Index Local",
    step4Desc: "Le bloc est scellé par les validateurs BFT. Aucun état par courrier n'est conservé dans le consensus : le client local indexe ses courriers reçus et envoyés.",

    specLabel: "Spécifications machine & humaine",
    specTitle: "Fiche technique du protocole CYBOU.",
    specDesc: "Données d'ingénierie structurées et indexables pour moteurs de recherche et assistants d'analyse IA.",
    factsheetHeading: "Paramètres canoniques de l'architecture CYBOU v0.0.1",

    dtConsensus: "Modèle de consensus",
    dtPoT: "Époques de confiance (PoT)",
    dtSupply: "Offre maximale (MAX_SUPPLY)",
    dtGrant: "Subvention d'accueil (Welcome Grant)",
    dtFees: "Régime des frais de transaction",
    dtMailTx: "Format MailTx (v1)",
    dtKeys: "Séparation des clés d'opérateur",
    dtStack: "Socle technologique",

    simLabel: "Validation d'état C++",
    simTitle: "Création de compte native & Preuve anti-Sybil.",
    simDesc: "Démonstration de la création native sans invitation selon AccountCreateOpV1 et AccountCreationWorkV1 (doc 70).",
    simBeneficiary: "Identifiant de compte (AccountId — 32 octets)",
    simWorkLabel: "Preuve de travail anti-Sybil (CYBOU/ACCOUNT-CREATE-WORK/V1)",
    simSubmit: "Calculer PoW & Créer le compte",
    simStatusLabel: "Statut du protocole",
    simStatusReady: "Prêt à créer l'identité",
    simStatusSuccess: "Compte créé • Preuve anti-Sybil validée • Bonus crédité",
    simPoolSource: "OnboardingPool (Réserve globale)",
    simBalanceTarget: "SystemBalance (Solde Système)",
    simAntiSybil: "Anti-Sybil & Intégrité",
    simQuota: "Quota initial par époque",

    faqLabel: "Questions fréquentes",
    faqTitle: "Architecture, Cryptographie & Gouvernance.",
    faqDesc: "Des réponses précises et transparentes sur nos choix techniques, notre feuille de route et la sécurité post-quantique.",
    faqPqBadge: "Sécurité Post-Quantique • PQ by Design",
    faqQ1: "Que signifie « Post-Quantique dès la conception » (PQ by design) pour CYBOU ?",
    faqA1P1: "La plupart des systèmes de communication historiques s'appuient sur RSA ou la cryptographie sur les courbes elliptiques classiques. Face à l'émergence des calculateurs quantiques, ils tentent de patcher a posteriori leurs protocoles, exposant toutes les communications passées aux attaques <strong>« Harvest Now, Decrypt Later » (HNDL)</strong> — où des acteurs étatiques enregistrent dès aujourd'hui les flux chiffrés pour les casser demain.",
    faqA1P2: "Dans CYBOU, la résistance post-quantique n'est pas une option ou une mise à niveau tardive : le protocole est <strong>PQ by design</strong> dès la version <code>v0.0.1</code>. La structure d'enveloppe <code>MailTx</code> et le chiffrement E2EE intègrent nativement le standard hybride <strong>HPKE (RFC 9180)</strong> combinant <strong>ML-KEM-768 (standard NIST / Kyber)</strong> et <strong>X25519</strong>, garantissant qu'aucun message scellé aujourd'hui ne pourra être déchiffré à l'ère quantique.",
    faqQ2: "Pourquoi CYBOU n'est-il pas encore téléchargeable pour le grand public ?",
    faqA2: "Nous refusons le marketing trompeur. CYBOU est actuellement à l'étape <code>v0.0.1</code> (baseline d'ingénierie et de recherche ouverte). Avant d'ouvrir le réseau au grand public, nous finalisons la transition du réseau de bootstrap <code>CYBOU-DEV v0.0.2</code> vers le consensus multi-validateurs BFT (seuil <code>f=1</code> avec 4 validateurs minimum) et la séparation étanche des clés d'opérateur (doc 68). Le code source et les 684 tests unitaires sont publiquement auditables.",
    faqQ3: "En quoi CYBOU diffère-t-il d'une messagerie électronique classique (SMTP/IMAP) ?",
    faqA3: "L'email classique dépend d'infrastructures de cloud centralisées, de serveurs de relais vulnérables aux réquisitions étrangères, et fait circuler les métadonnées et le contenu en clair entre hébergeurs. CYBOU fonctionne en réseau pair-à-pair décentralisé : chaque message est une transaction native <code>MailTx</code> validée par BFT, scellée avec un sel secret (doc 69), et déchiffrable uniquement sur le poste du destinataire sans passerelle intermédiaire.",
    faqQ4: "Comment fonctionne le consensus BFT et la tolérance aux pannes ?",
    faqA4: "Le consensus CYBOU repose sur une finalité BFT explicite sans minage énergivore. L'admission des validateurs est soumise à approbation opérateur, et chaque validateur dispose d'un poids égal à 1 (<code>weight = 1</code>). Un quorum minimum de 4 validateurs est strictement requis pour tolérer <code>f=1</code> validateur défaillant ou byzantin. Les époques de confiance (PoT) sont calculées en arithmétique entière à partir de la hauteur de bloc, sans dépendance aux horloges locales.",
    faqQ5: "Quelle est la finalité économique du jeton CYBOU ?",
    faqA5: "Le jeton CYBOU a une offre maximale stricte et non-gonflable de <strong>100 000 000 000 unités (0 décimale)</strong>. Il n'a aucune vocation spéculative : il sert à réguler l'accès au réseau et prévenir le pourriel (spam). Chaque nouveau compte satisfaisant la preuve de travail anti-Sybil reçoit un bonus d'accueil automatique directement sur son Solde Système (doc 70), sans invitation ni approbation centrale. Les frais d'émission sont déterministes selon la taille (pas d'enchères de priorité) et recyclés à 75% pour la sécurité du réseau et 25% pour la réserve d'accueil.",

    footNav: "Navigation",
    footDocs: "Spécifications",
    footGov: "Gouvernance"
  },

  en: {
    badgeStatus: "v0.0.1 Baseline",
    navStatus: "Project Status",
    navArch: "Pillars",
    navMailTx: "MailTx Protocol",
    navSpecs: "Tech Specs",
    navSim: "Simulator",
    navFaq: "FAQ",
    navRepo: "GitHub Repo",

    heroTag: "Open R&D • PQ by Design • C++20 P2P Architecture • European Sovereignty",
    heroAccent: "Sovereign P2P messaging & BFT ledger.",
    heroSubtitle: "Software architecture designed in France, freed from foreign cloud hyper-scalers. Post-quantum hybrid encryption, explicit BFT finality, and exclusive device authority.",

    statusCalloutTitle: "Development Status: v0.0.1 (Engineering Baseline & Hardening)",
    statusCalloutBody: "CYBOU is not a consumer product or desktop app deployed for the general public today. The network currently operates on an internal bootstrap chain (CYBOU-DEV v0.0.2). This site documents the architectural foundation, formal C++ state transitions, and real engineering progress.",

    heroBtnStatus: "View Implementation Status",
    heroBtnCode: "Source Code (GitHub)",

    matrixLabel: "Technical Transparency",
    matrixTitle: "v0.0.1 Implementation Matrix.",
    matrixDesc: "Strict, verifiable breakdown between tested unit code, active engineering work, and the long-term roadmap.",

    col1Title: "Validated & Tested (v0.0.1)",
    badgeDone: "PASSING",
    col1Item1: "<strong>Permissionless Anti-Sybil Onboarding (doc 70):</strong> protocol-native AccountCreateOpV1 with proof-of-work difficulty binding.",
    col1Item2: "<strong>Typed AccountId (32 bytes):</strong> deterministic balance validation and <code>SystemBalance</code> tracking.",
    col1Item3: "<strong>Atomic Onboarding Bonus:</strong> immediate transfer from <code>OnboardingPool</code> to <code>SystemBalance</code> upon account creation.",
    col1Item4: "<strong>LevelDB Snapshot Persistence:</strong> atomic state snapshots with clean CYBOU namespacing.",
    col1Item5: "<strong>Native MailTx Definition (doc 16):</strong> first-class operation, 1 recipient, bounded payload size, deterministic non-bidding fees.",
    col1Item6: "<strong>Test Coverage:</strong> 684 unit tests passing, complete elimination of legacy Bitcoin terminology and prefixes.",

    col2Title: "In Progress & Hardening",
    badgeWip: "IN PROGRESS",
    col2Item1: "<strong>Multi-Validator BFT Engine (doc 07):</strong> explicit finality with operator-approved admission (f=1 threshold at 4+ validators, equal weight = 1).",
    col2Item2: "<strong>Proof of Trust (PoT) Epochs:</strong> deterministic integer math derived strictly from block height (zero local wall-clock dependency).",
    col2Item3: "<strong>Strict Key Separation (doc 68):</strong> 4 isolated roles (Operator Authority, Validator, Release Signing, Treasury).",
    col2Item4: "<strong>PoW Retirement:</strong> final deprecation of residual proof-of-work mechanics inherited from the <code>CYBOU-DEV v0.0.2</code> bootstrap chain.",

    col3Title: "Planned Roadmap",
    badgePlanned: "PLANNED",
    col3Item1: "<strong>Post-Quantum HPKE Encapsulation:</strong> production integration of hybrid ML-KEM-768 + X25519 (doc 49).",
    col3Item2: "<strong>Distributed Object Storage:</strong> validator pre-store staging layer and long-term object storage for attachments at scale.",
    col3Item3: "<strong>Desktop GUI Client:</strong> native desktop user interface (Inbox, Sent, read-state) with index stores owned strictly on the local machine.",
    col3Item4: "<strong>Mail Evidence Bundles:</strong> cryptographically verifiable inclusion certificates and BFT finality proofs for third-party auditing.",

    archLabel: "Design Foundations",
    archTitle: "Breaking with Centralized Cloud Architectures.",
    archDesc: "The four architectural pillars guaranteeing long-term digital sovereignty and communication integrity.",

    bento1Title: "Zero Foreign Cloud Dependency",
    bento1Desc: "No centralized servers, no accounts under non-European jurisdiction, no single point of censorship or shutdown. Nodes communicate directly via a hardened C++20 P2P protocol without mandatory proxies.",
    bento1Metric: "Native protocol-level autonomous resilience",

    bento2Title: "Post-Quantum HPKE Encryption",
    bento2Desc: "Future-proof immunity against 'harvest-now-decrypt-later' threats using the hybrid ML-KEM-768 and X25519 standard (RFC 9180).",
    bento2Metric: "NIST post-quantum standard",

    bento3Title: "Exclusive Device Authority",
    bento3Desc: "Private keys never leave the host machine. The operator possesses no master key, back door, or administrative power to seize balances or intercept messages.",
    bento3Metric: "Zero back doors or master administrative keys",

    bento4Title: "Explicit BFT Finality & Deterministic Economics",
    bento4Desc: "Consensus through equal-weight Byzantine fault tolerance (weight = 1). Integer arithmetic, fixed 100,000,000,000 CYBOU supply ceiling, 0 decimals, and deterministic fee split: 3/4 Security, 1/4 Onboarding.",
    bento4Metric: "Strict maximum CYBOU token supply (0 decimals)",

    flowLabel: "Message Lifecycle",
    flowTitle: "The Lifecycle of a MailTx Transaction.",
    flowDesc: "How a plain-text message is committed, sealed, and verifiably ledgered on the peer-to-peer network.",

    step1Title: "Salted Content Commitment",
    step1Desc: "The message body is bound to an unpredictable cryptographic salt (doc 69). The resulting hash commits content without exposing cleartext to validators.",

    step2Title: "Hybrid HPKE Encryption",
    step2Desc: "End-to-end encryption targeted exclusively to the recipient's public key with post-quantum security. Intermediaries cannot inspect payload contents.",

    step3Title: "First-Class MailTx Operation",
    step3Desc: "Broadcasted as a native first-class transaction (never encoded as arbitrary Bitcoin script). Size-aware deterministic fee without bidding wars.",

    step4Title: "BFT Consensus & Local Indexing",
    step4Desc: "The block is sealed with explicit BFT finality. No per-mail consensus object is stored: the local client retains private Inbox and Sent index trees.",

    specLabel: "Machine & Human Specifications",
    specTitle: "CYBOU Protocol Technical Factsheet.",
    specDesc: "Structured engineering parameters formatted for search engine indexing and AI knowledge models.",
    factsheetHeading: "Canonical Architecture Parameters (CYBOU v0.0.1)",

    dtConsensus: "Consensus Model",
    dtPoT: "Proof of Trust Epochs (PoT)",
    dtSupply: "Maximum Supply (MAX_SUPPLY)",
    dtGrant: "Welcome Grant",
    dtFees: "Transaction Fee Structure",
    dtMailTx: "MailTx Specification (v1)",
    dtKeys: "Operator Key Isolation",
    dtStack: "Core Technology Stack",

    simLabel: "C++ State Transition",
    simTitle: "Native Account Creation & Anti-Sybil Proof-of-Work.",
    simDesc: "Demonstration of permissionless account creation according to AccountCreateOpV1 and AccountCreationWorkV1 (doc 70).",
    simBeneficiary: "Account Identifier (AccountId — 32 bytes)",
    simWorkLabel: "Anti-Sybil Proof-of-Work (CYBOU/ACCOUNT-CREATE-WORK/V1)",
    simSubmit: "Compute PoW & Create Account",
    simStatusLabel: "Protocol Status",
    simStatusReady: "Ready to create identity",
    simStatusSuccess: "Account Created • Anti-Sybil PoW Validated • Bonus Credited",
    simPoolSource: "OnboardingPool (Global Reserve)",
    simBalanceTarget: "SystemBalance (Credited)",
    simAntiSybil: "Anti-Sybil & Integrity",
    simQuota: "Initial Quota per Epoch",

    faqLabel: "Frequently Asked Questions",
    faqTitle: "Architecture, Cryptography & Governance.",
    faqDesc: "Transparent and rigorous answers regarding our technical foundation, roadmap, and post-quantum security.",
    faqPqBadge: "Post-Quantum Security • PQ by Design",
    faqQ1: "What does 'Post-Quantum by design' (PQ by design) mean for CYBOU?",
    faqA1P1: "Most legacy messaging systems rely on RSA or classical elliptic curve cryptography. Facing the rise of quantum computing, they attempt to patch protocols retroactively, leaving all historically recorded correspondence vulnerable to <strong>'Harvest Now, Decrypt Later' (HNDL)</strong> attacks — where adversaries intercept encrypted traffic today to decrypt it tomorrow.",
    faqA1P2: "In CYBOU, post-quantum resilience is not an optional future upgrade: the protocol is <strong>PQ by design</strong> from version <code>v0.0.1</code>. The native <code>MailTx</code> envelope structure and E2EE engine embed the hybrid <strong>HPKE (RFC 9180)</strong> standard combining <strong>ML-KEM-768 (NIST standard / Kyber)</strong> and <strong>X25519</strong>, ensuring sealed messages cannot be cracked in the quantum computing era.",
    faqQ2: "Why isn't CYBOU available for public download yet?",
    faqA2: "We reject deceptive marketing. CYBOU is currently at the <code>v0.0.1</code> baseline engineering stage. Before public release, we are completing the migration from the bootstrap chain (<code>CYBOU-DEV v0.0.2</code>) to the multi-validator BFT consensus (f=1 fault tolerance at 4+ validators) and strict operator key isolation (doc 68). The full source code and 684 unit tests are auditable on GitHub.",
    faqQ3: "How does CYBOU differ from standard email (SMTP/IMAP)?",
    faqA3: "Conventional email relies on centralized cloud providers, insecure relay servers, and exposes cleartext metadata and bodies across foreign jurisdictions. CYBOU is a direct peer-to-peer network: each message is a native <code>MailTx</code> verified via BFT, committed with a cryptographic salt (doc 69), and decryptable strictly on the recipient's machine without middlebox servers.",
    faqQ4: "How do BFT consensus and fault tolerance work?",
    faqA4: "CYBOU consensus operates on explicit BFT finality without wasteful mining. Validator admission is approved by the operator, and each validator holds an equal weight of 1 (<code>weight = 1</code>). A strict quorum of 4 validators is required to tolerate <code>f=1</code> faulty node. Proof of Trust (PoT) epochs are computed strictly via integer arithmetic from block height, with zero local wall-clock dependency.",
    faqQ5: "The CYBOU token has a fixed, non-inflatable supply ceiling of <strong>100,000,000,000 units (0 decimals)</strong>. It is not speculative: it serves strictly for network anti-spam and deterministic bandwidth allocation. Newly created accounts satisfying anti-Sybil proof-of-work receive an automatic onboarding bonus (doc 70) credited directly to System Balance without any operator invites or central approval. Transaction fees are non-bidding and split 75% for network security and 25% recycled to the onboarding reserve.",

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

function setLanguage(lang, updateUrl = false) {
  currentLang = lang;
  const btnFr = document.getElementById('lang-fr');
  const btnEn = document.getElementById('lang-en');
  if (btnFr) btnFr.classList.toggle('active', lang === 'fr');
  if (btnEn) btnEn.classList.toggle('active', lang === 'en');
  document.documentElement.lang = lang;

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

// --- 2. Interactive Permissionless Onboarding Simulator ---
function initOnboardingSimulator() {
  const btn = document.getElementById('sim-run-btn');
  const accountInput = document.getElementById('sim-account');
  const workInput = document.getElementById('sim-work');
  const statusVal = document.getElementById('sim-status-val');
  const sysBalVal = document.getElementById('sim-sysbal-val');
  const poolVal = document.getElementById('sim-pool-val');

  if (!btn || !accountInput) return;

  btn.addEventListener('click', () => {
    btn.textContent = currentLang === 'fr' 
      ? 'Calcul de la preuve anti-Sybil...' 
      : 'Computing anti-Sybil proof-of-work...';
    btn.style.opacity = '0.7';

    setTimeout(() => {
      btn.textContent = currentLang === 'fr' 
        ? 'Calculer PoW & Créer le compte' 
        : 'Compute PoW & Create Account';
      btn.style.opacity = '1';

      if (statusVal) {
        statusVal.textContent = currentLang === 'fr' 
          ? 'Compte créé • Preuve anti-Sybil validée • Bonus crédité' 
          : 'Account Created • Anti-Sybil PoW Validated • Bonus Credited';
        statusVal.className = 'output-val highlight';
      }
      if (sysBalVal) {
        sysBalVal.textContent = '6 000 CYBOU';
        sysBalVal.className = 'output-val highlight';
      }
      if (poolVal) {
        poolVal.textContent = 'Débit atomique confirmé (-6 000)';
      }
    }, 400);
  });
}
