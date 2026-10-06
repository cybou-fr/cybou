<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="fr_FR">
<context>
    <name>CybouActivity</name>
    <message>
        <source>Uploading %1</source>
        <translation type="vanished">Téléversement de %1</translation>
    </message>
    <message>
        <source>Downloading %1</source>
        <translation type="vanished">Téléchargement de %1</translation>
    </message>
    <message>
        <source>%1 was not uploaded</source>
        <translation type="vanished">%1 n’a pas été téléversé</translation>
    </message>
    <message>
        <source>Needs attention</source>
        <translation type="vanished">À vérifier</translation>
    </message>
    <message>
        <source>(no subject)</source>
        <translation type="vanished">(sans objet)</translation>
    </message>
    <message>
        <source>“%1” was not sent</source>
        <translation type="vanished">« %1 » n’a pas été envoyé</translation>
    </message>
    <message>
        <source>Sending “%1” to %2</source>
        <translation type="vanished">Envoi de « %1 » à %2</translation>
    </message>
    <message>
        <source>Sending %1 to %2</source>
        <translation type="vanished">Envoi de %1 à %2</translation>
    </message>
    <message>
        <source>Moving %1 to System Balance</source>
        <translation type="vanished">Transfert de %1 vers le solde système</translation>
    </message>
    <message>
        <source>Claiming your .cybou name</source>
        <translation type="vanished">Réservation de votre nom .cybou</translation>
    </message>
    <message>
        <source>Changing your recovery phrase</source>
        <translation type="vanished">Changement de votre phrase de récupération</translation>
    </message>
    <message>
        <source>Waiting for confirmation</source>
        <translation type="vanished">En attente de confirmation</translation>
    </message>
    <message>
        <source>Waiting for confirmation. Keep both the old and the new words until it is done.</source>
        <translation type="vanished">En attente de confirmation. Conservez l’ancienne et la nouvelle phrase jusqu’à la fin.</translation>
    </message>
    <message>
        <source>Saving changes on this computer…</source>
        <translation type="vanished">Enregistrement sur cet ordinateur…</translation>
    </message>
    <message>
        <source>Waiting to save changes…</source>
        <translation type="vanished">En attente d’enregistrement…</translation>
    </message>
</context>
<context>
    <name>CybouActivityButton</name>
    <message>
        <location filename="../cybouactivity.cpp" line="+96"/>
        <source>Operations in progress</source>
        <translation>Opérations en cours</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>1 needs attention</source>
        <translation>1 élément à vérifier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 need attention</source>
        <translation>%1 éléments à vérifier</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>1 in progress</source>
        <translation>1 en cours</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 in progress</source>
        <translation>%1 en cours</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Activity</source>
        <translation>Activité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Everything on its way to the network, and anything that needs you.</source>
        <translation>Tout ce qui est envoyé au réseau et ce qui nécessite votre attention.</translation>
    </message>
    <message>
        <location line="+72"/>
        <source>Try again</source>
        <translation>Réessayer</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Open</source>
        <translation>Ouvrir</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Nothing in progress.</source>
        <translation>Aucune opération en cours.</translation>
    </message>
</context>
<context>
    <name>CybouApplicationBackend</name>
    <message>
        <location filename="../cybouapplicationbackend.h" line="+85"/>
        <source>Your data cannot be secured for a new recovery phrase right now.</source>
        <translation>Vos données ne peuvent pas être protégées pour une nouvelle phrase de récupération pour le moment.</translation>
    </message>
</context>
<context>
    <name>CybouConsoleDialog</name>
    <message>
        <location filename="../cybouconsoledialog.cpp" line="+36"/>
        <source>CYBOU Read-Only Console</source>
        <translation>Console CYBOU en lecture seule</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Bounded Read-Only Console</source>
        <translation>Console bornée en lecture seule</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Restricted diagnostic inspection for own files, chunk trees, and network state. Shell, mutation, and scripting commands are disabled.</source>
        <translation>Inspection diagnostique restreinte des fichiers propres, arbres de blocs et état du réseau. Commandes shell, mutation et scripts désactivés.</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Type a command (e.g. &apos;help&apos;, &apos;status&apos;, &apos;files&apos;, &apos;storage&apos;)…</source>
        <translation>Tapez une commande (ex. &apos;help&apos;, &apos;status&apos;, &apos;files&apos;, &apos;storage&apos;)…</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Run</source>
        <translation>Exécuter</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Clear</source>
        <translation>Effacer</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>CYBOU Bounded Console initialized. Type &apos;help&apos; to view permitted read-only commands.</source>
        <translation>Console bornée CYBOU initialisée. Tapez &apos;help&apos; pour afficher les commandes autorisées en lecture seule.</translation>
    </message>
    <message>
        <location line="+72"/>
        <source>Vault locked. Private session history and output cleared.</source>
        <translation>Coffre verrouillé. Historique et sorties de session privée effacés.</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Available read-only commands:
  help               Show this command reference
  status             Show local node status and verified height
  storage            Show own storage usage, local capacity, and provider obligations
  files [filter]     List own unlocked files (ID, Name, Size, State, Replicas)
  file &lt;id|name&gt;     Show details for a specific own file
  chunks &lt;id|name&gt;   Inspect chunk tree, byte ranges, and BLAKE3 verification
  peers              List observed peer connections
  jobs               List active background jobs in progress
  clear              Clear console output

Security note: This console does not execute shell scripts, SQL queries, or mutation operations. All queries are strictly bounded to the currently unlocked Identity.</source>
        <translation type="unfinished">
      </translation>
    </message>
    <message>
        <location line="+25"/>
        <source>Network: %1 (%2)
Node: %3 | Online: %4 | Connection: %5
Finalized Height: %6
Identity: %7 (%8)</source>
        <translation type="unfinished">
      </translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Running</source>
        <translation>En cours d&apos;exécution</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Stopped</source>
        <translation>Arrêté</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Yes</source>
        <translation>Oui</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>No</source>
        <translation>Non</translation>
    </message>
    <message>
        <location line="-9"/>
        <location line="+13"/>
        <source>None</source>
        <translation>Aucun</translation>
    </message>
    <message>
        <location line="-19"/>
        <source>Active</source>
        <translation type="unfinished">Actif</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Creating</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Restoring</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Syncing</source>
        <translation type="unfinished">Synchronisation…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Locked</source>
        <translation type="unfinished">Verrouillé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Needs Attention</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+28"/>
        <source>Own Storage Summary:
  Own files: %1 items
  Own stored data: %2 (%3 bytes)
  Local capacity policy (V): %4
  Provider obligations: &lt;= %5 (floor(2V/3))
  Storage model: Uniform Full Node, intrinsic encrypted storage.</source>
        <translation type="unfinished">
      </translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Listing own unlocked files:</source>
        <translation>Liste des fichiers déverrouillés :</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>[Folder]</source>
        <translation>[Dossier]</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>(replicas: %1/%2)</source>
        <translation>(répliques : %1/%2)</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>  No matching files found.</source>
        <translation>  Aucun fichier correspondant trouvé.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Usage: file &lt;id|name&gt;</source>
        <translation>Utilisation : file &lt;id|nom&gt;</translation>
    </message>
    <message>
        <location line="+11"/>
        <location line="+36"/>
        <source>File not found in own catalog: %1</source>
        <translation>Fichier introuvable dans le catalogue propre : %1</translation>
    </message>
    <message>
        <location line="-32"/>
        <source>File Details:
  ID: %1
  Name: %2
  Root Content ID: %3
  Logical Size: %4 (%5 bytes)
  Chunks: %6 (512 KiB billing unit)
  State: %7
  Remote Replicas: %8 of %9
  Local Availability: %10
  Retrieval: %11</source>
        <translation type="unfinished">
      </translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+29"/>
        <source>Not reported</source>
        <translation>Non rapporté</translation>
    </message>
    <message>
        <location line="-20"/>
        <source>Usage: chunks &lt;id|name&gt;</source>
        <translation>Utilisation : chunks &lt;id|nom&gt;</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Chunk Tree: %1
  Root Content ID: %2
  Size: %3 | Chunks: %4 | Chunk size: 512 KiB
  Integrity: Content-addressed BLAKE3 Merkle tree</source>
        <translation type="unfinished">
      </translation>
    </message>
    <message>
        <location line="+22"/>
        <source>  ... and %1 more chunks (bounded output)</source>
        <translation>  ... et %1 autres blocs (sortie bornée)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>  Evidence: Authorized by owner self-capsule and finalized RootPublication.</source>
        <translation>  Preuve : Autorisé par la capsule propriétaire et RootPublication finalisée.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Observed Peer Connections (%1 peers):</source>
        <translation>Connexions de pairs observées (%1 pairs) :</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>  No peers currently connected.</source>
        <translation>  Aucun pair connecté actuellement.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Active Background Jobs:</source>
        <translation>Tâches de fond actives :</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>  No background jobs currently in progress.</source>
        <translation>  Aucune tâche de fond en cours.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Error: Command &apos;%1&apos; is not recognized or not permitted. This console is strictly read-only and accepts only: help, status, storage, files, file, chunks, peers, jobs, clear.</source>
        <translation>Erreur : La commande &apos;%1&apos; n&apos;est pas reconnue ou non autorisée. Cette console est strictement en lecture seule et accepte uniquement : help, status, storage, files, file, chunks, peers, jobs, clear.</translation>
    </message>
</context>
<context>
    <name>CybouCoreApplicationAdapter</name>
    <message>
        <source>The destination cannot be written.</source>
        <translation type="vanished">Impossible d’écrire dans la destination.</translation>
    </message>
    <message>
        <source>This content is temporarily unavailable. Try again later.</source>
        <translation type="vanished">Ce contenu est temporairement indisponible. Réessayez plus tard.</translation>
    </message>
    <message>
        <source>This content could not be verified.</source>
        <translation type="vanished">Impossible de vérifier ce contenu.</translation>
    </message>
    <message>
        <source>Your recovery data could not be verified.</source>
        <translation type="vanished">Impossible de vérifier vos données de récupération.</translation>
    </message>
    <message>
        <location filename="../cyboucoreapplicationadapter.cpp" line="+227"/>
        <source>Your recovery data could not be secured.</source>
        <translation>Impossible de protéger vos données de récupération.</translation>
    </message>
    <message>
        <location line="-156"/>
        <source>CYBOU was locked before your data was secured. The current recovery phrase stays active.</source>
        <translation>CYBOU a été verrouillé avant la protection de vos données. La phrase de récupération actuelle reste active.</translation>
    </message>
    <message>
        <location line="+120"/>
        <source>Your data cannot be secured for a new recovery phrase right now.</source>
        <translation>Vos données ne peuvent pas être protégées pour une nouvelle phrase de récupération pour le moment.</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+13"/>
        <source>The new recovery phrase is invalid.</source>
        <translation>La nouvelle phrase de récupération est invalide.</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>This action is not available yet.</source>
        <translation>Cette action n’est pas encore disponible.</translation>
    </message>
    <message>
        <source>The draft could not be saved.</source>
        <translation type="vanished">Impossible d’enregistrer le brouillon.</translation>
    </message>
    <message>
        <location filename="../cyboucoreapplicationadapter_mail.cpp" line="+296"/>
        <source>Could not prepare the message.</source>
        <translation>Impossible de préparer le message.</translation>
    </message>
    <message>
        <source>No CYBOU Identity has this name.</source>
        <translation type="vanished">Aucune identité CYBOU ne porte ce nom.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>%1 could not be read.</source>
        <translation>Impossible de lire %1.</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>%1 is not protected yet.</source>
        <translation>%1 n’est pas encore protégé.</translation>
    </message>
    <message>
        <source>The message could not be sent.</source>
        <translation type="vanished">Impossible d’envoyer le message.</translation>
    </message>
    <message>
        <location line="+64"/>
        <source>This message cannot be moved there.</source>
        <translation>Impossible de déplacer ce message à cet emplacement.</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Some messages could not be deleted.</source>
        <translation>Impossible de supprimer certains messages.</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>This attachment is not available.</source>
        <translation>Cette pièce jointe est indisponible.</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>The attachment could not be saved to Files.</source>
        <translation>Impossible d’enregistrer la pièce jointe dans Fichiers.</translation>
    </message>
    <message>
        <location filename="../cyboucoreapplicationadapter_files.cpp" line="+206"/>
        <source>The file could not be read.</source>
        <translation>Impossible de lire le fichier.</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>The file could not be uploaded.</source>
        <translation>Impossible de téléverser le fichier.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>This file has no content yet.</source>
        <translation>Ce fichier ne contient encore aucune donnée.</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>The folder could not be created.</source>
        <translation>Impossible de créer le dossier.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>The item could not be renamed.</source>
        <translation>Impossible de renommer cet élément.</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>The item could not be moved.</source>
        <translation>Impossible de déplacer cet élément.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Copy of %1</source>
        <translation>Copie de %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>The copy could not be created.</source>
        <translation>Impossible de créer la copie.</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>The item could not be moved to Trash.</source>
        <translation>Impossible de déplacer l’élément dans la corbeille.</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>The item could not be restored.</source>
        <translation>Impossible de restaurer l’élément.</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>The items could not be deleted.</source>
        <translation>Impossible de supprimer les éléments.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>This change cannot be retried; upload the file again.</source>
        <translation>Cette opération ne peut pas être relancée ; téléversez à nouveau le fichier.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The change still could not be sent. Try again later.</source>
        <translation>Impossible d’envoyer cette opération. Réessayez plus tard.</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>This change is already on its way and cannot be discarded.</source>
        <translation>Cette opération est déjà en cours d’envoi et ne peut pas être annulée.</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>The item could not be deleted.</source>
        <translation>Impossible de supprimer cet élément.</translation>
    </message>
    <message>
        <location filename="../cyboucoreapplicationadapter_mail.cpp" line="-332"/>
        <location line="+18"/>
        <location line="+21"/>
        <source>Mail is unavailable.</source>
        <translation>Le courrier est indisponible.</translation>
    </message>
    <message>
        <location line="+157"/>
        <source>Could not prepare the message. Your draft is kept.</source>
        <translation>Impossible de préparer le message. Votre brouillon est conservé.</translation>
    </message>
    <message>
        <location line="-110"/>
        <location line="+8"/>
        <source>Could not save the outgoing message. Your draft is kept.</source>
        <translation>Impossible d’enregistrer le message sortant. Votre brouillon est conservé.</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>No CYBOU Identity has this name. Your draft is kept.</source>
        <translation>Aucune identité CYBOU ne porte ce nom. Votre brouillon est conservé.</translation>
    </message>
    <message>
        <location line="-62"/>
        <source>The draft could not be saved. The message has not been sent.</source>
        <translation>Impossible d’enregistrer le brouillon. Le message n’a pas été envoyé.</translation>
    </message>
    <message>
        <location line="-39"/>
        <source>The draft could not be saved. Your text is kept open; try again.</source>
        <translation>Impossible d’enregistrer le brouillon. Votre texte reste ouvert ; réessayez.</translation>
    </message>
    <message>
        <location line="+70"/>
        <source>The message needs attention. Your draft is kept; retry continues the same publication.</source>
        <translation>Le message nécessite votre attention. Votre brouillon est conservé ; une nouvelle tentative poursuit la même publication.</translation>
    </message>
    <message>
        <location line="+92"/>
        <source>The outgoing message could not be saved. Your draft is kept.</source>
        <translation>Impossible d’enregistrer le message sortant. Votre brouillon est conservé.</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>This message could not be moved. Try again.</source>
        <translation>Impossible de déplacer ce message. Réessayez.</translation>
    </message>
    <message>
        <location line="-148"/>
        <source>These edits have not been sent. Start a new message: this draft is linked to an earlier saved publication.</source>
        <translation>Ces modifications n’ont pas été envoyées. Rédigez un nouveau message : ce brouillon est lié à une publication déjà enregistrée.</translation>
    </message>
    <message>
        <location line="-83"/>
        <source>The local view could not be refreshed. Try again.</source>
        <translation>La vue locale n’a pas pu être actualisée. Réessayez.</translation>
    </message>

    <message><source>Your encrypted data could not be opened. Lock your Identity and try again.</source><translation>Vos données chiffrées n’ont pas pu être ouvertes. Verrouillez votre identité puis réessayez.</translation></message>
    <message><source>Your data could not be prepared yet. CYBOU will retry automatically.</source><translation>Vos données n’ont pas encore pu être préparées. CYBOU réessaiera automatiquement.</translation></message>
    <message><source>Waiting for encrypted content from peers… You can open the local view while CYBOU retries.</source><translation>En attente du contenu chiffré des pairs… Vous pouvez ouvrir l’espace local pendant que CYBOU réessaie.</translation></message>
    <message><source>Your Identity is not available. Return to unlock and try again.</source><translation>Votre identité n’est pas disponible. Revenez au déverrouillage puis réessayez.</translation></message>
</context>
<context>
    <name>CybouCoreApplicationAdapter::IdentitySession</name>
    <message>
        <location filename="../cyboucoreapplicationadapter_identity_session.cpp" line="+106"/>
        <source>Your recovery data could not be verified.</source>
        <translation type="unfinished">Impossible de vérifier vos données de récupération.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Your recovery data could not be secured.</source>
        <translation type="unfinished">Impossible de protéger vos données de récupération.</translation>
    </message>
</context>
<context>
    <name>CybouCoreApplicationAdapter::IdentitySession::StorageProjection</name>
    <message>
        <location filename="../cyboucoreapplicationadapter_storage.cpp" line="+17"/>
        <location line="+23"/>
        <source>The destination cannot be written.</source>
        <translation type="unfinished">Impossible d’écrire dans la destination.</translation>
    </message>
    <message>
        <location line="-4"/>
        <source>This content is temporarily unavailable. Try again later.</source>
        <translation type="unfinished">Ce contenu est temporairement indisponible. Réessayez plus tard.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This content could not be verified.</source>
        <translation type="unfinished">Impossible de vérifier ce contenu.</translation>
    </message>
</context>
<context>
    <name>CybouDesktopController</name>
    <message>
        <source>This computer had data from an older CYBOU DEV network. It was moved to %1. Create or restore your Identity on the current network.</source>
        <translation type="vanished">Cet ordinateur contenait des données d’un ancien réseau CYBOU DEV. Elles ont été déplacées vers %1. Créez ou restaurez votre identité sur le réseau actuel.</translation>
    </message>
</context>
<context>
    <name>CybouDesktopModel</name>
    <message>
        <location filename="../cyboudesktopmodel.cpp" line="+76"/>
        <source>Needs attention</source>
        <translation>À vérifier</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Offline</source>
        <translation>Hors ligne</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Connecting</source>
        <translation>Connexion…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Syncing</source>
        <translation>Synchronisation…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Synced</source>
        <translation>Synchronisé</translation>
    </message>
    <message>
        <location line="+32"/>
        <location line="+870"/>
        <source>(no subject)</source>
        <translation>(sans objet)</translation>
    </message>
    <message>
        <location line="-867"/>
        <source>Mail to %1</source>
        <translation>Courrier à %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Mail from %1</source>
        <translation>Courrier de %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>%1 added to Files</source>
        <translation>%1 ajouté à Fichiers</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>%1 sent</source>
        <translation>%1 envoyé(s)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 received</source>
        <translation>%1 reçu(s)</translation>
    </message>
    <message numerus="yes">
        <source>You reached %1 network operations for this window. It resets in about %n minute(s).</source>
        <translation type="vanished">
            <numerusform>Vous avez atteint %1 opérations réseau pour cette fenêtre. Elle se réinitialise dans environ %n minute.</numerusform>
            <numerusform>Vous avez atteint %1 opérations réseau pour cette fenêtre. Elle se réinitialise dans environ %n minutes.</numerusform>
        </translation>
    </message>
    <message>
        <source>%1 does not belong to a CYBOU Identity.</source>
        <translation type="vanished">%1 n’appartient à aucune Identité CYBOU.</translation>
    </message>
    <message>
        <location line="+774"/>
        <source>Payments are not connected yet.</source>
        <translation>Les paiements ne sont pas encore connectés.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>This name does not belong to a CYBOU Identity.</source>
        <translation>Ce nom n’est associé à aucune identité CYBOU.</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+38"/>
        <source>Not enough CYBOU available.</source>
        <translation>Solde CYBOU disponible insuffisant.</translation>
    </message>
    <message>
        <location line="-37"/>
        <source>Not enough System Balance for the network service fee.</source>
        <translation>Le solde système ne suffit pas à payer les frais de service réseau.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>You cannot send CYBOU to yourself.</source>
        <translation>Vous ne pouvez pas vous envoyer des CYBOU.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+36"/>
        <source>CYBOU could not reach the network. Try again.</source>
        <translation>CYBOU n’a pas pu joindre le réseau. Réessayez.</translation>
    </message>
    <message>
        <location line="-35"/>
        <source>The payment could not be sent.</source>
        <translation>Le paiement n’a pas pu être envoyé.</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>The wallet is not connected yet.</source>
        <translation>Le portefeuille n’est pas encore connecté.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>CYBOU could not be moved to System Balance.</source>
        <translation>Impossible de transférer les CYBOU vers le solde système.</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Mail: %1</source>
        <translation>Courrier : %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Folder: %1</source>
        <translation>Dossier : %1</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>File: %1</source>
        <translation>Fichier : %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Files and folders update</source>
        <translation>Mise à jour des fichiers et dossiers</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>CYBOU Support</source>
        <translation>Assistance CYBOU</translation>
    </message>
    <message>
        <location line="+70"/>
        <source>Claiming %1.cybou…</source>
        <translation>Réservation de %1.cybou…</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Saving encrypted name claim…</source>
        <translation>Enregistrement chiffré de la réservation du nom…</translation>
    </message>
    <message>
        <location line="+53"/>
        <source>Your data cannot be secured for a new recovery phrase right now.</source>
        <translation>Vos données ne peuvent pas être protégées pour une nouvelle phrase de récupération pour le moment.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Securing your data for the new recovery phrase. Keep CYBOU open and unlocked.</source>
        <translation>Protection de vos données pour la nouvelle phrase de récupération. Gardez CYBOU ouvert et déverrouillé.</translation>
    </message>
    <message>
        <location line="+220"/>
        <source>Enter a name.</source>
        <translation>Saisissez un nom.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Use at least 5 characters.</source>
        <translation>Utilisez au moins 5 caractères.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Use at most 32 characters.</source>
        <translation>Utilisez au maximum 32 caractères.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Use lowercase letters a–z, digits and hyphens.</source>
        <translation>Utilisez des lettres minuscules a–z, des chiffres et des tirets.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>A name cannot start or end with a hyphen.</source>
        <translation>Un nom ne peut pas commencer ni se terminer par un tiret.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>A name cannot contain two hyphens in a row.</source>
        <translation>Un nom ne peut pas contenir deux tirets consécutifs.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Names cannot start with &quot;xn--&quot;.</source>
        <translation>Les noms ne peuvent pas commencer par « xn-- ».</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>A name needs at least one letter.</source>
        <translation>Un nom doit contenir au moins une lettre.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This name is reserved.</source>
        <translation>Ce nom est réservé.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>This name is not valid.</source>
        <translation>Ce nom n’est pas valide.</translation>
    </message>
    <message>
        <location line="+79"/>
        <source>Deriving your keys and checking the network…</source>
        <translation>Dérivation de vos clés et vérification du réseau…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Computing the anti-spam proof-of-work…</source>
        <translation>Calcul de la preuve de travail anti-spam…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Sending your Identity to the network…</source>
        <translation>Envoi de votre identité au réseau…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Waiting for the network to confirm it…</source>
        <translation>En attente de la confirmation du réseau…</translation>
    </message>
    <message>
        <source>Wait for the network to finish syncing before creating an Identity.</source>
        <translation type="vanished">Attendez la fin de la synchronisation du réseau avant de créer une identité.</translation>
    </message>
    <message>
        <location line="+239"/>
        <source>%1 saved to Files</source>
        <translation>%1 enregistré dans Fichiers</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>From Mail</source>
        <translation>Depuis Courrier</translation>
    </message>
    <message>
        <location filename="../cyboudesktopcontroller.cpp" line="+204"/>
        <source>Block finalized.</source>
        <translation>Bloc finalisé.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>No block was finalized. See the finalizer state.</source>
        <translation>Aucun bloc n’a été finalisé. Consultez l’état du finaliseur.</translation>
    </message>
    <message>
        <source>The AUTH change could not be prepared.</source>
        <translation type="vanished">La modification d’AUTH n’a pas pu être préparée.</translation>
    </message>
    <message>
        <source>AUTH change submitted for the next block.</source>
        <translation type="vanished">Modification d’AUTH soumise pour le prochain bloc.</translation>
    </message>
    <message>
        <source>The PoA signer is not active.</source>
        <translation type="vanished">Le signataire PoA n’est pas actif.</translation>
    </message>
    <message>
        <source>The AUTH change was rejected by local execution.</source>
        <translation type="vanished">La modification d’AUTH a été rejetée par l’exécution locale.</translation>
    </message>
    <message>
        <location line="-41"/>
        <source>The next storage settlement is due %1 UTC.</source>
        <translation>Le prochain règlement du stockage est dû le %1 UTC.</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Storage period %1 settled: %2 payouts, %3 CYBOU. It is finalized in the next block.</source>
        <translation>Période de stockage %1 réglée : %2 paiements, %3 CYBOU. Elle sera finalisée dans le prochain bloc.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The storage settlement was rejected by local execution.</source>
        <translation>Le règlement du stockage a été rejeté par l’exécution locale.</translation>
    </message>
    <message>
        <location line="+237"/>
        <source>This device held data of a previous CYBOU network. It was moved aside; restore your Identity with its phrase.</source>
        <translation>Cet appareil contenait les données d’un ancien réseau CYBOU. Elles ont été mises de côté ; restaurez votre identité avec sa phrase.</translation>
    </message>
    <message>
        <location filename="../cyboudesktopmodel.cpp" line="-1223"/>
        <source>Archiving message</source>
        <translation>Archivage du message</translation>
    </message>
    <message>
        <location line="-4"/>
        <source>Mail is unavailable.</source>
        <translation>Le courrier est indisponible.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Moving message</source>
        <translation>Déplacement du message</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Moving message to Trash</source>
        <translation>Déplacement du message dans la corbeille</translation>
    </message>
    <message>
        <location line="+112"/>
        <source>Preparing message</source>
        <translation>Préparation du message</translation>
    </message>
    <message>
        <location line="-30"/>
        <source>Saving draft</source>
        <translation>Enregistrement du brouillon</translation>
    </message>
    <message>
        <location line="-273"/>
        <source>Local refresh was interrupted. Try again.</source>
        <translation>L’actualisation locale a été interrompue. Réessayez.</translation>
    </message>
</context>
<context>
    <name>CybouFixtureApplicationBackend</name>
    <message>
        <location filename="../cyboufixturebackend.cpp" line="+67"/>
        <source>Mail is unavailable.</source>
        <translation type="unfinished">Le courrier est indisponible.</translation>
    </message>
    <message>
        <location line="+103"/>
        <source>This message cannot be moved there.</source>
        <translation type="unfinished">Impossible de déplacer ce message à cet emplacement.</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>This attachment is not protected yet.</source>
        <translation>Cette pièce jointe n’est pas encore protégée.</translation>
    </message>
    <message>
        <location line="+138"/>
        <source>Copy of %1</source>
        <translation>Copie de %1</translation>
    </message>
</context>
<context>
    <name>CybouMainWindow</name>
    <message>
        <location filename="../cyboumainwindow.cpp" line="+92"/>
        <source>Home</source>
        <translation>Accueil</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Mail</source>
        <translation>Courrier</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Files</source>
        <translation>Fichiers</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Wallet</source>
        <translation>Portefeuille</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+298"/>
        <source>Identity &amp; Security</source>
        <translation>Identité et sécurité</translation>
    </message>
    <message>
        <location line="-297"/>
        <source>Network</source>
        <translation>Réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+635"/>
        <source>Diagnostics</source>
        <translation>Diagnostics</translation>
    </message>
    <message>
        <location line="-634"/>
        <location line="+301"/>
        <source>Settings</source>
        <translation>Paramètres</translation>
    </message>
    <message>
        <source>Network Authority</source>
        <translation type="vanished">Autorité du réseau</translation>
    </message>
    <message>
        <location line="-276"/>
        <source>CYBOU could not start</source>
        <translation>CYBOU n’a pas pu démarrer</translation>
    </message>
    <message>
        <location line="+11"/>
        <location line="+131"/>
        <location line="+442"/>
        <location line="+353"/>
        <source>CYBOU</source>
        <translation>CYBOU</translation>
    </message>
    <message>
        <location line="-882"/>
        <source>CYBOU diagnostics</source>
        <translation>Diagnostics de CYBOU</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Network: %1
Network ID: %2
Node running: %3
Peers: %4
Finalized height: %5
Data directory: %6</source>
        <translation>Réseau : %1
ID réseau : %2
Nœud actif : %3
Pairs : %4
Hauteur finalisée : %5
Dossier de données : %6</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>yes</source>
        <translation>oui</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>no</source>
        <translation>non</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>unknown</source>
        <translation>inconnu</translation>
    </message>
    <message>
        <location line="+128"/>
        <location line="+1"/>
        <source>Search mail and files</source>
        <translation>Rechercher dans les courriers et fichiers</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Search mail and files (Ctrl+K)</source>
        <translation>Rechercher dans les courriers et fichiers (Ctrl+K)</translation>
    </message>
    <message>
        <location line="+61"/>
        <source>Your CYBOU Identity</source>
        <translation>Votre identité CYBOU</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+160"/>
        <source>No Identity</source>
        <translation>Aucune identité</translation>
    </message>
    <message>
        <location line="-158"/>
        <source>Identity active</source>
        <translation>Identité active</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Vault locked</source>
        <translation>Coffre verrouillé</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Not set up</source>
        <translation>Non configuré</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Lock local vault</source>
        <translation>Verrouiller le coffre local</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Diagnostics window</source>
        <translation>Fenêtre de diagnostic</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+1"/>
        <source>About CYBOU</source>
        <translation>À propos de CYBOU</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>CYBOU gives you one private Identity for Mail, Files, Names and Wallet.</source>
        <translation>CYBOU vous offre une identité privée pour vos courriers, fichiers, noms et votre portefeuille.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Hide CYBOU</source>
        <translation>Masquer CYBOU</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+329"/>
        <source>Quit CYBOU</source>
        <translation>Quitter CYBOU</translation>
    </message>
    <message>
        <location line="-280"/>
        <source>Upload files</source>
        <translation>Téléverser des fichiers</translation>
    </message>
    <message>
        <location line="+77"/>
        <source>Online • %1</source>
        <translation>En ligne • %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Connection status: %1</source>
        <translation>État de la connexion : %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Identity menu for %1</source>
        <translation>Menu de l’identité de %1</translation>
    </message>
    <message>
        <location line="+61"/>
        <location line="+2"/>
        <source>Search files (Enter for current view)</source>
        <translation>Rechercher dans les fichiers (Entrée pour la vue actuelle)</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Search files — Enter searches current view, or pick a suggestion (Ctrl+K)</source>
        <translation>Rechercher dans les fichiers — Entrée recherche dans la vue actuelle, ou choisissez une suggestion (Ctrl+K)</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+2"/>
        <source>Search mail and files (Enter for mail)</source>
        <translation>Rechercher dans les e-mails et fichiers (Entrée pour les e-mails)</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Search mail and files — Enter searches mail, or pick a suggestion (Ctrl+K)</source>
        <translation>Rechercher dans les e-mails et fichiers — Entrée recherche dans les e-mails, ou choisissez une suggestion (Ctrl+K)</translation>
    </message>
    <message>
        <location line="+32"/>
        <location line="+118"/>
        <source>(no subject)</source>
        <translation>(sans objet)</translation>
    </message>
    <message>
        <location line="-35"/>
        <source>Open CYBOU</source>
        <translation>Ouvrir CYBOU</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>New message from %1</source>
        <translation>Nouveau message de %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>New CYBOU Mail</source>
        <translation>Nouveau courrier CYBOU</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Open CYBOU to read it.</source>
        <translation>Ouvrez CYBOU pour le lire.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>New CYBOU payment</source>
        <translation>Nouveau paiement CYBOU</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Open CYBOU to view details.</source>
        <translation>Ouvrez CYBOU pour afficher les détails.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>%1 new items in CYBOU</source>
        <translation>%1 nouveaux éléments dans CYBOU</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Payment not sent</source>
        <translation>Paiement non envoyé</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>CYBOU locked after %1 minutes of inactivity.</source>
        <translation>CYBOU a été verrouillé après %1 minutes d’inactivité.</translation>
    </message>
    <message>
        <location line="+82"/>
        <source>Project files</source>
        <translation>Fichiers du projet</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Hello Alice,

Here are the final files.

Stan</source>
        <translation>Bonjour Alice,

Voici les fichiers finalisés.

Stan</translation>
    </message>
    <message>
        <location line="-790"/>
        <source>Central Authority</source>
        <translation>Autorité centrale</translation>
    </message>
    <message>
        <location line="+367"/>
        <source>This file is not ready to attach. Open its details to check protection.</source>
        <translation>Ce fichier n’est pas prêt à être joint. Consultez ses détails pour vérifier sa protection.</translation>
    </message>

    <message><source>Opening your Identity</source><translation>Ouverture de votre identité</translation></message>
    <message><source>Preparing your Mail and Files</source><translation>Préparation de vos e-mails et fichiers</translation></message>
    <message><source>Return to unlock</source><translation>Revenir au déverrouillage</translation></message>
    <message><source>Opening your encrypted local data…</source><translation>Ouverture de vos données locales chiffrées…</translation></message>
    <message><source>Discovering and decrypting your content…</source><translation>Recherche et déchiffrement de votre contenu…</translation></message>
    <message><source>Verified history: %p%</source><translation>Historique vérifié : %p%</translation></message>
    <message><source>Connected to the network. Preparing your private view.</source><translation>Connecté au réseau. Préparation de votre espace privé.</translation></message>
    <message><source>Connecting to the network… Local data remains available offline.</source><translation>Connexion au réseau… Vos données locales restent accessibles hors ligne.</translation></message>
    <message><source>Open the local view while loading continues</source><translation>Ouvrir l’espace local pendant le chargement</translation></message>
</context>
<context>
    <name>CybouProduct</name>
    <message>
        <location filename="../cybouproduct.h" line="+269"/>
        <source>Local</source>
        <translation>Local</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Securing…</source>
        <translation>Protection…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Protected</source>
        <translation>Protégé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Received</source>
        <translation>Reçu</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Temporarily unavailable</source>
        <translation>Temporairement indisponible</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Needs attention</source>
        <translation>À vérifier</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>On this device</source>
        <translation>Sur cet appareil</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Preparing…</source>
        <translation>Préparation…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Waiting for confirmation</source>
        <translation>En attente de confirmation</translation>
    </message>
    <message>
        <source>Validated</source>
        <translation type="vanished">Validé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Finalized</source>
        <translation>Finalisé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Failed</source>
        <translation>Échec</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Waiting for network</source>
        <translation>En attente du réseau</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Securing %1%</source>
        <translation>Protection %1 %</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Draft</source>
        <translation>Brouillon</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Sent</source>
        <translation>Envoyé</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Downloading…</source>
        <translation>Téléchargement…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Verifying…</source>
        <translation>Vérification…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Decrypting…</source>
        <translation>Déchiffrement…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Ready</source>
        <translation>Prêt</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Available offline</source>
        <translation>Disponible hors ligne</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Downloaded when opened</source>
        <translation>Téléchargé à l’ouverture</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>%1 B</source>
        <translation>%1 o</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 KB</source>
        <translation>%1 Ko</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 MB</source>
        <translation>%1 Mo</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 GB</source>
        <translation>%1 Go</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>Protecting: %1 of %2 remote copies</source>
        <translation>Protection : %1 copies distantes sur %2</translation>
    </message>
</context>
<context>
    <name>CybouUi</name>
    <message>
        <location filename="../cybouui.h" line="+541"/>
        <source>Encrypted on this computer and stored encrypted on the network</source>
        <translation>Chiffré sur cet ordinateur et stocké chiffré sur le réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>End-to-end encrypted; stored encrypted on this computer</source>
        <translation>Chiffré de bout en bout ; stocké chiffré sur cet ordinateur</translation>
    </message>
    <message>
        <location line="+142"/>
        <source>just now</source>
        <translation>à l&apos;instant</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 min ago</source>
        <translation>il y a %1 min</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 h ago</source>
        <translation>il y a %1 h</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 d ago</source>
        <translation>il y a %1 j</translation>
    </message>
</context>
<context>
    <name>CybouUiFixtures::Driver</name>
    <message>
        <source>Not enough CYBOU available.</source>
        <translation type="vanished">Solde CYBOU disponible insuffisant.</translation>
    </message>
    <message>
        <source>%1 sent to %2</source>
        <translation type="vanished">%1 envoyé à %2</translation>
    </message>
</context>
<context>
    <name>DiagnosticsPage</name>
    <message>
        <location filename="../pages/diagnosticspage.cpp" line="+223"/>
        <source>Node type</source>
        <translation>Type de nœud</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Full Node</source>
        <translation>Nœud complet</translation>
    </message>
    <message>
        <source>Storage used / capacity</source>
        <translation type="vanished">Stockage utilisé / capacité</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>PoA signer active</source>
        <translation>Signataire PoA actif</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Peer admission Geo database</source>
        <translation>Base Geo pour l’admission des pairs</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>Ready</source>
        <translation>Prête</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Waiting for a valid Geo database</source>
        <translation>En attente d’une base Geo valide</translation>
    </message>
    <message>
        <location line="-143"/>
        <source>Technical information about the CYBOU node running inside this app.</source>
        <translation>Informations techniques sur le nœud CYBOU exécuté dans cette application.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Showing fixture data. The CYBOU node is not running.</source>
        <translation>Données de démonstration : le nœud CYBOU n’est pas démarré.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Node</source>
        <translation>Nœud</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network</source>
        <translation>Réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Peers</source>
        <translation>Pairs</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+51"/>
        <source>Finalized height</source>
        <translation>Hauteur finalisée</translation>
    </message>
    <message>
        <location line="-50"/>
        <source>Finality</source>
        <translation>Finalité</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Details</source>
        <translation>Détails</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Services</source>
        <translation>Services</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Open Network Monitor</source>
        <translation>Ouvrir le moniteur réseau</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>CYBOU Network Monitor</source>
        <translation>Moniteur réseau CYBOU</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Peer endpoint</source>
        <translation>Point de terminaison du pair</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Advertised height</source>
        <translation>Hauteur annoncée</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Lag</source>
        <translation>Retard</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>StorageId</source>
        <translation>ID du fournisseur</translation>
    </message>
    <message>
        <source>Bootstrap</source>
        <translation type="vanished">Bootstrap</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+1"/>
        <source>OperationID</source>
        <translation>ID de l’opération</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Local assessment</source>
        <translation>Évaluation locale</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Application object ID</source>
        <translation>ID de l’objet applicatif</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Content state</source>
        <translation>État du contenu</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Remote replicas</source>
        <translation>Copies distantes</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Target</source>
        <translation>Objectif</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>%1 | %2 | Height %3 | Safety halt %4
NetworkID %5
Tip %6
State root %7</source>
        <translation>%1 | %2 | Hauteur %3 | Arrêt de sécurité %4
ID réseau %5
Tête %6
Racine d’état %7</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Online</source>
        <translation>En ligne</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Offline</source>
        <translation>Hors ligne</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>YES</source>
        <translation>OUI</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+70"/>
        <source>No</source>
        <translation>Non</translation>
    </message>
    <message>
        <source>Block peer</source>
        <translation type="vanished">Pair de blocs</translation>
    </message>
    <message>
        <source>Storage</source>
        <translation type="vanished">Stockage</translation>
    </message>
    <message>
        <location line="-59"/>
        <location line="+2"/>
        <location line="+3"/>
        <location line="+0"/>
        <location line="+2"/>
        <location line="+0"/>
        <source>Unknown</source>
        <translation>Inconnu</translation>
    </message>
    <message>
        <location line="-7"/>
        <source>Local pending</source>
        <translation>En attente localement</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Accepted remotely</source>
        <translation>Accepté à distance</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Finalized</source>
        <translation>Finalisé</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Rejected</source>
        <translation>Rejeté</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>History unavailable</source>
        <translation>Historique indisponible</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Open Read-Only Console</source>
        <translation>Ouvrir la console en lecture seule</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Open diagnostics window</source>
        <translation>Ouvrir la fenêtre de diagnostic</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Running</source>
        <translation>En cours</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Stopped</source>
        <translation>Arrêté</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>PoA verified</source>
        <translation>PoA vérifiée</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Waiting</source>
        <translation>En attente</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Yes</source>
        <translation>Oui</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Connection</source>
        <translation>Connexion</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Last sync</source>
        <translation>Dernière synchronisation</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Not yet</source>
        <translation>Pas encore</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Last error</source>
        <translation>Dernière erreur</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network ID</source>
        <translation>ID du réseau</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+1"/>
        <source>Available after node startup</source>
        <translation>Disponible après le démarrage du nœud</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Data directory</source>
        <translation>Dossier de données</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Finality model</source>
        <translation>Modèle de finalité</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Single-operator proof of authority (not Byzantine fault tolerant)</source>
        <translation>Preuve d’autorité à opérateur unique (sans tolérance aux fautes byzantines)</translation>
    </message>
    <message>
        <source>Authority</source>
        <translation type="vanished">Autorité</translation>
    </message>
    <message>
        <source>Validation eligible</source>
        <translation type="vanished">Éligible à la Validation</translation>
    </message>
    <message>
        <source>Identity Authority index</source>
        <translation type="vanished">Index de l’autorité des identités</translation>
    </message>
    <message>
        <source>Scanned to height %1  ·  %2</source>
        <translation type="vanished">Analysé jusqu’à la hauteur %1 · %2</translation>
    </message>
    <message>
        <source>Up to date</source>
        <translation type="vanished">À jour</translation>
    </message>
    <message>
        <source>Catching up</source>
        <translation type="vanished">Synchronisation en cours</translation>
    </message>
    <message>
        <source>Not available</source>
        <translation type="vanished">Indisponible</translation>
    </message>
    <message>
        <source>Identity Authority</source>
        <translation type="vanished">Autorité de l’identité</translation>
    </message>
    <message>
        <source>Informational only</source>
        <translation type="vanished">À titre indicatif</translation>
    </message>
    <message>
        <source>Validation</source>
        <translation type="vanished">Validation</translation>
    </message>
    <message>
        <source>Observing (informational, not final)</source>
        <translation type="vanished">Observation (indicative, non finale)</translation>
    </message>
    <message>
        <source>Hidden by settings</source>
        <translation type="vanished">Masqué dans les paramètres</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Identity</source>
        <translation>Identité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Wallet</source>
        <translation>Portefeuille</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Mail</source>
        <translation>Courrier</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Files</source>
        <translation>Fichiers</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Connected</source>
        <translation>Connecté</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Not connected yet</source>
        <translation>Pas encore connecté</translation>
    </message>
    <message>
        <location line="-24"/>
        <source>Storage held for others / limit</source>
        <translation>Stockage conservé pour les autres / limite</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Estimated storage service value</source>
        <translation>Valeur estimée du service de stockage</translation>
    </message>
    <message>
        <location line="-4"/>
        <source>Local storage used / capacity</source>
        <translation>Stockage local utilisé / capacité</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>~%1 CYBOU/day (estimate, not paid)</source>
        <translation>~%1 CYBOU/jour (estimation, non payée)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Unavailable</source>
        <translation>Indisponible</translation>
    </message>
</context>
<context>
    <name>Driver</name>
    <message>
        <location filename="../cybouuifixtures.cpp" line="+348"/>
        <source>Not enough CYBOU available.</source>
        <translation>Pas assez de CYBOU disponibles.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>%1 sent to %2</source>
        <translation>%1 envoyés à %2</translation>
    </message>
</context>
<context>
    <name>EmailPage</name>
    <message>
        <location filename="../pages/emailpage.cpp" line="+67"/>
        <source>Inbox</source>
        <translation>Boîte de réception</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Starred</source>
        <translation>Favoris</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Sent</source>
        <translation>Envoyés</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Drafts</source>
        <translation>Brouillons</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+486"/>
        <source>Archive</source>
        <translation>Archives</translation>
    </message>
    <message>
        <location line="-485"/>
        <source>Trash</source>
        <translation>Corbeille</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>To: %1</source>
        <translation>À : %1</translation>
    </message>
    <message>
        <location line="+47"/>
        <location line="+642"/>
        <source>%1, %2%3</source>
        <translation>%1, %2%3</translation>
    </message>
    <message>
        <location line="-641"/>
        <location line="+642"/>
        <source>, unread</source>
        <translation>, non lu</translation>
    </message>
    <message>
        <location line="-618"/>
        <source>Has attachments</source>
        <translation>Contient des pièces jointes</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Below support rate</source>
        <translation>Inférieur au tarif d’assistance</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>This message paid less than the support rate; it may be sent by a modified client or be spam.</source>
        <translation>Ce message a payé moins que le tarif d’assistance ; il peut provenir d’un client modifié ou être indésirable.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Protected: encrypted here and stored encrypted on the network</source>
        <translation>Protégé : chiffré ici et stocké chiffré sur le réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Received: end-to-end encrypted</source>
        <translation>Reçu : chiffré de bout en bout</translation>
    </message>
    <message>
        <location line="+10"/>
        <location line="+386"/>
        <source>(no subject)</source>
        <translation>(sans objet)</translation>
    </message>
    <message>
        <location line="-362"/>
        <location line="+104"/>
        <source>Compose</source>
        <translation>Rédiger</translation>
    </message>
    <message>
        <location line="-95"/>
        <source>Mail folders</source>
        <translation>Dossiers de courrier</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Contacts</source>
        <translation>Contacts</translation>
    </message>
    <message>
        <location line="+22"/>
        <location line="+1"/>
        <source>Search mail</source>
        <translation>Rechercher dans les courriers</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Empty Trash</source>
        <translation>Vider la corbeille</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Set up Identity</source>
        <translation>Configurer l’identité</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Messages</source>
        <translation>Messages</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>No message selected</source>
        <translation>Aucun message sélectionné</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Sending… It shows as Sent once it is stored securely.</source>
        <translation>Envoi… Le message apparaîtra dans Envoyés une fois stocké en sécurité.</translation>
    </message>
    <message>
        <location line="+84"/>
        <source>You&apos;re all caught up.</source>
        <translation>Tout est à jour.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>1 unread message in your Inbox.</source>
        <translation>1 message non lu dans votre boîte de réception.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 unread messages in your Inbox.</source>
        <translation>%1 messages non lus dans votre boîte de réception.</translation>
    </message>
    <message>
        <source>%1 conversations starred</source>
        <translation type="vanished">%1 conversations ajoutées aux favoris</translation>
    </message>
    <message>
        <source>Conversation archived</source>
        <translation type="vanished">Conversation archivée</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>%1 conversations archived</source>
        <translation>%1 conversations archivées</translation>
    </message>
    <message>
        <source>Moved to Trash</source>
        <translation type="vanished">Déplacé dans la corbeille</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 conversations moved to Trash</source>
        <translation>%1 conversations déplacées dans la corbeille</translation>
    </message>
    <message>
        <source>Moved to Inbox</source>
        <translation type="vanished">Déplacé dans la boîte de réception</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 conversations moved to Inbox</source>
        <translation>%1 conversations déplacées dans la boîte de réception</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Undo</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Delete this message forever? It is removed from this mailbox and cannot be restored here.</source>
        <translation>Supprimer définitivement ce message ? Il sera retiré de cette boîte et ne pourra pas être restauré ici.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Delete %1 messages forever? They are removed from this mailbox and cannot be restored here.</source>
        <translation>Supprimer définitivement %1 messages ? Ils seront retirés de cette boîte et ne pourront pas être restaurés ici.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Eligible sent publications can be revoked after finalization, freeing publication quota and initiating managed purge. Recipients and other holders may retain copies.</source>
        <translation>Les publications envoyées éligibles peuvent être retirées après finalisation, libérant le quota de publication et déclenchant une suppression gérée. Les destinataires et d’autres détenteurs peuvent conserver des copies.</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+53"/>
        <source>Delete forever</source>
        <translation>Supprimer définitivement</translation>
    </message>
    <message>
        <location line="-50"/>
        <source>Message deleted</source>
        <translation>Message supprimé</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 messages deleted</source>
        <translation>%1 messages supprimés</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Edit draft</source>
        <translation>Modifier le brouillon</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Open</source>
        <translation>Ouvrir</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Reply</source>
        <translation>Répondre</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Forward</source>
        <translation>Transférer</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Mark as read</source>
        <translation>Marquer comme lu</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Mark as unread</source>
        <translation>Marquer comme non lu</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Remove star</source>
        <translation>Retirer des favoris</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Star</source>
        <translation>Ajouter aux favoris</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Move to Inbox</source>
        <translation>Déplacer dans la boîte de réception</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Move to Trash</source>
        <translation>Déplacer dans la corbeille</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Discard draft</source>
        <translation>Supprimer le brouillon</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Draft discarded</source>
        <translation>Brouillon supprimé</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>%1 conversations</source>
        <translation>%1 conversations</translation>
    </message>
    <message>
        <location line="+108"/>
        <location line="+10"/>
        <source>%1, %2</source>
        <translation>%1, %2</translation>
    </message>
    <message>
        <location line="+93"/>
        <source>No messages match “%1”.</source>
        <translation>Aucun message ne correspond à « %1 ».</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 is empty.</source>
        <translation>%1 est vide.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Mail needs your CYBOU Identity. Create or restore it on Home.</source>
        <translation>Le courrier nécessite votre identité CYBOU. Créez-la ou restaurez-la depuis l’accueil.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Mail is not connected yet. Messages will appear here once it is.</source>
        <translation>Le courrier n’est pas encore connecté. Les messages apparaîtront ici une fois la connexion établie.</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>Re: %1</source>
        <translation>Re : %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>

On %1, %2 wrote:
%3</source>
        <translation>

Le %1, %2 a écrit :
%3</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Fwd: %1</source>
        <translation>Tr : %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>

---------- Forwarded message ----------
From: %1
To: %2
Subject: %3

%4</source>
        <translation>

---------- Message transféré ----------
De : %1
À : %2
Objet : %3

%4</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>People you mail or pay appear here.</source>
        <translation>Les personnes à qui vous écrivez ou payez apparaîtront ici.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Write to %1</source>
        <translation>Écrire à %1</translation>
    </message>
    <message>
        <location line="-433"/>
        <source>Archiving…</source>
        <translation>Archivage…</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Moving messages…</source>
        <translation>Déplacement des messages…</translation>
    </message>
    <message>
        <location line="+14"/>
        <source> · %1 could not be moved. Try again.</source>
        <translation> · %1 n’ont pas pu être déplacés. Réessayez.</translation>
    </message>
</context>
<context>
    <name>HomePage</name>
    <message>
        <location filename="../pages/homepage.cpp" line="+219"/>
        <source>Your CYBOU Identity</source>
        <translation>Votre identité CYBOU</translation>
    </message>
    <message>
        <source>Protected</source>
        <translation type="vanished">Protégé</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Identity &amp;&amp; Security</source>
        <translation>Identité et sécurité</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Mail</source>
        <translation>Courrier</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+373"/>
        <source>Compose</source>
        <translation>Rédiger</translation>
    </message>
    <message>
        <location line="-372"/>
        <source>Files</source>
        <translation>Fichiers</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+374"/>
        <source>Upload</source>
        <translation>Téléverser</translation>
    </message>
    <message>
        <location line="-373"/>
        <location line="+356"/>
        <source>Wallet</source>
        <translation>Portefeuille</translation>
    </message>
    <message>
        <location line="-355"/>
        <source>Send</source>
        <translation>Envoyer</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Next steps</source>
        <translation>Prochaines étapes</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Hide</source>
        <translation>Masquer</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>Recent activity</source>
        <translation>Activité récente</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>Nothing yet. Your mail, files and payments will appear here.</source>
        <translation>Rien pour le moment. Vos courriers, fichiers et paiements apparaîtront ici.</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>Needs attention</source>
        <translation>À vérifier</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Mail and Files are still being restored. They appear as they are verified.</source>
        <translation>La restauration des courriers et fichiers est en cours. Ils apparaissent au fur et à mesure de leur vérification.</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+12"/>
        <source>Not connected yet</source>
        <translation>Pas encore connecté</translation>
    </message>
    <message>
        <location line="-11"/>
        <source>%1 unread</source>
        <translation>%1 non lu(s)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>No unread mail</source>
        <translation>Aucun courrier non lu</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>1 draft</source>
        <translation>1 brouillon</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 drafts</source>
        <translation>%1 brouillons</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Inbox</source>
        <translation>Boîte de réception</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>1 file</source>
        <translation>1 fichier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 files</source>
        <translation>%1 fichiers</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 used</source>
        <translation>%1 utilisé(s)</translation>
    </message>
    <message>
        <location line="+251"/>
        <source>Mail is end-to-end encrypted using hybrid post-quantum cryptography.</source>
        <translation>Les e-mails sont chiffrés de bout en bout avec une cryptographie post-quantique hybride.</translation>
    </message>
    <message>
        <source>Available  ·  %1 in System Balance</source>
        <translation type="vanished">Disponible · %1 dans le solde système</translation>
    </message>
    <message>
        <source>Available</source>
        <translation type="vanished">Disponible</translation>
    </message>
    <message>
        <source>System Balance %1  ·  %2</source>
        <translation type="vanished">Solde système %1  ·  %2</translation>
    </message>
    <message>
        <location line="-227"/>
        <source>Today</source>
        <translation>Aujourd’hui</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Yesterday</source>
        <translation>Hier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Earlier</source>
        <translation>Plus ancien</translation>
    </message>
    <message>
        <location line="+91"/>
        <location line="+5"/>
        <location line="+108"/>
        <source>Check your recovery phrase</source>
        <translation>Vérifier votre phrase de récupération</translation>
    </message>
    <message>
        <location line="-107"/>
        <source>Take out your written 24 words. CYBOU asks for three of them to confirm your copy still restores this Identity. The words are never shown here.</source>
        <translation>Prenez votre copie des 24 mots. CYBOU vous en demande trois pour vérifier qu’elle restaure toujours cette identité. Les mots ne sont jamais affichés ici.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Vault password</source>
        <translation>Mot de passe du coffre</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Word #%1</source>
        <translation>Mot n° %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+83"/>
        <source>Check</source>
        <translation>Vérifier</translation>
    </message>
    <message>
        <location line="-76"/>
        <source>Checking…</source>
        <translation>Vérification…</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>The vault password is incorrect.</source>
        <translation>Le mot de passe du coffre est incorrect.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>These words do not match. Check your written copy; if it is lost, show the phrase in Identity &amp; Security and write it down again.</source>
        <translation>Ces mots ne correspondent pas. Vérifiez votre copie papier ; si elle est perdue, affichez la phrase dans Identité et sécurité et notez-la à nouveau.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Recovery phrase checked. Keep your copy safe.</source>
        <translation>Phrase de récupération vérifiée. Gardez votre copie en lieu sûr.</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>Your 24 words are the only way back if this computer is lost. Confirm your copy.</source>
        <translation>Vos 24 mots sont le seul moyen de récupérer votre identité en cas de perte de cet ordinateur. Vérifiez votre copie.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>System Balance is running low</source>
        <translation>Le solde système est presque épuisé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>It covers about %1 more network operations for Mail, Files and payments.</source>
        <translation>Il couvre encore environ %1 opérations réseau pour le courrier, les fichiers et les paiements.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Claiming your .cybou name</source>
        <translation>Réservation de votre nom .cybou</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Waiting for the network to confirm it.</source>
        <translation>En attente de confirmation du réseau.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Details</source>
        <translation>Détails</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Claim your .cybou name</source>
        <translation>Réserver votre nom .cybou</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>People reach you as name.cybou instead of a long ID.</source>
        <translation>Les autres peuvent vous contacter à nom.cybou plutôt qu’avec un identifiant long.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Claim</source>
        <translation>Réserver</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Send your first message</source>
        <translation>Envoyer votre premier message</translation>
    </message>
    <message>
        <source>Mail is end-to-end encrypted and post-quantum protected.</source>
        <translation type="vanished">Les courriers sont chiffrés de bout en bout et protégés contre les attaques quantiques.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Upload your first file</source>
        <translation>Téléverser votre premier fichier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Files are encrypted on this computer before they leave it.</source>
        <translation>Les fichiers sont chiffrés sur cet ordinateur avant d’en sortir.</translation>
    </message>
    <message>
        <location line="-251"/>
        <source>System Balance %1</source>
        <translation>Solde système %1</translation>
    </message>
    <message>
        <location line="-142"/>
        <location line="+113"/>
        <source>Active</source>
        <translation type="unfinished">Actif</translation>
    </message>
    <message>
        <location line="-52"/>
        <source>Refresh</source>
        <translation>Actualiser</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Refresh local activity</source>
        <translation>Actualiser l’activité locale</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Read current local Mail and Files indexes. This does not force network sync or storage audits.</source>
        <translation>Relire les index locaux du courrier et des fichiers. Cette action ne force ni la synchronisation réseau ni les audits de stockage.</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+40"/>
        <source>Current local activity</source>
        <translation>Activité locale actuelle</translation>
    </message>
    <message>
        <location line="-34"/>
        <source>Local refresh is unavailable right now.</source>
        <translation>L’actualisation locale est indisponible pour le moment.</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Refreshing local activity…</source>
        <translation>Actualisation de l’activité locale…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Refresh failed. Try again.</source>
        <translation>L’actualisation a échoué. Réessayez.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Local view refreshed at %1</source>
        <translation>Vue locale actualisée le %1</translation>
    </message>
</context>
<context>
    <name>IdentityPage</name>
    <message>
        <location filename="../pages/identitypage.cpp" line="+96"/>
        <location line="+5"/>
        <source>Show recovery phrase</source>
        <translation>Afficher la phrase de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Anyone who sees your recovery phrase can take over your Identity, Mail, Files, Names and Wallet. Make sure nobody is watching your screen and nothing is recording it.</source>
        <translation>Toute personne qui voit votre phrase de récupération peut prendre le contrôle de votre identité, de vos courriers, fichiers, noms et portefeuille. Vérifiez que personne ne regarde votre écran et qu’aucun enregistrement n’est en cours.</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+307"/>
        <location line="+38"/>
        <source>Vault password</source>
        <translation>Mot de passe du coffre</translation>
    </message>
    <message>
        <location line="-343"/>
        <source>I understand and want to show my recovery phrase</source>
        <translation>Je comprends et souhaite afficher ma phrase de récupération</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Reveal</source>
        <translation>Afficher</translation>
    </message>
    <message>
        <location line="+42"/>
        <location line="+299"/>
        <source>Name not claimed</source>
        <translation>Nom non réservé</translation>
    </message>
    <message>
        <location line="-298"/>
        <source>The name could not be claimed.</source>
        <translation>Le nom n’a pas pu être réservé.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Recovery phrase replaced</source>
        <translation>Phrase de récupération remplacée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Your new recovery phrase is now active. The old phrase no longer restores this Identity.</source>
        <translation>Votre nouvelle phrase de récupération est active. L’ancienne ne permet plus de restaurer cette identité.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Waiting for network confirmation</source>
        <translation>En attente de confirmation du réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>The new recovery phrase becomes active after network confirmation. Keep both phrases until then.</source>
        <translation>La nouvelle phrase de récupération sera activée après confirmation du réseau. Gardez les deux phrases jusque-là.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Recovery phrase not replaced</source>
        <translation>Phrase de récupération non remplacée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Your current recovery phrase is still active.</source>
        <translation>Votre phrase de récupération actuelle reste active.</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>No Identity on this computer yet</source>
        <translation>Aucune identité sur cet ordinateur</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Create a new Identity, or restore yours with its 24-word recovery phrase.</source>
        <translation>Créez une identité ou restaurez la vôtre avec sa phrase de récupération de 24 mots.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Create Identity</source>
        <translation>Créer une identité</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Restore from recovery phrase</source>
        <translation>Restaurer à partir de la phrase de récupération</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>Lock vault</source>
        <translation>Verrouiller le coffre</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Account</source>
        <translation>Compte</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy</source>
        <translation>Copier</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+130"/>
        <source>Account ID</source>
        <translation>ID du compte</translation>
    </message>
    <message>
        <location line="-129"/>
        <source>Balance</source>
        <translation>Solde</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>System Balance</source>
        <translation>Solde système</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Your 24-word recovery phrase restores this Identity on another device. Phrase presence in this vault is not a substitute for a tested restore.</source>
        <translation>Votre phrase de récupération de 24 mots restaure cette Identité sur un autre appareil. La présence de la phrase dans ce coffre-fort ne remplace pas un test de restauration réel.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Post-quantum encryption</source>
        <translation>Chiffrement post-quantique</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Hybrid ML-KEM-768 with X25519 for messaging and storage capsules. Quantum-resistant against future decrypt-later attacks.</source>
        <translation>Hybride ML-KEM-768 avec X25519 pour la messagerie et les capsules de stockage. Résistant aux futures attaques quantiques par déchiffrement différé.</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>Configured in vault</source>
        <translation>Configurée dans le coffre-fort</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Hybrid ML-KEM active</source>
        <translation>ML-KEM hybride actif</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>Ed25519 + ML-DSA-65 · configured</source>
        <translation>Ed25519 + ML-DSA-65 · configuré</translation>
    </message>
    <message>
        <source>Authority above 10,000,000 AUTH enables Validation. Current limits are shown in Wallet.</source>
        <translation type="vanished">Une Autorité supérieure à 10 000 000 AUTH permet la Validation. Les limites actuelles sont affichées dans Portefeuille.</translation>
    </message>
    <message>
        <location line="-130"/>
        <source>Share your Account ID only if someone cannot find your .cybou name.</source>
        <translation>Ne partagez votre ID de compte que si quelqu’un ne trouve pas votre nom .cybou.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>CYBOU names</source>
        <translation>Noms CYBOU</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+186"/>
        <source>Claim CYBOU name</source>
        <translation>Réserver un nom CYBOU</translation>
    </message>
    <message>
        <location line="-180"/>
        <location line="+113"/>
        <source>Recovery</source>
        <translation>Récupération</translation>
    </message>
    <message>
        <location line="-112"/>
        <source>Recovery phrase</source>
        <translation>Phrase de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Local vault</source>
        <translation>Coffre local</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Show recovery phrase…</source>
        <translation>Afficher la phrase de récupération…</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Security</source>
        <translation>Sécurité</translation>
    </message>
    <message>
        <source>Post-quantum protection</source>
        <translation type="vanished">Protection post-quantique</translation>
    </message>
    <message>
        <source>Identity Authority</source>
        <translation type="vanished">Autorité de l’identité</translation>
    </message>
    <message>
        <source>Derived from finalized history.</source>
        <translation type="vanished">Calculée à partir de l’historique finalisé.</translation>
    </message>
    <message>
        <source>Authority</source>
        <translation type="vanished">Autorité</translation>
    </message>
    <message>
        <source>Validation qualification</source>
        <translation type="vanished">Qualification de validation</translation>
    </message>
    <message>
        <source>Authority is informational. It does not control network limits or PoA finality.</source>
        <translation type="vanished">L’autorité est fournie à titre indicatif. Elle ne détermine ni les limites du réseau ni la finalité PoA.</translation>
    </message>
    <message>
        <source>View details</source>
        <translation type="vanished">Afficher les détails</translation>
    </message>
    <message>
        <source>Hide details</source>
        <translation type="vanished">Masquer les détails</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Danger zone</source>
        <translation>Zone sensible</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Replacing your recovery phrase replaces all Identity keys. The old phrase stops working after network confirmation.</source>
        <translation>Le remplacement de votre phrase de récupération renouvelle toutes les clés de l’identité. L’ancienne phrase cessera de fonctionner après confirmation du réseau.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Replace recovery phrase…</source>
        <translation>Remplacer la phrase de récupération…</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Advanced security details</source>
        <translation>Détails de sécurité avancés</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>Your CYBOU Identity</source>
        <translation>Votre identité CYBOU</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>No CYBOU name yet</source>
        <translation>Aucun nom CYBOU pour le moment</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Verified CYBOU name</source>
        <translation>Nom CYBOU vérifié</translation>
    </message>
    <message>
        <source>Secured</source>
        <translation type="vanished">Sécurisé</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Locked</source>
        <translation>Verrouillé</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Unlocked</source>
        <translation>Déverrouillé</translation>
    </message>
    <message>
        <source>Active</source>
        <translation type="vanished">Actif</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Primary</source>
        <translation>Principal</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Claim a .cybou name so people can reach you as name.cybou.</source>
        <translation>Réservez un nom .cybou pour être joignable à nom.cybou.</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Key epoch</source>
        <translation>Époque de clé</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+4"/>
        <source>Not reported yet</source>
        <translation>Pas encore indiqué</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>Authorization</source>
        <translation>Autorisation</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Ed25519 + ML-DSA-44 · valid</source>
        <translation>Ed25519 + ML-DSA-44 · valide</translation>
    </message>
    <message>
        <source>Ed25519 + ML-DSA-65 · secured</source>
        <translation type="vanished">Ed25519 + ML-DSA-65 · sécurisé</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Key encapsulation</source>
        <translation>Encapsulation de clé</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Hybrid post-quantum KEM · published</source>
        <translation>KEM hybride post-quantique · publiée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Created at finalized height</source>
        <translation>Créé à la hauteur finalisée</translation>
    </message>
    <message>
        <source>Validator-qualified</source>
        <translation type="vanished">Qualifié comme validateur</translation>
    </message>
    <message>
        <source>Below %1</source>
        <translation type="vanished">Inférieur à %1</translation>
    </message>
    <message>
        <source>Age</source>
        <translation type="vanished">Ancienneté</translation>
    </message>
    <message>
        <source>Activity</source>
        <translation type="vanished">Activité</translation>
    </message>
    <message>
        <source>System contribution</source>
        <translation type="vanished">Contribution système</translation>
    </message>
    <message>
        <source>Authority age</source>
        <translation type="vanished">Ancienneté de l’autorité</translation>
    </message>
    <message>
        <source>Qualifying activity</source>
        <translation type="vanished">Activité qualifiante</translation>
    </message>
    <message>
        <source>System Balance contribution</source>
        <translation type="vanished">Contribution au solde système</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Account ID copied</source>
        <translation>ID de compte copié</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Recovery phrase not shown</source>
        <translation>Phrase de récupération non affichée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>The password is incorrect, or this vault does not store the recovery words.</source>
        <translation>Le mot de passe est incorrect ou ce coffre ne contient pas les mots de récupération.</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Replace recovery phrase</source>
        <translation>Remplacer la phrase de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>CYBOU will create a new recovery phrase and replace all Identity keys. The old phrase stops working after network confirmation. Continue?</source>
        <translation>CYBOU créera une nouvelle phrase de récupération et renouvellera toutes les clés de l’identité. L’ancienne phrase cessera de fonctionner après confirmation du réseau. Continuer ?</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+38"/>
        <source>Confirm with your vault password</source>
        <translation>Confirmez avec le mot de passe du coffre</translation>
    </message>
    <message>
        <location line="-33"/>
        <source>Incorrect password</source>
        <translation>Mot de passe incorrect</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>The vault password is incorrect.</source>
        <translation>Le mot de passe du coffre est incorrect.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Cannot create a recovery phrase</source>
        <translation>Impossible de créer une phrase de récupération</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Secure randomness is unavailable.</source>
        <translation>La génération aléatoire sécurisée est indisponible.</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Choose your name (5–32 characters). You will be reachable as name.cybou.</source>
        <translation>Choisissez votre nom (5 à 32 caractères). Vous serez joignable à nom.cybou.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Choose another name</source>
        <translation>Choisir un autre nom</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>A name claim is already in progress.</source>
        <translation>Une réservation de nom est déjà en cours.</translation>
    </message>
</context>
<context>
    <name>MailCompose</name>
    <message>
        <location filename="../pages/mailcompose.cpp" line="+160"/>
        <source>From this computer</source>
        <translation>Depuis cet ordinateur</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>From CYBOU Files</source>
        <translation>Depuis les fichiers CYBOU</translation>
    </message>
    <message>
        <location line="+139"/>
        <source>Attach from CYBOU Files</source>
        <translation>Joindre un fichier CYBOU</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Choose protected files. Their encrypted content is reused without uploading it again.</source>
        <translation>Choisissez des fichiers protégés. Leur contenu chiffré est réutilisé sans nouvel envoi.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>File</source>
        <translation>Fichier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Size</source>
        <translation>Taille</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>State</source>
        <translation>État</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Not protected yet</source>
        <translation>Pas encore protégé</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>There are no files to attach in CYBOU Files.</source>
        <translation>Aucun fichier à joindre dans les fichiers CYBOU.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Attach</source>
        <translation>Joindre</translation>
    </message>
    <message>
        <location line="+127"/>
        <source>The Identity is checked when you send.</source>
        <translation>L’identité sera vérifiée lors de l’envoi.</translation>
    </message>
    <message>
        <location line="-413"/>
        <location line="+11"/>
        <source>New message</source>
        <translation>Nouveau message</translation>
    </message>
    <message>
        <location line="-4"/>
        <source>Back to list</source>
        <translation>Retour à la liste</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+1"/>
        <source>Close and keep the draft</source>
        <translation>Fermer et conserver le brouillon</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+4"/>
        <source>To</source>
        <translation>À</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>name.cybou</source>
        <translation>nom.cybou</translation>
    </message>
    <message>
        <location line="+25"/>
        <location line="+3"/>
        <source>Subject</source>
        <translation>Objet</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Message</source>
        <translation>Message</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Write your message</source>
        <translation>Écrivez votre message</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Drop files to attach them</source>
        <translation>Déposez des fichiers pour les joindre</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Send</source>
        <translation>Envoyer</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Send (Ctrl+Enter)</source>
        <translation>Envoyer (Ctrl+Entrée)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Attach file</source>
        <translation>Joindre un fichier</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Attach files</source>
        <translation>Joindre des fichiers</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Discard</source>
        <translation>Supprimer</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Discard this draft</source>
        <translation>Supprimer ce brouillon</translation>
    </message>
    <message>
        <location line="+57"/>
        <source>  ·  Verified identity</source>
        <translation> · identité vérifiée</translation>
    </message>
    <message>
        <location line="+103"/>
        <location line="+53"/>
        <source>Protected</source>
        <translation>Protégé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Received</source>
        <translation>Reçu</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>On this device</source>
        <translation>Sur cet appareil</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+1"/>
        <source>Remove %1</source>
        <translation>Retirer %1</translation>
    </message>
    <message>
        <location line="+51"/>
        <source>Add a recipient.</source>
        <translation>Ajoutez un destinataire.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Send to one recipient at a time.</source>
        <translation>Envoyez à un seul destinataire à la fois.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Use a CYBOU name, for example alice.cybou.</source>
        <translation>Utilisez un nom CYBOU, par exemple alice.cybou.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>CYBOU Support  ·  a message here costs about %1 from System Balance (support rate)</source>
        <translation>Assistance CYBOU · un message coûte environ %1 du solde système (tarif assistance)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>CYBOU Support  ·  messages here cost more than usual (support rate)</source>
        <translation>Assistance CYBOU · ces messages coûtent plus cher que d’habitude (tarif assistance)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1  ·  Verified identity</source>
        <translation>%1 · identité vérifiée</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>The name is checked when you send.</source>
        <translation>Le nom sera vérifié lors de l’envoi.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Mail needs your CYBOU Identity.</source>
        <translation>Le courrier nécessite votre identité CYBOU.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>This feature is not connected yet.</source>
        <translation>Cette fonctionnalité n’est pas encore connectée.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Write a message.</source>
        <translation>Écrivez un message.</translation>
    </message>
    <message>
        <source>This message has not been sent. It stays in Drafts.</source>
        <translation type="vanished">Ce message n’a pas été envoyé. Il reste dans les brouillons.</translation>
    </message>
    <message>
        <source>Draft saved</source>
        <translation type="vanished">Brouillon enregistré</translation>
    </message>
    <message>
        <source>Draft discarded</source>
        <translation type="vanished">Brouillon supprimé</translation>
    </message>
    <message>
        <location line="+95"/>
        <source>Draft not saved. Mail is unavailable; your text is kept here.</source>
        <translation>Brouillon non enregistré. Le courrier est indisponible ; votre texte reste ici.</translation>
    </message>
    <message>
        <location line="-9"/>
        <source>Draft saved on this computer</source>
        <translation>Brouillon enregistré sur cet ordinateur</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Preparing message. Your draft is kept until it is saved.</source>
        <translation>Préparation du message. Votre brouillon est conservé jusqu’à l’enregistrement.</translation>
    </message>
    <message>
        <location line="-34"/>
        <source>Save failed. Your text is kept here; close again to retry.</source>
        <translation>Échec de l’enregistrement. Votre texte reste ici ; fermez à nouveau pour réessayer.</translation>
    </message>
    <message>
        <location line="-8"/>
        <source>Saving draft…</source>
        <translation>Enregistrement du brouillon…</translation>
    </message>
    <message>
        <location line="+60"/>
        <source>This message has not been sent. Your text is kept here.</source>
        <translation>Ce message n’a pas été envoyé. Votre texte reste ici.</translation>
    </message>
    <message>
        <location line="-95"/>
        <source>Unsaved changes</source>
        <translation>Modifications non enregistrées</translation>
    </message>
</context>
<context>
    <name>MailReader</name>
    <message>
        <location filename="../pages/mailreader.cpp" line="+43"/>
        <source>Me</source>
        <translation>Moi</translation>
    </message>
    <message>
        <location line="+58"/>
        <source>Message</source>
        <translation>Message</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Back</source>
        <translation>Retour</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Back to list (Esc)</source>
        <translation>Retour à la liste (Échap)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Back to list</source>
        <translation>Retour à la liste</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+225"/>
        <source>Archive</source>
        <translation>Archiver</translation>
    </message>
    <message>
        <location line="-223"/>
        <source>Move to Trash</source>
        <translation>Déplacer dans la corbeille</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Star</source>
        <translation>Ajouter aux favoris</translation>
    </message>
    <message>
        <location line="+182"/>
        <source>Conversation archived</source>
        <translation>Conversation archivée</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Moved to Inbox</source>
        <translation>Déplacé dans la boîte de réception</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Undo</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>Moved to Trash</source>
        <translation>Déplacé dans la corbeille</translation>
    </message>
    <message>
        <location line="-127"/>
        <source>Details</source>
        <translation>Détails</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+1"/>
        <location line="+311"/>
        <location line="+5"/>
        <source>Security details</source>
        <translation>Détails de sécurité</translation>
    </message>
    <message>
        <location line="-298"/>
        <source>Retry</source>
        <translation>Réessayer</translation>
    </message>
    <message>
        <location line="+43"/>
        <source>Reply</source>
        <translation>Répondre</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Forward</source>
        <translation>Transférer</translation>
    </message>
    <message>
        <location line="+88"/>
        <source>(no subject)</source>
        <translation>(sans objet)</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>To: %1</source>
        <translation>À : %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Move to Inbox</source>
        <translation>Déplacer dans la boîte de réception</translation>
    </message>
    <message>
        <source>Protected end to end  •  Post-quantum protected  •  Network confirmed</source>
        <translation type="vanished">Protégé de bout en bout · protection post-quantique · confirmé par le réseau</translation>
    </message>
    <message>
        <source>Protected end to end  •  Post-quantum protected  •  Stored on the network</source>
        <translation type="vanished">Protégé de bout en bout · protection post-quantique · stocké sur le réseau</translation>
    </message>
    <message>
        <source>Protected end to end  •  %1</source>
        <translation type="vanished">Protégé de bout en bout · %1</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>Waiting for network. Your message is saved and will be sent when CYBOU reconnects.</source>
        <translation>En attente du réseau. Votre message est enregistré et sera envoyé lorsque CYBOU se reconnectera.</translation>
    </message>
    <message>
        <source>Validated. The network has checked your message; it becomes final once confirmed.</source>
        <translation type="vanished">Validé. Le réseau a vérifié votre message ; il sera final après confirmation.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Waiting for confirmation… The network is confirming your message.</source>
        <translation>En attente de confirmation… Le réseau confirme votre message.</translation>
    </message>
    <message>
        <source>Validated… Network validators checked your message; waiting for final confirmation.</source>
        <translation type="vanished">Validé… Les validateurs du réseau ont vérifié votre message ; en attente de la confirmation finale.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Preparing… Your message is being encrypted on this computer.</source>
        <translation>Préparation… Votre message est chiffré sur cet ordinateur.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Securing… Confirmed by the network. Keep CYBOU open until your message is stored securely.</source>
        <translation>Protection en cours… Confirmé par le réseau. Laissez CYBOU ouvert jusqu’à ce que votre message soit stocké en toute sécurité.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Needs attention. Your message could not be secured yet. It is kept on this computer.</source>
        <translation>Action requise. Votre message n’a pas encore pu être protégé. Il reste sur cet ordinateur.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Temporarily unavailable — retrying.</source>
        <translation>Temporairement indisponible — nouvelle tentative.</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>Attachments</source>
        <translation>Pièces jointes</translation>
    </message>
    <message>
        <location line="+18"/>
        <location line="+22"/>
        <location line="+21"/>
        <source>Saved to Files</source>
        <translation>Enregistré dans Fichiers</translation>
    </message>
    <message>
        <location line="-34"/>
        <source>Download</source>
        <translation>Télécharger</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>More actions</source>
        <translation>Autres actions</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>More actions for %1</source>
        <translation>Autres actions pour %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Save to Files</source>
        <translation>Enregistrer dans Fichiers</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Download attachment</source>
        <translation>Télécharger la pièce jointe</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>Sender identity</source>
        <translation>Identité de l’expéditeur</translation>
    </message>
    <message>
        <source>Verified</source>
        <translation type="vanished">Vérifié</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Identity authorization</source>
        <translation>Autorisation de l’identité</translation>
    </message>
    <message>
        <source>Valid</source>
        <translation type="vanished">Valide</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network confirmation</source>
        <translation>Confirmation du réseau</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Finalized</source>
        <translation>Finalisé</translation>
    </message>
    <message>
        <source>Validated (not final yet)</source>
        <translation type="vanished">Validé (pas encore final)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Waiting</source>
        <translation>En attente</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Content protection</source>
        <translation>Protection du contenu</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Content availability</source>
        <translation>Disponibilité du contenu</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Post-quantum authorization</source>
        <translation>Autorisation post-quantique</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Ed25519 + ML-DSA</source>
        <translation>Ed25519 + ML-DSA</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Post-quantum key encapsulation</source>
        <translation>Encapsulation de clé post-quantique</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>X25519 + ML-KEM</source>
        <translation>X25519 + ML-KEM</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>ADVANCED</source>
        <translation>AVANCÉ</translation>
    </message>
    <message>
        <location line="-7"/>
        <location line="+8"/>
        <source>Not reported yet</source>
        <translation>Pas encore indiqué</translation>
    </message>
    <message>
        <location line="-161"/>
        <source>End-to-end encrypted  •  Hybrid post-quantum  •  Network confirmed</source>
        <translation>Chiffré de bout en bout  •  Post-quantique hybride  •  Confirmé par le réseau</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>End-to-end encrypted  •  Hybrid post-quantum  •  Stored on the network</source>
        <translation>Chiffré de bout en bout  •  Post-quantique hybride  •  Stocké sur le réseau</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>End-to-end encrypted  •  %1</source>
        <translation>Chiffré de bout en bout  •  %1</translation>
    </message>
    <message>
        <location line="+158"/>
        <source>Finalized height</source>
        <translation>Hauteur finalisée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Operation ID</source>
        <translation>ID de l’opération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Root content ID</source>
        <translation>ID de la racine du contenu</translation>
    </message>
    <message>
        <location line="-12"/>
        <location line="+3"/>
        <source>No separate verification result reported</source>
        <translation>Aucun résultat de vérification distinct signalé</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>Included in a finalized operation</source>
        <translation>Inclus dans une opération finalisée</translation>
    </message>
    <message>
        <location line="-204"/>
        <source>Archiving…</source>
        <translation>Archivage…</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Moving message…</source>
        <translation>Déplacement du message…</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Moved to Sent</source>
        <translation>Déplacé dans les messages envoyés</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>Move to Sent</source>
        <translation>Déplacer dans les messages envoyés</translation>
    </message>
</context>
<context>
    <name>NetworkAuthorityPage</name>
    <message>
        <location filename="../pages/networkauthoritypage.cpp" line="+175"/>
        <source>Finalized height</source>
        <translation>Hauteur finalisée</translation>
    </message>
    <message>
        <source>Height change observed</source>
        <translation type="vanished">Changement de hauteur observé</translation>
    </message>
    <message>
        <location line="+169"/>
        <source>Safety halt</source>
        <translation>Arrêt de sécurité</translation>
    </message>
    <message>
        <location line="-165"/>
        <source>Identities</source>
        <translation>Identités</translation>
    </message>
    <message>
        <location line="-41"/>
        <source>PoA finalizer</source>
        <translation>Finaliseur PoA</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Finalize one block</source>
        <translation>Finaliser un bloc</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Available while finalization is paused</source>
        <translation>Disponible lorsque la finalisation est en pause</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>Last new block</source>
        <translation>Dernier nouveau bloc</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+282"/>
        <source>Waiting for next block</source>
        <translation>En attente du prochain bloc</translation>
    </message>
    <message>
        <source>Validators (AUTH &gt; 10M)</source>
        <translation type="vanished">Validateurs (AUTH &gt; 10M)</translation>
    </message>
    <message>
        <location line="-210"/>
        <source>Candidate operations</source>
        <translation>Opérations candidates</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Operations this node executed against its finalized state. Each is executed again before signing.</source>
        <translation>Opérations exécutées par ce nœud sur son état finalisé. Chacune est réexécutée avant signature.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Recently finalized</source>
        <translation>Récemment finalisées</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Operations this node saw finalized recently, newest first.</source>
        <translation>Opérations que ce nœud a vues finalisées récemment, les plus récentes d’abord.</translation>
    </message>
    <message>
        <source>Adjust Authority</source>
        <translation type="vanished">Ajuster l’Autorité</translation>
    </message>
    <message>
        <source>Signs a PoaAuthAdjustment for the next block. GRANT adds AUTH; BURN removes it (never below 0). AUTH cannot be transferred and grants no finalization power.</source>
        <translation type="vanished">Signe un PoaAuthAdjustment pour le prochain bloc. GRANT ajoute de l’AUTH ; BURN en retire (jamais sous 0). L’AUTH n’est pas transférable et ne confère aucun pouvoir de finalisation.</translation>
    </message>
    <message>
        <source>name.cybou or Account ID</source>
        <translation type="vanished">nom.cybou ou identifiant de compte</translation>
    </message>
    <message>
        <source>Identity</source>
        <translation type="vanished">Identité</translation>
    </message>
    <message>
        <source>AUTH</source>
        <translation type="vanished">AUTH</translation>
    </message>
    <message>
        <source>AUTH amount</source>
        <translation type="vanished">Montant d’AUTH</translation>
    </message>
    <message>
        <source>Grant</source>
        <translation type="vanished">Accorder</translation>
    </message>
    <message>
        <source>Burn</source>
        <translation type="vanished">Retirer</translation>
    </message>
    <message>
        <source>Network totals</source>
        <translation type="vanished">Totaux du réseau</translation>
    </message>
    <message>
        <location line="-68"/>
        <source>Verified explorer</source>
        <translation>Explorateur vérifié</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Paginated in-memory index of candidate operations and verified blocks. 10 items per page.</source>
        <translation>Index paginé en mémoire des opérations candidates et des blocs vérifiés. 10 éléments par page.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Filter by operation ID, block height, or status…</source>
        <translation>Filtrer par identifiant d&apos;opération, hauteur de bloc ou statut…</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+362"/>
        <source>Phase / Height</source>
        <translation>Phase / Hauteur</translation>
    </message>
    <message>
        <location line="-361"/>
        <location line="+358"/>
        <source>Identifier</source>
        <translation>Identifiant</translation>
    </message>
    <message>
        <location line="-357"/>
        <location line="+361"/>
        <source>Classification</source>
        <translation>Classification</translation>
    </message>
    <message>
        <location line="-360"/>
        <location line="+361"/>
        <source>Status</source>
        <translation>Statut</translation>
    </message>
    <message>
        <location line="-346"/>
        <source>Previous</source>
        <translation>Précédent</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Next</source>
        <translation>Suivant</translation>
    </message>
    <message>
        <location line="+15"/>
        <location line="+316"/>
        <source>Selected item details</source>
        <translation>Détails de l&apos;élément sélectionné</translation>
    </message>
    <message>
        <location line="-312"/>
        <source>Off-chain evidence &amp; signer safety</source>
        <translation>Preuves hors chaîne et sécurité du signataire</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Safety journals, settlement readiness, and operational signer status.</source>
        <translation>Journaux de sécurité, état des règlements et statut opérationnel du signataire.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Signing safety</source>
        <translation>Sécurité de signature</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Settlement readiness</source>
        <translation>Prêt pour règlement</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Signer authority</source>
        <translation>Autorité du signataire</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Canonical state root commitments</source>
        <translation>Engagements canoniques de la racine d&apos;état</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Account and storage values cryptographically committed by the latest state root.</source>
        <translation>Valeurs de compte et de stockage cryptographiquement engagées par la dernière racine d&apos;état.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Peer heights are their own announcements, not verified state.</source>
        <translation>Les hauteurs des pairs sont leurs propres annonces, pas un état vérifié.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Chain</source>
        <translation>Chaîne</translation>
    </message>
    <message>
        <location line="+68"/>
        <source>Finalizing</source>
        <translation>Finalisation en cours</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Valid candidates are executed and signed into a block as soon as one is waiting.</source>
        <translation>Les candidats valides sont exécutés et signés dans un bloc dès qu’il y en a un en attente.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Paused</source>
        <translation>En pause</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>No blocks are produced. Candidates wait in the pool; finalize one block on demand or resume.</source>
        <translation>Aucun bloc n’est produit. Les candidats attendent dans le pool ; finalisez un bloc à la demande ou reprenez.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Signing safety stopped finalization fail-closed. Inspect the signing journal and evidence before any further signing.</source>
        <translation>La sécurité de signature a arrêté la finalisation (fail-closed). Examinez le journal de signature et les preuves avant toute nouvelle signature.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Signer unavailable</source>
        <translation>Signataire indisponible</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The PoA signer is not active. Unlock the vault of this Identity to finalize.</source>
        <translation>Le signataire PoA n’est pas actif. Déverrouillez le coffre de cette Identité pour finaliser.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Resume</source>
        <translation>Reprendre</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Pause</source>
        <translation>Pause</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>Candidate (volatile)</source>
        <translation>Candidate (volatile)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Candidate operation</source>
        <translation>Opération candidate</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Block %1</source>
        <translation>Bloc %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Pending</source>
        <translation>En attente</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Verified operation</source>
        <translation>Opération vérifiée</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Finalized</source>
        <translation>Finalisée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Submitted</source>
        <translation>Soumise</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Block %1 (Tip)</source>
        <translation>Bloc %1 (Sommet)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Finalized block</source>
        <translation>Bloc finalisé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Finalized by PoA key</source>
        <translation>Finalisé par la clé PoA</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Page %1 of %2 (%3 items)</source>
        <translation>Page %1 sur %2 (%3 éléments)</translation>
    </message>
    <message>
        <location line="+49"/>
        <source>Volatile candidate in local pool. Executed against finalized state; re-executed before block signing.</source>
        <translation>Candidate volatile dans le pool local. Exécutée contre l&apos;état finalisé ; réexécutée avant signature du bloc.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Committed in finalized PoA block. Cryptographically verified against latest state root.</source>
        <translation>Engagée dans un bloc PoA finalisé. Vérifiée cryptographiquement contre la dernière racine d&apos;état.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Verification notes</source>
        <translation>Notes de vérification</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Select an operation or block in the explorer table to inspect its full un-truncated identifier and verification status.</source>
        <translation>Sélectionnez une opération ou un bloc dans la table de l&apos;explorateur pour inspecter son identifiant complet non tronqué et son statut de vérification.</translation>
    </message>
    <message>
        <location line="-262"/>
        <source>None in this view</source>
        <translation>Aucun dans cette vue</translation>
    </message>
    <message>
        <source>No candidate operations. The pool is empty.</source>
        <translation type="vanished">Aucune opération candidate. Le pool est vide.</translation>
    </message>
    <message>
        <location line="+79"/>
        <source>and %1 more</source>
        <translation>et %1 de plus</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>waiting for the next block</source>
        <translation>en attente du prochain bloc</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>block %1</source>
        <translation>bloc %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Nothing finalized in this session yet.</source>
        <translation>Rien de finalisé dans cette session pour l’instant.</translation>
    </message>
    <message>
        <source>base height %1</source>
        <translation type="vanished">hauteur de base %1</translation>
    </message>
    <message>
        <location line="-152"/>
        <source>Spendable Balance</source>
        <translation>Solde disponible</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>System Balance</source>
        <translation>Solde système</translation>
    </message>
    <message>
        <source>Authority</source>
        <translation type="vanished">Autorité</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>.cybou names</source>
        <translation>Noms .cybou</translation>
    </message>
    <message>
        <location line="+111"/>
        <source>Fail-closed durable append-only journal active. Equivocation conflicts resolved by min(BlockID).</source>
        <translation>Journal durable en ajout seul actif (fail-closed). Conflits d&apos;équivocation résolus par min(BlockID).</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Off-chain replica service receipts and audit confirmations tracked. Settlement transactions execute upon period close.</source>
        <translation>Reçus de service des répliques hors chaîne et confirmations d&apos;audit suivis. Les transactions de règlement s&apos;exécutent à la clôture de la période.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Authorized signer: Genesis-authorized PoA signing key is active. Finalization and settlement controls are enabled.</source>
        <translation>Signataire autorisé : La clé de signature PoA autorisée par la genèse est active. Les contrôles de finalisation et de règlement sont activés.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Read-only console: Active signing key is not unlocked or authorized for this network. Finalization and settlement actions are restricted.</source>
        <translation>Console en lecture seule : La clé de signature active n&apos;est pas déverrouillée ou autorisée pour ce réseau. Les actions de finalisation et de règlement sont restreintes.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Idle chain: Pool is empty (0 candidates). Blocks are produced on demand as operations arrive, not on an idle empty-block timer.</source>
        <translation>Chaîne au repos : Le pool est vide (0 candidat). Les blocs sont produits à la demande à l&apos;arrivée des opérations, et non sur un minuteur de blocs vides.</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>%1  ·  %2 commits pending</source>
        <translation>%1  ·  %2 engagements en attente</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>No peers connected.</source>
        <translation>Aucun pair connecté.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>height %1  ·  in step</source>
        <translation>hauteur %1  ·  à jour</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>height %1  ·  %2 behind</source>
        <translation>hauteur %1  ·  %2 de retard</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>  ·  StorageId %1</source>
        <translation>  ·  StorageId %1</translation>
    </message>
    <message>
        <location line="-163"/>
        <source>Connection</source>
        <translation>Connexion</translation>
    </message>
    <message>
        <source>Signing…</source>
        <translation type="vanished">Signature…</translation>
    </message>
    <message>
        <source>Grant AUTH</source>
        <translation type="vanished">Accorder de l’AUTH</translation>
    </message>
    <message>
        <source>Burn AUTH</source>
        <translation type="vanished">Retirer de l’AUTH</translation>
    </message>
    <message>
        <source>Grant %1 to %2?

This is signed with the genesis PoA key and becomes canonical once finalized.</source>
        <translation type="vanished">Accorder %1 à %2 ?

Cette opération est signée avec la clé PoA de la genèse et devient canonique une fois finalisée.</translation>
    </message>
    <message>
        <source>Burn %1 from %2?

AUTH never goes below 0. This is signed with the genesis PoA key and becomes canonical once finalized.</source>
        <translation type="vanished">Retirer %1 à %2 ?

L’AUTH ne descend jamais sous 0. Cette opération est signée avec la clé PoA de la genèse et devient canonique une fois finalisée.</translation>
    </message>
    <message>
        <source>Enter a .cybou name or a 64-character Account ID and a whole AUTH amount.</source>
        <translation type="vanished">Saisissez un nom .cybou ou un identifiant de compte de 64 caractères et un montant entier d’AUTH.</translation>
    </message>
    <message>
        <location line="-91"/>
        <source>Connected peers</source>
        <translation>Pairs connectés</translation>
    </message>
    <message>
        <source>Finality</source>
        <translation type="vanished">Finalité</translation>
    </message>
    <message>
        <source>Supply and pools</source>
        <translation type="vanished">Offre et réserves</translation>
    </message>
    <message>
        <location line="+129"/>
        <source>%1 s ago</source>
        <translation>Il y a %1 s</translation>
    </message>
    <message>
        <source>HALTED</source>
        <translation type="vanished">ARRÊTÉ</translation>
    </message>
    <message>
        <source>No</source>
        <translation type="vanished">Non</translation>
    </message>
    <message>
        <source>Model</source>
        <translation type="vanished">Modèle</translation>
    </message>
    <message>
        <source>Genesis-bound single-operator hybrid-PQ PoA (centralized finality, not BFT)</source>
        <translation type="vanished">PoA hybride post-quantique à opérateur unique, liée au genesis (finalité centralisée, sans tolérance BFT)</translation>
    </message>
    <message>
        <source>Finalizer key</source>
        <translation type="vanished">Clé de finalisation</translation>
    </message>
    <message>
        <source>Matches this Identity&apos;s recovery phrase (proven from genesis)</source>
        <translation type="vanished">Correspond à la phrase de récupération de cette identité (preuve issue du genesis)</translation>
    </message>
    <message>
        <source>Not observed yet</source>
        <translation type="vanished">Pas encore observé</translation>
    </message>
    <message>
        <source>P2P connection</source>
        <translation type="vanished">Connexion P2P</translation>
    </message>
    <message>
        <source>Height tracking</source>
        <translation type="vanished">Suivi de la hauteur</translation>
    </message>
    <message>
        <source>Height changed %1 s ago in this view</source>
        <translation type="vanished">La hauteur a changé il y a %1 s dans cette vue</translation>
    </message>
    <message>
        <source>Waiting to observe a height change in this view</source>
        <translation type="vanished">En attente d’un changement de hauteur dans cette vue</translation>
    </message>
    <message>
        <location line="-41"/>
        <source>Tip</source>
        <translation>Tête</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>State root</source>
        <translation>Racine d’état</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network ID</source>
        <translation>ID du réseau</translation>
    </message>
    <message>
        <source>Pending .cybou name commits</source>
        <translation type="vanished">Réservations de noms .cybou en attente</translation>
    </message>
    <message>
        <source>Spendable Balance (all Identities)</source>
        <translation type="vanished">Solde disponible (toutes identités)</translation>
    </message>
    <message>
        <source>System Balance (all Identities)</source>
        <translation type="vanished">Solde système (toutes identités)</translation>
    </message>
    <message>
        <source>Onboarding pool</source>
        <translation type="vanished">Réserve de bienvenue</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>Peers</source>
        <translation>Pairs</translation>
    </message>
    <message>
        <source>None connected</source>
        <translation type="vanished">Aucun pair connecté</translation>
    </message>
    <message>
        <source>No StorageId verified</source>
        <translation type="vanished">Aucun StorageId vérifié</translation>
    </message>
    <message>
        <source>StorageId verified: %1</source>
        <translation type="vanished">StorageId vérifié : %1</translation>
    </message>
    <message>
        <source>%1  ·  height %2  ·  lag %3</source>
        <translation type="vanished">%1 · hauteur %2 · retard %3</translation>
    </message>
    <message>
        <source>This Identity derives this network&apos;s genesis PoA key. When its vault is unlocked, this desktop can operate the PoA finalizer through a vault-backed signer. This page reports finalized state independently validated by this node.</source>
        <translation type="vanished">Cette identité dérive la clé PoA genesis de ce réseau. Lorsque son coffre est déverrouillé, ce poste peut faire fonctionner le finaliseur PoA au moyen d’un signataire protégé par le coffre. Cette page présente l’état finalisé validé indépendamment par ce nœud.</translation>
    </message>
    <message>
        <source>Local PoA signer</source>
        <translation type="vanished">Signataire PoA local</translation>
    </message>
    <message>
        <source>Enabled in the unlocked vault</source>
        <translation type="vanished">Activé dans le coffre déverrouillé</translation>
    </message>
    <message>
        <source>Not enabled</source>
        <translation type="vanished">Non activé</translation>
    </message>
    <message>
        <location line="-103"/>
        <source>Settle storage period</source>
        <translation>Régler la période de stockage</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Pays verified storage service of the next complete period and returns escrow of ended leases</source>
        <translation>Paie le stockage vérifié de la prochaine période complète et restitue le séquestre des baux terminés</translation>
    </message>
    <message>
        <location line="+17"/>
        <location line="+79"/>
        <source>Storage escrow</source>
        <translation>Séquestre de stockage</translation>
    </message>
</context>
<context>
    <name>NetworkPage</name>
    <message>
        <location filename="../pages/networkpage.cpp" line="+342"/>
        <source>Network Overview</source>
        <translation>Aperçu du réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Observed P2P network connections, local consensus state, and storage capacity.</source>
        <translation>Connexions réseau P2P observées, état du consensus local et capacité de stockage.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Connectivity</source>
        <translation>Connectivité</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Verified height</source>
        <translation>Hauteur vérifiée</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Connected peers</source>
        <translation>Pairs connectés</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Storage capacity (V)</source>
        <translation>Capacité de stockage (V)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Content protection</source>
        <translation>Protection du contenu</translation>
    </message>
    <message>
        <location line="+23"/>
        <location line="+212"/>
        <source>Endpoint</source>
        <translation>Point de terminaison</translation>
    </message>
    <message>
        <location line="-212"/>
        <source>Type</source>
        <translation>Type</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+214"/>
        <source>Advertised height</source>
        <translation>Hauteur annoncée</translation>
    </message>
    <message>
        <location line="-214"/>
        <source>Lag</source>
        <translation>Retard</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+218"/>
        <source>StorageId</source>
        <translation>StorageId</translation>
    </message>
    <message>
        <location line="-199"/>
        <source>Selected Peer Details</source>
        <translation>Détails du pair sélectionné</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>Just now</source>
        <translation>À l’instant</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Source: Local node observations • Sample: Connected peers (%1) • Updated: %2
Schematic illustrative map for observed peer connections. Locations are schematic illustrations, not physical node geolocation or network-wide census.</source>
        <translation>Source : Observations du nœud local • Échantillon : Pairs connectés (%1) • Mis à jour : %2
Carte schématique illustrative des connexions observées. Les emplacements sont des représentations schématiques et ne constituent pas une géolocalisation physique ni un recensement global du réseau.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Online</source>
        <translation>En ligne</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Connecting</source>
        <translation>Connexion…</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Offline</source>
        <translation>Hors ligne</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Locally verified PoA tip</source>
        <translation>Sommet PoA vérifié localement</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Waiting for finality</source>
        <translation>En attente de finalité</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Direct mesh sessions</source>
        <translation>Sessions directes du maillage</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Held for others: %1</source>
        <translation>Stockage pour tiers : %1</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>%1 protected · %2 securing</source>
        <translation>%1 protégés · %2 en sécurisation</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Own encrypted publications</source>
        <translation>Publications chiffrées propres</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Local Network (LAN)</source>
        <translation>Réseau local (LAN)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>LAN / Loopback</source>
        <translation>LAN / Boucle locale</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>France (schematic)</source>
        <translation>France (schématique)</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>%1 blocks</source>
        <translation>%1 blocs</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>0 (in sync)</source>
        <translation>0 (synchronisé)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Pending proof</source>
        <translation>Preuve en attente</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>Select a peer from the list or map to view connection details.</source>
        <translation>Sélectionnez un pair dans la liste ou sur la carte pour voir ses détails.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Classification</source>
        <translation>Classification</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 (unverified announcement)</source>
        <translation>%1 (annonce non vérifiée)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Tip delta</source>
        <translation>Écart de hauteur</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 blocks behind local tip</source>
        <translation>%1 blocs de retard sur le sommet local</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>In sync with local chain</source>
        <translation>Synchronisé avec la chaîne locale</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Pending proof (no on-demand storage relationship)</source>
        <translation>Preuve en attente (aucune relation de stockage à la demande)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Observation</source>
        <translation>Observation</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Direct active P2P mesh session · TLS transport pinned</source>
        <translation>Session maillée P2P active directe · Transport TLS épinglé</translation>
    </message>

    <message><source>Advanced · Diagnostics</source><translation>Avancé · Diagnostics</translation></message>
    <message><source>Close</source><translation>Fermer</translation></message>
    <message><source>Connection</source><translation>Connexion</translation></message>
    <message><source>Connected</source><translation>Connecté</translation></message>
    <message><source>Known · disconnected</source><translation>Connu · déconnecté</translation></message>
    <message><source>Unknown</source><translation>Inconnu</translation></message>
    <message><source>Previously observed in this app session. Current height and reachability unknown.</source><translation>Observé précédemment pendant cette session. Hauteur et disponibilité actuelles inconnues.</translation></message>
    <message><source>%1/%2 protected</source><translation>%1/%2 protégés</translation></message>
    <message><source>%1 securing</source><translation>%1 en sécurisation</translation></message>
    <message><source>Direct active P2P mesh session · TLS</source><translation>Session P2P directe active · TLS</translation></message>
</context>
<context>
    <name>OnboardingView</name>
    <message>
        <location filename="../pages/onboardingview.cpp" line="+88"/>
        <source>No Identity with this recovery phrase exists on the network yet, or this computer has not finished syncing. Wait until CYBOU shows Synced and try again.</source>
        <translation>Aucune identité avec cette phrase de récupération n’existe encore sur le réseau, ou cet ordinateur n’a pas fini de se synchroniser. Attendez que CYBOU affiche Synchronisé, puis réessayez.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>The network did not confirm in time. Your request may still be finalized: keep CYBOU open and try again in a few minutes.</source>
        <translation>Le réseau n’a pas confirmé à temps. Votre demande peut encore être finalisée : gardez CYBOU ouvert et réessayez dans quelques minutes.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>This phrase was replaced by a newer one for this Identity. Use the current recovery phrase.</source>
        <translation>Cette phrase a été remplacée par une plus récente pour cette identité. Utilisez la phrase de récupération actuelle.</translation>
    </message>
    <message>
        <location line="+75"/>
        <source>Restore did not complete. Try again.</source>
        <translation>La restauration n’a pas abouti. Réessayez.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Your Identity could not be created. Please try again.</source>
        <translation>Votre identité n’a pas pu être créée. Veuillez réessayer.</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>Welcome to CYBOU</source>
        <translation>Bienvenue dans CYBOU</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>One identity for your private
Mail, Files, Names and Wallet.</source>
        <translation>Une identité pour vos courriers, fichiers, noms et portefeuille privés.</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+158"/>
        <source>Create Identity</source>
        <translation>Créer une identité</translation>
    </message>
    <message>
        <source>Restore from mnemonic</source>
        <translation type="vanished">Restaurer à partir d’une phrase mnémonique</translation>
    </message>
    <message>
        <source>Post-quantum protected</source>
        <translation type="vanished">Protégé contre les attaques quantiques</translation>
    </message>
    <message>
        <location line="-155"/>
        <source>Restore with recovery phrase</source>
        <translation>Restaurer avec la phrase de récupération</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Hybrid post-quantum encryption</source>
        <translation>Chiffrement post-quantique hybride</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>CYBOU is catching up with known peers (block %1). Identity creation uses the locally verified finalized state.</source>
        <translation>CYBOU rattrape les pairs connus (bloc %1). La création d’Identité utilise l’état finalisé vérifié localement.</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>STEP 1 OF 3</source>
        <translation>ÉTAPE 1 SUR 3</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Create a local vault password</source>
        <translation>Créer un mot de passe pour le coffre local</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This password unlocks CYBOU on this computer. It never leaves your device. Your recovery phrase, not this password, restores your Identity elsewhere.</source>
        <translation>Ce mot de passe déverrouille CYBOU sur cet ordinateur. Il ne quitte jamais votre appareil. Pour restaurer votre identité ailleurs, utilisez votre phrase de récupération, et non ce mot de passe.</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+362"/>
        <source>Vault password</source>
        <translation>Mot de passe du coffre</translation>
    </message>
    <message>
        <location line="-361"/>
        <location line="+198"/>
        <source>At least %1 characters</source>
        <translation>Au moins %1 caractères</translation>
    </message>
    <message>
        <location line="-195"/>
        <source>Repeat password</source>
        <translation>Répéter le mot de passe</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+200"/>
        <source>Repeat the password</source>
        <translation>Saisissez à nouveau le mot de passe</translation>
    </message>
    <message>
        <location line="-191"/>
        <location line="+202"/>
        <source>Back</source>
        <translation>Retour</translation>
    </message>
    <message>
        <location line="-201"/>
        <source>Continue</source>
        <translation>Continuer</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>STEP 2 OF 3</source>
        <translation>ÉTAPE 2 SUR 3</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Write down your 24 recovery words</source>
        <translation>Notez vos 24 mots de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>These words restore your Identity, Mail, Files, Names and Wallet on any computer. Anyone who has them controls your Identity. Write them on paper, in order, and keep them somewhere safe. CYBOU cannot recover them for you.</source>
        <translation>Ces mots restaurent votre identité, vos courriers, fichiers, noms et votre portefeuille sur tout ordinateur. Toute personne qui les possède contrôle votre identité. Notez-les dans l’ordre sur papier et conservez-les en lieu sûr. CYBOU ne peut pas les récupérer pour vous.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Cancel</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>I have written them down</source>
        <translation>J’ai noté les mots</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>STEP 3 OF 3</source>
        <translation>ÉTAPE 3 SUR 3</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Confirm your recovery words</source>
        <translation>Confirmez vos mots de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Enter the requested words to confirm you saved the phrase correctly.</source>
        <translation>Saisissez les mots demandés pour confirmer que vous avez correctement enregistré la phrase.</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Show words again</source>
        <translation>Afficher à nouveau les mots</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Creating your Identity</source>
        <translation>Création de votre identité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This takes a moment. You can keep CYBOU open while it finishes.</source>
        <translation>Cela prend un instant. Vous pouvez laisser CYBOU ouvert pendant cette opération.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Preparing keys</source>
        <translation>Préparation des clés</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Creating Identity</source>
        <translation>Création de l’identité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Waiting for network confirmation</source>
        <translation>En attente de confirmation du réseau</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Identity active</source>
        <translation>Identité active</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Start again</source>
        <translation>Recommencer</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Restore your Identity</source>
        <translation>Restaurer votre identité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Enter your 24-word recovery phrase</source>
        <translation>Saisissez votre phrase de récupération de 24 mots</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Word %1</source>
        <translation>Mot %1</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Paste phrase</source>
        <translation>Coller la phrase</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Local vault password</source>
        <translation>Mot de passe du coffre local</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>It protects this Identity on this computer only. The recovery phrase above is what restores it anywhere.</source>
        <translation>Il protège cette identité sur cet ordinateur uniquement. C’est la phrase de récupération ci-dessus qui la restaure partout.</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Restore Identity</source>
        <translation>Restaurer l’identité</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>Restoring your Identity</source>
        <translation>Restauration de votre identité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>CYBOU is rebuilding your data from the network. Mail and Files appear as they are verified.</source>
        <translation>CYBOU restaure vos données depuis le réseau. Les courriers et fichiers apparaissent au fur et à mesure de leur vérification.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Identity recovered</source>
        <translation>Identité restaurée</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Wallet state recovered</source>
        <translation>État du portefeuille restauré</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Names recovered</source>
        <translation>Noms restaurés</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Mail</source>
        <translation>Courrier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Files</source>
        <translation>Fichiers</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Open CYBOU</source>
        <translation>Ouvrir CYBOU</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Mail and Files keep restoring in the background.</source>
        <translation>La restauration des courriers et fichiers continue en arrière-plan.</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Your Identity is ready</source>
        <translation>Votre identité est prête</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Now choose your CYBOU name. People send you mail and payments at name.cybou instead of a long ID.</source>
        <translation>Choisissez votre nom CYBOU. Les autres pourront vous envoyer courriers et paiements à nom.cybou plutôt qu’à un identifiant long.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>yourname</source>
        <translation>votrenom</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+17"/>
        <source>5–32 characters: lowercase letters, digits and hyphens.</source>
        <translation>5 à 32 caractères : lettres minuscules, chiffres et tirets.</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>Claim name</source>
        <translation>Réserver ce nom</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Skip for now</source>
        <translation>Ignorer pour le moment</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>You can also claim a name later in Identity &amp; Security.</source>
        <translation>Vous pourrez réserver un nom plus tard dans Identité et sécurité.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>You will be reachable as %1.cybou</source>
        <translation>Vous serez joignable à %1.cybou</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>A name claim is already in progress.</source>
        <translation>Une réservation de nom est déjà en cours.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Claiming %1.cybou. It becomes yours once the network confirms it.</source>
        <translation>Réservation de %1.cybou. Ce nom vous appartiendra après confirmation du réseau.</translation>
    </message>
    <message>
        <location line="+22"/>
        <location line="+356"/>
        <source>Welcome back</source>
        <translation>Ravi de vous revoir</translation>
    </message>
    <message>
        <location line="-353"/>
        <source>Enter your local vault password to unlock CYBOU on this computer.</source>
        <translation>Saisissez le mot de passe du coffre local pour déverrouiller CYBOU sur cet ordinateur.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Unlock</source>
        <translation>Déverrouiller</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Restore a different Identity</source>
        <translation>Restaurer une autre identité</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Your keys could not be prepared on this computer.</source>
        <translation>Vos clés n’ont pas pu être préparées sur cet ordinateur.</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>Weak</source>
        <translation>Faible</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Fair</source>
        <translation>Moyen</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Strong</source>
        <translation>Fort</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Use at least %1 characters.</source>
        <translation>Utilisez au moins %1 caractères.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The passwords do not match.</source>
        <translation>Les mots de passe ne correspondent pas.</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>Word #%1</source>
        <translation>Mot n° %1</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Word #%1 does not match. Check your written copy.</source>
        <translation>Le mot n° %1 ne correspond pas. Vérifiez votre copie.</translation>
    </message>
    <message>
        <location line="+61"/>
        <source>%1 / %2 words · %3 not in the recovery word list (marked in red)</source>
        <translation>%1 / %2 mots · %3 absents de la liste des mots de récupération (en rouge)</translation>
    </message>
    <message>
        <source>%1 / %2 words · %3 not recognised</source>
        <translation type="vanished">%1 / %2 mots · %3 non reconnu(s)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1 / %2 words</source>
        <translation>%1 / %2 mots</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Valid recovery phrase</source>
        <translation>Phrase de récupération valide</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>These 24 words are not a valid recovery phrase. Check the order and the spelling of each word.</source>
        <translation>Ces 24 mots ne forment pas une phrase de récupération valide. Vérifiez l’ordre et l’orthographe de chaque mot.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>%1 / %2 characters</source>
        <translation>%1 / %2 caractères</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Long enough</source>
        <translation>Longueur suffisante</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Passwords match</source>
        <translation>Les mots de passe correspondent</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>The passwords do not match</source>
        <translation>Les mots de passe ne correspondent pas</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>CYBOU is still starting on this computer…</source>
        <translation>CYBOU démarre encore sur cet ordinateur…</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Enter all 24 words</source>
        <translation>Saisissez les 24 mots</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Fix the recovery phrase</source>
        <translation>Corrigez la phrase de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Choose a password of at least %1 characters</source>
        <translation>Choisissez un mot de passe d’au moins %1 caractères</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Repeat the same password</source>
        <translation>Répétez le même mot de passe</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>These words are not a valid CYBOU recovery phrase. Check the spelling and order.</source>
        <translation>Ces mots ne forment pas une phrase de récupération CYBOU valide. Vérifiez l’orthographe et l’ordre.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Restore could not start. Check the phrase and try again.</source>
        <translation>La restauration n’a pas pu démarrer. Vérifiez la phrase et réessayez.</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Unlocking…</source>
        <translation>Déverrouillage…</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>The password is incorrect.</source>
        <translation>Le mot de passe est incorrect.</translation>
    </message>
    <message>
        <location line="+65"/>
        <source>Welcome back, %1</source>
        <translation>Ravi de vous revoir, %1</translation>
    </message>
    <message>
        <source>Restore did not complete. Check the phrase and try again.</source>
        <translation type="vanished">La restauration n’a pas abouti. Vérifiez la phrase et réessayez.</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../cybouapplication.cpp" line="+51"/>
        <source>Protected communication infrastructure</source>
        <translation>Infrastructure de communication protégée</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Use the specified CYBOU data directory.</source>
        <translation>Utiliser le dossier de données CYBOU indiqué.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>directory</source>
        <translation>dossier</translation>
    </message>
    <message>
        <source>Use an explicit network file with an isolated --datadir.</source>
        <translation type="vanished">Utiliser un fichier réseau explicite avec un --datadir isolé.</translation>
    </message>
    <message>
        <source>file</source>
        <translation type="vanished">fichier</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Connect to this CYBOU P2P IP:port endpoint.</source>
        <translation>Se connecter à ce point de terminaison CYBOU P2P IP:port.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>endpoint</source>
        <translation>point de terminaison</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+15"/>
        <source>CYBOU</source>
        <translation>CYBOU</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>--peer requires IP:port.</source>
        <translation>--peer nécessite IP:port.</translation>
    </message>
    <message>
        <source>--network requires an explicit isolated --datadir and --peer.</source>
        <translation type="vanished">--network nécessite un --datadir isolé explicite et --peer.</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Could not create the data directory: %1</source>
        <translation>Impossible de créer le dossier de données : %1</translation>
    </message>
</context>
<context>
    <name>RecoveryPhraseDialog</name>
    <message>
        <location filename="../recoveryphrasedialog.cpp" line="+59"/>
        <source>Save your recovery words</source>
        <translation>Enregistrer vos mots de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Save your new recovery words</source>
        <translation>Enregistrer vos nouveaux mots de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Your recovery words</source>
        <translation>Vos mots de récupération</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Write down these 24 words in order</source>
        <translation>Notez ces 24 mots dans l’ordre</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Write down these 24 new words in order</source>
        <translation>Notez ces 24 nouveaux mots dans l’ordre</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Your 24 recovery words in order</source>
        <translation>Vos 24 mots de récupération dans l’ordre</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>These 24 words are the only way to restore your identity on a new installation. Anyone who sees them controls your identity. Write them down on paper and store them offline.</source>
        <translation>Ces 24 mots sont le seul moyen de restaurer votre identité sur une nouvelle installation. Toute personne qui les voit contrôle votre identité. Notez-les sur papier et conservez-les hors ligne.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>These words replace your current recovery phrase. Once the network confirms the change, only the new words restore your Identity and the old words stop working. Your AccountID, name, Mail, Files and Wallet stay the same.</source>
        <translation>Ces mots remplacent votre phrase de récupération actuelle. Dès que le réseau confirme le changement, seuls les nouveaux mots restaureront votre identité et les anciens cesseront de fonctionner. Votre AccountID, votre nom, vos courriers, fichiers et portefeuille restent inchangés.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Anyone who sees these words can take over your identity. Keep them private.</source>
        <translation>Toute personne qui voit ces mots peut prendre le contrôle de votre identité. Gardez-les secrets.</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Copy words</source>
        <translation>Copier les mots</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Advanced: save unencrypted file…</source>
        <translation>Avancé : enregistrer un fichier non chiffré…</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Opens a save dialog, then reports the exact file path here.</source>
        <translation>Ouvre une fenêtre d’enregistrement, puis affiche ici le chemin exact du fichier.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Show in folder</source>
        <translation>Afficher dans le dossier</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Opens the folder that contains the saved file.</source>
        <translation>Ouvre le dossier qui contient le fichier enregistré.</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Nothing has been saved to a file yet.</source>
        <translation>Aucun fichier n’a encore été enregistré.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Confirm you wrote the words down</source>
        <translation>Confirmez que vous avez noté les mots</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+8"/>
        <source>Word #%1:</source>
        <translation>Mot n° %1 :</translation>
    </message>
    <message>
        <location line="-5"/>
        <location line="+8"/>
        <source>type the word</source>
        <translation>saisissez le mot</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Change recovery phrase</source>
        <translation>Modifier la phrase de récupération</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Create identity</source>
        <translation>Créer une identité</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Cancel</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Canceling keeps your current recovery phrase — nothing is sent to the network.</source>
        <translation>L’annulation conserve votre phrase de récupération actuelle ; rien n’est envoyé au réseau.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Canceling discards this identity — nothing is created and nothing is sent to the network.</source>
        <translation>L’annulation abandonne cette identité ; rien n’est créé ni envoyé au réseau.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Close</source>
        <translation>Fermer</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>Words copied. The clipboard will be cleared after one minute if unchanged. Clipboard history may retain a copy.</source>
        <translation>Mots copiés. Le presse-papiers sera effacé après une minute s’il n’a pas changé. Son historique peut conserver une copie.</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+15"/>
        <source>Save recovery words</source>
        <translation>Enregistrer les mots de récupération</translation>
    </message>
    <message>
        <location line="-14"/>
        <source>The file will contain your 24 recovery words in plain text. Anyone who can read that file can take over your identity.

A save dialog will open, proposing the file name &quot;%1&quot; in your home folder. After saving, the exact path will be shown in this window.

Save anyway?</source>
        <translation>Le fichier contiendra vos 24 mots de récupération en texte brut. Toute personne pouvant le lire peut prendre le contrôle de votre identité.

Une fenêtre d’enregistrement proposera le nom « %1 » dans votre dossier personnel. Après l’enregistrement, le chemin exact s’affichera ici.

Enregistrer quand même ?</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Saving canceled before the file dialog — nothing was written.</source>
        <translation>Enregistrement annulé avant l’ouverture de la fenêtre ; rien n’a été écrit.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Text file (*.txt)</source>
        <translation>Fichier texte (*.txt)</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Saving canceled in the file dialog — no file was written.</source>
        <translation>Enregistrement annulé dans la fenêtre ; aucun fichier n’a été écrit.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>This file already exists. Choose a new file name; nothing was overwritten.</source>
        <translation>Ce fichier existe déjà. Choisissez un autre nom ; aucun fichier n’a été remplacé.</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+21"/>
        <source>Cannot save</source>
        <translation>Impossible d’enregistrer</translation>
    </message>
    <message>
        <location line="-20"/>
        <source>The recovery words could not be written to:
%1

%2</source>
        <translation>Impossible d’écrire les mots de récupération dans :
%1

%2</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Save failed — the file was not created.</source>
        <translation>Échec de l’enregistrement ; le fichier n’a pas été créé.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Save failed — file access could not be restricted to the current user.</source>
        <translation>Échec de l’enregistrement ; l’accès au fichier n’a pas pu être limité à l’utilisateur actuel.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Writing the recovery words failed and the incomplete file was removed:
%1

%2</source>
        <translation>L’écriture des mots de récupération a échoué et le fichier incomplet a été supprimé :
%1

%2</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Save failed — the incomplete file was removed.</source>
        <translation>Échec de l’enregistrement ; le fichier incomplet a été supprimé.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Recovery words saved to file:
%1</source>
        <translation>Mots de récupération enregistrés dans le fichier :
%1</translation>
    </message>
</context>
<context>
    <name>SchematicFranceMap</name>
    <message>
        <location filename="../pages/networkpage.cpp" line="-409"/>
        <source>Observed Mesh Peers • Schematic Map</source>
        <translation>Pairs du maillage observés • Carte schématique</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Illustrative regional anchors · Not physical geolocation</source>
        <translation>Repères régionaux illustratifs · Pas de géolocalisation physique</translation>
    </message>
    <message>
        <location line="+60"/>
        <source>LAN / Local</source>
        <translation>LAN / Local</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>No peer connections observed</source>
        <translation>Aucune connexion de pair observée</translation>
    </message>
    <message><source>%1 connected · %2 known, disconnected (pale)</source><translation>%1 connectés · %2 connus, déconnectés (pâles)</translation></message>
    <message><source>Observed mesh peers</source><translation>Pairs du maillage observés</translation></message>
</context>
<context>
    <name>SettingsPage</name>
    <message>
        <location filename="../pages/settingspage.cpp" line="+150"/>
        <source>General</source>
        <translation>Général</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Start CYBOU at login</source>
        <translation>Lancer CYBOU à l’ouverture de session</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Startup setting</source>
        <translation>Paramètre de démarrage</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>CYBOU could not update the start-at-login setting.</source>
        <translation>CYBOU n’a pas pu modifier le démarrage à l’ouverture de session.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Keep running in background when the window is closed</source>
        <translation>Continuer en arrière-plan lorsque la fenêtre est fermée</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+5"/>
        <source>Language</source>
        <translation>Langue</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Appearance</source>
        <translation>Apparence</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+5"/>
        <source>Theme</source>
        <translation>Thème</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Same as system</source>
        <translation>Identique au système</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Light</source>
        <translation>Clair</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Dark</source>
        <translation>Sombre</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Privacy and security</source>
        <translation>Confidentialité et sécurité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Notify me about new mail, received payments and problems</source>
        <translation>M’avertir des nouveaux courriers, paiements reçus et problèmes</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Show Mail previews in notifications</source>
        <translation>Afficher un aperçu des courriers dans les notifications</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>When off, notifications only say “New CYBOU Mail”.</source>
        <translation>Désactivé, les notifications indiquent seulement « Nouveau courrier CYBOU ».</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+5"/>
        <source>Lock CYBOU after inactivity</source>
        <translation>Verrouiller CYBOU après une période d’inactivité</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 minutes</source>
        <translation>%1 minutes</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Never</source>
        <translation>Jamais</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Files</source>
        <translation>Fichiers</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+11"/>
        <source>Download folder</source>
        <translation>Dossier de téléchargement</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>Change…</source>
        <translation>Modifier…</translation>
    </message>
    <message>
        <source>Validation</source>
        <translation type="vanished">Validation</translation>
    </message>
    <message>
        <source>Only network-finalized results change your balances, Mail and Files. Validation status is shown for information.</source>
        <translation type="vanished">Seuls les résultats finalisés par le réseau modifient vos soldes, courriers et fichiers. Le statut de validation est fourni à titre indicatif.</translation>
    </message>
    <message>
        <source>Show validation status</source>
        <translation type="vanished">Afficher le statut de validation</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Network storage</source>
        <translation>Stockage réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Every Full Node stores encrypted network content. Disk space is allocated automatically while keeping free space for your computer.</source>
        <translation>Chaque nœud complet stocke du contenu réseau chiffré. L’espace disque est alloué automatiquement tout en conservant de l’espace libre pour votre ordinateur.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Advanced</source>
        <translation>Avancé</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Data directory</source>
        <translation>Dossier de données</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Open Diagnostics</source>
        <translation>Ouvrir les diagnostics</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>Available after node startup</source>
        <translation>Disponible après le démarrage du nœud</translation>
    </message>
</context>
<context>
    <name>StoragePage</name>
    <message>
        <location filename="../pages/storagepage.cpp" line="+140"/>
        <location line="+1142"/>
        <source>My files</source>
        <translation>Mes fichiers</translation>
    </message>
    <message>
        <location line="-1141"/>
        <source>Recent</source>
        <translation>Récents</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+535"/>
        <source>Starred</source>
        <translation>Favoris</translation>
    </message>
    <message>
        <location line="-534"/>
        <source>Trash</source>
        <translation>Corbeille</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Today</source>
        <translation>Aujourd’hui</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Yesterday</source>
        <translation>Hier</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>New</source>
        <translation>Nouveau</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+216"/>
        <location line="+648"/>
        <source>New folder</source>
        <translation>Nouveau dossier</translation>
    </message>
    <message>
        <location line="-861"/>
        <location line="+0"/>
        <location line="+210"/>
        <location line="+2"/>
        <source>Upload files</source>
        <translation>Téléverser des fichiers</translation>
    </message>
    <message>
        <location line="-210"/>
        <location line="+867"/>
        <location line="+107"/>
        <source>Upload folder</source>
        <translation>Téléverser un dossier</translation>
    </message>
    <message>
        <location line="-969"/>
        <source>Files navigation</source>
        <translation>Navigation des fichiers</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Storage</source>
        <translation>Stockage</translation>
    </message>
    <message>
        <location line="+27"/>
        <location line="+1"/>
        <source>Back to parent folder</source>
        <translation>Retour au dossier parent</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Location</source>
        <translation>Emplacement</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+1"/>
        <source>Search files</source>
        <translation>Rechercher des fichiers</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+6"/>
        <source>Empty Trash</source>
        <translation>Vider la corbeille</translation>
    </message>
    <message>
        <source>Permanently delete everything in Trash? This cannot be undone.</source>
        <translation type="vanished">Supprimer définitivement tout le contenu de la corbeille ? Cette action est irréversible.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Delete everything in Trash from your catalog? After eligible publication revocation is finalized, providers are instructed to purge unshared chunks. Other copies may remain.</source>
        <translation>Supprimer tout le contenu de la corbeille de votre catalogue ? Après finalisation du retrait des publications éligibles, les fournisseurs doivent supprimer les chunks non partagés. D’autres copies peuvent subsister.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Emptying Trash. It is done once the network confirms it.</source>
        <translation>Vidage de la corbeille. La suppression sera effective après confirmation du réseau.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>List view</source>
        <translation>Vue en liste</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Grid view</source>
        <translation>Vue en grille</translation>
    </message>
    <message>
        <location line="+32"/>
        <location line="+1028"/>
        <source>Star</source>
        <translation>Ajouter aux favoris</translation>
    </message>
    <message>
        <location line="-1025"/>
        <location line="+1034"/>
        <source>Move to Trash</source>
        <translation>Déplacer dans la corbeille</translation>
    </message>
    <message>
        <location line="-1031"/>
        <location line="+138"/>
        <location line="+198"/>
        <location line="+697"/>
        <source>%1 items moved to Trash</source>
        <translation>%1 éléments déplacés dans la corbeille</translation>
    </message>
    <message>
        <location line="-1033"/>
        <location line="+139"/>
        <location line="+73"/>
        <location line="+125"/>
        <location line="+697"/>
        <source>Undo</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location line="-1028"/>
        <location line="+1"/>
        <source>Clear selection</source>
        <translation>Effacer la sélection</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+35"/>
        <source>Files</source>
        <translation>Fichiers</translation>
    </message>
    <message>
        <location line="-32"/>
        <source>Name</source>
        <translation>Nom</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Size</source>
        <translation>Taille</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Modified</source>
        <translation>Modifié</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+1112"/>
        <source>Status</source>
        <translation>État</translation>
    </message>
    <message>
        <location line="-1033"/>
        <location line="+936"/>
        <source>Details</source>
        <translation>Détails</translation>
    </message>
    <message>
        <location line="-899"/>
        <location line="+198"/>
        <location line="+697"/>
        <source>Moved to Trash</source>
        <translation>Déplacé dans la corbeille</translation>
    </message>
    <message>
        <location line="-822"/>
        <source>Moved to “%1”</source>
        <translation>Déplacé dans « %1 »</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 items moved to “%2”</source>
        <translation>%1 éléments déplacés dans « %2 »</translation>
    </message>
    <message>
        <location line="+38"/>
        <location line="+263"/>
        <source>%1 items</source>
        <translation>%1 éléments</translation>
    </message>
    <message>
        <location line="-179"/>
        <source>%1 items starred</source>
        <translation>%1 éléments ajoutés aux favoris</translation>
    </message>
    <message>
        <location line="+179"/>
        <source>1 item</source>
        <translation>1 élément</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>No files match “%1”.</source>
        <translation>Aucun fichier ne correspond à « %1 ».</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Trash is empty.</source>
        <translation>La corbeille est vide.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Star files to find them here quickly.</source>
        <translation>Ajoutez des fichiers aux favoris pour les retrouver rapidement.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Files you add or open appear here.</source>
        <translation>Les fichiers ajoutés ou ouverts apparaîtront ici.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>No files yet. Drag files here or use New to upload.</source>
        <translation>Aucun fichier pour le moment. Déposez des fichiers ici ou utilisez Nouveau pour les téléverser.</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+31"/>
        <source>Search results</source>
        <translation>Résultats de recherche</translation>
    </message>
    <message>
        <location line="+44"/>
        <source>%1 selected</source>
        <translation>%1 sélectionné(s)</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Files needs your CYBOU Identity. Create or restore it on Home.</source>
        <translation>Les fichiers nécessitent votre identité CYBOU. Créez-la ou restaurez-la depuis l’accueil.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Files is not connected yet. Your files will appear here once it is.</source>
        <translation>Les fichiers ne sont pas encore connectés. Ils apparaîtront ici une fois la connexion établie.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Storage almost full · %1 used of %2</source>
        <translation>Stockage presque plein · %1 utilisés sur %2</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1 used of %2</source>
        <translation>%1 utilisés sur %2</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 used</source>
        <translation>%1 utilisés</translation>
    </message>
    <message>
        <location line="+72"/>
        <source>Folder name</source>
        <translation>Nom du dossier</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Untitled folder</source>
        <translation>Dossier sans titre</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Folder “%1” created</source>
        <translation>Dossier « %1 » créé</translation>
    </message>
    <message>
        <location line="+110"/>
        <source>Cancel</source>
        <translation type="unfinished">Annuler</translation>
    </message>
    <message>
        <location line="+78"/>
        <location line="+88"/>
        <source>Rename</source>
        <translation>Renommer</translation>
    </message>
    <message>
        <location line="-88"/>
        <source>New name</source>
        <translation>Nouveau nom</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+82"/>
        <source>Move</source>
        <translation>Déplacer</translation>
    </message>
    <message>
        <location line="-80"/>
        <source>Move to</source>
        <translation>Déplacer vers</translation>
    </message>
    <message>
        <location line="+31"/>
        <location line="+44"/>
        <location line="+213"/>
        <source>Download</source>
        <translation>Télécharger</translation>
    </message>
    <message>
        <location line="-229"/>
        <location line="+202"/>
        <source>Restore</source>
        <translation>Restaurer</translation>
    </message>
    <message>
        <location line="-200"/>
        <source>Restored</source>
        <translation>Restauré</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 items restored</source>
        <translation>%1 éléments restaurés</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+1"/>
        <location line="+205"/>
        <location line="+4"/>
        <source>Delete forever</source>
        <translation>Supprimer définitivement</translation>
    </message>
    <message>
        <location line="-208"/>
        <source>Delete from your catalog? After eligible publication revocation is finalized, providers are instructed to purge unshared chunks. Other copies may remain.</source>
        <translation>Supprimer de votre catalogue ? Après finalisation du retrait des publications éligibles, les fournisseurs doivent supprimer les chunks non partagés. D’autres copies peuvent subsister.</translation>
    </message>
    <message>
        <location line="+155"/>
        <source>Encrypted before sending · Recoverable with your account recovery phrase</source>
        <translation>Chiffré avant envoi · Récupérable avec la phrase de récupération de votre compte</translation>
    </message>
    <message>
        <location line="+106"/>
        <source>Confidentiality assurance</source>
        <translation>Assurance de confidentialité</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Hybrid post-quantum encryption before upload (ML-KEM-768 + X25519). Plaintext and filename never sent to network.</source>
        <translation>Chiffrement post-quantique hybride avant envoi (ML-KEM-768 + X25519). Le contenu en clair et les noms de fichiers ne sont jamais envoyés au réseau.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Integrity assurance</source>
        <translation>Assurance d’intégrité</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Content-addressed BLAKE3 Merkle tree. Each chunk verified on retrieval against authorized RootPublication commitment.</source>
        <translation>Arbre de Merkle BLAKE3 adressé par le contenu. Chaque chunk est vérifié à la récupération par rapport à l’engagement RootPublication autorisé.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Availability &amp; durability scope</source>
        <translation>Portée de disponibilité et durabilité</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Measured copies: %1 of %2 target. Replica deduplication is by StorageId; does not prove independent physical host failure domains.</source>
        <translation>Copies mesurées : %1 sur %2 cibles. La déduplication des réplicas s’effectue par StorageId ; ne prouve pas des domaines de défaillance matérielle indépendants.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Recovery assurance</source>
        <translation>Assurance de récupération</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Recoverable on any node using your account recovery phrase via owner self-capsule. Historical capsules preserved across rotation.</source>
        <translation>Récupérable sur n’importe quel nœud avec votre phrase de récupération de compte via l’auto-capsule propriétaire. Les capsules historiques sont préservées lors des rotations.</translation>
    </message>
    <message>
        <source>Delete this item from your catalog? After eligible publication revocation is finalized, providers are instructed to purge unshared chunks. Other copies may remain.</source>
        <translation type="vanished">Supprimer cet élément de votre catalogue ? Après finalisation du retrait des publications éligibles, les fournisseurs doivent supprimer les chunks non partagés. D’autres copies peuvent subsister.</translation>
    </message>
    <message>
        <source>Remove from your Files? CYBOU releases retained storage according to the Storage retention policy.</source>
        <translation type="vanished">Retirer de vos fichiers ? CYBOU libérera l’espace retenu conformément à la politique de conservation du stockage.</translation>
    </message>
    <message>
        <location line="-256"/>
        <source>Open</source>
        <translation>Ouvrir</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Make a copy</source>
        <translation>Créer une copie</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy created</source>
        <translation>Copie créée</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Remove star</source>
        <translation>Retirer des favoris</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Send by CYBOU Mail</source>
        <translation>Envoyer par courrier CYBOU</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>Folder</source>
        <translation>Dossier</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>PDF document</source>
        <translation>Document PDF</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Image</source>
        <translation>Image</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Archive</source>
        <translation>Archive</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>File</source>
        <translation>Fichier</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 file</source>
        <translation>%1 fichier</translation>
    </message>
    <message>
        <location line="+54"/>
        <source>Close</source>
        <translation>Fermer</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Modified %1</source>
        <translation>Modifié le %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>On this computer</source>
        <translation>Sur cet ordinateur</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Downloaded to</source>
        <translation>Téléchargé dans</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 in %2</source>
        <translation>%1 dans %2</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Downloaded copy: %1 — drag the file out of CYBOU to copy it</source>
        <translation>Copie téléchargée : %1 — faites glisser le fichier hors de CYBOU pour le copier</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>On the network</source>
        <translation>Sur le réseau</translation>
    </message>
    <message>
        <source>Not stored on the network yet</source>
        <translation type="vanished">Pas encore stocké sur le réseau</translation>
    </message>
    <message>
        <source>%1 of %2 encrypted copies</source>
        <translation type="vanished">%1 copies chiffrées sur %2</translation>
    </message>
    <message>
        <source>%1 encrypted copies</source>
        <translation type="vanished">%1 copies chiffrées</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Encryption</source>
        <translation>Chiffrement</translation>
    </message>
    <message>
        <source>Encrypted content; recovery capsules may preserve access</source>
        <translation type="vanished">Contenu chiffré ; les capsules de récupération peuvent préserver l’accès</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Owner</source>
        <translation>Propriétaire</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>You</source>
        <translation>Vous</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Try again</source>
        <translation>Réessayer</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Discard</source>
        <translation>Abandonner</translation>
    </message>
    <message>
        <source>Permanently delete this item? This cannot be undone.</source>
        <translation type="vanished">Supprimer définitivement cet élément ? Cette action est irréversible.</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>Send by Mail</source>
        <translation>Envoyer par courrier</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Advanced</source>
        <translation>Avancé</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Not reported yet</source>
        <translation>Pas encore indiqué</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Root content identifier</source>
        <translation>Identifiant de racine du contenu</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Chunk count</source>
        <translation>Nombre de blocs</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 chunks (512 KiB unit)</source>
        <translation>%1 blocs (unité de 512 Kio)</translation>
    </message>
    <message>
        <source>Integrity evidence</source>
        <translation type="vanished">Preuve d&apos;intégrité</translation>
    </message>
    <message>
        <source>Content-addressed BLAKE3 Merkle tree. Each chunk verified on retrieval.</source>
        <translation type="vanished">Arbre de Merkle BLAKE3 adressé par contenu. Chaque bloc vérifié à la récupération.</translation>
    </message>
    <message>
        <source>Authorized reference</source>
        <translation type="vanished">Référence autorisée</translation>
    </message>
    <message>
        <source>Recoverable via owner self-capsule. Authorized by finalized RootPublication.</source>
        <translation type="vanished">Récupérable via la capsule propriétaire. Autorisé par RootPublication finalisée.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Inspect chunk tree</source>
        <translation>Inspecter l&apos;arbre de blocs</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Finalized height</source>
        <translation>Hauteur finalisée</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Protection status</source>
        <translation>État de la protection</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Local availability</source>
        <translation>Disponibilité locale</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Retrieval status</source>
        <translation>État de récupération</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Not retrieved on this computer</source>
        <translation>Non récupéré sur cet ordinateur</translation>
    </message>
    <message>
        <location line="-133"/>
        <source>Encrypted copies: %1 of %2</source>
        <translation>Copies chiffrées : %1 sur %2</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Encrypted copies: %1</source>
        <translation>Copies chiffrées : %1</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>Remote copies have not been measured yet</source>
        <translation>Les copies distantes n’ont pas encore été mesurées</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Replicating to network</source>
        <translation>Réplication réseau en cours</translation>
    </message>
    <message>
        <location line="-125"/>
        <location line="+210"/>
        <source>Mail is unavailable.</source>
        <translation>Le courrier est indisponible.</translation>
    </message>
    <message>
        <location line="-209"/>
        <location line="+210"/>
        <source>This file must reach Protected before it can be attached by reference.</source>
        <translation>Ce fichier doit être protégé avant de pouvoir être joint par référence.</translation>
    </message>
    <message>
        <location line="-56"/>
        <source>Deletion lifecycle</source>
        <translation>Cycle de vie de la suppression</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>• Phase 1: Local catalog removal (immediate)
• Phase 2: Finalized publication revocation (stops new admissions)
• Phase 3: Storage lease closure (after period close)
• Phase 4: Provider chunk purge (remote acknowledgements unconfirmed)
• Phase 5: Retained copies (downloaded or shared copies remain)</source>
        <translation>• Phase 1 : Retrait du catalogue local (immédiat)
• Phase 2 : Révocation de publication finalisée (bloque les nouvelles admissions)
• Phase 3 : Fermeture du bail de stockage (après clôture de période)
• Phase 4 : Purge des chunks par les fournisseurs (accusés de réception distants non confirmés)
• Phase 5 : Copies conservées (les copies téléchargées ou partagées subsistent)</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Delete this item from your catalog?

Deletion proceeds through 5 distinct phases:
1. Immediate removal from your local catalog view.
2. Finalized RootPublication revocation on the network, stopping new admissions.
3. Storage lease closure upon billing period expiration.
4. Provider chunk purge: compliant providers purge unshared chunks. Remote purge acknowledgements are not cryptographically notarized; network cannot prove erasure of uncooperative or offline copies.
5. Retained copies: any copies previously downloaded, shared with recipients, or backed up externally remain unaffected.

Proceed with permanent deletion?</source>
        <translation>Supprimer cet élément de votre catalogue ?

La suppression se déroule en 5 phases distinctes :
1. Retrait immédiat de votre vue de catalogue local.
2. Révocation RootPublication finalisée sur le réseau, arrêtant les nouvelles admissions.
3. Fermeture du bail de stockage à l’expiration de la période de facturation.
4. Purge des chunks : les fournisseurs conformes purgent les chunks non partagés. Les accusés de purge distants ne sont pas notariés par cryptographie ; le réseau ne peut prouver l’effacement de copies non coopératives ou hors ligne.
5. Copies conservées : les copies préalablement téléchargées, partagées avec des destinataires ou sauvegardées en externe ne sont pas affectées.

Poursuivre la suppression définitive ?</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Download is available when network protection is complete.</source>
        <translation>Le téléchargement sera disponible lorsque la protection réseau sera complète.</translation>
    </message>
    <message>
        <location line="-381"/>
        <source>Finding files and folders. Nothing uploaded yet.</source>
        <translation>Recherche des fichiers et dossiers. Aucun élément téléversé.</translation>
    </message>
    <message>
        <location line="-48"/>
        <source>Finding files and folders: %1 found. Nothing uploaded yet.</source>
        <translation>Recherche des fichiers et dossiers : %1 trouvés. Aucun élément téléversé.</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>Preparing upload: %1 of %2 items queued. Network protection continues separately.</source>
        <translation>Préparation : %1 éléments sur %2 mis en file. La protection réseau se poursuit séparément.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 items queued for upload. %2 symbolic links skipped. Follow protection in Files.</source>
        <translation>%1 éléments mis en file de téléversement. %2 liens symboliques ignorés. Suivez la protection dans Fichiers.</translation>
    </message>
    <message>
        <location line="-40"/>
        <location line="+8"/>
        <location line="+23"/>
        <source>Import stopped. %1 items already queued; remaining items were not uploaded.</source>
        <translation>Import arrêté. %1 éléments déjà mis en file ; les autres n’ont pas été téléversés.</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>Import cancelled. %1 items already queued; remaining items were not uploaded.</source>
        <translation>Import annulé. %1 éléments déjà mis en file ; les autres n’ont pas été téléversés.</translation>
    </message>
    <message>
        <location line="-22"/>
        <source>A folder import is already in progress.</source>
        <translation>Un import de dossier est déjà en cours.</translation>
    </message>
    <message>
        <location line="+46"/>
        <location line="+14"/>
        <location line="+13"/>
        <location line="+4"/>
        <source>The folder could not be read. No items were uploaded.</source>
        <translation>Le dossier n’a pas pu être lu. Aucun élément téléversé.</translation>
    </message>
    <message>
        <location line="-27"/>
        <source>The folder contains an unreadable or unsupported item. No items were uploaded.</source>
        <translation>Le dossier contient un élément illisible ou non pris en charge. Aucun élément téléversé.</translation>
    </message>
    <message>
        <location line="-43"/>
        <location line="+46"/>
        <location line="+17"/>
        <source>This import exceeds 10,000 items or 64 folder levels. Select smaller folders. No items were uploaded.</source>
        <translation>Cet import dépasse 10 000 éléments ou 64 niveaux de dossiers. Sélectionnez des dossiers plus petits. Aucun élément téléversé.</translation>
    </message>
</context>
<context>
    <name>WalletPage</name>
    <message>
        <location filename="../pages/walletpage.cpp" line="+48"/>
        <source>block %1</source>
        <translation>bloc %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Received from %1</source>
        <translation>Reçu de %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Sent to %1</source>
        <translation>Envoyé à %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network service fee</source>
        <translation>Frais de service réseau</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Onboarding credit</source>
        <translation>Crédit de bienvenue</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Moved to System Balance</source>
        <translation>Transféré au solde système</translation>
    </message>
    <message>
        <location line="+43"/>
        <source>AVAILABLE</source>
        <translation>DISPONIBLE</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>SYSTEM BALANCE</source>
        <translation>SOLDE SYSTÈME</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+212"/>
        <source>Pays network fees for Mail, Files and payments</source>
        <translation>Paie les frais réseau du courrier, des fichiers et des paiements</translation>
    </message>
    <message>
        <location line="-209"/>
        <source>Add from Balance</source>
        <translation>Ajouter depuis le solde</translation>
    </message>
    <message>
        <source>AUTHORITY</source>
        <translation type="vanished">AUTORITÉ</translation>
    </message>
    <message>
        <source>Earned by network use; cannot be sent</source>
        <translation type="vanished">Acquise par l’usage du réseau ; non transférable</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+67"/>
        <source>Send</source>
        <translation>Envoyer</translation>
    </message>
    <message>
        <location line="-63"/>
        <source>Receive</source>
        <translation>Recevoir</translation>
    </message>
    <message>
        <source>Your current limits</source>
        <translation type="vanished">Vos limites actuelles</translation>
    </message>
    <message>
        <source>Network storage</source>
        <translation type="vanished">Stockage réseau</translation>
    </message>
    <message>
        <source>Operations per block</source>
        <translation type="vanished">Opérations par bloc</translation>
    </message>
    <message>
        <source>Validation</source>
        <translation type="vanished">Validation</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Send CYBOU</source>
        <translation>Envoyer des CYBOU</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+6"/>
        <location line="+331"/>
        <source>To</source>
        <translation>À</translation>
    </message>
    <message>
        <location line="-332"/>
        <source>name.cybou</source>
        <translation>nom.cybou</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Amount</source>
        <translation>Montant</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+348"/>
        <source>Whole CYBOU</source>
        <translation>CYBOU entiers</translation>
    </message>
    <message>
        <location line="-347"/>
        <source>Amount in CYBOU</source>
        <translation>Montant en CYBOU</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>Cancel</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Recent activity</source>
        <translation>Activité récente</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>No activity yet.</source>
        <translation>Aucune activité pour le moment.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Moving CYBOU to System Balance. It updates once the network confirms it.</source>
        <translation>Transfert de CYBOU vers le solde système. Le solde sera mis à jour après confirmation du réseau.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+321"/>
        <source>CYBOU could not be moved to System Balance.</source>
        <translation>Le transfert de CYBOU vers le solde système a échoué.</translation>
    </message>
    <message>
        <location line="-297"/>
        <source>Payment submitted</source>
        <translation>Paiement envoyé</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Submitted. Your balance changes once the network confirms it.</source>
        <translation>Envoyé. Votre solde changera après confirmation du réseau.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>The payment could not be sent.</source>
        <translation>Le paiement n’a pas pu être envoyé.</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>You have no spendable CYBOU yet.</source>
        <translation>Vous n’avez pas encore de CYBOU disponible.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Move spendable CYBOU into System Balance (cannot be undone)</source>
        <translation>Transférer des CYBOU disponibles vers le solde système (irréversible)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>You have no spendable CYBOU to move.</source>
        <translation>Vous n’avez pas de CYBOU disponible à transférer.</translation>
    </message>
    <message>
        <source>Pays network fees: about %1 more operations</source>
        <translation type="vanished">Paie les frais réseau : environ %1 opérations supplémentaires</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>Wallet needs your CYBOU Identity. Create or restore it on Home.</source>
        <translation>Le portefeuille a besoin de votre identité CYBOU. Créez-la ou restaurez-la depuis l’accueil.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Payments are not connected yet.</source>
        <translation>Les paiements ne sont pas encore connectés.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>You have no spendable CYBOU yet. Use Receive to share your name so others can pay you. Mail and Files keep working: their fees come from System Balance.</source>
        <translation>Vous n’avez pas encore de CYBOU disponible. Utilisez Recevoir pour partager votre nom afin que les autres puissent vous payer. Le courrier et les fichiers restent disponibles : leurs frais sont prélevés sur le solde système.</translation>
    </message>
    <message>
        <source>%1 used of %2</source>
        <translation type="vanished">%1 utilisés sur %2</translation>
    </message>
    <message>
        <source>%1 used  ·  unlimited</source>
        <translation type="vanished">%1 utilisés  ·  illimité</translation>
    </message>
    <message>
        <source>Unlimited</source>
        <translation type="vanished">Illimité</translation>
    </message>
    <message>
        <source>Eligible to sign</source>
        <translation type="vanished">Peut signer</translation>
    </message>
    <message>
        <source>Above %1</source>
        <translation type="vanished">Au-delà de %1</translation>
    </message>
    <message>
        <source>%1 more to the next tier. Each finalized file or message publication and each move to System Balance adds 1 AUTH (at most 1 per block).</source>
        <translation type="vanished">Encore %1 jusqu’au palier suivant. Chaque publication finalisée de fichier ou de message et chaque transfert vers le Solde système ajoute 1 AUTH (au plus 1 par bloc).</translation>
    </message>
    <message>
        <source>Top tier: operations and storage are not limited by AUTH.</source>
        <translation type="vanished">Palier maximal : les opérations et le stockage ne sont pas limités par l’AUTH.</translation>
    </message>
    <message>
        <source>Network operations</source>
        <translation type="vanished">Opérations réseau</translation>
    </message>
    <message>
        <source>Largest file</source>
        <translation type="vanished">Fichier le plus volumineux</translation>
    </message>
    <message>
        <source>%1 of %2 in this window  ·  up to %3 per block</source>
        <translation type="vanished">%1 sur %2 dans cette fenêtre  ·  jusqu’à %3 par bloc</translation>
    </message>
    <message numerus="yes">
        <source>The window resets in about %n minute(s).</source>
        <translation type="vanished">
            <numerusform>La fenêtre se réinitialise dans environ %n minute.</numerusform>
            <numerusform>La fenêtre se réinitialise dans environ %n minutes.</numerusform>
        </translation>
    </message>
    <message>
        <source>%1 more to the next tier. Each finalized file or message publication and each move to System Balance adds 1 AUTH (at most 1 per block). Every operation also needs %2-bit proof-of-work on this computer before the network accepts it.</source>
        <translation type="vanished">Encore %1 jusqu’au palier suivant. Chaque publication finalisée de fichier ou de message et chaque transfert vers le Solde système ajoute 1 AUTH (au plus 1 par bloc). Chaque opération exige aussi une preuve de travail de %2 bits sur cet ordinateur avant que le réseau ne l’accepte.</translation>
    </message>
    <message>
        <source>Validator tier: the highest limits. Every operation needs %1-bit proof-of-work.</source>
        <translation type="vanished">Palier validateur : les limites les plus élevées. Chaque opération exige une preuve de travail de %1 bits.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Use a CYBOU name, for example alice.cybou.</source>
        <translation>Utilisez un nom CYBOU, par exemple alice.cybou.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>You cannot send CYBOU to yourself.</source>
        <translation>Vous ne pouvez pas vous envoyer des CYBOU.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>%1  ·  Verified identity</source>
        <translation>%1 · identité vérifiée</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Network service fee: %1 (from System Balance)</source>
        <translation>Frais de service réseau : %1 (prélevés sur le solde système)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network service fee: calculated when sending</source>
        <translation>Frais de service réseau : calculés lors de l’envoi</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Not enough CYBOU available.</source>
        <translation>Solde CYBOU disponible insuffisant.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Sending…</source>
        <translation>Envoi…</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Confirm and send</source>
        <translation>Confirmer et envoyer</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Review</source>
        <translation>Vérifier</translation>
    </message>
    <message>
        <source>&lt;b&gt;Send %1 to %2&lt;/b&gt;&lt;br&gt;Network service fee: %3 from System Balance&lt;br&gt;Payments cannot be reversed.</source>
        <translation type="vanished">&lt;b&gt;Envoyer %1 à %2&lt;/b&gt;&lt;br&gt;Frais de service réseau : %3, prélevés sur le solde système&lt;br&gt;Les paiements sont irréversibles.</translation>
    </message>
    <message>
        <location line="-37"/>
        <source>Pays network fees · ~%1 standard fees (excludes storage rent)</source>
        <translation>Règle les frais réseau · ~%1 frais standard (hors loyer de stockage)</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>&lt;b&gt;Send %1 to %2&lt;/b&gt;&lt;br&gt;Paid from Available Balance · Network service fee: %3 from System Balance&lt;br&gt;Payments are final and cannot be reversed.</source>
        <translation>&lt;b&gt;Envoyer %1 à %2&lt;/b&gt;&lt;br&gt;Prélevé sur le solde disponible · Frais réseau : %3 sur le solde système&lt;br&gt;Les paiements sont définitifs et irréversibles.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>calculated when sending</source>
        <translation>calculés lors de l’envoi</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Sending %1 to %2…</source>
        <translation>Envoi de %1 à %2…</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The payment could not be started.</source>
        <translation>Le paiement n’a pas pu démarrer.</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+5"/>
        <source>Receive CYBOU</source>
        <translation>Recevoir des CYBOU</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Share your CYBOU name. Payments to it arrive in your Available balance.</source>
        <translation>Partagez votre nom CYBOU. Les paiements reçus apparaîtront dans votre solde disponible.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Copy</source>
        <translation>Copier</translation>
    </message>
    <message>
        <location line="+67"/>
        <source>Status</source>
        <translation>État</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Balance</source>
        <translation>Solde</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>System Balance (network service budget)</source>
        <translation>Solde système (budget des services réseau)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Available (spendable)</source>
        <translation>Disponible (dépensable)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Paid for</source>
        <translation>Payé pour</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>From</source>
        <translation>De</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Time</source>
        <translation>Date</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Confirmed in block</source>
        <translation>Confirmé dans le bloc</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Not yet</source>
        <translation>Pas encore</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Operation ID</source>
        <translation>ID de l’opération</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Copy operation ID</source>
        <translation>Copier l’ID de l’opération</translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+5"/>
        <source>Add to System Balance</source>
        <translation>Ajouter au solde système</translation>
    </message>
    <message>
        <source>System Balance pays CYBOU network fees for your Mail, Files and payments. Moving CYBOU there also counts once toward your Identity Authority.</source>
        <translation type="vanished">Le solde système paie les frais réseau de votre courrier, vos fichiers et vos paiements. Les CYBOU transférés y contribuent aussi une fois à l’autorité de votre identité.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Amount (available: %1)</source>
        <translation>Montant (disponible : %1)</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>&lt;b&gt;This cannot be undone.&lt;/b&gt; System Balance can never be sent or moved back.</source>
        <translation>&lt;b&gt;Cette action est irréversible.&lt;/b&gt; Le solde système ne peut jamais être envoyé ni récupéré.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Move to System Balance</source>
        <translation>Transférer au solde système</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>Network service fees  ·  %1</source>
        <translation>Frais de service réseau · %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>System Balance  ·  latest: %1</source>
        <translation>Solde système · dernier : %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Show each fee</source>
        <translation>Afficher chaque frais</translation>
    </message>
    <message numerus="yes">
        <source>Validated  ·  %n signature(s)</source>
        <translation type="vanished">
            <numerusform>Validé  ·  %n signature</numerusform>
            <numerusform>Validé  ·  %n signatures</numerusform>
        </translation>
    </message>
    <message>
        <location line="+20"/>
        <source>System Balance  ·  %1</source>
        <translation>Solde système · %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>System Balance</source>
        <translation>Solde système</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Available</source>
        <translation>Disponible</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Show details</source>
        <translation>Afficher les détails</translation>
    </message>
    <message>
        <location line="-102"/>
        <source>System Balance pays CYBOU network fees and storage for your Mail, Files and payments.</source>
        <translation>Le solde système paie les frais réseau CYBOU et le stockage de votre courrier, de vos fichiers et de vos paiements.</translation>
    </message>
</context>
</TS>
