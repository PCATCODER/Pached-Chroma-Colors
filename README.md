# Rainbow Icon patched

Changes:
- Added an Enabled toggle.
- Added an Affect setting:
  - 0: Color 1
  - 1: Color 2
  - 2: Glow
  - 3: Outline
  - 4: All
- Prevented duplicate SimplePlayer tracking.
- Added safer hue wrapping.
- The in-level hook now respects the selected target.
- Existing palette-sync behavior is preserved.

Note: the Affect control is an integer slider because this project does not currently define a custom enum setting. The labels above map the values.
