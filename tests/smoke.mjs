// smoke.mjs -- headless functional test of the Geomview web build.
//
//   cd tests && npm install playwright && npx playwright install chromium
//   node smoke.mjs [dist-dir]                 (default: ../dist)
//
// Uses Chrome/Chromium from $CHROME, else the browser Playwright installed
// (npx playwright install chromium).  Serves dist-dir on a free port, then:
//   1. starts Geomview with a model, checks the Motif main panel, the Tools
//      panel and the camera window were drawn and nothing threw;
//   2. drag-rotates the model in the camera window and checks the image
//      changed;
//   3. opens File > Open, double-clicks a model, checks it was loaded;
//   4. turns on continuous rotation and checks frames keep coming while a
//      Motif menu is open (the event loop yields to the browser).
// Screenshots are written to tests/out/.
import { chromium } from "playwright-core";
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const dist = path.resolve(process.argv[2] || path.join(here, "..", "dist"));
const out = path.join(here, "out");
fs.mkdirSync(out, { recursive: true });

const types = { ".html": "text/html", ".js": "text/javascript",
  ".wasm": "application/wasm", ".data": "application/octet-stream" };
const server = http.createServer((req, res) => {
  const f = path.join(dist, decodeURIComponent(new URL(req.url, "http://x").pathname));
  if (!f.startsWith(dist) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) {
    res.writeHead(404); res.end(); return;
  }
  res.writeHead(200, { "Content-Type": types[path.extname(f)] || "application/octet-stream" });
  fs.createReadStream(f).pipe(res);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const base = `http://127.0.0.1:${server.address().port}/geomview.html`;

const browser = await chromium.launch({
  executablePath: process.env.CHROME || undefined,
  args: ["--no-sandbox"],
});
let failures = 0;
const check = (ok, what) => {
  console.log(`${ok ? "ok  " : "FAIL"} ${what}`);
  if (!ok) failures++;
};

// Has the X client drawn into the window under (x, y)?  x11.wasm backs
// every window with a <canvas>; an undrawn one is fully transparent.
async function drawnAt(page, x, y) {
  return page.evaluate(([x, y]) => {
    const w = document.elementsFromPoint(x, y)
      .find((e) => e.hasAttribute && e.hasAttribute("data-x11-wasm-xid"));
    const c = w && w.querySelector(":scope > canvas");
    if (!c) return false;
    const d = c.getContext("2d").getImageData(0, 0, c.width, c.height).data;
    for (let i = 3; i < d.length; i += 4) if (d[i]) return true;
    return false;
  }, [x, y]);
}
async function snapshot(page, clip) {
  return (await page.screenshot({ clip })).toString("base64");
}

async function run(query, fn) {
  const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  page.on("crash", () => errors.push("renderer crashed"));
  await page.goto(base + query);
  await page.waitForTimeout(4000);
  await fn(page);
  check(errors.length === 0, `no page errors${errors.length ? ": " + errors.join("; ") : ""}`);
  await page.close();
}

const CAM = { x: 549, y: 164, width: 450, height: 450 };   // camera window
const model = "?arg=/usr/share/geomview/geom/hdodec.off";

await run(model, async (page) => {
  await page.screenshot({ path: path.join(out, "1-startup.png") });
  check(await drawnAt(page, 33, 25), "Motif main panel is drawn (File menu button)");
  check(await drawnAt(page, 438, 220), "Tools panel is drawn (rotate button)");
  check(await drawnAt(page, 774, 389), "camera window is drawn");
  const before = await snapshot(page, CAM);
  await page.mouse.move(700, 400);
  await page.mouse.down();
  await page.mouse.move(820, 330, { steps: 20 });
  await page.mouse.up();
  await page.waitForTimeout(1500);
  const after = await snapshot(page, CAM);
  check(before !== after, "drag in camera window rotates the model");
  await page.screenshot({ path: path.join(out, "2-rotated.png") });
});

await run("", async (page) => {
  await page.mouse.click(33, 25);               // File
  await page.waitForTimeout(500);
  await page.mouse.click(40, 71);               // Open
  await page.waitForTimeout(1500);
  await page.screenshot({ path: path.join(out, "3-files-panel.png") });
  const empty = await snapshot(page, CAM);
  await page.mouse.dblclick(235, 451);          // 4th entry of the file list
  await page.waitForTimeout(2500);
  const loaded = await snapshot(page, CAM);
  check(empty !== loaded, "File > Open loads a model into the camera");
  await page.screenshot({ path: path.join(out, "4-opened.png") });
});

await run(model + "&arg=-c&arg=" +
    encodeURIComponent("(transform-incr world world world rotate 0.02 0.03 0)"),
  async (page) => {
    await page.mouse.click(127, 25);              // open the Inspect menu
    await page.waitForTimeout(300);
    const a = await snapshot(page, CAM);
    await page.waitForTimeout(700);
    const b = await snapshot(page, CAM);
    check(a !== b, "animation continues while a Motif menu is posted");
    await page.screenshot({ path: path.join(out, "5-spinning-menu.png") });
    await page.keyboard.press("Escape");
  });

await browser.close();
server.close();
console.log(failures ? `${failures} check(s) failed` : "all checks passed");
process.exit(failures ? 1 : 0);
