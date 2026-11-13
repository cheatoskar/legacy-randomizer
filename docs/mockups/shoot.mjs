// Usage: set PLAYWRIGHT_DIR to a folder with Playwright installed (npm i playwright), then: node <this file>
import { createRequire } from 'module';
import { pathToFileURL, fileURLToPath } from 'url';
import path from 'path';
const pw = process.env.PLAYWRIGHT_DIR;
if (!pw) {
  console.error('PLAYWRIGHT_DIR is not set: point it to a folder with Playwright installed (npm i playwright).');
  process.exit(1);
}
const { chromium } = createRequire(path.join(path.resolve(pw), 'package.json'))('playwright');
const dir = path.dirname(fileURLToPath(import.meta.url));
const html = pathToFileURL(path.join(dir, 'mockups.html')).href;
const names = ['01-start', '02-hud', '03-results', '04-settings'];
const b = await chromium.launch();
const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
for (let i = 0; i < 4; i++) {
  await p.goto(`${html}?s=${i + 1}`);
  await p.waitForTimeout(300);
  await p.screenshot({ path: path.join(dir, names[i] + '.png') });
}
await b.close();
