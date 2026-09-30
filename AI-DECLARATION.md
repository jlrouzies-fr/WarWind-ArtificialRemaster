---
version: "0.1.2"
level: copilot
processes:
  design: pair
  implementation: copilot
  testing: pair
  documentation: copilot
  review: pair
  deployment: assist
components:
  mod: copilot
  cnc-ddraw-patches: copilot
  shaders: copilot
  tools: copilot
  cutscenes: assist
  docs: copilot
---

This format is based on [AI-DECLARATION.md](https://ai-declaration.md/en/0.1.2).

## Notes from myself

War Wind Artificial Remaster is AI-written under human direction, and it would be misleading to
present it any other way.

I am a software engineer and IT administrator, not a reverse engineer or a game modder. What I
claim is the direction and the proving: deciding that a 1997 game should get a widescreen view, a
modern HUD and a lighting pass while `WW.EXE` stays untouched, choosing what to build next, saying
when a menu or an effect is wrong, and then playing every build on the real renderer, which is the
only place any of this is proven. I also make the releases.

I take no credit for what this stands on. War Wind is DreamForge Intertainment's and SSI's,
cnc-ddraw is FunkyFr3sh's, and the cutscene upscaling is done by Real-ESRGAN and RIFE. See
[Credits](README.md#credits).

## AI notes

What that split looks like in practice:

- **The author owns the decisions and the judgement of the result:** what to build, how the
  game should feel and look, which layout, which effect is right and which is too much. Most
  of the visual work went through the author's play-tests and screenshots of what was wrong:
  the glow too big, the shadows too faint or not matching the sprites, a menu that did not
  read right.
- **Claude owns the writing:** the reverse engineering of `WW.EXE`, the mod DLL, the
  cnc-ddraw patches, the Modern Graphics shaders, the Python and PowerShell tooling, the
  installer and the documentation. It proposes designs and reports what it verified and what
  it only assumed.
- **Testing is shared.** Claude tests in a sandbox that runs the game on a hidden desktop and
  renders frame dumps offline; the author play-tests on the real renderer, which is what
  catches the faults a sandbox cannot see.
- **Cutscenes: `assist`.** The upscaling itself is done by Real-ESRGAN and RIFE; the pipeline
  around them is AI-written.
- **Deployment: `assist`.** The packaging scripts are AI-written; releases are made and
  checked by the author.

**Generated media.** Unlike the code, the upscaled cutscenes and `assets/backdrop.png` are
AI-processed versions of the original game's video and art (Real-ESRGAN and RIFE are neural
models). Nothing in them is new art: they are the game's own frames, enlarged. The rest of the
repository is code, shaders, tooling and documentation, and the original game files are not
included.

This declaration covers the maintainer's own work. Third-party components (cnc-ddraw and the
tools listed in the README credits) are the work of their own authors.
