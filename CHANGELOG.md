# Changelog
## [1.3.0] - 2026-06-14
### Added
- Implemented 1-bit habit tracker synced with practice data
- Habit editing, custom colors, sync options, and deletion.
- Add monochrome theme
- Add simple meditation exercise with background music

## [1.2.6] - 2026-06-12
### Fixed
- Import now replaces only matching session files from the ZIP and never removes local sessions missing from the import.
- Fixed web scaling behavior.

## [1.2.5] - 2026-06-12
### Added
- BTC address for donations

## [1.2.4] - 2026-06-11
### Fixed
- Android sessions continue correctly when the notification shade causes focus loss with background playback enabled.
- Android safe-area insets now offset the viewport at the top instead of creating bottom-only padding.
- Progressive start speed defaults to 3, normal speed defaults to 6, and the start speed editor preview animates at the exact selected speed.
- Dropdown menu text is clipped cleanly while scrolling.

## [1.2.3] - 2026-06-10
### Fixed
- History editing now scrolls to the active field once and keeps 16px Unifont row text aligned without clipping.

## [1.2.2] - 2026-06-10
### Fixed
- Android touch targets now align correctly on devices with navigation bar window insets.

## [1.2.1] - 2026-06-10
### Added
- Breath hold display mode setting with circle progress or stopwatch display
- Slower speed scale and progressive start speed control

## [1.2.0] - 2026-06-08
### Added
- Flint-based UI and theme system
- Session volume slider with immediate persistence
- Additional language support: Czech (Čeština), Indonesian (Bahasa Indonesia), Japanese (日本語), Korean (한국어), and Chinese (中文)
- Proper font glyphs
- File picker for import

## [1.1.9] - 2026-06-04
### Added
- Hierarchical settings organization with 3 main categories
- Category selection screen with card-based navigation
- Back button for hierarchical navigation
### Fixed
- Settings navigation state and Android back button behavior

## [1.1.8] - 2026-06-01
### Added
- Editable history days with pencil actions for sessions and rounds
- Inline history editing for session time and round seconds with cursor movement, click-to-position, validation, and save action
- Confirmation modal before deleting history sessions or rounds
- Sound settings page and optional advanced session controls setting
### Fixed
- Incomplete or zero-second rounds are discarded instead of being saved or shown in results
- History edit view no longer shows average time on session rows
- Tutorial and navigation buttons resize more reliably across narrow viewports
- Reduced excessive page side padding and improved small-screen layout behavior

## [1.1.7] - 2026-05-31
### Added
- First-run language picker and in-app language switching
- Spanish, French, and Portuguese translations
- Localized Android/F-Droid metadata for German, Spanish, French, and Portuguese
- Explicit Save Results and Discard actions on the results screen
### Fixed
- Language dropdown now renders above other controls
- Tutorial blank lines now render correctly from locale files
- Preview breathing circle now reflects the selected speed directly
- History round details now appear directly under the selected session
- Removed confusing delete button from history session rows

## [1.1.6] - 2026-05-30
### Added
- Background meditation timer - sessions continue when screen is off
- "Play in background" toggle in breathing settings (Android only)
- Android wake lock support with user control
- Background sound playback
### Fixed
- Meditation pausing when screen turns off
- Thread synchronization issues between main loop and background timer
- Toggle button now uses theme colors instead of hardcoded colors

## [1.1.5] - 2026-05-29
### Fixed
- Results page layout improvements:
  - Consistent side padding (32px) across all pages
  - Increased spacing between average time and round times
  - Left-aligned round times for better readability
  - Fixed scrollbar appearing prematurely (corrected height calculations)
  - Accurate content height measurements for better scroll behavior
- Dropdown menu hover highlighting no longer clipped at bottom edge
- Text layout height calculation now matches actual rendered height
- Tutorial page content height calculations fixed (no more premature scrollbars)

## [1.1.4] - 2026-05-29
### Added
- Proper text reflow with word wrapping
### Fixed
- GitHub releases and F-Droid builds now include all assets

## [1.1.3] - 2026-05-29
### Fixed
- Icon button fallback system - buttons now render with procedural graphics when asset images fail to load (prevents empty buttons on devices with asset loading issues)
- Added UIIconType system for icon identification and fallback rendering
- Updated all icon button functions to support fallback graphics (ui_draw_icon_btn, ui_draw_icon_btn_padded, ui_draw_icon_link, ui_draw_nav_button, ui_draw_nav_button_expand)

## [1.1.2] - 2026-05-29
### Fixed
- Data storage location
- Punch hole spacing
- Crashes on newer devices

## [1.1.1] - 2026-05-28
### Fixed
- Slider focus now properly releases when mouse button is released (prevented adjusting sliders when clicking outside)

## [1.1.0] - 2026-05-28
### Added
- Tiny version bump
### Fixed
- Changelog extraction now correctly populates fastlane metadata

## [1.0.9] - 2026-05-28
### Added
- Android share sheet for data export (native Intent integration)
- Direct sharing to Telegram, Downloads, or any app that handles ZIP files
- FileProvider support for secure file sharing on Android 7.0+
### Fixed
- ZIP export now uses native Android share sheet instead of saving to private storage
- JNI class loading fix for ShareHelper on native threads

## [1.0.8] - 2026-05-28
### Added
- Android back button support with screen-aware behavior
- Confirmation modal when exiting active sessions (Save & Exit, Discard, or Cancel)
- Session data preservation: completed rounds can be saved before exiting
### Fixed
- Modal text word wrap to handle last word correctly
- Back button closes modal (cancel action) when pressed during confirmation

## [1.0.7] - 2026-05-27
### Added
- F-Droid metadata (screenshots, feature graphic, icon)
- Automatic version tag generation for releases
- Fastlane changelog integration

## [1.0.6] - 2026-05-27
### Fixed
- Bell now plays before the last breath (at maxbreaths - 1) instead of after
- Session history not loading and potential segfault due to path buffer overflow (increased HISTORY_PATH_SIZE from 96 to 256)
- Tutorial text clarified to explicitly mention Wim Hof Method

## [1.0.5] - 2026-05-27
### Added
- Theme system with 8 color themes (Forest, Ocean, Sky, Sunset, Lavender - each with light/dark variants)
- Settings dropdown menu for easier navigation
- Improved fullscreen mode with better DPI scaling
### Fixed
- Touch input scaling for dropdown menus and toggle switches
- Tab bar positioning on Android (now accounts for system navbar)
- Removed toggle switch circle indicator for cleaner UI

## [1.0.4] - 2026-05-26
### Added
- Centralized icon size management system (SMALL, MEDIUM, LARGE)
- Standardized icon button functions for consistent sizing
### Fixed
- Improved accessibility with larger icons for older users (+25-37%)
- DPI scaling for all button padding and bevel borders on mobile
- X button positioning and scaling on high DPI screens
- Button dimensions and icon centering across all screens
- Missing icons (telegram, globe, stripe, monero, home, trash) in WebAssembly build

## [1.0.3] - 2026-05-26
### Fixed
- Improved DPI scaling for scrollbars (width and thumb height now scale properly)
- Fixed scrollbar not appearing on tall viewports/settings screens (content height now scales correctly)
- Fixed X icon too small on mobile (increased max size from 18px to 28px for better touch targets)
- Improved scrollbar thumb touch hit area for easier grabbing
- Fixed missing breath sounds during recovery phase (breath in at start, breath out after 15-second hold)
### Added
- Added external links in settings (Telegram, Website, Monero, Stripe donation)
- Added content-area drag scrolling for mobile (click and drag anywhere to scroll)

## [1.0.2] - 2026-05-26
### Fixed
- Fixed "Starting in" countdown text positioning (now displays above circle)
- Fixed first round to always use 3-second countdown regardless of settings
- Fixed slider knob vertical centering (DPI scaling issue)
- Fixed slider touch hit area scaling for better mobile interaction
- Fixed speed meter position in tutorial (removed double DPI scaling)
- Fixed results screen DPI scaling for consistent appearance

## [1.0.1] - 2026-05-26
### Fixed
- Fixed WebAssembly local storage persistence
- Updated Android target SDK version

## [1.0.0] - 2026-05-25
### Added
- Initial release
- Breathing exercise tutorial with 5 steps
- Configurable settings: speed (1-9), rounds (1-10), breaths (15-80), pause duration (0-30s)
- Session history tracking and statistics
- Support for Linux, Windows, Android, and WebAssembly platforms
