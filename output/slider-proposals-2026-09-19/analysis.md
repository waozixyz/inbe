# Slider proposal

Status: visual proposals only, generated with the built-in image tool. No slider redesign has been implemented.

## Findings

- Inbe `draw_scale_row` shifts slider bounds right by the minus-button width and gap. This also shifts the label, whereas other settings labels use the full row's left edge.
- Kryon's `SliderLabelTextPaintFor` positions the label above the supplied bounds using font size plus a gap. The header therefore lacks space owned by the widget's layout.
- `SliderCellTextPaintFor` vertically centers the numeric value in the cell, placing it across the track. A foreground suitable for the background can have poor contrast against the filled track.
- Inbe passes the stored integer scale in tenths with `%d`, displaying `10`. It computes a separate effective DPI multiplier string but does not use it. The proposed user-facing value is the preference multiplier, `1.0×`, independent of device density.

## Recommendation

Use proposal 01 with the precise alignment shown in `recommended.png`: no additional card, label top-left at the settings margin, current value top-right, track and optional step buttons below. The image-generation outputs are illustrative; actual geometry and tick count must be deterministic in code.

## Core implementation plan

1. Extend the existing `Slider(SliderProps)` surface, not an Inbe-specific replacement. Core layout measures the entire header, interaction row, and optional supporting text inside its bounds.
2. Keep the header aligned to the outer widget bounds; inset only the track when optional decrement/increment controls are present. Default slider label inset should not silently inherit button padding.
3. Use measured text line height, wrap long labels, reserve value width, and grow the overall component height. Define behavior for narrow widths and right-to-left layout.
4. Give label, displayed value, value indicator, track, active track, thumb, and focus state independent semantic style roles. Use on-surface text for the header; pair an optional chip's background with its own contrasting foreground.
5. Format display/accessibility values separately from storage. Scale settings use 0.5×–2.5× in 0.1× increments while retaining integer-tenths storage. Do not change the applied scale during drag; preserve the current commit-on-release behavior.
6. Preserve keyboard adjustment, disabled state, touch capture, cancellation, and accessibility semantics. Proposed minimum interaction targets are 48 dp; visible track/thumb need not fill that area.
7. Keep layout decisions in shared Kryon runtime code and drawing in the relevant backend. Validate native/Go parity, all supported styles, dark/light palettes, narrow screens, large text, pointer endpoints, and long translations.
8. Implement and commit in canonical Kryon master first, then update Inbe's clean vendor pointer and simplify its row layout. Never edit the vendored runtime.

Optional proposal 03 adds a selected/dragging value indicator and discrete tick marks. Material's official slider documentation supports formatted value labels and discrete steps; the exact spacing in these mockups is a proposal, not a quoted specification.

Reference: https://github.com/material-components/material-components-android/blob/master/docs/components/Slider.md

Prompts: `prompt.md` and `detail-prompt.md`. Images: `comparison.png` and `recommended.png`.
