#include <stdexcept>
#include <stdio.h>
#include "render.h"

int get_tmp_file(std::string& result) {
    int fd;
    char name[] = "/tmp/fileXXXXXX";
    fd = mkstemp(name);
    if (fd < 0) {
        return -1;
    }
    close(fd); // Theoretically really bad but we are not concerned about adversaries, only solution is to hack on GTK.
    result.assign(name + 5); // Remove /tmp/
    debug_print("Created filename %s\n", result.c_str());
    return 0;
}

// Copied from a really sketchy website
long get_file_size(FILE *file)
{
	long lCurPos, lEndPos;
	lCurPos = ftell(file);
	fseek(file, 0, 2);
	lEndPos = ftell(file);
	fseek(file, lCurPos, 0);
	return lEndPos;
}

gboolean find_file_printer(GtkPrinter* printer, char** data) {
	if (!g_strcmp0(G_OBJECT_TYPE_NAME(gtk_printer_get_backend(printer)), "GtkPrintBackendFile")) {
		*data = g_strdup(gtk_printer_get_name(printer));
		return TRUE;
	}
	return FALSE;
}

GtkWidget *window;
WebKitWebView *web_view;
GtkPageSetup *page_setup;
GtkPrintSettings *print_settings;

bool gtk_err = false; // This does not need to be thread safe.

void web_view_load_failed (WebKitWebView  *web_view,
               WebKitLoadEvent load_event,
               gchar          *failing_uri,
               gpointer        error,
               gpointer        user_data) {
    gtk_err = true;
}

void web_view_load_changed(WebKitWebView* web_view, WebKitLoadEvent load_event, gpointer user_data) {
    // Check for !gtk_err because this is fired even if load-failed is fired
    if (load_event == WEBKIT_LOAD_FINISHED) {
        debug_print ("load changed sig\n");
        gtk_main_quit();
    }
}

void web_view_print_finished(WebKitPrintOperation *print_operation, gpointer user_data) {
    debug_print("Print finished signal\n");
    gtk_main_quit();
}

void web_view_print_vailed (WebKitPrintOperation *print_operation, GError *error, gpointer user_data) {
    gtk_err = true;
}


void setup_printer(GtkPageOrientation orientation, gdouble top, gdouble bottom, gdouble left, gdouble right, GtkUnit unit) {
    print_settings = gtk_print_settings_new();
    page_setup = gtk_page_setup_new();

    gtk_print_settings_set_quality(print_settings, GTK_PRINT_QUALITY_HIGH);

    char* printer = NULL;
	gtk_enumerate_printers((GtkPrinterFunc)find_file_printer, &printer, NULL, TRUE);
	gtk_print_settings_set_printer(print_settings, printer);

    //TODO: custom paper sizes
    auto* paperSize = gtk_paper_size_new(gtk_paper_size_get_default());
    gtk_page_setup_set_paper_size(page_setup, paperSize);

    gtk_page_setup_set_top_margin(page_setup, top, unit);
    gtk_page_setup_set_bottom_margin(page_setup, bottom, unit);
    gtk_page_setup_set_left_margin(page_setup, left, unit);
    gtk_page_setup_set_right_margin(page_setup, right, unit);

    gtk_print_settings_set_orientation(print_settings, orientation);

    debug_print("gtk_init\n");
    gtk_init(0, NULL);

    window = gtk_offscreen_window_new();
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768); //TODO: Configurable

    debug_print("create_webview\n");
    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(web_view));

    g_signal_connect(web_view, "load-changed", G_CALLBACK(web_view_load_changed), NULL);
}

void load_web_view(const uint8_t *html, size_t html_length) {
    debug_print("load html\n");
    webkit_web_view_load_bytes(web_view, g_bytes_new_static(html, html_length), "text/html", NULL, "http://www.untapt.com"); // NULL here means utf8

    debug_print("get_focus\n");
    gtk_widget_grab_focus(GTK_WIDGET(web_view));

    // Make sure the main window and all its contents are visible
    debug_print("show\n");
    gtk_widget_show_all(window);
}

int do_print(std::string& filename) {
    auto error = get_tmp_file(filename);
    if (error < 0) {
        return -1;
    }

    gtk_print_settings_set(print_settings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, "pdf");
    gtk_print_settings_set(print_settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, filename.c_str());
    gtk_print_settings_set(print_settings, GTK_PRINT_SETTINGS_OUTPUT_DIR, "/tmp");

    auto* op = webkit_print_operation_new(web_view);
    webkit_print_operation_set_page_setup(op, page_setup);
    webkit_print_operation_set_print_settings(op, print_settings);

    g_signal_connect(op, "finished", G_CALLBACK(web_view_print_finished), NULL);

    debug_print("do print\n");
    webkit_print_operation_print(op);

    //Wait
    gtk_main();

    if (gtk_err) {
        return -1;
    }

    return 0;
}

uint8_t *render(const uint8_t *html, size_t html_length, size_t *length, std::string **error) {
    gtk_err = false;

    load_web_view(html, html_length);

    gtk_main();
    debug_print("done loop\n");

    if (gtk_err) {
        *error = new std::string("Error loading html.");
        return NULL;
    }

    std::string filename;

    if (do_print(filename) < 0) {
        *error = new std::string("Error printing.");
        return NULL;
    }

    debug_print("Sending filename %s back\n", filename.c_str());

    std::string full_path = std::string("/tmp/").append(filename).append(".pdf");
    debug_print("Opening %s\n", full_path.c_str());

    FILE* fp = nullptr;

    if ((fp = fopen(full_path.c_str(), "rb")) == NULL) {
        *error = new std::string("Error opening print output.");
        return NULL;
    }

    auto file_size = get_file_size(fp);
    debug_print("File size is %zd\n", file_size);

    auto pdf_data = new uint8_t[file_size];
    fread(pdf_data, file_size, 1, fp);
    *length = file_size;

    fclose(fp);
    remove(full_path.c_str());
    remove(full_path.substr(0,full_path.length()-4).c_str());

    return pdf_data;
}

