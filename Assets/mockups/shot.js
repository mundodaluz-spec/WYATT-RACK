const { chromium } = require('playwright');
const fs = require('fs'); const path = require('path');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1280, height: 820 }, deviceScaleFactor: 2 });
  const dir = '/tmp/claude-0/mock/html'; const out = '/tmp/claude-0/mock/png';
  fs.mkdirSync(out, { recursive: true });
  for (const f of fs.readdirSync(dir).filter(f => f.endsWith('.html')).sort()) {
    await p.goto('file://' + path.join(dir, f)); await p.waitForTimeout(250);
    await p.screenshot({ path: path.join(out, f.replace('.html', '.png')) });
  }
  await b.close();
})();
