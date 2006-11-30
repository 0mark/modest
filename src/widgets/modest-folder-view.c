/* Copyright (c) 2006, Nokia Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of the Nokia Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <glib/gi18n.h>
#include <string.h>

#include <tny-gtk-account-tree-model.h>
#include <tny-account-store.h>
#include <tny-account.h>
#include <tny-folder.h>
#include <modest-icon-names.h>
#include <modest-icon-factory.h>
#include <modest-tny-account-store.h>

#include "modest-folder-view.h"


/* 'private'/'protected' functions */
static void modest_folder_view_class_init  (ModestFolderViewClass *klass);
static void modest_folder_view_init        (ModestFolderView *obj);
static void modest_folder_view_finalize    (GObject *obj);

static gboolean     update_model             (ModestFolderView *self,
					      ModestTnyAccountStore *account_store);
static gboolean     update_model_empty       (ModestFolderView *self);

static void         on_selection_changed     (GtkTreeSelection *sel, gpointer data);
static void         on_subscription_changed  (TnyStoreAccount *store_account, TnyFolder *folder,
					      ModestFolderView *self);

static gboolean     modest_folder_view_update_model     (ModestFolderView *self,
							 TnyAccountStore *account_store);
static const gchar *get_account_name_from_folder        (GtkTreeModel *model, GtkTreeIter iter);

static void         modest_folder_view_disconnect_store_account_handlers (GtkTreeView *self);

enum {
	FOLDER_SELECTED_SIGNAL,
	LAST_SIGNAL
};

typedef struct _ModestFolderViewPrivate ModestFolderViewPrivate;
struct _ModestFolderViewPrivate {

	TnyAccountStore     *account_store;
	TnyFolder           *cur_folder;
	gboolean             view_is_empty;

	gulong               sig1, sig2;
	gulong              *store_accounts_handlers;
	GMutex              *lock;
	GtkTreeSelection    *cur_selection;
	TnyFolderStoreQuery *query;

};
#define MODEST_FOLDER_VIEW_GET_PRIVATE(o)			        \
	(G_TYPE_INSTANCE_GET_PRIVATE((o),				\
				     MODEST_TYPE_FOLDER_VIEW,	        \
				     ModestFolderViewPrivate))
/* globals */
static GObjectClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = {0}; 

GType
modest_folder_view_get_type (void)
{
	static GType my_type = 0;
	if (!my_type) {
		static const GTypeInfo my_info = {
			sizeof(ModestFolderViewClass),
			NULL,		/* base init */
			NULL,		/* base finalize */
			(GClassInitFunc) modest_folder_view_class_init,
			NULL,		/* class finalize */
			NULL,		/* class data */
			sizeof(ModestFolderView),
			1,		/* n_preallocs */
			(GInstanceInitFunc) modest_folder_view_init,
			NULL
		};
				
		my_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
		                                  "ModestFolderView",
		                                  &my_info, 0);		
	}
	return my_type;
}

static void
modest_folder_view_class_init (ModestFolderViewClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*) klass;

	parent_class            = g_type_class_peek_parent (klass);
	gobject_class->finalize = modest_folder_view_finalize;
	
	klass->update_model = modest_folder_view_update_model;

	g_type_class_add_private (gobject_class,
				  sizeof(ModestFolderViewPrivate));
	
 	signals[FOLDER_SELECTED_SIGNAL] = 
		g_signal_new ("folder_selected",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ModestFolderViewClass,
					       folder_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER); 
}



static void
text_cell_data  (GtkTreeViewColumn *column,  GtkCellRenderer *renderer,
		 GtkTreeModel *tree_model,  GtkTreeIter *iter,  gpointer data)
{
	GObject *rendobj;
	gchar *fname;
	gint unread;
	TnyFolderType type;
	
	gtk_tree_model_get (tree_model, iter,
			    TNY_GTK_ACCOUNT_TREE_MODEL_NAME_COLUMN, &fname,
			    TNY_GTK_ACCOUNT_TREE_MODEL_TYPE_COLUMN, &type,
			    TNY_GTK_ACCOUNT_TREE_MODEL_UNREAD_COLUMN, &unread, -1);
	rendobj = G_OBJECT(renderer);

	if (unread > 0) {
		gchar *folder_title = g_strdup_printf ("%s (%d)", fname, unread);
		g_object_set (rendobj,"text", folder_title,  "weight", 800, NULL);
		g_free (folder_title);
	} else 
		g_object_set (rendobj,"text", fname, "weight", 400, NULL);
		
	g_free (fname);
}

/* FIXME: move these to TnyMail */
enum {

	TNY_FOLDER_TYPE_NOTES = TNY_FOLDER_TYPE_ROOT + 1, /* urgh */
	TNY_FOLDER_TYPE_DRAFTS,
	TNY_FOLDER_TYPE_CONTACTS,
	TNY_FOLDER_TYPE_CALENDAR
};
	
static TnyFolderType
guess_folder_type (const gchar* name)
{
	TnyFolderType type;
	gchar *folder;

	g_return_val_if_fail (name, TNY_FOLDER_TYPE_NORMAL);
	
	type = TNY_FOLDER_TYPE_NORMAL;
	folder = g_utf8_strdown (name, strlen(name));

	if (strcmp (folder, "inbox") == 0 ||
	    strcmp (folder, _("inbox")) == 0)
		type = TNY_FOLDER_TYPE_INBOX;
	else if (strcmp (folder, "outbox") == 0 ||
		 strcmp (folder, _("outbox")) == 0)
		type = TNY_FOLDER_TYPE_OUTBOX;
	else if (g_str_has_prefix(folder, "junk") ||
		 g_str_has_prefix(folder, _("junk")))
		type = TNY_FOLDER_TYPE_JUNK;
	else if (g_str_has_prefix(folder, "trash") ||
		 g_str_has_prefix(folder, _("trash")))
		type = TNY_FOLDER_TYPE_JUNK;
	else if (g_str_has_prefix(folder, "sent") ||
		 g_str_has_prefix(folder, _("sent")))
		type = TNY_FOLDER_TYPE_SENT;

	/* these are not *really* TNY_ types */
	else if (g_str_has_prefix(folder, "draft") ||
		 g_str_has_prefix(folder, _("draft")))
		type = TNY_FOLDER_TYPE_DRAFTS;
	else if (g_str_has_prefix(folder, "notes") ||
		 g_str_has_prefix(folder, _("notes")))
		type = TNY_FOLDER_TYPE_NOTES;
	else if (g_str_has_prefix(folder, "contacts") ||
		 g_str_has_prefix(folder, _("contacts")))
		type = TNY_FOLDER_TYPE_CONTACTS;
	else if (g_str_has_prefix(folder, "calendar") ||
		 g_str_has_prefix(folder, _("calendar")))
		type = TNY_FOLDER_TYPE_CALENDAR;
	
	g_free (folder);
	return type;
}


static void
icon_cell_data  (GtkTreeViewColumn *column,  GtkCellRenderer *renderer,
		 GtkTreeModel *tree_model,  GtkTreeIter *iter, gpointer data)
{
	GObject *rendobj;
	GdkPixbuf *pixbuf;
	TnyFolderType type;
	gchar *fname = NULL;
	gint unread;
	
	rendobj = G_OBJECT(renderer);
	gtk_tree_model_get (tree_model, iter,
			    TNY_GTK_ACCOUNT_TREE_MODEL_TYPE_COLUMN, &type,
			    TNY_GTK_ACCOUNT_TREE_MODEL_NAME_COLUMN, &fname,
			    TNY_GTK_ACCOUNT_TREE_MODEL_UNREAD_COLUMN, &unread, -1);
	rendobj = G_OBJECT(renderer);
	
	if (type == TNY_FOLDER_TYPE_NORMAL)
		type = guess_folder_type (fname);
	
	if (fname)
		g_free (fname);

	switch (type) {
	case TNY_FOLDER_TYPE_ROOT:
		pixbuf = modest_icon_factory_get_icon (MODEST_FOLDER_ICON_ACCOUNT);
                break;
	case TNY_FOLDER_TYPE_INBOX:
                pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_INBOX);
                break;
        case TNY_FOLDER_TYPE_OUTBOX:
                pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_OUTBOX);
                break;
        case TNY_FOLDER_TYPE_JUNK:
                pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_JUNK);
                break;
        case TNY_FOLDER_TYPE_SENT:
                pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_SENT);
                break;
	case TNY_FOLDER_TYPE_DRAFTS:
		pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_DRAFTS);
                break;
	case TNY_FOLDER_TYPE_NOTES:
		pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_NOTES);
                break;
	case TNY_FOLDER_TYPE_CALENDAR:
		pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_CALENDAR);
                break;
	case TNY_FOLDER_TYPE_CONTACTS:
                pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_CONTACTS);
                break;
	case TNY_FOLDER_TYPE_NORMAL:
        default:
                pixbuf = modest_icon_factory_get_small_icon (MODEST_FOLDER_ICON_NORMAL);
                break;
        }

	g_object_set (rendobj,
		      "pixbuf-expander-open",
		      modest_icon_factory_get_icon (MODEST_FOLDER_ICON_OPEN),
		      "pixbuf-expander-closed",
		      modest_icon_factory_get_icon (MODEST_FOLDER_ICON_CLOSED),
		      "pixbuf", pixbuf,
		      NULL);
}

static void
modest_folder_view_init (ModestFolderView *obj)
{
	ModestFolderViewPrivate *priv;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *sel;
	
	priv =	MODEST_FOLDER_VIEW_GET_PRIVATE(obj);
	
	priv->view_is_empty  = TRUE;
	priv->account_store  = NULL;
	priv->cur_folder     = NULL;
	priv->query          = NULL;
	priv->lock           = g_mutex_new ();
	
	column = gtk_tree_view_column_new ();	
	gtk_tree_view_append_column (GTK_TREE_VIEW(obj),
				     column);
	
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						icon_cell_data, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, renderer,
						text_cell_data, NULL, NULL);
	
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW(obj));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);

	gtk_tree_view_column_set_spacing (column, 2);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_fixed_width (column, TRUE);		
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW(obj), FALSE);
	gtk_tree_view_set_enable_search     (GTK_TREE_VIEW(obj), FALSE);

}

static void
modest_folder_view_disconnect_store_account_handlers (GtkTreeView *self)
{
	TnyIterator *iter;
	ModestFolderViewPrivate *priv;
	GtkTreeModel *model;
	GtkTreeModelSort *sortable;
	gint i = 0;

	sortable = GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (self));
	if (!sortable)
		return; 

	model = gtk_tree_model_sort_get_model (sortable);
	if (!model)
		return; 

	priv =	MODEST_FOLDER_VIEW_GET_PRIVATE (self);	
	iter = tny_list_create_iterator (TNY_LIST (model));
	do {
		g_signal_handler_disconnect (G_OBJECT (tny_iterator_get_current (iter)),
					     priv->store_accounts_handlers [i++]);
		tny_iterator_next (iter);
	} while (!tny_iterator_is_done (iter));
}


static void
modest_folder_view_finalize (GObject *obj)
{
	ModestFolderViewPrivate *priv;
	GtkTreeSelection    *sel;
	
	g_return_if_fail (obj);
	
	priv =	MODEST_FOLDER_VIEW_GET_PRIVATE(obj);
	if (priv->account_store) {
		g_signal_handler_disconnect (G_OBJECT(priv->account_store),
					     priv->sig1);
		g_object_unref (G_OBJECT(priv->account_store));
		priv->account_store = NULL;
	}

	if (priv->lock) {
		g_mutex_free (priv->lock);
		priv->lock = NULL;
	}

	if (priv->store_accounts_handlers) {
		modest_folder_view_disconnect_store_account_handlers (GTK_TREE_VIEW (obj));
		g_free (priv->store_accounts_handlers);
		priv->store_accounts_handlers = NULL;
	}

	if (priv->query) {
		g_object_unref (G_OBJECT (priv->query));
		priv->query = NULL;
	}

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW(obj));
	if (sel)
		g_signal_handler_disconnect (G_OBJECT(sel), priv->sig2);
	
	G_OBJECT_CLASS(parent_class)->finalize (obj);
}


static void
on_account_update (TnyAccountStore *account_store, const gchar *account,
		   gpointer user_data)
{
	update_model_empty (MODEST_FOLDER_VIEW(user_data));
	
	if (!update_model (MODEST_FOLDER_VIEW(user_data), 
			   MODEST_TNY_ACCOUNT_STORE(account_store)))
		g_printerr ("modest: failed to update model for changes in '%s'",
			    account);
}

void
modest_folder_view_set_title (ModestFolderView *self, const gchar *title)
{
	GtkTreeViewColumn *col;
	
	g_return_if_fail (self);

	col = gtk_tree_view_get_column (GTK_TREE_VIEW(self), 0);
	if (!col) {
		g_printerr ("modest: failed get column for title\n");
		return;
	}

	gtk_tree_view_column_set_title (col, title);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(self),
					   title != NULL);
}

GtkWidget*
modest_folder_view_new (ModestTnyAccountStore *account_store, 
			TnyFolderStoreQuery *query)
{
	GObject *self;
	ModestFolderViewPrivate *priv;
	GtkTreeSelection *sel;
	
	g_return_val_if_fail (account_store, NULL);
	
	self = G_OBJECT(g_object_new(MODEST_TYPE_FOLDER_VIEW, NULL));
	priv = MODEST_FOLDER_VIEW_GET_PRIVATE(self);
	
	priv->account_store = g_object_ref (G_OBJECT (account_store));
	if (query)
		priv->query = g_object_ref (G_OBJECT (query));
	
	if (!update_model (MODEST_FOLDER_VIEW(self),
			   MODEST_TNY_ACCOUNT_STORE(account_store)))
		g_printerr ("modest: failed to update model\n");
	
	priv->sig1 = g_signal_connect (G_OBJECT(account_store), "account_update",
				       G_CALLBACK (on_account_update), self);	
	
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
	priv->sig2 = g_signal_connect (sel, "changed",
				       G_CALLBACK(on_selection_changed), self);
				     	
	return GTK_WIDGET(self);
}


const gchar *
modest_folder_view_get_selected_account (ModestFolderView *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	ModestFolderViewPrivate *priv;
	
	g_return_val_if_fail (self, NULL);
	priv = MODEST_FOLDER_VIEW_GET_PRIVATE(self);

	gtk_tree_selection_get_selected (priv->cur_selection, &model, &iter);

	return get_account_name_from_folder (model, iter);
}

static gboolean
update_model_empty (ModestFolderView *self)
{
	GtkTreeIter  iter;
	GtkTreeStore *store;
	ModestFolderViewPrivate *priv;
	
	g_return_val_if_fail (self, FALSE);
	priv = MODEST_FOLDER_VIEW_GET_PRIVATE(self);

	/* Disconnect old handlers */
	if (priv->store_accounts_handlers) {
		modest_folder_view_disconnect_store_account_handlers (GTK_TREE_VIEW (self));
		g_free (priv->store_accounts_handlers);
		priv->store_accounts_handlers = NULL;
	}

	/* Create the new model */
	store = gtk_tree_store_new (1, G_TYPE_STRING);
	gtk_tree_store_append (store, &iter, NULL);

	gtk_tree_store_set (store, &iter, 0,
			    _("(empty)"), -1);

	gtk_tree_view_set_model (GTK_TREE_VIEW(self),
				 GTK_TREE_MODEL(store));
	g_object_unref (store);

	priv->view_is_empty = TRUE;

	g_signal_emit (G_OBJECT(self), signals[FOLDER_SELECTED_SIGNAL], 0,
		       NULL);
	return TRUE;
}


static void
update_store_account_handlers (ModestFolderView *self, TnyList *account_list)
{
	gint size;
	ModestFolderViewPrivate *priv;
	TnyIterator *iter;
	
	priv =	MODEST_FOLDER_VIEW_GET_PRIVATE(self);

	/* Listen to subscription changes */
	size = tny_list_get_length (TNY_LIST (account_list)) * sizeof (gulong);

	g_assert (priv->store_accounts_handlers == NULL); /* don't leak */
	priv->store_accounts_handlers = g_malloc0 (size);
	iter = tny_list_create_iterator (account_list);
	
	if (!tny_iterator_is_done (iter)) 	
		priv->view_is_empty = FALSE; 
	else {
		gint i = 0;
		while (!tny_iterator_is_done (iter)) {
			
			priv->store_accounts_handlers [i++] =
				g_signal_connect (G_OBJECT (tny_iterator_get_current (iter)),
						  "subscription_changed",
						  G_CALLBACK (on_subscription_changed),
						  self);
			tny_iterator_next (iter);
			}
	}	
	g_object_unref (iter);			       
}

static gboolean
update_model (ModestFolderView *self, ModestTnyAccountStore *account_store)
{
	ModestFolderViewPrivate *priv;

	TnyList          *account_list;
	GtkTreeModel     *model, *sortable;

	g_return_val_if_fail (account_store, FALSE);
	priv =	MODEST_FOLDER_VIEW_GET_PRIVATE(self);

	update_model_empty (self); /* cleanup */
	
	//model        = GTK_TREE_MODEL(tny_gtk_account_tree_model_new (TRUE, priv->query)); /* async */
	model        = GTK_TREE_MODEL(tny_gtk_account_tree_model_new (TRUE, NULL)); /* async */

	account_list = TNY_LIST(model);

	tny_account_store_get_accounts (TNY_ACCOUNT_STORE(account_store),
					account_list,
					TNY_ACCOUNT_STORE_STORE_ACCOUNTS);

	if (account_list) {
		sortable = gtk_tree_model_sort_new_with_model (model);
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sortable),
						      TNY_GTK_ACCOUNT_TREE_MODEL_NAME_COLUMN, 
						      GTK_SORT_ASCENDING);
		gtk_tree_view_set_model (GTK_TREE_VIEW(self), sortable);

		update_store_account_handlers (self, account_list);
	}

	g_object_unref (model);
	return TRUE;
}


static void
on_selection_changed (GtkTreeSelection *sel, gpointer user_data)
{
	GtkTreeModel            *model;
	TnyFolder               *folder = NULL;
	GtkTreeIter             iter;
	ModestFolderView        *tree_view;
	ModestFolderViewPrivate *priv;
	gint type;
	const gchar *account_name;

	g_return_if_fail (sel);
	g_return_if_fail (user_data);
	
	priv = MODEST_FOLDER_VIEW_GET_PRIVATE(user_data);
	priv->cur_selection = sel;

	
	/* is_empty means that there is only the 'empty' item */
	if (priv->view_is_empty)
		return;
	
	/* folder was _un_selected if true */
	if (!gtk_tree_selection_get_selected (sel, &model, &iter)) {
		priv->cur_folder = NULL; /* FIXME: need this? */
		return; 
	}

	tree_view = MODEST_FOLDER_VIEW (user_data);

	gtk_tree_model_get (model, &iter, 
			    TNY_GTK_ACCOUNT_TREE_MODEL_TYPE_COLUMN, &type, 
			    TNY_GTK_ACCOUNT_TREE_MODEL_INSTANCE_COLUMN, &folder, 
			    -1);

	if (type == TNY_FOLDER_TYPE_ROOT) {
		account_name = tny_account_get_name (TNY_ACCOUNT (folder));
	} else {
		if (priv->cur_folder) 
			tny_folder_expunge (priv->cur_folder, NULL); /* FIXME */
		priv->cur_folder = folder;

		/* FIXME: this is ugly */
		account_name = get_account_name_from_folder (model, iter);
			
		g_signal_emit (G_OBJECT(tree_view), signals[FOLDER_SELECTED_SIGNAL], 0,
			       folder);
	}

}

static void 
on_subscription_changed  (TnyStoreAccount *store_account, 
			  TnyFolder *folder,
			  ModestFolderView *self)
{
	/* TODO: probably we won't need a full reload, just the store
	   account or even the parent of the folder */

	ModestFolderViewPrivate *priv;

	priv =	MODEST_FOLDER_VIEW_GET_PRIVATE(self);
	update_model (self, MODEST_TNY_ACCOUNT_STORE (priv->account_store));
}


static gboolean
modest_folder_view_update_model (ModestFolderView *self, TnyAccountStore *account_store)
{
	gboolean retval;
	
	g_return_val_if_fail (MODEST_IS_FOLDER_VIEW (self), FALSE);
	retval = update_model (self, MODEST_TNY_ACCOUNT_STORE(account_store)); /* ugly */

	g_signal_emit (G_OBJECT(self), signals[FOLDER_SELECTED_SIGNAL],
		       0, NULL);

	return retval;
}

static const gchar *
get_account_name_from_folder (GtkTreeModel *model, GtkTreeIter iter)
{
	GtkTreePath *path;
	GtkTreeIter new_iter;
	TnyFolder *account_folder;
	gint depth, i;

	path = gtk_tree_model_get_path (model, &iter);
	depth = gtk_tree_path_get_depth (path);

	for (i = 1; i < depth; ++i)
		gtk_tree_path_up (path);

	gtk_tree_model_get_iter (model, &new_iter, path);
	gtk_tree_model_get (model, &new_iter, 
			    TNY_GTK_ACCOUNT_TREE_MODEL_INSTANCE_COLUMN, &account_folder, 
			    -1);
	return tny_account_get_name (TNY_ACCOUNT (account_folder));
}
