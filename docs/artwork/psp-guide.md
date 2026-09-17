# PSP control-diagram illustration

Generated with the built-in image model on 2026-09-17 at the user's request.
Current source: `psp-guide-1004-source.png`; runtime:
`../../ports/extremetuxracer/data/textures/psp-guide.png`. The first, superseded
generic illustration is retained as `psp-guide-source.png`.

The source is resized to a 512 × 256 power-of-two texture. It is displayed at the original landscape proportions. The PSP-specific texture cap is increased only for this asset; the RGB565 upload uses 256 KiB. Labels and leader lines are rendered by the game for native-resolution readability and accurate control mapping. They are not invented lettering baked by the image model. The graphic is a stylized illustration, not a precise hardware schematic.

The illustrated screen is now covered at runtime by `psp-guide-gameplay.png`,
an unretouched 480 × 272 framebuffer capture from the physical PSP running
Frozen River on 2026-09-17. The game draws it as a separate texture inside the
screen bezel; it is **not** passed through an image model. Only the surrounding
console illustration and winter background are generated artwork.

```sh
magick docs/artwork/psp-guide-1004-source.png -filter Lanczos -resize '512x256!' ports/extremetuxracer/data/textures/psp-guide.png
```

## Photo-referenced PSP-1004 revision

Built-in imagegen, not the CLI fallback. Inputs: previous runtime guide as edit
target/composition reference, Stefano Palazzo's genuine PSP-1004 photograph,
and Evan-Amos's original-generation PSP photograph as supplemental detail
reference. Attribution and licenses: [references/README.md](references/README.md).

### Final revision prompt

Use case: precise-object-edit. Asset: game control-guide BACKGROUND for a real PSP game. Image 1 is the EDIT TARGET / layout and winter background reference; image 2 is an actual Sony PSP-1004 photograph, authoritative hardware reference; image 3 is a detailed original PSP-1000 photograph showing the same generation's authentic button shapes, glossy black housing, perforated top speaker grille, silver edges and bottom HOME/VOL/brightness/music/SELECT/START buttons. Replace the generic illustrated console in image 1 with a meticulous photo-real original black PSP-1004 based on the real photo references. No fake redesigned controller, no PSP Slim/Go/Vita. Straight-on perfectly horizontal front orthographic view, no perspective skew, full authentic silhouette. Retain the understated snow-white/ice-blue background with frost only at edges; make it restrained, premium, precise rather than cheesy cartoon. Landscape 16:9 final composition. Console occupies x=20%..80%, y=28%..73%, centered; screen opening must be an axis-aligned 16:9 rectangle approximately x=32.8%..67%, y=30.8%..64.8%. Keep exact real PSP-1004 anatomy and correct D-pad, lower-left analog nub, face-button diamond, transparent L/R shoulder buttons, correct tiny original hardware markings. IMPORTANT: SCREEN MUST BE UNIFORM PURE BLACK, completely empty, no invented game scenery, no wallpaper, no reflections over screen; the real unchanged PSP gameplay screenshot is inserted by game code afterward. Leave generous clean blank margins for native labels: x<20%, x>80%, y<25%, y>76%. No callout lines, no extra labels/captions/slogan/UI, no hands, no wooden surface, no strap, no unrelated props, no watermark. Preserve believable hardware details from actual photographs; reference 1 supplies composition only, NOT hardware design. Finely detailed black plastic and subtle cool studio lighting.

## Superseded initial prompt

Use case: stylized-concept. Asset type: a game onboarding control-diagram BACKGROUND illustration for Extreme Tux Racer on PSP, landscape 16:9. Production illustration, not a UI mockup. A single beautifully drawn black Sony PSP handheld console, perfectly straight-on front orthographic view, centered horizontally and vertically, taking exactly the middle 56 percent of canvas width (x=22% to 78%) and roughly y=30% to 72%. Keep accurate recognisable PSP silhouette: rounded horizontal black body, wide central screen, D-pad cross on left, analog nub lower left, four small circular action buttons on right in diamond arrangement with triangle at top, circle at right, cross at bottom and square at left; L/R shoulder controls along the two top corners; tiny START button below the screen toward the right. Device screen shows a tiny cheerful snowy downhill landscape and a penguin racing away, no text. Cool daylight catches black plastic and polished rim, slight tasteful blue reflections, very crisp shape, friendly winter-game manual illustration rather than photoreal product advertising. Backdrop: light icy sky-blue snowy alpine paper, delicate frost crystals restricted to outermost corners and a very faint snowdrift at bottom; pale uncluttered center and generous quiet empty space to left and right of console and above/below. IMPORTANT: no labels, no callout lines, no arrows, no captions, no title, no logo text, no watermark, no extra objects or controller, no hands, no decorative UI. All control labels and thin leader lines will be drawn later by the game for exact positioning and readable text on a 480x272 screen. The left and right 20 percent bands MUST remain nearly empty and pale enough for dark blue labels. Do not crop the device. Restrained crisp high-quality painted game-manual art, snow-white and ice-blue palette, charming and clean.
