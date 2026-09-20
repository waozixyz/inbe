import Base
import ./main.bend as Policy

law fresh_install_uses_current:
  for +current: Policy.Presence
  for +move: Policy.Move
  {Policy.choose(Policy.Absent{}, current, move) == Policy.Current{} : Policy.Root}

law successful_move_uses_current:
  {Policy.choose(Policy.Present{}, Policy.Absent{}, Policy.Succeeded{}) == Policy.Current{} : Policy.Root}

law failed_move_keeps_legacy:
  {Policy.choose(Policy.Present{}, Policy.Absent{}, Policy.Failed{}) == Policy.Legacy{} : Policy.Root}

law conflict_keeps_legacy:
  for +move: Policy.Move
  {Policy.choose(Policy.Present{}, Policy.Present{}, move) == Policy.Legacy{} : Policy.Root}

law current_dir_is_inbe_everywhere:
  for +platform: Policy.Platform
  {Policy.current_dir(platform) == Policy.Inbe{} : Policy.DirName}

law legacy_dirs_differ_from_current:
  for +platform: Policy.Platform
  {Policy.dirs_distinct(Policy.current_dir(platform), Policy.legacy_dir(platform)) == Policy.Different{} : Policy.Same}

law current_db_is_inbe:
  {Policy.current_db() == Policy.InbeDb{} : Policy.DbName}

law legacy_db_is_breathing:
  {Policy.legacy_db() == Policy.BreathingDb{} : Policy.DbName}

law export_entry_is_importable:
  {Policy.importable(Policy.export_entry()) == Policy.Yes{} : Policy.Importable}

law historical_entries_stay_importable:
  for +entry: Policy.EntryName
  {Policy.importable(entry) == Policy.Yes{} : Policy.Importable}
