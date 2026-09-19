'use strict';
// A dependency-free static distribution. Only explicit runtime files are copied.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const output = path.join(root, 'dist');
const files = ['index.html', 'src/core.js', 'src/app.js', 'src/styles.css', 'src/practice.css'];
const hashes = [];
for (const file of files) {
  const content = fs.readFileSync(path.join(root, file));
  const destination = path.join(output, file);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.writeFileSync(destination, content);
  hashes.push(`${crypto.createHash('sha256').update(content).digest('hex')}  ${file}`);
}
fs.writeFileSync(path.join(output, 'SHA256SUMS.txt'), hashes.join('\n') + '\n');
console.log('已生成 savior/dist/：5 个静态运行文件及 SHA256SUMS.txt。');
console.log('可直接打开 dist/index.html，或将 dist 作为静态网站根目录发布。');
