import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { checkLaws } from '../vendor/kryon/tools/bend-laws.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const platforms = ['Android', 'Web', 'Windows', 'Posix'];
const presence = ['Absent', 'Present'];
const moves = ['Succeeded', 'Failed'];
const requiredLaws = ['fresh_install_uses_current', 'successful_move_uses_current',
    'failed_move_keeps_legacy', 'conflict_keeps_legacy', 'current_dir_is_inbe_everywhere',
    'legacy_dirs_differ_from_current', 'current_db_is_inbe', 'legacy_db_is_breathing',
    'export_entry_is_importable', 'historical_entries_stay_importable'];

// The single place storage name strings are defined, keyed by the proved
// constructors they belong to. Bend owns the policy; this map owns the bytes.
const dirNames = { Inbe: 'inbe', Breathing: 'breathing', BreathSession: 'BreathSession' };
const dbNames = { InbeDb: 'inbe.db', BreathingDb: 'breathing.db' };
const entryNames = { BreathingDataDb: 'breathing-data/breathing.db', InbeDataDb: 'inbe-data/inbe.db' };

const c = name => `main.${name}`;

export async function generateStorageLayout(packageDir, output) {
    const proof = await checkLaws(path.join(packageDir, 'PROOF.bend'));
    assert.deepEqual(proof.laws.toSorted(), requiredLaws.toSorted(), 'storage.layout.required_laws');

    const decisions = proof.table('main.choose');
    assert.deepEqual(decisions.domains,
        [presence.map(c), presence.map(c), moves.map(c)], 'storage.layout.choose_domains');

    // Rows arrive depth-first over the asserted domains: legacy, current, move.
    const rootOf = value => {
        assert.ok(value.constructor === c('Current') || value.constructor === c('Legacy'),
            'storage.layout.choose_value');
        return value.constructor === c('Current') ? 0 : 1;
    };
    const table = [[[0, 0], [0, 0]], [[0, 0], [0, 0]]];
    assert.equal(decisions.rows.length, 8, 'storage.layout.choose_rows');
    decisions.rows.forEach((row, index) => {
        const legacy = presence.findIndex(name => row.arguments[0] === c(name));
        const current = presence.findIndex(name => row.arguments[1] === c(name));
        const move = moves.findIndex(name => row.arguments[2] === c(name));
        assert.ok(legacy >= 0 && current >= 0 && move >= 0, 'storage.layout.choose_row');
        assert.equal(index, (legacy * 2 + current) * 2 + move, 'storage.layout.choose_order');
        table[legacy][current][move] = rootOf(row.value);
    });

    const perPlatform = name => {
        const result = proof.table(`main.${name}`);
        assert.deepEqual(result.domains, [platforms.map(c)], `storage.layout.${name}_domain`);
        assert.equal(result.rows.length, platforms.length, `storage.layout.${name}_rows`);
        return result.rows.map(row => {
            assert.ok(row.arguments[0] === c(platforms[result.rows.indexOf(row)]),
                `storage.layout.${name}_order`);
            const constructor = row.value.constructor.slice('main.'.length);
            assert.ok(dirNames[constructor] !== undefined, `storage.layout.${name}_string_missing`);
            return constructor;
        });
    };
    const currentDirs = perPlatform('current_dir');
    const legacyDirs = perPlatform('legacy_dir');

    const scalar = (name, map, label) => {
        const result = proof.table(`main.${name}`);
        assert.deepEqual(result.domains, [], `storage.layout.${name}_domain`);
        assert.equal(result.rows.length, 1, `storage.layout.${name}_rows`);
        const constructor = result.rows[0].value.constructor.slice('main.'.length);
        assert.ok(map[constructor] !== undefined, `storage.layout.${label}_string_missing`);
        return map[constructor];
    };
    const currentDb = scalar('current_db', dbNames, 'current_db');
    const legacyDb = scalar('legacy_db', dbNames, 'legacy_db');
    const exportEntry = scalar('export_entry', entryNames, 'export_entry');

    const importable = proof.table('main.importable');
    assert.deepEqual(importable.domains, [Object.keys(entryNames).map(c)],
        'storage.layout.importable_domain');
    assert.equal(importable.rows.length, Object.keys(entryNames).length,
        'storage.layout.importable_rows');
    const importEntries = importable.rows.map(row => {
        const constructor = row.arguments[0].slice('main.'.length);
        assert.equal(row.value.constructor, c('Yes'), `storage.layout.${constructor}_unimportable`);
        assert.ok(entryNames[constructor] !== undefined, 'storage.layout.importable_string_missing');
        return entryNames[constructor];
    });
    assert.ok(importEntries.includes(exportEntry), 'storage.layout.export_not_importable');

    const dirOf = constructor => dirNames[constructor];
    const platformDefine = (prefix, dirs) =>
        platforms.map((platform, index) => `#define ${prefix}_${platform.toUpperCase()} "${dirOf(dirs[index])}"`);

    const content = `/* Generated by scripts/generate-storage-layout.mjs from proved Bend ${proof.version} policy. */
#ifndef STORAGE_LAYOUT_H
#define STORAGE_LAYOUT_H

/* Root choice indexed [legacy_present][current_present][move_succeeded]:
   STORAGE_ROOT_CURRENT uses the current layout, STORAGE_ROOT_LEGACY keeps
   reading the previous one. */
#define STORAGE_ROOT_CURRENT 0
#define STORAGE_ROOT_LEGACY 1
static const int storage_layout_decisions[2][2][2] = {
${table.map(legacy => `    {${legacy.map(row => `{${row.join(', ')}}`).join(', ')}}`).join(',\n')}
};

/* Well-known names. These defines are the only source of the storage
   vocabulary; hand-typed copies in src/ are rejected by check-storage-literals. */
#define STORAGE_DIR_NAME "${dirNames.Inbe}"
#define STORAGE_DB_NAME "${currentDb}"
#define STORAGE_DB_NAME_LEGACY "${legacyDb}"
${platformDefine('STORAGE_DIR_LEGACY', legacyDirs).join('\n')}

/* Export bundle; the entry format is frozen for backup compatibility. */
#define STORAGE_EXPORT_ENTRY_DB "${exportEntry}"
#define STORAGE_EXPORT_ENTRY_META "breathing-data/metadata.json"
#define STORAGE_EXPORT_META_FORMAT "breathing-data-sqlite"
#define STORAGE_EXPORT_PREFIX "${dirNames.Inbe}"

/* Every archive entry name an import must recognize; export format first. */
#define STORAGE_IMPORT_ENTRY_COUNT ${importEntries.length}
static const char *const storage_import_entries[STORAGE_IMPORT_ENTRY_COUNT] = {
${importEntries.map(entry => `    "${entry}"`).join(',\n')}
};

/* Web pre-mount home layout. */
#define STORAGE_WEB_HOME "/home/${dirNames.Inbe}"
#define STORAGE_WEB_HOME_LEGACY "/home/${dirNames.Breathing}"

/* Scratch database names used while importing. */
#define STORAGE_IMPORT_DB_TMP "import-${dirNames.Inbe}.db"
#define STORAGE_IMPORT_DB_INSPECT "import-inspect-${dirNames.Inbe}.db"

/* Export artifacts sharing the same vocabulary. */
#define STORAGE_EXPORT_FILENAME_FMT "${dirNames.Inbe}-%lld.zip"
#define STORAGE_EXPORT_FILENAME_FALLBACK "${dirNames.Inbe}.zip"
#define STORAGE_EXPORT_SESSIONS_CSV "${dirNames.Inbe}-sessions.csv"
#define STORAGE_WEB_EXPORT_TMP "/tmp/${dirNames.Inbe}-web-export.zip"
#define STORAGE_DIR_ARCHIVE_FMT "%s.before-breathing-migration-%d"

/* Historical on-disk session filenames written before the sqlite era. */
#define STORAGE_SESSION_LEGACY_SCAN "${dirNames.Breathing}-%2d%2d%2d"

/* Platform-selected names so callers need no per-platform chains. */
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
#define STORAGE_DIR_LEGACY_HERE STORAGE_DIR_LEGACY_WEB
#define STORAGE_EXPECT_DIR_HERE STORAGE_EXPECT_DIR_WEB
#elif defined(ANDROID_BUILD)
#define STORAGE_DIR_LEGACY_HERE STORAGE_DIR_LEGACY_ANDROID
#define STORAGE_EXPECT_DIR_HERE STORAGE_EXPECT_DIR_ANDROID
#elif defined(_WIN32)
#define STORAGE_DIR_LEGACY_HERE STORAGE_DIR_LEGACY_WINDOWS
#define STORAGE_EXPECT_DIR_HERE STORAGE_EXPECT_DIR_WINDOWS
#else
#define STORAGE_DIR_LEGACY_HERE STORAGE_DIR_LEGACY_POSIX
#define STORAGE_EXPECT_DIR_HERE STORAGE_EXPECT_DIR_POSIX
#endif

/* Golden vectors for tests: the directory each platform must resolve to. */
${platformDefine('STORAGE_EXPECT_DIR', currentDirs).join('\n')}
#endif
`;
    fs.mkdirSync(path.dirname(output), { recursive: true });
    if (!fs.existsSync(output) || fs.readFileSync(output, 'utf8') !== content) {
        const temporary = `${output}.${process.pid}.tmp`;
        try {
            fs.writeFileSync(temporary, content);
            fs.renameSync(temporary, output);
        } finally {
            fs.rmSync(temporary, { force: true });
        }
    }
    return proof;
}

if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) {
    try {
        assert.ok(process.argv.length <= 3, 'usage: node scripts/generate-storage-layout.mjs [output.h]');
        const output = process.argv[2] ?? path.join(root, 'build/proofs/storage_layout.h');
        const proof = await generateStorageLayout(path.join(root, 'laws/storage_layout'), output);
        console.log(`storage.layout: ${proof.laws.length} Bend laws proved; 8 decisions generated`);
    } catch (error) {
        console.error(error.message);
        process.exitCode = 1;
    }
}
