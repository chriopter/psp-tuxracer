# PSP control atlas

Created with the built-in image-generation tool on 2026-09-17 at the user's request.
Original: `psp-controls-source.png`. Runtime: `../../ports/extremetuxracer/data/textures/psp-controls.png`.

The original generated alpha is preserved. Runtime preparation uses ImageMagick Lanczos resizing to 256 × 128, with no redrawing or substituted symbols. Four columns, two rows; each runtime cell is 64 × 64. Order: Cross, Circle, Square, Triangle, Start, L, R, D-pad. The renderer uses one shared RGBA4444 texture (64 KiB), with the procedural icons retained only as a missing-asset fallback.

Regenerate the runtime asset:

```sh
magick docs/artwork/psp-controls-source.png -filter Lanczos -resize '256x128!' ports/extremetuxracer/data/textures/psp-controls.png
```

## Final prompt

Use case: stylized-concept. Asset type: production-ready game UI controller icon sprite atlas for Extreme Tux Racer PSP. Generate ONE precisely aligned 4-column by 2-row sprite sheet, landscape 2:1 aspect ratio, ideally 1024x512. Each equal square cell contains one centered button icon, identical apparent size, generous 15 percent padding. Row 1 left to right: blue CROSS (X), coral pink hollow CIRCLE (O), lavender hollow SQUARE, mint green hollow upright TRIANGLE. Row 2 left to right: START button as a simple right-pointing play triangle (ice white), left shoulder button with ONLY the letter L, right shoulder button with ONLY the letter R, four-direction D-PAD silhouette (not an X). Strict eight-icon order. Style: coherent premium PSP-era handheld controls for an alpine racing game, front-facing orthographic, dark midnight navy circular button bodies with very subtle icy blue beveled rims, crisp bold thick high-contrast glyphs, restrained shading. Shoulder L/R bodies may be softly rounded squares but stay in the same cell footprint. Must remain instantly legible when each cell is rendered at 24 pixels wide. Background: uniform flat solid midnight navy #0c1929 in all cells. No grid lines, no dividers, no labels outside buttons, no additional words, no logos, no snowflakes, no scenery, no perspective, no mockup, no glow haze, no thin intricate lines. Full sheet edge-to-edge, equal cell spacing and aligned centers, all eight icons complete and uncropped. Colors should harmonize with #12263a header panels, cyan #42aed4 accents and pale #e1eff8 text.
