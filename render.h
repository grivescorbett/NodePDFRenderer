#ifndef RENDER_H_
#define RENDER_H_

#include <string>
#include <cstdio>
#include <gtk/gtkunixprint.h>
#include <webkit2/webkit2.h>

#define DEBUG 0
#define debug_print(...) \
            do { if (DEBUG) fprintf(stderr, __VA_ARGS__); } while (0)

uint8_t *render(const uint8_t *html, size_t html_length, size_t *length, std::string **error);
void setup_printer(GtkPageOrientation orientation, gdouble top, gdouble bottom, gdouble left, gdouble right, GtkUnit unit);


#endif /* RENDER_H_ */