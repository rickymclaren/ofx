/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Richard 2011 <richard@ACER5920G>
 * 
 * gtk-money is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * gtk-money is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <gtk/gtk.h>


#include <glib/gi18n.h>

#include <stdio.h>
#include <string.h>
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

typedef struct tx_type {
	char *date;
	char *amount;
	char *name;
	char *memo;
	char *balance;
} Tx_Type;

static GSList *tx_list = NULL;			// Single Linked List of transactions
static Tx_Type* tx;						// Current transaction
static float balance = 0.0;				// Current balance

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

static void
process_xml(xmlNode * a_node)
{
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
			if (strcmp((char *) cur_node->name, "DTPOSTED") == 0) {
				tx = calloc(1, sizeof(Tx_Type));
				tx->date = g_strdup((char *)xmlNodeGetContent (cur_node));
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
				tx_list = g_slist_prepend(tx_list, tx);
				tx = calloc(1, sizeof(Tx_Type));
			}

			if (strcmp((char *) cur_node->name, "BALAMT") == 0) {
				balance = atof((char *)xmlNodeGetContent (cur_node));
			}
			
			
        }

        process_xml(cur_node->children);
    }
}

/* For testing propose use the local (not installed) ui file */
/* #define UI_FILE PACKAGE_DATA_DIR"/gtk_money/ui/gtk_money.ui" */
#define UI_FILE "src/gtk_money.ui"

/* Signal handlers */
/* Note: These may not be declared static because signal autoconnection
 * only works with non-static methods
 */

/* Called when the window is closed */
void
destroy (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

static GtkWindow*
create_window (void)
{
	GtkWindow *window;
	GtkBuilder *builder;
	GError* error = NULL;
	GtkTreeIter iter;
	GtkListStore *store;

	/* Load UI from file */
	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_file (builder, UI_FILE, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}

	/* Auto-connect signal handlers */
	gtk_builder_connect_signals (builder, NULL);

	/* Get the window object from the ui file */
	window = GTK_WINDOW (gtk_builder_get_object (builder, "window"));

	store = GTK_LIST_STORE (gtk_builder_get_object (builder, "liststore1"));

	GSList* item = tx_list;
	while (item) {
		tx = item->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			               0, tx->date,
	    		           1, tx->amount,
		                   2, g_strjoin(" - ", tx->name, tx->memo, NULL),
		                   3, tx->balance,
	        		       -1);
		item = g_slist_next(item);
	}

	g_object_unref (builder);

	return window;
}

void
running_balance() {
	double tx_balance = balance;
	GSList* item = tx_list;
	while (item) {
		tx = item->data;
		tx->balance = g_strdup_printf("%.2f", tx_balance);	
		item = g_slist_next(item);
		tx_balance -= atof(tx->amount);
	}

}

int
main (int argc, char *argv[])
{
 	GtkWindow *window;


#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

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
	
	gtk_init (&argc, &argv);

	window = create_window ();
	gtk_window_maximize (window);
	gtk_widget_show (GTK_WIDGET(window));

	gtk_main ();
	return 0;
}
