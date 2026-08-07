const pkg = require('../package.json');
const { createProxyMiddleware } = require('http-proxy-middleware');
const target = process.env.PROXY || pkg.proxy;
const skipWs = process.env.SKIP_WS_PROXY === '1' || /127\.0\.0\.1:3080|localhost:3080/.test(target);

module.exports = function (app) {
  const onError = (err, req, res) => {
    console.warn('[proxy]', err.code || err.message);
    if (res && typeof res.writeHead === 'function' && !res.headersSent) {
      res.writeHead(502);
      res.end('Bad gateway');
    }
  };

  app.use(
    createProxyMiddleware('/rest', {
      target,
      changeOrigin: true,
      onError
    })
  );
  app.use(
    createProxyMiddleware('/api', {
      target,
      changeOrigin: true,
      onError
    })
  );

  // Mock API has no WebSocket — skip to avoid crashing webpack-dev-server
  if (!skipWs) {
    app.use(
      createProxyMiddleware('/ws', {
        target: target.replace(/^http(s?):\/\//, 'ws$1://'),
        ws: true,
        changeOrigin: true,
        onError
      })
    );
  }
};
