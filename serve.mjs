/**
 * serve.mjs — a tiny zero-dependency static file server for local play.
 * ES modules can't be loaded over file://, so run the game through this:
 *
 *     node serve.mjs
 *
 * then open http://localhost:8123 . No npm install required.
 */
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';

const PORT = process.env.PORT || 8123;
const ROOT = process.cwd();

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png', '.jpg': 'image/jpeg', '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon', '.woff2': 'font/woff2',
};

createServer(async (req, res) => {
  try {
    let urlPath = decodeURIComponent(req.url.split('?')[0]);
    if (urlPath === '/') urlPath = '/index.html';
    // Prevent path traversal outside the project root.
    const filePath = normalize(join(ROOT, urlPath));
    if (!filePath.startsWith(ROOT)) { res.writeHead(403); res.end('Forbidden'); return; }
    const data = await readFile(filePath);
    res.writeHead(200, { 'Content-Type': MIME[extname(filePath).toLowerCase()] || 'application/octet-stream' });
    res.end(data);
  } catch {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('404 Not Found');
  }
}).listen(PORT, () => console.log(`The Mythic Chronicles running at http://localhost:${PORT}`));
