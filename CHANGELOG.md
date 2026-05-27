# Changelog
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
