#include <gtk/gtk.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

enum
{
    COL_DATE = 0,
    COL_AMOUNT,
    COL_BALANCE,
    COL_MEMO,
    NUM_COLS
} ;

typedef struct tx_type {
    char *date;
    long julian_day;
    char *amount;
    char *name;
    char *memo;
    char *balance;
    float fBalance;
} Tx_Type;

static GSList *tx_list = NULL;             // Single Linked List of transactions
static GSList *graph_list = NULL;          // Single Linked List of transactions with missing days filled in
static Tx_Type* tx;                        // Current transaction
static float balance = 0.0;                // Current balance
static float min = FLT_MAX;
static float max = FLT_MIN;
static long min_day = LONG_MAX;
static long max_day = LONG_MIN;

char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

char *sub_string(char *src, int offset, int length, char *buffer) {
    memcpy(buffer, &src[offset], length);
    buffer[length] = 0;
    return buffer;
}

typedef struct date_t {
    int year;
    int month;
    int day;
} Date_Type;

Date_Type *str_to_date(char *str, Date_Type *date) {
    char buffer[5];
    date->year = atoi(sub_string(str, 0, 4, buffer));
    date->month = atoi(sub_string(str, 4, 2, buffer));
    date->day = atoi(sub_string(str, 6, 2, buffer));
    return date;
}

long julian_day(char *str) {
    assert(strlen(str) == 8);
    Date_Type date;
    str_to_date(str, &date);

    int y = date.year;
    int m = date.month;
    int d = date.day;

    y+=8000;
    if(m<3) { y--; m+=12; }
    return (y*365) +(y/4) -(y/100) +(y/400) -1200820
              +(m*153+3)/5-92
              +d-1;
}

Date_Type *julian_to_date(long day, Date_Type *date) {
    long Z = day+0.5;
    long W = (Z - 1867216.25)/36524.25;
    long X = W/4;
    long A = Z+1+W-X;
    long B = A+1524;
    long C = (B-122.1)/365.25;
    long D = 365.25 * C;
    long E = (B-D)/30.6001;
    long F = 30.6001 * E;
    date->day = B-D-F;
    date->month = E < 13 ? E-1 : E-13;
    date->year = date->month <= 2 ? C-4715 : C-4716;
    return date;
}

char *date_to_str(Date_Type *dt) {
    return g_strdup_printf("%04d%02d%02d", dt->year, dt->month, dt->day);
}

char* get_xml(void) {

    const gchar* DOWNLOADS = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
    gchar* ofx_file = "";

    GDir* dir = g_dir_open (DOWNLOADS, 0, NULL);
    const gchar* file_name;
    while ((file_name = g_dir_read_name (dir))) {
        if (g_str_has_suffix (file_name, ".ofx")) {
            if ((strlen(ofx_file) == 0) || (g_strcmp0(ofx_file, file_name) < 0)) {
                ofx_file = g_strdup(file_name);
            }
        }
    }
    g_dir_close (dir);

    if (strlen(ofx_file) == 0) {
        printf("%s", "Could not find an ofx file");
        exit(1);
    }

    ofx_file = g_strjoin(G_DIR_SEPARATOR_S, DOWNLOADS, ofx_file, NULL);
    printf("Using %s\n", ofx_file);
    FILE *fp = fopen(ofx_file, "r");

    fseek(fp, 0, SEEK_END);
    int fileSize = ftell(fp);
    rewind(fp);
 
    char *data = (char*) calloc(sizeof(char), fileSize + 20);
 
    fread(data, 1, fileSize, fp);
    if(ferror(fp)){
        puts("fread()");
        exit(1);
    }
    fclose(fp);

    return strstr(data, "<OFX>");
    
}

static void process_xml(xmlNode * a_node) {
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (strcmp((char *) cur_node->name, "STMTTRN") == 0) {
                tx = calloc(1, sizeof(Tx_Type));
                tx_list = g_slist_prepend(tx_list, tx);
            }
            if (strcmp((char *) cur_node->name, "DTPOSTED") == 0) {
                tx->date = g_strdup((char *)xmlNodeGetContent (cur_node));
                tx->julian_day = julian_day(tx->date);
            }
            if (strcmp((char *) cur_node->name, "NAME") == 0) {
                tx->name = g_strdup((char *)xmlNodeGetContent (cur_node));
            }
            if (strcmp((char *) cur_node->name, "TRNAMT") == 0) {
                tx->amount = g_strdup((char *)xmlNodeGetContent (cur_node));
            }
            if (strcmp((char *) cur_node->name, "MEMO") == 0) {
                tx->memo = g_strdup((char *)xmlNodeGetContent (cur_node));
                tx->balance = "N/A";
            }

            if (strcmp((char *) cur_node->name, "BALAMT") == 0) {
                balance = atof((char *)xmlNodeGetContent (cur_node));
            }
            
            
        }

        process_xml(cur_node->children);
    }
}

/* Called when the window is closed */
static void destroy (GtkWidget *widget, gpointer data) {
    gtk_main_quit ();
}

/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;

static int BORDER_LEFT = 50;
static int BORDER_TOP = 10;
static int BORDER_BOTTOM = 10;
static float scale_x, scale_y;
static int origin_x, origin_y;

static void draw_line(cairo_t *cr, float x1, float y1, float x2, float y2) {
    x1 = origin_x + x1 * scale_x;
    x2 = origin_x + x2 * scale_x;
    y1 = origin_y + y1 * scale_y;
    y2 = origin_y + y2 * scale_y;
    cairo_move_to( cr, x1, y1);
    cairo_line_to( cr, x2, y2);
    cairo_stroke(cr);
}

/* Create a new backing pixmap of the appropriate size */
static gint configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    if (pixmap)
        gdk_pixmap_unref(pixmap);

    pixmap = gdk_pixmap_new(widget->window,
                          widget->allocation.width,
                          widget->allocation.height,
                          -1);
    gdk_draw_rectangle (pixmap,
                      widget->style->white_gc,
                      TRUE,
                      0, 0,
                      widget->allocation.width,
                      widget->allocation.height);

    cairo_t *cr = gdk_cairo_create(pixmap);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width (cr, 1.0);

    // Text 
    cairo_move_to(cr, 0.0, BORDER_TOP);
    cairo_show_text(cr, g_strdup_printf("%.2f", max));
    cairo_move_to(cr, 0.0, widget->allocation.height);
    cairo_show_text(cr, g_strdup_printf("%.2f", min));

    // Graph
    scale_x = (widget->allocation.width - BORDER_LEFT) / (max_day - min_day);
    scale_y = (widget->allocation.height - BORDER_TOP - BORDER_BOTTOM) / (max - min);
    origin_x = BORDER_LEFT;
    origin_y = BORDER_TOP + max * scale_y;
    scale_y *= -1;

    // Axis
    draw_line(cr, 0, min, 0, max);
    draw_line(cr, 0, 0, max_day - min_day, 0);

    cairo_set_source_rgb(cr, 0, 0, 0.5);
    cairo_set_line_width (cr, 2.0);

    // Bars
    GSList* item = graph_list;
    Date_Type dt;
    while (item) {
        tx = item->data;
        long day = tx->julian_day - min_day;
        float amt = tx->fBalance;
        str_to_date(tx->date, &dt);
        draw_line(cr, day, 0, day, amt);
        if (dt.day == 20) {
            cairo_move_to(cr, origin_x + day * scale_x, widget->allocation.height - 5);
            cairo_show_text(cr, months[dt.month-1]);
        }
        item = g_slist_next(item);
    }

    cairo_destroy(cr);
  
    return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint expose_event (GtkWidget *widget, GdkEventExpose *event) {
    gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                  pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);

    return FALSE;
}

void running_balance() {
    double tx_balance = balance;
    GSList* item = tx_list;
    while (item) {
        tx = item->data;
        tx->fBalance = tx_balance;
        tx->balance = g_strdup_printf("%.2f", tx_balance);    
        min = tx_balance < min ? tx_balance : min;
        max = tx_balance > max ? tx_balance : max;
        min_day = tx->julian_day < min_day ? tx->julian_day : min_day;
        max_day = tx->julian_day > max_day ? tx->julian_day : max_day;
        item = g_slist_next(item);
        tx_balance -= atof(tx->amount);
    }

}

Tx_Type *find_tx_for_day(long day) {
    GSList* item = tx_list;
    while (item) {
        tx = item->data;
        if (tx->julian_day == day) {
            return tx;
        }
        item = g_slist_next(item);
    }
    return NULL;
}

void create_graph_data() {
    long day;
    Tx_Type *previous_tx;
    Date_Type dt;
    for (day = min_day; day <= max_day; day++) {
        tx = find_tx_for_day(day);
        if (tx) {
              graph_list = g_slist_prepend(graph_list, tx);
            previous_tx = tx;

        } else {
            tx = calloc(1, sizeof(Tx_Type));
            memcpy(tx, previous_tx, sizeof(Tx_Type));
            tx->julian_day = day;
            tx->date = date_to_str(julian_to_date(day, &dt));
            graph_list = g_slist_prepend(graph_list, tx);
        }
    }
}

static GtkTreeModel *create_and_fill_model (void) {
    GtkListStore  *store;
    GtkTreeIter    iter;
  
    store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    GSList* item = tx_list;
    while (item) {
        tx = item->data;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_DATE, tx->date,
                           COL_AMOUNT, tx->amount,
                           COL_MEMO, g_strjoin(" - ", tx->name, tx->memo, NULL),
                           COL_BALANCE, tx->balance,
                           -1);
        item = g_slist_next(item);
    }

    return GTK_TREE_MODEL (store);
}

static GtkWindow* create_window (void) {
    GtkWindow *window;
    GError* error = NULL;
    GtkTreeIter iter;
    GtkListStore *store;

    // Build from bottom up

    GtkWidget *view = gtk_tree_view_new();
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                 -1,      
                                                "Date",  
                                                renderer,
                                                "text", COL_DATE,
                                                NULL);    
    renderer = gtk_cell_renderer_text_new ();
    g_object_set(renderer, "xalign", 1.0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Amount",  
                                               renderer,
                                               "text", COL_AMOUNT,
                                               NULL);    
    renderer = gtk_cell_renderer_text_new ();
    g_object_set(renderer, "xalign", 1.0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Balance",  
                                               renderer,
                                               "text", COL_BALANCE,
                                               NULL);    
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Memo",  
                                               renderer,
                                               "text", COL_MEMO,
                                               NULL);    

    GtkTreeModel *model = create_and_fill_model ();

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

    /* The tree view has acquired its own reference to the
     *  model, so we can drop ours. That way the model will
     *  be freed automatically when the tree view is destroyed */

    g_object_unref (model);    
    gtk_widget_show (view);

    GtkWidget *draw = gtk_drawing_area_new();
    g_signal_connect (draw, "configure-event", G_CALLBACK (configure_event), NULL);
    g_signal_connect (draw, "expose-event", G_CALLBACK (expose_event), NULL);
    gtk_widget_show (draw);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), view);    
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); 
    gtk_widget_show (scroll);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), scroll, gtk_label_new ("Ledger"));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), draw, gtk_label_new ("Graph"));
    gtk_widget_show (notebook);

    window = GTK_WINDOW( gtk_window_new (GTK_WINDOW_TOPLEVEL));
    g_signal_connect (window, "delete-event", G_CALLBACK (destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    gtk_container_add (GTK_CONTAINER (window), notebook);    

    return window;
}

int main (int argc, char *argv[]) {
    GtkWindow *window;

    LIBXML_TEST_VERSION

    char *content = get_xml();

    xmlDocPtr doc; /* the resulting document tree */

    /*
     * The document being in memory, it have no base per RFC 2396,
     * and the "noname.xml" argument will serve as its base.
     */
    doc = xmlReadMemory(content, strlen(content), "noname.xml", NULL, 0);
    if (doc == NULL) {
        fprintf(stderr, "Failed to parse document\n");
        exit(-1);
    }
    
    xmlNode *root_element = xmlDocGetRootElement(doc);
    process_xml(root_element);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    running_balance();
    printf("Min balance = %f\n", min);
    printf("Max balance = %f\n", max);
    printf("Number of days = %ld\n", max_day - min_day + 1);

    create_graph_data();
    
    gtk_init (&argc, &argv);

    window = create_window ();
    gtk_window_maximize (window);
    gtk_widget_show (GTK_WIDGET(window));

    gtk_main ();
    return 0;
}
