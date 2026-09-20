import Base

type Platform is Data:
  Android{}
  Web{}
  Windows{}
  Posix{}

type Presence is Data:
  Absent{}
  Present{}

type Move is Data:
  Succeeded{}
  Failed{}

type Root is Data:
  Current{}
  Legacy{}

type DirName is Data:
  Inbe{}
  Breathing{}
  BreathSession{}

type DbName is Data:
  InbeDb{}
  BreathingDb{}

type EntryName is Data:
  BreathingDataDb{}
  InbeDataDb{}

type Importable is Data:
  Yes{}
  No{}

type Same is Data:
  Different{}
  Same{}

# The directory name the current layout uses, per platform.
def current_dir(platform: Platform) -> DirName:
  match platform:
    case Android{}:
      Inbe{}
    case Web{}:
      Inbe{}
    case Windows{}:
      Inbe{}
    case Posix{}:
      Inbe{}

# The directory name the immediately previous layout used, per platform.
def legacy_dir(platform: Platform) -> DirName:
  match platform:
    case Android{}:
      Breathing{}
    case Web{}:
      Breathing{}
    case Windows{}:
      BreathSession{}
    case Posix{}:
      Breathing{}

def dirs_distinct(current: DirName, legacy: DirName) -> Same:
  match current:
    case Inbe{}:
      match legacy:
        case Inbe{}:
          Same{}
        case Breathing{}:
          Different{}
        case BreathSession{}:
          Different{}
    case Breathing{}:
      match legacy:
        case Inbe{}:
          Different{}
        case Breathing{}:
          Same{}
        case BreathSession{}:
          Different{}
    case BreathSession{}:
      match legacy:
        case Inbe{}:
          Different{}
        case Breathing{}:
          Different{}
        case BreathSession{}:
          Same{}

def choose(legacy: Presence, current: Presence, move: Move) -> Root:
  match legacy:
    case Absent{}:
      Current{}
    case Present{}:
      match current:
        case Present{}:
          Legacy{}
        case Absent{}:
          match move:
            case Succeeded{}:
              Current{}
            case Failed{}:
              Legacy{}

def current_db() -> DbName:
  InbeDb{}

def legacy_db() -> DbName:
  BreathingDb{}

# The export bundle entry is frozen for backup compatibility.
def export_entry() -> EntryName:
  BreathingDataDb{}

# Every entry format ever written by an export stays importable.
def importable(entry: EntryName) -> Importable:
  match entry:
    case BreathingDataDb{}:
      Yes{}
    case InbeDataDb{}:
      Yes{}
