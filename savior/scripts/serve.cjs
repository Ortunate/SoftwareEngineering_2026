'use strict';
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const allowed = new Set(['/index.html', '/src/learning.js', '/src/core.js', '/src/app.js', '/src/styles.css', '/src/practice.css']);
const types = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8' };
function createServer() { return http.createServer((req, res) => {
  let pathname;
  try { pathname = decodeURIComponent(new URL(req.url, 'http://localhost').pathname); }
  catch { res.writeHead(400); return res.end('Bad request'); }
  if (!['GET', 'HEAD'].includes(req.method)) { res.writeHead(405); return res.end('Method not allowed'); }
  if (pathname === '/') pathname = '/index.html';
  if (!allowed.has(pathname)) { res.writeHead(404); return res.end('Not found'); }
  fs.readFile(path.join(root, pathname), (error, contents) => {
    if (error) { res.writeHead(500); return res.end('Read error'); }
    res.writeHead(200, { 'Content-Type': types[path.extname(pathname)], 'Cache-Control': 'no-cache', 'X-Content-Type-Options': 'nosniff' });
    res.end(req.method === 'HEAD' ? undefined : contents);
  });
}); }
if (require.main === module) {
  const port = Number(process.env.PORT || 4173);
  const server = createServer();
  server.on('error', error => { console.error(`启动失败：${error.message}`); process.exitCode = 1; });
  server.listen(port, '127.0.0.1', () => console.log(`小芽口算：http://127.0.0.1:${port}（Ctrl+C 停止）`));
}
module.exports = { createServer };
