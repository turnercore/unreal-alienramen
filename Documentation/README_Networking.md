# Networking Docs

Networking/session lifecycle is owned by `UARSessionSubsystem` and settings-driven routing.

This is the shared networking entry point for all three game modes.

## Core page

- [Online session subsystem](README_SessionSubsystem.md)

This includes LAN/online routing, stay-offline policy, seat caps, and backend expansion guidance.

## Read This When

- a gameplay feature must work in listen-server plus remote-client play
- couch co-op and online/LAN seat behavior affects the flow
- Shop, Invader, or Scrapyard UI is trying to call backend-specific session nodes directly
- mode travel/session lifecycle ownership is unclear
