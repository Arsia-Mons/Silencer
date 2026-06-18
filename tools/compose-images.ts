#!/usr/bin/env bun
/**
 * Compose two PNG images side-by-side for visual regression comparison.
 * Usage: bun compose-images.ts <left-image> <right-image> <output-path> [left-label] [right-label]
 */

import sharp from "sharp";

async function composeImages(
  leftPath: string,
  rightPath: string,
  outputPath: string,
  leftLabel = "Origin/Main",
  rightLabel = "Current"
): Promise<void> {
  const leftImage = sharp(leftPath);
  const rightImage = sharp(rightPath);

  const [leftMeta] = await Promise.all([
    leftImage.metadata(),
    rightImage.metadata(),
  ]);

  const width = leftMeta.width || 960;
  const height = leftMeta.height || 720;
  const labelHeight = 30;
  const padding = 10;
  const totalWidth = width * 2 + padding * 3;
  const totalHeight = height + labelHeight + padding * 2;

  // Create label SVGs
  const createLabel = (text: string): Buffer => {
    return Buffer.from(`
      <svg width="${width}" height="${labelHeight}" xmlns="http://www.w3.org/2000/svg">
        <rect width="${width}" height="${labelHeight}" fill="#222" />
        <text x="50%" y="50%" font-family="Arial" font-size="12" fill="white" text-anchor="middle" dominant-baseline="middle">${text}</text>
      </svg>
    `);
  };

  const leftLabelBuf = createLabel(leftLabel);
  const rightLabelBuf = createLabel(rightLabel);

  // Resize images to ensure consistent dimensions
  const leftBuf = await leftImage.resize(width, height, { fit: "contain" }).png().toBuffer();
  const rightBuf = await rightImage.resize(width, height, { fit: "contain" }).png().toBuffer();

  // Build composite using sharp
  await sharp({
    create: {
      width: totalWidth,
      height: totalHeight,
      channels: 4,
      background: { r: 40, g: 40, b: 40, alpha: 1 },
    },
  })
    .composite([
      { input: leftLabelBuf, left: padding, top: padding },
      { input: leftBuf, left: padding, top: padding + labelHeight },
      { input: rightLabelBuf, left: padding * 2 + width, top: padding },
      { input: rightBuf, left: padding * 2 + width, top: padding + labelHeight },
    ])
    .png()
    .toFile(outputPath);

  console.log(`Composite created: ${outputPath}`);
}

const args = process.argv.slice(2);
if (args.length < 3) {
  console.error(
    "Usage: bun compose-images.ts <left> <right> <output> [left-label] [right-label]"
  );
  process.exit(1);
}

const [left, right, output, leftLabel, rightLabel] = args;

try {
  await composeImages(left, right, output, leftLabel, rightLabel);
} catch (err) {
  console.error(`Error composing images: ${err}`);
  process.exit(1);
}
