# Changelog
## [1.8.9] - 2026-07-17
### Changed
- Fixed Android native builds by embedding the current Noto font sources instead of stale generated `assets/fonts/ui.*` files.
- Fixed Android UI font loading by enabling TTF/OTF support in the Android raylib build.
- Updated sun-salutation assets to use pose sheets while keeping the figure setting focused on the man sequence for now.
- Improved release packaging automation, Linux package targets, and store metadata.

## [1.8.8] - 2026-07-13
### Changed
- Updated Flint icon integration to use categorized sprite sheets for app UI, language, payments, platforms, tiles, and profile pictures.
- Updated Inbe to use the new Flint icon categories and removed legacy eye icon usage.
- Fixed profile picture picker selection so it closes the picker, keeps the sidebar open, and does not click through to sidebar links.

## [1.8.7] - 2026-07-12
### Changed
- Fixed Firefox web persistence, mobile habit deletion, and customizable nav tap handling.
- Added a blank-home easter egg screen and updated embedded asset lists.
- Updated release and store packaging automation.

## [1.8.6] - 2026-07-11
### Changed
- Added Firefox add-on packaging, release metadata updates, and CI/release automation.
- Improved web audio asset validation and Firefox/LibreWolf smoke testing.
- Fixed stale screen-local modal state so hidden modals cannot keep input captured after navigation.

## [1.8.5] - 2026-07-11
### Changed
- Updated Flint to improve file dialogs, cursor behavior, focus reset, icon slider popups, and shared raylib build locking.
- Smoothed WHM breath and recovery countdown progress so the ring advances steadily during each breath and countdown.
- Improved session controls, release-store publishing files, and build packaging metadata.

## [1.8.4] - 2026-07-11
### Changed
- Reworked Profile configuration navigation to use a full-width dropdown instead of the tab bar.
- Updated sidebar return behavior for Profile and Settings flows.
- Updated Flint profile widgets and profile picture assets, including sidebar profile image sizing and picker scrolling.

## [1.8.3] - 2026-07-10
### Changed
- Improved ui in practice screen

## [1.8.2] - 2026-07-09
### Changed
- improved desktop experience
- add system themes

## [1.8.1] - 2026-07-03
### Fixed
- storage settings for android 17+

## [1.8.0] - 2026-07-02
### Changed
- upgrade to sync protocol v3
- ui fixes

## [1.7.12] - 2026-07-01
### Changed
- fix web
- deduplicate code

## [1.7.11] - 2026-07-01
### Addded
- add more banners

## [1.7.10] - 2026-07-01
### Changed
- updated whm banner

## [1.7.9] - 2026-07-01
### Fixed
- improve sync review

## [1.7.8] - 2026-07-01
### Changed
- Redesign the Practice home screen around carousel-style practice cards with explicit start, manual, configure, and music actions.
- Move meditation music configuration into a shared practice music panel with per-practice enablement and track selection.

## [1.7.7] - 2026-07-01
### Changed
- Move habit tracking overview out of Profile and into the Habits tab with direct day toggles, reorder controls, and local view persistence.
- Rework Settings into an overview page with section title bars and shared return/title bar UI.

## [1.7.6] - 2026-06-30
### Added
- habit overview

## [1.7.5] - 2026-06-29
### Fixed
- Fix Android 15 relaunch freeze.
- Code cleanup

## [1.7.4] - 2026-06-29
### Fixed
- Clean up practice stats, friend removal, sync review, and Android inset handling.

## [1.7.3] - 2026-06-29
### Fixed
- bottom inset in practice

### Changed
- Improve habit management

## [1.7.2] - 2026-06-29
### Added
- Add local `make install` target.
- Add Profile controls for habit ordering and practice visibility.

### Changed
- Move sync status into the Data view and simplify the habits add option.
- Clean up the README.

## [1.7.1] - 2026-06-28
### Changed
- Move shared Lyra sync protocol code into Flint and simplify Inbe sync client wiring.

## [1.7.0] - 2026-06-28
### Changed
- Rework Android safe-area handling to use one native inset path with stable bottom navigation spacing.
- Move leaderboard stats to synced source data, with local current-user rows shown immediately.

### Fixed
- Fix profile/friend sync freshness, leaderboard empty states, modal input capture, pet asset embedding, and wrapped text previews.

## [1.6.6] - 2026-06-28
### Changed
- updated the practice screen ui/ux

## [1.6.5] - 2026-06-28
### Fixed
- Repair habit-day sync for zero-count completions.
- Require liboqs at build time for every sync-enabled target.

## [1.6.4] - 2026-06-27
### Changed
- Move meditation duration selection into practice configuration with custom time support.
- Add centered in-session meditation add-time controls.
- Use Flint-managed liboqs, LibreSSL, and curl build paths.

### Fixed
- Fix Android native build to use Flint's liboqs submodule.

## [1.6.3] - 2026-06-27
### Changed
- Improve theme, habit, and localization UI polish

## [1.6.2] - 2026-06-27
### Changed
- Improve sun salutation

## [1.6.1] - 2026-06-27
### Changed
- Improved notification in session
- Standardized settings screen1

## [1.6.0] - 2026-06-26
### Added
- Sun Salutation practice with Yoga habit sync support.

### Changed
- Improve sync reliability and review handling.

## [1.5.9] - 2026-06-26
### Fixed
- Android session notification and wakelock for screen-locked playback

## [1.5.8] - 2026-06-25
### Fixed
- Background play for meditation practice

## [1.5.7] - 2026-06-25
- Fix test music

## [1.5.6] - 2026-06-24
### Fixed
- Modal button issue

## [1.5.5] - 2026-06-24
### Fixed
- modal capture bug
- fix sync diff

## [1.5.4] - 2026-06-24
### Changed
- Use Flint-managed modal handling so modal dialogs block background input consistently.
- Add optional sync account aliases, with compact public ID display and full ID reveal modal.
- Make sync state tracking more robust with snapshot hash checks and review flow for unresolved differences.
- Use Flint's built-in font chopping pipeline and verify multilingual locale glyph coverage.

## [1.5.3] - 2026-06-23
### Fixed
- modal issues

## [1.5.2] - 2026-06-23
### Changed
- let you disable screen transitions
- added theme modal

## [1.5.1] - 2026-06-22
### Fixed
- icon scaling
- fix inset bug
### Added
- toggle background music

## [1.5.0] - 2026-06-22
### Changed
- New logo
- improve tabbar ui
### Fixed
- sync issues

## [1.4.11] - 2026-06-21
### Added
- tab bar mode for habits and exercises
### Changed
- fix ut click file

## [1.4.10] - 2026-06-21
### Added
- Added guide to habits page
### Changed
- fix orientation issues

## [1.4.9] - 2026-06-21
### Fixed
- Fix Practice startup, manual navigation, configuration scrolling, and sync backfill issues.
- Add skip feature to on screen guide

## [1.4.8] - 2026-06-21
### Changed
- Fixed start screen
- Fixed schema on first sync

## [1.4.7] - 2026-06-21
### Changed
- Rework Practice into Manual, start, and configuration tabs with a new localized first-run guide.
- Simplify Meditation's manual to one page and make bottom navigation more compact.

## [1.4.6] - 2026-06-20
### Changed
- Move habit weekly, monthly, statistics, and edit views into icon tabs under the habit selector.
- Improve Flint form, modal, tab, and scrollbar widgets for measured layouts and cleaner scrolling.

### Fixed
- Prevent habit info modal text from overlapping the OK button.

## [1.4.5] - 2026-06-20
### Changed
- Fix spacing issues and translations

## [1.4.4] - 2026-06-20
### Added
- Add Russian localization.
- Add a habit stats view with streak, recent activity, weekday, and linked-practice diagrams.
- Add optional in-session volume controls, hidden by default.
- Add Mint and Cobalt themes.

### Changed
- Use one canonical pixel font atlas with Flint-managed DPI scaling and cleaner title sizing.
- Simplify habit counting so multiple counts are opt-in and independent of linked practices.
- Improve habit weekly/calendar spacing, mobile calendar touch feedback, and theme picker ordering.
- Reorganize repeated screen UI into reusable Flint helpers.

### Fixed
- Build the Ubuntu Touch Click package with the OpenStore package id
- Refactor code base
- Sync habit multiple-count settings correctly across clients and preserve queued local habit edits.
- Preserve linked session day behavior while hiding redundant count badges.

## [1.4.3] - 2026-06-20
### Fixed
- Refresh expired native sync auth tokens before sending queued changes.
- Require first-time meditation users to continue through the tutorial before starting.

## [1.4.2] - 2026-06-20
### Fixed
- Improve sync auth, websocket reconnects, and queued habit/session updates.
- Reduce SQLite-related UI flicker and speed up bulk import sync queueing.
- Fixed dropdown menus so temporary option lists cannot crash theme/device selection.
- Fixed Linux test and release builds to link against the vendored curl library path.

## [1.4.1] - 2026-06-19
### Changed
- Improve account management

## [1.4.0] - 2026-06-19
### Added
- optional sync client, offline account creation
- simplify tutorial

## [1.3.6] - 2026-06-18
### Fixed
- Fixed Tickmate `.db` imports so zero-based months map correctly and long habit histories load without dropping later dates.

## [1.3.5] - 2026-06-18
### Fixed
- Improved Android data import for `.db` backups, weekly habit rows, and import conflict handling.

## [1.3.4] - 2026-06-18
### Changed
- Reorganized WHM and Meditation into practice-owned modules for cleaner code boundaries.
- Split the meditation manual into an image intro page and a shorter music/gong setup page.
- Expanded release automation to build Web, Linux AppImage, Windows, APK, and AAB artifacts after tests pass.

## [1.3.3] - 2026-06-17
### Fixed
- Import now restores habits, habit sync settings, and optionally app settings from Inbe SQLite exports.

## [1.3.2] - 2026-06-17
### Fixed
- Unified scroll containers so content stays out of scrollbar tracks and scroll positions clamp cleanly at the bottom.
- Android meditation audio downloads now show a localized network-permission/connectivity modal when the host cannot be resolved.
- Windows builds now support runtime meditation audio downloads.

## [1.3.1] - 2026-06-17
### Fixed
- Fixed Android orientation resizing and habit import merging by name.

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
