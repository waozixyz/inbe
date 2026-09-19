# Android launcher comparison

The two PNGs compare the current Android icon against all four redesign concepts, at a nominal 48 dp launcher size and in an enlarged view. They are launcher mockups, not screenshots from the connected phone.

- `01-android-circle.png`: circular adaptive-icon mask.
- `02-android-squircle.png`: representative squircle mask.

The current icon uses the actual `droid/app/src/main/res/drawable/ic_launcher_foreground.png` and its configured `#87CEEB` background. Its full foreground layer maps to 108 units, with the central 72-unit viewport masked for the launcher. Existing artwork positioning is preserved.

Candidate artwork is centered by its visible alpha bounds and fitted to a maximum 60-unit dimension inside that viewport. Both the existing sky-blue background and a proposed pale background (`#F1F8FA`) are shown. The pale background is a preview choice, not an application change. Candidate thumbnails are resampled for display; source artwork is unchanged.

The home-screen panels use illustrative wallpaper and neighboring app symbols. Launcher masks, normalization, size and animations can vary by device. The squircle shown is representative, not a claim to match a particular manufacturer's mask.

Reference: https://developer.android.com/develop/ui/compose/system/icon_design_adaptive

Interactive source: `/mnt/storage/Projects/.visualizations/inbe-android-20260919/android-icon-comparison.html`.

Verified: all 14 icon canvases render; design, mask and background controls change the previews; mobile and desktop layouts and light/dark appearance were visually checked.
