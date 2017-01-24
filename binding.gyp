{
  "targets": [
    {
      "target_name": "PDFRenderer",
      "sources": [
        "main.cpp",
        "render.cpp",
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "/usr/include/gtk-3.0/unix-print"
      ],
      "libraries": [
        "-lwebkit2gtk-4.0","-lgtk-3","-lgdk-3","-lpangocairo-1.0","-lpango-1.0","-latk-1.0","-lcairo-gobject","-lcairo","-lgdk_pixbuf-2.0","-lsoup-2.4","-lgio-2.0","-lgobject-2.0","-ljavascriptcoregtk-4.0","-lglib-2.0"
      ],
      "cflags": [ "-std=c++11","-pthread","-I/usr/include/webkitgtk-4.0","-I/usr/include/gtk-3.0","-I/usr/include/at-spi2-atk/2.0","-I/usr/include/at-spi-2.0","-I/usr/include/dbus-1.0","-I/usr/lib/x86_64-linux-gnu/dbus-1.0/include","-I/usr/include/gtk-3.0","-I/usr/include/gio-unix-2.0/","-I/usr/include/cairo","-I/usr/include/pango-1.0","-I/usr/include/harfbuzz","-I/usr/include/pango-1.0","-I/usr/include/atk-1.0","-I/usr/include/cairo","-I/usr/include/pixman-1","-I/usr/include/freetype2","-I/usr/include/libpng12","-I/usr/include/gdk-pixbuf-2.0","-I/usr/include/libpng12","-I/usr/include/libsoup-2.4","-I/usr/include/libxml2","-I/usr/include/webkitgtk-4.0","-I/usr/include/glib-2.0","-I/usr/lib/x86_64-linux-gnu/glib-2.0/include"],
    }
  ]
}