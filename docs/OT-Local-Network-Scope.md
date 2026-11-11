# Project OT local network scope

`network:LoopbackOnly` defaults false. Existing standalone and LAN behavior remains unchanged when false. Same-PC Project OT cabinets opt in: their original UDP transport binds 127.0.0.1, disables broadcast/UPnP and rejects external destinations. Payload layout, sequence handling, communication-board synchronization and guest timing are unchanged. The explicit local peer is 127.0.0.1. No firewall exemption is installed.

Project OT Solo disables unused network output, GGPO, satellite, battle cable, BBA and UPnP at its existing launch-argument boundary. LAN explicitly turns loopback-only off and retains its existing 38900/TCP metadata and 38901/38902 UDP transport and MTU behavior. The runtime remains GPL Flycast-derived software, retains flycast.exe filename, asInvoker execution and licensing. The dedicated integration may use the approved Project OT icon and accurate Flycast Lean description; neither is a signing identity.

The installed executable path is content-addressed and stable for one payload in one installation. New payloads and separate installations intentionally have different paths. Do not evade Windows policy by replacing an approved executable in place. Windows controls firewall authorization, SmartScreen/reputation and enterprise policy; physical prompt behavior must be observed separately.
