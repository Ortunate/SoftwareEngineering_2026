'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const http = require('node:http');
const { createServer } = require('../scripts/serve.cjs');

function request(port, pathname, method = 'GET') {
  return new Promise((resolve, reject) => {
    const req = http.request({ hostname: '127.0.0.1', port, path: pathname, method }, res => {
      const buffers = [];
      res.on('data', data => buffers.push(data));
      res.on('end', () => resolve({ status: res.statusCode, headers: res.headers, body: Buffer.concat(buffers).toString('utf8') }));
    });
    req.on('error', reject);
    req.end();
  });
}
test('T13: local server delivers app resources and excludes private development files', async () => {
  const server = createServer();
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(0, '127.0.0.1', resolve); });
  try {
    const port = server.address().port;
    const index = await request(port, '/');
    assert.equal(index.status, 200);
    assert.match(index.body, /小芽口算/);
    assert.match(index.headers['content-type'], /text\/html/);
    for (const resource of ['/src/core.js', '/src/app.js', '/src/styles.css', '/src/practice.css']) {
      const response = await request(port, resource);
      assert.equal(response.status, 200);
      assert.ok(response.body.length > 100);
      assert.equal(response.headers['x-content-type-options'], 'nosniff');
    }
    for (const pathname of ['/package.json', '/docs/requirements/requirements.md', '/tests/core.test.cjs', '/../.git/config', '/%2e%2e/.git/config']) {
      assert.equal((await request(port, pathname)).status, 404);
    }
    assert.equal((await request(port, '/%ZZ')).status, 400);
    assert.equal((await request(port, '/', 'POST')).status, 405);
    const head = await request(port, '/', 'HEAD');
    assert.equal(head.status, 200);
    assert.equal(head.body, '');
  } finally { await new Promise(resolve => server.close(resolve)); }
});
