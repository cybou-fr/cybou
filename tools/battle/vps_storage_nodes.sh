#!/usr/bin/env bash
# Installs permanent DEVNET storage peers next to the bootstrap on the DEV VPS:
# ordinary Full Nodes cybou-storage@2 and @3 on ports 29462 and 29463.
# Beta durability needs two remote replicas at distinct StorageIds; with only the
# bootstrap reachable no publication can reach Protected.
set -euo pipefail
BIN=/home/debian/cybou/build/bin/cybou
sudo tee /etc/systemd/system/cybou-storage@.service >/dev/null <<'UNIT'
[Unit]
Description=CYBOU DEVNET storage peer %i (ordinary Full Node next to the bootstrap)
After=network-online.target cybou-node.service
Wants=network-online.target

[Service]
Type=simple
User=debian
Group=debian
WorkingDirectory=/var/lib/cybou/storage-%i
ExecStart=/home/debian/cybou/build/bin/cybou node run --network devnet --data-dir /var/lib/cybou/storage-%i/state --listen 0.0.0.0:2946%i --advertise 51.255.46.58:2946%i --peers /var/lib/cybou/storage-%i/peers.txt --peer-admission france --capacity 15GiB --event-log /var/lib/cybou/storage-%i/events.jsonl
Restart=on-failure
RestartSec=3
UMask=0077
NoNewPrivileges=true
PrivateTmp=true
PrivateDevices=true
ProtectSystem=strict
ProtectHome=read-only
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
ProtectHostname=true
LockPersonality=true
RestrictSUIDSGID=true
RestrictRealtime=true
SystemCallArchitectures=native
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
CapabilityBoundingSet=
AmbientCapabilities=
ReadWritePaths=/var/lib/cybou/storage-%i

[Install]
WantedBy=multi-user.target
UNIT
for i in 2 3; do
    dir=/var/lib/cybou/storage-$i
    sudo install -d -o debian -g debian -m 700 "$dir" "$dir/state"
    [ -d "$dir/state/geo" ] || sudo cp -r /var/lib/cybou/node/state/geo "$dir/state/geo"
    other=$((5 - i))
    printf '127.0.0.1 29461\n127.0.0.1 2946%s\n' "$other" | sudo tee "$dir/peers.txt" >/dev/null
    sudo chown -R debian:debian "$dir"
done
# Open the two ports persistently and in the running ruleset.
if ! grep -q '29462' /etc/nftables.conf; then
    sudo sed -i 's/tcp dport { 22, 29461 } accept/tcp dport { 22, 29461, 29462, 29463 } accept/' /etc/nftables.conf
    sudo nft -c -f /etc/nftables.conf && sudo nft -f /etc/nftables.conf
fi
sudo systemctl daemon-reload
sudo systemctl enable --now cybou-storage@2.service cybou-storage@3.service
sleep 8
systemctl is-active cybou-node.service cybou-storage@2.service cybou-storage@3.service
sudo ss -ltnp | grep -E '2946[123]'
