import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
import { generateSyncRetry } from '../scripts/generate-sync-retry.mjs';

const root = path.resolve(import.meta.dirname, '..');
const header = path.join(root, 'vendor/kryon/include/sync.h');

async function fixture(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'sync-retry-proof-'));
    try {
        fs.cpSync(path.join(root, 'laws/sync_retry'), path.join(directory, 'laws/sync_retry'), { recursive: true });
        const policy = path.join(directory, 'laws/sync_retry');
        const output = path.join(directory, 'build/proofs/sync_retry_table.h');
        await run({ directory, policy, output });
    } finally {
        fs.rmSync(directory, { recursive: true, force: true });
    }
}

test('proved table generation is deterministic and repairs stale generated content', async () => {
    await fixture(async ({ policy, output }) => {
        const proof = await generateSyncRetry(policy, output, header);
        assert.equal(proof.laws.length, 11);
        const first = fs.readFileSync(output, 'utf8');
        const mtime = fs.statSync(output).mtimeMs;
        await generateSyncRetry(policy, output, header);
        assert.equal(fs.statSync(output).mtimeMs, mtime);
        fs.writeFileSync(output, 'stale or hand-edited output');
        await generateSyncRetry(policy, output, header);
        assert.equal(fs.readFileSync(output, 'utf8'), first);
    });
});

test('policy changes that break each class of behavior fail their proofs', async () => {
    const mutations = [
        ['      60', '      61'],
        ['      Decision{0, 0, 0}', '      Decision{1, 0, 0}'],
        ['    case AuthFailed{}:\n      stop(attempt)', '    case AuthFailed{}:\n      retry(attempt)'],
        ['    case Capped{}:\n      Capped{}', '    case Capped{}:\n      Initial{}'],
    ];
    for (const [before, after] of mutations) {
        await fixture(async ({ policy, output }) => {
            await generateSyncRetry(policy, output, header);
            const old = fs.readFileSync(output, 'utf8');
            const source = path.join(policy, 'main.bend');
            const text = fs.readFileSync(source, 'utf8');
            assert.ok(text.includes(before));
            fs.writeFileSync(source, text.replace(before, after));
            await assert.rejects(generateSyncRetry(policy, output, header), /LAWS\./);
            assert.equal(fs.readFileSync(output, 'utf8'), old);
        });
    }
});

test('changed runtime enum cannot silently acquire an unproved mapping', async () => {
    await fixture(async ({ directory, policy, output }) => {
        const changedHeader = path.join(directory, 'sync.h');
        fs.writeFileSync(changedHeader, fs.readFileSync(header, 'utf8').replace('SYNC_AUTH_FAILED', 'SYNC_NEW_RESULT'));
        await assert.rejects(generateSyncRetry(policy, output, changedHeader), /result_enum_changed/);
    });
});

test('C compilation rejects renumbered result codes', async () => {
    await fixture(async ({ directory, policy, output }) => {
        const enumDeclaration = fs.readFileSync(header, 'utf8').match(/typedef enum SyncResult\s*\{[^}]+\}\s*SyncResult;/)[0];
        const changedHeader = path.join(directory, 'sync.h');
        fs.writeFileSync(changedHeader, enumDeclaration.replace('SYNC_OK = 0', 'SYNC_OK = 1'));
        await generateSyncRetry(policy, output, changedHeader);
        const result = spawnSync(process.env.CC ?? 'cc', ['-x', 'c', '-fsyntax-only', '-I', directory, output], {
            encoding: 'utf8', timeout: 30000,
        });
        assert.ifError(result.error);
        assert.notEqual(result.status, 0);
        assert.match(result.stderr, /RetryResultCodesMatch/);
    });
});

test('actual Make proof prerequisite rejects broken policy despite an existing header', async () => {
    await fixture(async ({ directory, policy, output }) => {
        fs.mkdirSync(path.join(directory, 'scripts'));
        fs.mkdirSync(path.join(directory, 'vendor'));
        fs.mkdirSync(path.join(directory, 'src/core'), { recursive: true });
        for (const file of ['scripts/generate-sync-retry.mjs',
            'scripts/generate-storage-layout.mjs', 'src/core/version.h', 'Makefile']) {
            fs.copyFileSync(path.join(root, file), path.join(directory, file));
        }
        fs.cpSync(path.join(root, 'laws/storage_layout'), path.join(directory, 'laws/storage_layout'),
            { recursive: true });
        fs.symlinkSync(path.join(root, 'vendor/kryon'), path.join(directory, 'vendor/kryon'), 'dir');
        const make = () => spawnSync('make', ['build/proofs/sync_retry_table.h'], {
            cwd: directory, encoding: 'utf8', timeout: 30000,
        });
        const good = make();
        assert.ifError(good.error);
        assert.equal(good.status, 0, good.stdout + good.stderr);
        const old = fs.readFileSync(output, 'utf8');
        const source = path.join(policy, 'main.bend');
        fs.writeFileSync(source, fs.readFileSync(source, 'utf8').replace('      60', '      61'));
        const bad = make();
        assert.ifError(bad.error);
        assert.notEqual(bad.status, 0);
        assert.match(bad.stdout + bad.stderr, /LAWS\./);
        assert.equal(fs.readFileSync(output, 'utf8'), old);
    });
});
