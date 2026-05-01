#!/usr/bin/env bun
// Assemble the five npm packages from the release artifacts into a single
// staging directory ready for `npm publish`. Run by the publish-npm job in
// .github/workflows/release.yml; runnable locally for staging-shape checks.
//
//   bun clients/tui/scripts/stage-npm-packages.ts \
//     --version 1.2.3 \
//     --artifacts ./artifacts \
//     --out ./npm-staging \
//     --tui-pkg ./clients/tui
//
// Layout the script produces:
//
//   <out>/silencer/                       — top-level (@arsia-mons/silencer)
//   <out>/silencer-darwin-arm64/          — engine bundle, darwin/arm64
//   <out>/silencer-linux-x64/             — engine bundle, linux/x64
//   <out>/silencer-win32-x64/             — engine bundle, win32/x64
//   <out>/silencer-tui/                   — unscoped redirect to @arsia-mons/silencer
//
// Each <out>/<dir> is a self-contained npm package — `cd` in and `npm publish`.

import { mkdir, rm, cp, writeFile, readFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { join, resolve } from 'node:path';
import { existsSync } from 'node:fs';

interface Args {
  version: string;
  artifacts: string;
  out: string;
  tuiPkg: string;
}

function parseArgs(): Args {
  const args = process.argv.slice(2);
  const get = (flag: string): string => {
    const i = args.indexOf(flag);
    if (i === -1 || i + 1 >= args.length) {
      throw new Error(`missing required arg: ${flag}`);
    }
    return args[i + 1]!;
  };
  return {
    version: get('--version'),
    artifacts: resolve(get('--artifacts')),
    out: resolve(get('--out')),
    tuiPkg: resolve(get('--tui-pkg')),
  };
}

async function run(
  cmd: string,
  argv: string[],
  cwd?: string,
): Promise<void> {
  await new Promise<void>((res, rej) => {
    const p = spawn(cmd, argv, { cwd, stdio: 'inherit' });
    p.on('exit', (code) => {
      if (code === 0) res();
      else rej(new Error(`${cmd} ${argv.join(' ')} exited ${code}`));
    });
    p.on('error', rej);
  });
}

const PROJECT_DESCRIPTION =
  'An action/strategy multiplayer side-scrolling platform game set on a futuristic Mars.';

function platformPkgJson(
  platform: 'darwin' | 'linux' | 'win32',
  arch: 'arm64' | 'x64',
  version: string,
): Record<string, unknown> {
  return {
    name: `@arsia-mons/silencer-${platform}-${arch}`,
    version,
    description: PROJECT_DESCRIPTION,
    os: [platform],
    cpu: [arch],
  };
}

async function stagePlatformPackage(opts: {
  out: string;
  pkgDir: string;
  platform: 'darwin' | 'linux' | 'win32';
  arch: 'arm64' | 'x64';
  version: string;
  artifact: string;
  // Function that, given the destination dir, extracts the artifact's contents
  // directly into it (flattening any wrapper directory).
  extract: (dest: string) => Promise<void>;
}): Promise<void> {
  const dest = join(opts.out, opts.pkgDir);
  await mkdir(dest, { recursive: true });
  await opts.extract(dest);
  await writeFile(
    join(dest, 'package.json'),
    JSON.stringify(
      platformPkgJson(opts.platform, opts.arch, opts.version),
      null,
      2,
    ) + '\n',
  );
  console.log(`staged ${opts.pkgDir}`);
}

async function stageDarwinArm64(args: Args): Promise<void> {
  // Artifact: silencer-macos-arm64.zip — a `ditto -ck --keepParent`'d
  // Silencer.app, so the zip contains `Silencer.app/...` at the root. The
  // `publish-npm` job runs on ubuntu-latest where `ditto` is unavailable;
  // `unzip` exists on both macOS and Linux and preserves the bundle's
  // notarization + stapled signature.
  await stagePlatformPackage({
    out: args.out,
    pkgDir: 'silencer-darwin-arm64',
    platform: 'darwin',
    arch: 'arm64',
    version: args.version,
    artifact: join(args.artifacts, 'macos-arm64', 'silencer-macos-arm64.zip'),
    extract: async (dest) => {
      await run('unzip', [
        '-q',
        join(args.artifacts, 'macos-arm64', 'silencer-macos-arm64.zip'),
        '-d',
        dest,
      ]);
    },
  });
}

async function stageLinuxX64(args: Args): Promise<void> {
  // Artifact: silencer-linux-x64.tar.gz — contains a top-level `silencer/`
  // dir (binary + bundled .so + assets/). Strip the dir so the package
  // contents live at the staging root.
  await stagePlatformPackage({
    out: args.out,
    pkgDir: 'silencer-linux-x64',
    platform: 'linux',
    arch: 'x64',
    version: args.version,
    artifact: join(args.artifacts, 'linux-x64', 'silencer-linux-x64.tar.gz'),
    extract: async (dest) => {
      await run('tar', [
        'xzf',
        join(args.artifacts, 'linux-x64', 'silencer-linux-x64.tar.gz'),
        '-C',
        dest,
        '--strip-components=1',
      ]);
    },
  });
}

async function stageWin32X64(args: Args): Promise<void> {
  // Artifact: silencer-windows-x64.zip — contains a top-level `silencer/`
  // dir (Silencer.exe + DLLs + assets/). Unzip then flatten.
  await stagePlatformPackage({
    out: args.out,
    pkgDir: 'silencer-win32-x64',
    platform: 'win32',
    arch: 'x64',
    version: args.version,
    artifact: join(args.artifacts, 'windows-x64', 'silencer-windows-x64.zip'),
    extract: async (dest) => {
      const tmp = `${dest}.tmp`;
      await mkdir(tmp, { recursive: true });
      await run('unzip', [
        '-q',
        join(args.artifacts, 'windows-x64', 'silencer-windows-x64.zip'),
        '-d',
        tmp,
      ]);
      // Move tmp/silencer/* → dest/.
      await run('sh', [
        '-c',
        `mv "${tmp}"/silencer/* "${dest}/" && rmdir "${tmp}/silencer" "${tmp}"`,
      ]);
    },
  });
}

async function stageTopLevel(args: Args): Promise<void> {
  // The top-level package is `clients/tui/` after `bun build`, with the
  // version + optionalDependencies pinned to the release version.
  const dest = join(args.out, 'silencer');
  await mkdir(dest, { recursive: true });

  const distDir = join(args.tuiPkg, 'dist');
  if (!existsSync(join(distDir, 'index.js'))) {
    throw new Error(
      `expected ${distDir}/index.js — run \`bun run build\` in ${args.tuiPkg} first`,
    );
  }
  await cp(distDir, join(dest, 'dist'), { recursive: true });

  const srcPkg = JSON.parse(
    await readFile(join(args.tuiPkg, 'package.json'), 'utf8'),
  ) as Record<string, unknown>;
  srcPkg.version = args.version;
  const optDeps = srcPkg.optionalDependencies as Record<string, string>;
  for (const k of Object.keys(optDeps)) optDeps[k] = args.version;
  // devDependencies aren't installed for end-users; drop to avoid resolver
  // surprises.
  delete srcPkg.devDependencies;
  delete srcPkg.scripts;

  await writeFile(
    join(dest, 'package.json'),
    JSON.stringify(srcPkg, null, 2) + '\n',
  );
  console.log('staged silencer (top-level)');
}

async function stageTuiRedirect(args: Args): Promise<void> {
  // Unscoped `silencer-tui` is a one-line meta package that re-imports the
  // real package. Same bin name, so `npm i -g silencer-tui && silencer-tui`
  // keeps working for anyone who used the old name.
  const dest = join(args.out, 'silencer-tui');
  await mkdir(dest, { recursive: true });

  const pkg = {
    name: 'silencer-tui',
    version: args.version,
    description: PROJECT_DESCRIPTION,
    type: 'module',
    bin: { 'silencer-tui': './index.js' },
    files: ['index.js'],
    dependencies: {
      '@arsia-mons/silencer': args.version,
    },
  };
  await writeFile(
    join(dest, 'package.json'),
    JSON.stringify(pkg, null, 2) + '\n',
  );
  // The redirect is one import. The real package's entry point starts the
  // engine at module load (main().catch(...)), so this is enough.
  await writeFile(
    join(dest, 'index.js'),
    "#!/usr/bin/env bun\nimport '@arsia-mons/silencer';\n",
  );
  console.log('staged silencer-tui (redirect)');
}

async function main(): Promise<void> {
  const args = parseArgs();
  if (!/^\d+\.\d+\.\d+(-.+)?$/.test(args.version)) {
    throw new Error(`bad version: ${args.version}`);
  }
  await rm(args.out, { recursive: true, force: true });
  await mkdir(args.out, { recursive: true });

  await stageDarwinArm64(args);
  await stageLinuxX64(args);
  await stageWin32X64(args);
  await stageTopLevel(args);
  await stageTuiRedirect(args);

  console.log(`\nstaged 5 packages at ${args.version} in ${args.out}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
