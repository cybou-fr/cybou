/* ==========================================================================
   CYBOU.FR — Apple-Inspired Interactive Engine
   Bilingual dynamic toggle, mail viewer interaction, voucher simulator
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initLanguageSwitch();
  initMailMockup();
  initVoucherSimulator();
});

// --- 1. Language Toggle (French / English) ---
const translations = {
  fr: {
    navVision: "Vision",
    navEmail: "CYBOU Email",
    navArchitecture: "Architecture",
    navVoucher: "Bon d'invitation",
    navSpecs: "Spécifications",
    navDownload: "Télécharger",
    heroTag: "Réseau Pair-à-Pair Européen • Conçu en France",
    heroTitle1: "La messagerie souveraine.",
    heroTitle2: "Sans compromis.",
    heroSubtitle: "Émancipée des géants du cloud. Protégée par cryptographie post-quantique. Registre pair-à-pair à finalité BFT immédiate.",
    heroBtnDiscover: "Découvrir CYBOU Email",
    heroBtnSpecs: "Lire les spécifications",
    bentoLabel: "Piliers fondamentaux",
    bentoTitle: "Une rupture technologique majeure.",
    bentoDesc: "Pourquoi CYBOU redéfinit l'intégrité et la souveraineté des communications.",
    card1Title: "Zéro dépendance aux géants du cloud",
    card1Desc: "Aucun serveur central, aucun hébergeur étranger, aucun point unique de défaillance. Le réseau s'exécute directement entre pairs indépendants.",
    card2Title: "Chiffrement Post-Quantique HPKE",
    card2Desc: "Vos correspondances sont scellées avec l'algorithme hybride ML-KEM-768 et X25519. Protégé dès aujourd'hui contre les ordinateurs quantiques de demain.",
    card3Title: "Autorité exclusive de l'appareil",
    card3Desc: "Vos clés privées ne quittent jamais votre machine. L'opérateur ne dispose d'aucun passe-partout ni pouvoir de censure sur vos soldes.",
    card4Title: "Consensus BFT & Époques PoT",
    card4Desc: "Finalité explicite sans minage énergivore. Époques de consensus Proof of Trust calculées strictement à partir de la hauteur de bloc en arithmétique entière.",
    flowLabel: "Protocole MailTx",
    flowTitle: "De la composition à la finalité.",
    flowDesc: "Comment chaque message devient un enregistrement cryptographique immuable.",
    step1Title: "Rédaction & Engagement salé",
    step1Desc: "Le message texte est lié à un sel cryptographique imprédictible et converti en empreinte de contenu infalsifiable.",
    step2Title: "Chiffrement Hybride E2EE",
    step2Desc: "Encapsulation HPKE ciblée exclusivement pour la clé publique du destinataire. Aucun tiers ne peut déchiffrer.",
    step3Title: "Opération MailTx native",
    step3Desc: "Le message est diffusé comme transaction native de premier rang sur le réseau P2P avec frais déterminés sans surenchère.",
    step4Title: "Finalité BFT & Réception",
    step4Desc: "Le bloc est scellé par les validateurs BFT. Le destinataire hors-ligne synchronise et déchiffre son courrier localement.",
    simLabel: "Simulateur d'onboarding",
    simTitle: "Vérifiez un bon d'invitation (Invite Voucher).",
    simDesc: "L'accès au réseau et l'attribution de la subvention d'accueil (6 000 CYBOU) reposent sur un bon d'opérateur vérifié.",
    simBeneficiary: "Identifiant du compte (AccountId)",
    simVoucherId: "Numéro du bon d'invitation",
    simSubmit: "Exécuter l'admission sur l'état",
    simStatusReady: "En attente de vérification",
    simStatusSuccess: "Admis • Subvention 6 000 CYBOU créditée",
    simPoolSource: "OnboardingPool (Réserve d'accueil)",
    simBalanceTarget: "SystemBalance (Solde Système)",
    simQuota: "Quota d'émission par époque PoT",
    simStatusLabel: "Statut de la transition",
    dlTitle: "Prenez le contrôle de vos communications.",
    dlDesc: "Téléchargez le nœud complet CYBOU Desktop et commencez à échanger souverainement.",
    dlWindows: "Windows (MSVC)",
    dlWindowsDesc: "Client complet cybou.exe pour Windows 10/11.",
    dlLinux: "Linux / Validateur",
    dlLinuxDesc: "Binaire d'infrastructure et nœud de validation pour Ubuntu/Debian.",
    dlBtn: "Télécharger les binaires"
  },
  en: {
    navVision: "Vision",
    navEmail: "CYBOU Email",
    navArchitecture: "Architecture",
    navVoucher: "Invite Voucher",
    navSpecs: "Specifications",
    navDownload: "Download",
    heroTag: "European Sovereign Peer-to-Peer Network • Designed in France",
    heroTitle1: "Sovereign messaging.",
    heroTitle2: "Zero compromise.",
    heroSubtitle: "Free from cloud hyper-scalers. Shielded by post-quantum encryption. Native peer-to-peer state with immediate BFT finality.",
    heroBtnDiscover: "Explore CYBOU Email",
    heroBtnSpecs: "Read Specifications",
    bentoLabel: "Core Pillars",
    bentoTitle: "A fundamental technological breakthrough.",
    bentoDesc: "Why CYBOU redefines communication integrity and digital sovereignty.",
    card1Title: "Zero Foreign Cloud Dependency",
    card1Desc: "No centralized servers, no foreign hosts, no single point of failure. The network executes directly between independent peers.",
    card2Title: "Post-Quantum HPKE Encryption",
    card2Desc: "Correspondence is sealed using hybrid ML-KEM-768 and X25519. Immune today against tomorrow's quantum computing attacks.",
    card3Title: "Exclusive Device Authority",
    card3Desc: "Private keys never leave your device. The operator possesses no master key, back door, or power to freeze user balances.",
    card4Title: "BFT Consensus & PoT Epochs",
    card4Desc: "Explicit finality without energy-intensive mining. Proof of Trust epochs computed deterministically from block height using integer math.",
    flowLabel: "MailTx Protocol",
    flowTitle: "From composition to finality.",
    flowDesc: "How every message becomes an immutable cryptographic record.",
    step1Title: "Composition & Salted Commitment",
    step1Desc: "Text content is bound to an unpredictable cryptographic salt, producing a tamper-proof commitment digest.",
    step2Title: "Hybrid E2EE Encryption",
    step2Desc: "HPKE encapsulation targeted exclusively to the recipient's public key. No intermediary can intercept or decrypt.",
    step3Title: "First-Class MailTx Operation",
    step3Desc: "Dispatched as a native first-class transaction on the P2P network with deterministic, non-bidding size fees.",
    step4Title: "BFT Finality & Delivery",
    step4Desc: "The block is sealed by BFT validators. Offline recipients synchronize and decrypt mail locally in their client.",
    simLabel: "Onboarding Simulator",
    simTitle: "Verify an Invite Voucher.",
    simDesc: "Network onboarding and the Welcome Grant (6,000 CYBOU) are granted through verified operator vouchers.",
    simBeneficiary: "Beneficiary AccountId",
    simVoucherId: "Invite Voucher Code",
    simSubmit: "Execute State Transition",
    simStatusReady: "Awaiting Verification",
    simStatusSuccess: "Verified • 6,000 CYBOU Grant Credited",
    simPoolSource: "OnboardingPool (Source)",
    simBalanceTarget: "SystemBalance (Credited)",
    simQuota: "Outgoing Quota per PoT Epoch",
    simStatusLabel: "Transition Status",
    dlTitle: "Take control of your communications.",
    dlDesc: "Download the full CYBOU Desktop node and start exchanging sovereignly.",
    dlWindows: "Windows (MSVC)",
    dlWindowsDesc: "Full-node cybou.exe client for Windows 10/11.",
    dlLinux: "Linux / Validator",
    dlLinuxDesc: "Infrastructure binary & validator node for Ubuntu/Debian.",
    dlBtn: "Download Binaries"
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

  const dict = translations[lang];
  for (const [key, val] of Object.entries(dict)) {
    const el = document.querySelector(`[data-i18n="${key}"]`);
    if (el) {
      el.textContent = val;
    }
  }
}

// --- 2. Interactive macOS Mail Mockup ---
const sampleEmails = [
  {
    sender: "sophie.delorme@cybou",
    subject: "Audit de sécurité et homologation européenne",
    time: "14:18",
    badge: "ML-KEM-768",
    proof: "BFT #28419",
    body: `
      <p>Bonjour Marc,</p>
      <p>Nous avons finalisé le passage en revue des spécifications du protocole MailTx pour notre déploiement en France. L'isolation stricte des clés d'autorité opérateur (68_OPERATOR_KEY_SEPARATION) garantit qu'aucune clé maîtresse ne peut compromettre la confidentialité de nos correspondances.</p>
      <p>L'engagement salé de contenu (69_MAIL_EVIDENCE_BUNDLE) et la finalité BFT explicite répondent parfaitement aux critères d'audit de souveraineté numérique.</p>
      <p>Bien cordialement,<br><strong>Sophie Delorme</strong><br><span style="color: #86868b; font-size: 0.8rem;">Déléguée à la Sécurité Numérique</span></p>
    `
  },
  {
    sender: "alexandre.blanc@cybou",
    subject: "Activation du 4ème nœud validateur BFT",
    time: "11:05",
    badge: "BFT f=1",
    proof: "BFT #28412",
    body: `
      <p>Bonjour à tous,</p>
      <p>Le 4ème validateur BFT a été admis avec succès par consensus. Le réseau atteint désormais la tolérance aux pannes byzantines f=1 avec des poids de vote strictement égaux à 1.</p>
      <p>Les époques Proof of Trust continuent d'avancer selon la hauteur de bloc finalisée sans aucune dérive d'horloge locale.</p>
    `
  },
  {
    sender: "contact@anonyme.cybou",
    subject: "Bienvenue sur le réseau CYBOU",
    time: "Hier",
    badge: "Welcome",
    proof: "Genesis DEV",
    body: `
      <p>Félicitations pour la création de votre identité CYBOU.</p>
      <p>Votre bon d'invitation vous a attribué 6 000 CYBOU de solde système (SystemBalance). Vous disposez dès maintenant d'un quota de 25 envois par époque PoT.</p>
    `
  }
];

function initMailMockup() {
  const mailItems = document.querySelectorAll('.mail-item');
  const viewTitle = document.getElementById('mockup-title');
  const viewSender = document.getElementById('mockup-sender');
  const viewProof = document.getElementById('mockup-proof-num');
  const viewBody = document.getElementById('mockup-body');

  if (!mailItems.length || !viewTitle || !viewSender || !viewBody) return;

  mailItems.forEach((item, index) => {
    item.addEventListener('click', () => {
      mailItems.forEach(i => i.classList.remove('selected'));
      item.classList.add('selected');

      const data = sampleEmails[index];
      if (data) {
        viewTitle.textContent = data.subject;
        viewSender.textContent = data.sender;
        if (viewProof) viewProof.textContent = data.proof;
        viewBody.innerHTML = data.body;
      }
    });
  });
}

// --- 3. Interactive Invite Voucher Simulator ---
function initVoucherSimulator() {
  const btn = document.getElementById('sim-run-btn');
  const accountInput = document.getElementById('sim-account');
  const voucherInput = document.getElementById('sim-voucher');
  const statusVal = document.getElementById('sim-status-val');
  const sysBalVal = document.getElementById('sim-sysbal-val');
  const poolVal = document.getElementById('sim-pool-val');

  if (!btn || !accountInput || !voucherInput) return;

  btn.addEventListener('click', () => {
    btn.textContent = currentLang === 'fr' ? 'Validation cryptographique en cours...' : 'Verifying cryptographic signature...';
    btn.style.opacity = '0.7';

    setTimeout(() => {
      btn.textContent = currentLang === 'fr' ? 'Exécuter l\'admission sur l\'état' : 'Execute State Transition';
      btn.style.opacity = '1';

      if (statusVal) {
        statusVal.textContent = currentLang === 'fr' 
          ? 'Admis • Subvention 6 000 CYBOU créditée' 
          : 'Verified • 6,000 CYBOU Grant Credited';
        statusVal.className = 'output-val highlight';
      }
      if (sysBalVal) {
        sysBalVal.textContent = '6 000 CYBOU';
        sysBalVal.className = 'output-val highlight';
      }
      if (poolVal) {
        poolVal.textContent = '99 999 994 000 CYBOU';
      }
    }, 450);
  });
}
