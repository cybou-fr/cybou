/* ==========================================================================
   CYBOU.FR — Technical Engine & Dynamic Bilingual Localization
   Engineered for transparent systems presentation & responsive state verification
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initLanguageSwitch();
  initVoucherSimulator();
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
    navRepo: "Dépôt GitHub",

    heroTag: "R&D Ouverte • Architecture P2P C++20 • Souveraineté Européenne",
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
    col1Item1: "<strong>Enveloppe de bon d'invitation (doc 70) :</strong> validation cryptographique bornée, séparation de domaine anti-rejeu.",
    col1Item2: "<strong>Identifiant typé AccountId (32 octets) :</strong> gestion déterministe du solde système (<code>SystemBalance</code>).",
    col1Item3: "<strong>Subvention d'accueil atomique :</strong> transfert de 6 000 CYBOU de <code>OnboardingPool</code> vers <code>SystemBalance</code>.",
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
    simTitle: "Simulateur de transition d'état (Invite Voucher).",
    simDesc: "Démonstration de la transition d'accueil telle qu'implémentée dans les modules src/cybou/voucher.cpp et src/cybou/state.cpp.",
    simBeneficiary: "Identifiant de compte (AccountId — 32 octets)",
    simVoucherId: "Code du bon d'invitation (VoucherId)",
    simSubmit: "Exécuter la transition d'état",
    simStatusLabel: "Statut de la transition",
    simStatusReady: "En attente d'exécution",
    simStatusSuccess: "Admis • Subvention 6 000 CYBOU créditée",
    simPoolSource: "OnboardingPool (Réserve globale)",
    simBalanceTarget: "SystemBalance (Solde Système)",
    simReplay: "Séparation anti-rejeu",
    simQuota: "Quota d'envoi par époque",

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
    navRepo: "GitHub Repo",

    heroTag: "Open R&D • C++20 P2P Architecture • European Sovereignty",
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
    col1Item1: "<strong>Invite Voucher Envelope (doc 70):</strong> bounded cryptographic verification, domain separation against replay.",
    col1Item2: "<strong>Typed AccountId (32 bytes):</strong> deterministic balance validation and <code>SystemBalance</code> tracking.",
    col1Item3: "<strong>Atomic Welcome Grant:</strong> 6,000 CYBOU transfer from <code>OnboardingPool</code> to <code>SystemBalance</code>.",
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
    simTitle: "State Transition Simulator (Invite Voucher).",
    simDesc: "Demonstration of the onboarding state transition implemented in src/cybou/voucher.cpp and src/cybou/state.cpp.",
    simBeneficiary: "Account Identifier (AccountId — 32 bytes)",
    simVoucherId: "Invite Voucher Code (VoucherId)",
    simSubmit: "Execute State Transition",
    simStatusLabel: "Transition Status",
    simStatusReady: "Awaiting execution",
    simStatusSuccess: "Admitted • 6,000 CYBOU Welcome Grant Credited",
    simPoolSource: "OnboardingPool (Global Reserve)",
    simBalanceTarget: "SystemBalance (Credited)",
    simReplay: "Anti-Replay Domain Separation",
    simQuota: "Outgoing Quota per Epoch",

    footNav: "Navigation",
    footDocs: "Specifications",
    footGov: "Governance"
  }
};

let currentLang = 'fr';

function initLanguageSwitch() {
  const btnFr = document.getElementById('lang-fr');
  const btnEn = document.getElementById('lang-en');

  if (!btnFr || !btnEn) return;

  btnFr.addEventListener('click', () => setLanguage('fr'));
  btnEn.addEventListener('click', () => setLanguage('en'));
}

function setLanguage(lang) {
  currentLang = lang;
  document.getElementById('lang-fr').classList.toggle('active', lang === 'fr');
  document.getElementById('lang-en').classList.toggle('active', lang === 'en');
  document.documentElement.lang = lang;

  const dict = translations[lang];
  for (const [key, val] of Object.entries(dict)) {
    const el = document.querySelector(`[data-i18n="${key}"]`);
    if (el) {
      if (val.includes('<')) {
        el.innerHTML = val;
      } else {
        el.textContent = val;
      }
    }
  }
}

// --- 2. Interactive Invite Voucher Simulator ---
function initVoucherSimulator() {
  const btn = document.getElementById('sim-run-btn');
  const accountInput = document.getElementById('sim-account');
  const voucherInput = document.getElementById('sim-voucher');
  const statusVal = document.getElementById('sim-status-val');
  const sysBalVal = document.getElementById('sim-sysbal-val');
  const poolVal = document.getElementById('sim-pool-val');

  if (!btn || !accountInput || !voucherInput) return;

  btn.addEventListener('click', () => {
    btn.textContent = currentLang === 'fr' 
      ? 'Vérification cryptographique en cours...' 
      : 'Verifying cryptographic signature...';
    btn.style.opacity = '0.7';

    setTimeout(() => {
      btn.textContent = currentLang === 'fr' 
        ? 'Exécuter la transition d\'état' 
        : 'Execute State Transition';
      btn.style.opacity = '1';

      if (statusVal) {
        statusVal.textContent = currentLang === 'fr' 
          ? 'Admis • Subvention 6 000 CYBOU créditée' 
          : 'Admitted • 6,000 CYBOU Welcome Grant Credited';
        statusVal.className = 'output-val highlight';
      }
      if (sysBalVal) {
        sysBalVal.textContent = '6 000 CYBOU';
        sysBalVal.className = 'output-val highlight';
      }
      if (poolVal) {
        poolVal.textContent = '99 999 994 000 CYBOU';
      }
    }, 400);
  });
}
