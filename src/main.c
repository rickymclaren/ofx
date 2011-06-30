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

//#include <config.h>
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

/* Called when the window is closed */
static void
destroy (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
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

static GtkTreeModel *
create_and_fill_model (void)
{
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

static GtkWindow*
create_window (void)
{
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
  	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                               -1,      
                                               "Amount",  
                                               renderer,
                                               "text", COL_AMOUNT,
                                               NULL);	
 	renderer = gtk_cell_renderer_text_new ();
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
	
	GtkWidget *button2 = gtk_button_new_with_label ("Goodbye World");
    gtk_widget_show (button2);

	GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), view);	
	gtk_scrolled_window_set_policy(scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); 
    gtk_widget_show (scroll);

	GtkWidget* notebook = gtk_notebook_new();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), scroll, gtk_label_new ("Ledger"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), button2, gtk_label_new ("Graph"));
    gtk_widget_show (notebook);

	window = GTK_WINDOW( gtk_window_new (GTK_WINDOW_TOPLEVEL));
	g_signal_connect (window, "delete-event",
	          G_CALLBACK (destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    gtk_container_add (GTK_CONTAINER (window), notebook);	

	return window;
}

int
main (int argc, char *argv[])
{
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
	
	gtk_init (&argc, &argv);

	window = create_window ();
	gtk_window_maximize (window);
	gtk_widget_show (GTK_WIDGET(window));

	gtk_main ();
	return 0;
}
