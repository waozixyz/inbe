import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { generateStorageLayout } from '../scripts/generate-storage-layout.mjs';

const root = path.resolve(import.meta.dirname, '..');

async function fixture(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'storage-layout-proof-'));
    try {
        const policy = path.join(directory, 'laws/storage_layout');
        fs.cpSync(path.join(root, 'laws/storage_layout'), policy, { recursive: true });
        const output = path.join(directory, 'storage_layout.h');
        await run({ policy, output });
    } finally {
        fs.rmSync(directory, { recursive: true, force: true });
    }
}

test('the checked policy generates a stable layout header', async () => {
    await fixture(async ({ policy, output }) => {
        const proof = await generateStorageLayout(policy, output);
        assert.equal(proof.laws.length, 10);
        const first = fs.readFileSync(output, 'utf8');
        assert.match(first, /#define STORAGE_DIR_NAME "inbe"/);
        assert.match(first, /#define STORAGE_DIR_LEGACY_WINDOWS "BreathSession"/);
        assert.match(first, /#define STORAGE_DB_NAME "inbe\.db"/);
        assert.match(first, /#define STORAGE_EXPORT_ENTRY_DB "breathing-data\/breathing\.db"/);
        assert.match(first, /"inbe-data\/inbe\.db"/);
        assert.match(first, /#define STORAGE_WEB_HOME "\/home\/inbe"/);
        const mtime = fs.statSync(output).mtimeMs;
        await generateStorageLayout(policy, output);
        assert.equal(fs.statSync(output).mtimeMs, mtime);
        fs.writeFileSync(output, 'stale or hand-edited output');
        await generateStorageLayout(policy, output);
        assert.equal(fs.readFileSync(output, 'utf8'), first);
    });
});

test('policies that break each class of layout guarantee fail their proofs', async () => {
    const mutations = [
        ['      Current{}\n    case Present{}:',
         '      Legacy{}\n    case Present{}:',
         'fresh install must resolve to the current layout'],
        ['            case Succeeded{}:\n              Current{}\n            case Failed{}:\n              Legacy{}',
         '            case Succeeded{}:\n              Current{}\n            case Failed{}:\n              Current{}',
         'a failed move must keep the legacy data'],
        ['    case Windows{}:\n      Inbe{}',
         '    case Windows{}:\n      Breathing{}',
         'every platform must use the inbe directory'],
        ['    case InbeDataDb{}:\n      Yes{}',
         '    case InbeDataDb{}:\n      No{}',
         'historical export entries must stay importable'],
        ['def legacy_db() -> DbName:\n  BreathingDb{}',
         'def legacy_db() -> DbName:\n  InbeDb{}',
         'the legacy database name must differ from the current one'],
    ];
    for (const [before, after, reason] of mutations) {
        await fixture(async ({ policy, output }) => {
            await generateStorageLayout(policy, output);
            const old = fs.readFileSync(output, 'utf8');
            const source = path.join(policy, 'main.bend');
            const text = fs.readFileSync(source, 'utf8');
            assert.ok(text.includes(before), `mutation target missing: ${reason}`);
            fs.writeFileSync(source, text.replace(before, after));
            await assert.rejects(generateStorageLayout(policy, output), /LAWS\.|storage\.layout/,
                reason);
            assert.equal(fs.readFileSync(output, 'utf8'), old);
        });
    }
});
