# Flycast Lean VOOT

### A dedicated Flycast runtime for Virtual-On: Oratorio Tangram M.S.B.S. Ver. 5.66

**Preserving the original NAOMI arcade experience while bringing its presentation and performance forward on modern PC hardware.**

## About the Project

*Virtual-On: Oratorio Tangram* remains one of SEGA's most distinctive arcade experiences. Its fast-paced combat, precise Twin-Stick controls, and linked cabinet battles helped define what made the original game special.

Despite several console releases, preserving the original NAOMI 5.66 experience on modern hardware presents its own challenges, particularly when it comes to display compatibility, performance, and linked multiplayer.

Flycast already provides a strong foundation for running the original arcade software. Rather than recreating the game or developing another emulator from scratch, this project builds upon that foundation with targeted improvements focused on *Oratorio Tangram*.

The initial goal was straightforward: establish a reliable, lightweight runtime that preserves the original game while removing unnecessary overhead.

As development progressed, the work expanded into areas that required deeper investigation of the original NAOMI hardware behavior and Flycast's execution pipeline.

## Beyond a Lightweight Runtime

Flycast Lean VOOT is more than a stripped-down emulator build.

The project introduces source-level improvements to Flycast's existing architecture, focusing on areas where the original game's assumptions and modern PC hardware don't always align.

One of the largest efforts has been modern display support.

Simply increasing rendering resolution or stretching the original 4:3 picture isn't enough to provide proper widescreen gameplay. Achieving genuine horizontal expansion required investigating how VOOT submits geometry, how Flycast handles PowerVR clipping and projection, and how the original game determines which objects remain visible.

These investigations led to targeted corrections that allow the game world to expand horizontally while retaining the original vertical framing and preserving the underlying gameplay.

Additional work focuses on SH-4 execution efficiency, rendering performance, cabinet communications, and reliable runtime integration.

The objective isn't to change how *Oratorio Tangram* plays. It's to address the technical limitations surrounding how the original software runs and appears today.

## Development Philosophy

The original NAOMI 5.66 release remains our reference.

Gameplay mechanics, movement, weapons, timing, and arcade behavior should remain faithful to the source material. Improvements must be measurable, reproducible, and validated against that baseline.

Not every experiment becomes a permanent improvement. Some investigations expose limitations, others require a different approach, and occasionally the original behavior proves to be the better choice.

Those findings are worth preserving alongside the successful changes.

Development documentation covers the technical decisions, implementation details, measured results, and known limitations so other developers can understand the work and build upon it.

## Part of PROJECT OT

Flycast Lean VOOT began as the dedicated execution runtime for **PROJECT OT: Virtual On Experience**, a separate fan-created project focused on bringing the original arcade experience to modern Windows systems.

PROJECT OT provides the surrounding player experience, including controller management, display configuration, audio, session coordination, and multiplayer presentation.

Flycast Lean VOOT focuses on executing the original NAOMI software.

Keeping these responsibilities separate allows the runtime to remain independently maintained and documented while supporting integration with the broader PROJECT OT framework.

## Open Development and Preservation

Flycast Lean VOOT is derived from the open-source Flycast project and retains its applicable GPL licensing obligations.

The repository documents our modifications alongside the original upstream foundation, with appropriate attribution to Flycast's developers and the research that helped inform this work.

No commercial game ROMs, NAOMI BIOS files, or proprietary SEGA assets are included.

**Preserve the game. Improve the experience.**

## Upstream Flycast

This project is derived from [Flycast](https://github.com/flyinghead/flycast). The unmodified source reference is retained on the [`upstream`](https://github.com/hyp36rmax/flycast-lean-voot/tree/upstream) branch. Technical documentation and additional repository navigation are being reviewed in [draft PR #1](https://github.com/hyp36rmax/flycast-lean-voot/pull/1).
