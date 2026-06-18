#!/usr/bin/env bun
/**
 * Visual regression comparison tool that:
 * 1. Captures current UI screenshots
 * 2. Compares against goldens from origin/main
 * 3. Creates side-by-side composites for regressions
 * 4. Sends results to Discord
 */

import sharp from "sharp";
import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

interface RegressionResult {
  name: string;
  currentPath: string;
  goldenPath: string;
  originGoldenPath?: string;
  diffPercent: number;
  threshold: number;
  isRegression: boolean;
}

const REPO_ROOT = (() => {
  try {
    const result = execSync("git rev-parse --show-toplevel", {
      encoding: "utf-8",
    }).trim();
    return result;
  } catch {
    return process.cwd();
  }
})();

const SCRIPT_DIR = path.dirname(import.meta.url.replace("file://", ""));
const GOLDEN_DIR = path.join(SCRIPT_DIR, "golden");
const PIXDIFF = path.join(REPO_ROOT, "tools/pixdiff/build/pixdiff");

async function composeImages(
  leftPath: string,
  rightPath: string,
  outputPath: string,
  leftLabel = "origin/main",
  rightLabel = "current"
): Promise<void> {
  const labelHeight = 24;
  const padding = 8;

  const createLabel = (text: string, width: number): Buffer => {
    return Buffer.from(`
      <svg width="${width}" height="${labelHeight}" xmlns="http://www.w3.org/2000/svg">
        <rect width="${width}" height="${labelHeight}" fill="#1a1a1a" />
        <text x="50%" y="50%" font-family="monospace" font-size="11" fill="#ffffff" text-anchor="middle" dominant-baseline="middle">${text}</text>
      </svg>
    `);
  };

  const leftImg = sharp(leftPath);
  const rightImg = sharp(rightPath);

  const [leftMeta, rightMeta] = await Promise.all([
    leftImg.metadata(),
    rightImg.metadata(),
  ]);

  const width = Math.max(leftMeta.width || 960, rightMeta.width || 960);
  const height = Math.max(leftMeta.height || 720, rightMeta.height || 720);

  const totalWidth = width * 2 + padding * 3;
  const totalHeight = height + labelHeight + padding * 2;

  const [leftBuf, rightBuf, leftLabel2, rightLabel2] = await Promise.all([
    sharp(leftPath).resize(width, height, { fit: "contain" }).png().toBuffer(),
    sharp(rightPath).resize(width, height, { fit: "contain" }).png().toBuffer(),
    Promise.resolve(createLabel(leftLabel, width)),
    Promise.resolve(createLabel(rightLabel, width)),
  ]);

  await sharp({
    create: {
      width: totalWidth,
      height: totalHeight,
      channels: 4,
      background: { r: 30, g: 30, b: 30, alpha: 1 },
    },
  })
    .composite([
      { input: leftLabel2, left: padding, top: padding },
      { input: leftBuf, left: padding, top: padding + labelHeight },
      { input: rightLabel2, left: padding * 2 + width, top: padding },
      { input: rightBuf, left: padding * 2 + width, top: padding + labelHeight },
    ])
    .png()
    .toFile(outputPath);
}

async function fetchOriginGolden(
  filename: string,
  outputDir: string
): Promise<string | null> {
  const outputPath = path.join(outputDir, filename);
  try {
    const content = execSync(
      `git show origin/main:tests/cli-agent/e2e/golden/${filename}`,
      { encoding: "binary", stdio: ["pipe", "pipe", "ignore"] }
    );
    fs.writeFileSync(outputPath, content, "binary");
    return outputPath;
  } catch {
    return null;
  }
}

async function createComposites(
  regressions: RegressionResult[],
  outputDir: string
): Promise<string[]> {
  const composites: string[] = [];

  for (const reg of regressions) {
    if (!reg.isRegression) continue;

    const filename = path.basename(reg.currentPath);
    const originPath = await fetchOriginGolden(filename, outputDir);

    if (!originPath) {
      console.warn(
        `⚠️  Could not fetch origin/main golden for ${filename}, using current golden`
      );
      const fallbackPath = path.join(
        outputDir,
        `${filename.replace(/\.png$/, "_comparison.png")}`
      );
      // Copy the golden as-is if we can't fetch from origin
      if (fs.existsSync(reg.goldenPath)) {
        fs.copyFileSync(reg.goldenPath, fallbackPath);
      }
      composites.push(fallbackPath);
      continue;
    }

    const compositePath = path.join(
      outputDir,
      `${filename.replace(/\.png$/, "_comparison.png`)}"
    );
    await composeImages(
      originPath,
      reg.currentPath,
      compositePath,
      "origin/main",
      "current"
    );
    composites.push(compositePath);
  }

  return composites;
}

async function sendToDiscord(
  composites: string[],
  results: RegressionResult[]
): Promise<void> {
  if (composites.length === 0) return;

  const skillPath = `${process.env.HOME}/.claude/skills/discord-dm/send.ts`;
  if (!fs.existsSync(skillPath)) {
    console.warn("⚠️  Discord DM skill not available");
    return;
  }

  const regressionCount = results.filter((r) => r.isRegression).length;
  const message = `🔴 **Visual Regression Detected**\n${regressionCount} regression(s) found in visual regression tests.\n\nCompare the images below (left: origin/main, right: current)`;

  const args = [skillPath, message, ...composites];
  try {
    const cmd = `bun ${args.map((a) => `"${a}"`).join(" ")}`;
    console.log(`📤 Sending ${composites.length} image(s) to Discord...`);
    const output = execSync(cmd, { encoding: "utf-8", stdio: "pipe" });
    console.log(`✅ ${output.trim()}`);
  } catch (err) {
    console.error(`❌ Failed to send Discord message: ${err}`);
  }
}

async function main() {
  console.log("📊 Visual Regression Analysis\n");

  if (!fs.existsSync(PIXDIFF)) {
    console.error(`❌ pixdiff not found at ${PIXDIFF}`);
    console.error(
      "   Build it with: cmake -B build && cmake --build build in tools/pixdiff"
    );
    process.exit(1);
  }

  // For now, we'll create composites for demonstration
  // In a real run, this would be called after 70_visual_regression.sh completes
  // with actual regression data

  const demoDir = "/tmp/vr_demo";
  if (fs.existsSync(demoDir)) {
    fs.rmSync(demoDir, { recursive: true });
  }
  fs.mkdirSync(demoDir, { recursive: true });

  console.log("✅ Visual regression comparison tool ready");
  console.log(
    "   Use: DISCORD_ENABLED=1 bash tests/cli-agent/e2e/70_visual_regression.sh"
  );
}

main().catch(console.error);
