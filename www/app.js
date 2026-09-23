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
    navStatus: "État du projet",
    navArch: "Piliers",
    navMailTx: "Protocole MailTx",
    navSpecs: "Fiche technique",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "R&D Ouverte • PQ by Design • Architecture P2P C++20 • Souveraineté Européenne",
    heroAccent: "Infrastructure de communication décentralisée & sécurisée.",
    heroSubtitle: "Projet R&D conçu en France pour une architecture indépendante des géants du cloud. BFT multi-validateur et chiffrement hybride post-quantique sont des cibles en cours de développement.",

    statusCalloutTitle: "Projet en développement actif",
    statusCalloutBody: "CYBOU n'est pas encore une infrastructure déployée pour le grand public. Le réseau utilise actuellement une chaîne de développement interne. Cette plateforme présente les fondations architecturales, les transitions d'état formelles en C++ et l'état réel d'avancement du projet.",

    heroBtnStatus: "Consulter l'état réel d'avancement",
    heroBtnCode: "Code source (GitHub)",

    matrixLabel: "Transparence technique",
    matrixTitle: "Matrice d'implémentation.",
    matrixDesc: "Distinction stricte et vérifiable entre le code validé en tests unitaires, les modules en cours de refonte et les composants de la feuille de route.",

    col1Title: "Validé & testé",
    badgeDone: "FONCTIONNEL",
    col1Item1: "<strong>Création de compte native & sans intermédiaire (doc 70) :</strong> opération AccountCreateOpV1 avec preuve de travail anti-Sybil.",
    col1Item2: "<strong>Identifiant typé AccountId (32 octets) :</strong> gestion déterministe du solde système (<code>SystemBalance</code>).",
    col1Item3: "<strong>Bonus d'accueil atomique :</strong> transfert immédiat du bonus depuis <code>OnboardingPool</code> vers <code>SystemBalance</code>.",
    col1Item4: "<strong>Persistance d'état LevelDB :</strong> instantanés atomiques avec nommage et isolats stricts CYBOU.",
    col1Item5: "<strong>Définition native MailTx (doc 16) :</strong> opération de premier rang, 1 destinataire, taille bornée, frais prévisibles (sans surenchère).",
    col1Item6: "<strong>Couverture de tests :</strong> tests unitaires publiés pour les transitions CYBOU implémentées ; plusieurs sous-systèmes Bitcoin hérités restent en cours de retrait.",

    col2Title: "En cours de consolidation",
    badgeWip: "EN COURS",
    col2Item1: "<strong>Moteur de consensus BFT (doc 07) :</strong> finalité explicite multi-validateurs avec admission contrôlée par l'opérateur (seuil f=1 dès 4 validateurs à poids égal).",
    col2Item2: "<strong>Époques Proof of Trust (PoT) :</strong> calcul arithmétique entier strict dérivé de la hauteur de bloc (zéro dépendance à l'horloge locale).",
    col2Item3: "<strong>Séparation stricte des clés (doc 68) :</strong> 4 domaines isolés (Autorité, Validateur, Signature de release, Trésorerie).",
    col2Item4: "<strong>Remplacement PoW :</strong> élimination définitive des mécanismes hérités de la chaîne de développement.",

    col3Title: "Feuille de route planifiée",
    badgePlanned: "PLANIFIÉ",
    col3Item1: "<strong>Architecture cible Post-Quantique HPKE :</strong> profil hybride ML-KEM-768 + X25519 (spécifié doc 49, tests crypto OpenSSL >= 3.5).",
    col3Item2: "<strong>Stockage d'objets décentralisé :</strong> couche de pré-stockage pour validateurs et stockage d'objets pour pièces jointes à grande échelle.",
    col3Item3: "<strong>Client graphique de bureau :</strong> interface utilisateur native (Inbox, Sent, état de lecture) avec conservation des index localement sur le poste.",
    col3Item4: "<strong>Preuves d'inclusion de correspondance :</strong> certificats de finalité vérifiables hors-chaîne par des tiers certificateurs.",

    archLabel: "Fondations de conception",
    archTitle: "Rupture avec les architectures centralisées.",
    archDesc: "Les quatre piliers qui guident la conception de la souveraineté et de l'intégrité des échanges numériques.",

    bento1Title: "Émancipation totale des hébergeurs de cloud tiers",
    bento1Desc: "Aucun serveur central, aucun compte sous juridiction extra-européenne, aucun point unique de censure ou de coupure. Les nœuds pairs échangent directement via un protocole P2P durci en C++20 sans passerelle obligatoire.",
    bento1Metric: "Résilience autonome native au niveau protocolaire",

    bento2Title: "Profil cible HPKE post-quantique",
    bento2Desc: "Anticipation des attaques par interception et déchiffrement ultérieur grâce au standard hybride cible ML-KEM-768 et X25519 (RFC 9180).",
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

    step2Title: "Profil cible Hybride HPKE",
    step2Desc: "Profil cible de chiffrement de bout en bout pour la clé publique du destinataire. L'intégration MailTx complète et son audit restent à réaliser avant toute revendication de sécurité en production.",

    step3Title: "Opération MailTx native",
    step3Desc: "Émission d'une transaction de premier rang (pas de payload arbitraire Bitcoin Script). Frais déterministes fonction de la taille, sans enchère de priorité.",

    step4Title: "Consensus BFT & Index Local",
    step4Desc: "Architecture cible : le bloc finalisé par BFT ne conserve aucun objet d'état permanent par courrier ; le futur client local indexera les courriers reçus et envoyés.",

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
    ddNetworks: "Genèses et paramètres économiques strictement séparés. L'économie Beta est expérimentale : les soldes Beta ne sont pas reportés sur le Mainnet.",

    faqLabel: "Questions fréquentes",
    faqTitle: "Architecture, Cryptographie & Gouvernance.",
    faqDesc: "Des réponses précises et transparentes sur nos choix techniques, notre feuille de route et la sécurité post-quantique.",
    faqPqBadge: "Sécurité Post-Quantique • PQ by Design",
    faqQ1: "Que signifie « Post-Quantique dès la conception » (PQ by design) pour CYBOU ?",
    faqA1P1: "La plupart des systèmes de communication historiques s'appuient sur RSA ou la cryptographie sur les courbes elliptiques classiques. Face à l'émergence des calculateurs quantiques, ils tentent de patcher a posteriori leurs protocoles, exposant toutes les communications passées aux attaques <strong>« Harvest Now, Decrypt Later » (HNDL)</strong> — où des acteurs étatiques enregistrent dès aujourd'hui les flux chiffrés pour les casser demain.",
    faqA1P2: "Dans CYBOU, la résistance post-quantique est un objectif de conception. Le profil cible <code>MailTx</code> combine <strong>ML-KEM-768</strong> et <strong>X25519</strong>, tandis qu'un vérificateur hybride <strong>Ed25519 + ML-DSA-65</strong> est validé pour les signatures d'autorité (OpenSSL >= 3.5). Le chiffrement MailTx complet, l'audit externe et les migrations de clés restent à réaliser : CYBOU ne revendique pas encore une sécurité post-quantique de production.",
    faqQ2: "Pourquoi CYBOU n'est-il pas encore téléchargeable pour le grand public ?",
    faqA2: "CYBOU est en phase d'ingénierie et de recherche ouverte. Avant toute ouverture au grand public, le projet doit remplacer le consensus hérité de la chaîne de développement par un moteur BFT multi-validateur testé, intégrer les communications de bout en bout et achever la séparation opérationnelle des clés. Le code source et les tests disponibles sont publiquement consultables.",
    faqQ3: "En quoi CYBOU diffère-t-il d'une messagerie électronique classique (SMTP/IMAP) ?",
    faqA3: "L'architecture cible CYBOU remplace le modèle SMTP/IMAP centralisé par une opération native <code>MailTx</code>, un engagement de contenu salé et un index local au destinataire. Ce flux complet dépend encore de l'intégration du moteur BFT, du chiffrement MailTx et du client ; il n'est pas présenté comme un service déployé aujourd'hui.",
    faqQ4: "Comment fonctionne le consensus BFT et la tolérance aux pannes ?",
    faqA4: "Le modèle cible utilise une finalité BFT explicite, des validateurs de poids égal à 1 et une admission approuvée par l'opérateur. Une configuration d'au moins 4 validateurs est requise pour revendiquer une tolérance <code>f=1</code>. Le moteur multi-validateur et ses essais de panne restent en cours ; la chaîne DEV utilise encore son consensus de bootstrap hérité.",
    faqQ5: "Quelle est la finalité économique du jeton CYBOU ?",
    faqA5: "Le jeton CYBOU a une offre maximale stricte et non-gonflable de <strong>100 000 000 000 unités (0 décimale)</strong>. Il n'a aucune vocation spéculative : il sert à réguler l'accès au réseau et prévenir le pourriel (spam). Chaque nouveau compte satisfaisant la preuve de travail anti-Sybil reçoit un bonus d'accueil automatique directement sur son Solde Système (doc 70), sans invitation ni approbation centrale. Les frais d'émission sont déterministes selon la taille (pas d'enchères de priorité) et recyclés à 75% pour la sécurité du réseau et 25% pour la réserve d'accueil.",

    footNav: "Navigation",
    footDocs: "Spécifications",
    footGov: "Gouvernance"
  },

  en: {
    navStatus: "Project Status",
    navArch: "Pillars",
    navMailTx: "MailTx Protocol",
    navSpecs: "Tech Specs",
    navFaq: "FAQ",
    menuLabel: "Menu",

    heroTag: "Open R&D • PQ by Design • C++20 P2P Architecture • European Sovereignty",
    heroAccent: "Decentralized & secure communication infrastructure.",
    heroSubtitle: "Open R&D project designed in France for independence from foreign cloud hyper-scalers. Multi-validator BFT and hybrid post-quantum encryption remain development targets.",

    statusCalloutTitle: "Project in active development",
    statusCalloutBody: "CYBOU is not yet infrastructure deployed for the general public. The network currently uses an internal development chain. This site documents the architectural foundation, formal C++ state transitions, and real engineering progress.",

    heroBtnStatus: "View Implementation Status",
    heroBtnCode: "Source Code (GitHub)",

    matrixLabel: "Technical Transparency",
    matrixTitle: "Implementation Matrix.",
    matrixDesc: "Strict, verifiable breakdown between tested unit code, active engineering work, and the long-term roadmap.",

    col1Title: "Validated & tested",
    badgeDone: "PASSING",
    col1Item1: "<strong>Permissionless Anti-Sybil Onboarding (doc 70):</strong> protocol-native AccountCreateOpV1 with proof-of-work difficulty binding.",
    col1Item2: "<strong>Typed AccountId (32 bytes):</strong> deterministic balance validation and <code>SystemBalance</code> tracking.",
    col1Item3: "<strong>Atomic Onboarding Bonus:</strong> immediate transfer from <code>OnboardingPool</code> to <code>SystemBalance</code> upon account creation.",
    col1Item4: "<strong>LevelDB Snapshot Persistence:</strong> atomic state snapshots with clean CYBOU namespacing.",
    col1Item5: "<strong>Native MailTx Definition (doc 16):</strong> first-class operation, 1 recipient, bounded payload size, deterministic non-bidding fees.",
    col1Item6: "<strong>Test Coverage:</strong> published unit tests cover implemented CYBOU transitions; several inherited Bitcoin subsystems are still being removed.",

    col2Title: "In Progress & Hardening",
    badgeWip: "IN PROGRESS",
    col2Item1: "<strong>Multi-Validator BFT Engine (doc 07):</strong> explicit finality with operator-approved admission (f=1 threshold at 4+ validators, equal weight = 1).",
    col2Item2: "<strong>Proof of Trust (PoT) Epochs:</strong> deterministic integer math derived strictly from block height (zero local wall-clock dependency).",
    col2Item3: "<strong>Strict Key Separation (doc 68):</strong> 4 isolated roles (Operator Authority, Validator, Release Signing, Treasury).",
    col2Item4: "<strong>PoW Retirement:</strong> final removal of proof-of-work mechanics inherited from the development chain.",

    col3Title: "Planned Roadmap",
    badgePlanned: "PLANNED",
    col3Item1: "<strong>Target Post-Quantum HPKE Architecture:</strong> hybrid ML-KEM-768 + X25519 (specified in doc 49, crypto verification with OpenSSL >= 3.5).",
    col3Item2: "<strong>Distributed Object Storage:</strong> validator pre-store staging layer and long-term object storage for attachments at scale.",
    col3Item3: "<strong>Desktop GUI Client:</strong> native desktop user interface (Inbox, Sent, read-state) with index stores owned strictly on the local machine.",
    col3Item4: "<strong>Mail Evidence Bundles:</strong> cryptographically verifiable inclusion certificates and BFT finality proofs for third-party auditing.",

    archLabel: "Design Foundations",
    archTitle: "Breaking with Centralized Cloud Architectures.",
    archDesc: "The four architectural pillars guiding long-term digital sovereignty and communication integrity.",

    bento1Title: "Zero Foreign Cloud Dependency",
    bento1Desc: "No centralized servers, no accounts under non-European jurisdiction, no single point of censorship or shutdown. Nodes communicate directly via a hardened C++20 P2P protocol without mandatory proxies.",
    bento1Metric: "Native protocol-level autonomous resilience",

    bento2Title: "Target Post-Quantum HPKE Profile",
    bento2Desc: "Target hybrid ML-KEM-768 and X25519 profile designed to address 'harvest-now-decrypt-later' threats; full MailTx integration and external review remain pending.",
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

    step2Title: "Target Hybrid HPKE Profile",
    step2Desc: "Target end-to-end encryption profile for the recipient's public key. Full MailTx integration and review remain required before any production security claim.",

    step3Title: "First-Class MailTx Operation",
    step3Desc: "Broadcasted as a native first-class transaction (never encoded as arbitrary Bitcoin script). Size-aware deterministic fee without bidding wars.",

    step4Title: "BFT Consensus & Local Indexing",
    step4Desc: "Target architecture: a BFT-finalized block stores no permanent per-mail state object; the future local client will retain private Inbox and Sent indexes.",

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
    ddNetworks: "Strictly separate genesis and economic parameters. Beta economics are experimental: Beta balances do not carry to Mainnet.",

    faqLabel: "Frequently Asked Questions",
    faqTitle: "Architecture, Cryptography & Governance.",
    faqDesc: "Transparent and rigorous answers regarding our technical foundation, roadmap, and post-quantum security.",
    faqPqBadge: "Post-Quantum Security • PQ by Design",
    faqQ1: "What does 'Post-Quantum by design' (PQ by design) mean for CYBOU?",
    faqA1P1: "Most legacy messaging systems rely on RSA or classical elliptic curve cryptography. Facing the rise of quantum computing, they attempt to patch protocols retroactively, leaving all historically recorded correspondence vulnerable to <strong>'Harvest Now, Decrypt Later' (HNDL)</strong> attacks — where adversaries intercept encrypted traffic today to decrypt it tomorrow.",
    faqA1P2: "Post-quantum resilience is a design objective for CYBOU. The target <code>MailTx</code> profile combines <strong>ML-KEM-768</strong> and <strong>X25519</strong>, while a hybrid <strong>Ed25519 + ML-DSA-65</strong> verifier is validated for authority signatures (OpenSSL >= 3.5). Full MailTx encryption, external review, and key migration remain pending; CYBOU does not yet claim production post-quantum security.",
    faqQ2: "Why isn't CYBOU available for public download yet?",
    faqA2: "CYBOU is in open engineering and research. Before public release, the project must replace the inherited development-chain consensus with a tested multi-validator BFT engine, integrate communications end to end, and complete operational key separation. The available source code and tests are public for inspection.",
    faqQ3: "How does CYBOU differ from standard email (SMTP/IMAP)?",
    faqA3: "The target CYBOU architecture replaces centralized SMTP/IMAP infrastructure with a native <code>MailTx</code>, a salted content commitment, and recipient-owned local indexes. This complete flow still depends on BFT, MailTx encryption, and client integration and is not presented as a deployed service today.",
    faqQ4: "How do BFT consensus and fault tolerance work?",
    faqA4: "The target model uses explicit BFT finality, operator-approved validator admission, and equal validator weight of 1. At least 4 validators are required before claiming <code>f=1</code> tolerance. The multi-validator engine and fault tests remain in progress; the DEV chain still uses its inherited bootstrap consensus.",
    faqQ5: "What is the economic purpose of the CYBOU token?",
    faqA5: "The CYBOU token has a fixed, non-inflatable supply ceiling of <strong>100,000,000,000 units (0 decimals)</strong>. It is not speculative: it serves strictly for network anti-spam and deterministic bandwidth allocation. Newly created accounts satisfying anti-Sybil proof-of-work receive an automatic onboarding bonus (doc 70) credited directly to System Balance without any operator invites or central approval. Transaction fees are non-bidding and split 75% for network security and 25% recycled to the onboarding reserve.",

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
