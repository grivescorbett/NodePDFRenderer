# NodePDFRenderer

To setup environment on debian:jessie

``` bash
$ sudo apt-get install libwebkit2gtk-4.0-dev xvfb
```

To run node with xvfb to support the addon

``` bash
$ xvfb-run node
```

Example API usage

``` javascript
const fs = require('fs');
const PDFRenderer = require('./build/Release/PDFRenderer');

PDFRenderer.start();

const html = Buffer.from('<html><body>Hello World!</body></html>', 'utf8');
PDFRenderer.render(html, (err, pdfData) => {
  if (err) {
    console.log('err', err);
  } else {
    console.log('pdf size', pdfData.length);
    const ws = fs.createWriteStream('out.pdf');
    ws.write(pdfData);
    ws.end();
  }
});
```
