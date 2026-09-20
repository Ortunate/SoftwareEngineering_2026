// Run in one process so sandboxed environments do not need child-process permissions.
require('./core.test.cjs');
require('./server.test.cjs');
require('./learning.test.cjs');
