// Usage (from C:/Users/osaro/tools/playwright): node <this file>
import { createRequire } from 'module';
const { chromium } = createRequire('C:/Users/osaro/tools/playwright/')('playwright');
import { pathToFileURL, fileURLToPath } from 'url';
import path from 'path';
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
