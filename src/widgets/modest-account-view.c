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
#include "modest-account-view.h"

#include <modest-account-mgr.h>
#include <modest-account-mgr-helpers.h>
#include <modest-tny-account.h>
#include <modest-text-utils.h>
#include <modest-runtime.h>
#include <modest-signal-mgr.h>

#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <string.h> /* For strcmp(). */

/* 'private'/'protected' functions */
static void modest_account_view_class_init    (ModestAccountViewClass *klass);
static void modest_account_view_init          (ModestAccountView *obj);
static void modest_account_view_finalize      (GObject *obj);

static void modest_account_view_select_account (ModestAccountView *account_view, 
						const gchar* account_name);

static void on_default_account_changed         (ModestAccountMgr *mgr,
						gpointer user_data);

static void on_display_name_changed            (ModestAccountMgr *self, 
						const gchar *account,
						gpointer user_data);

typedef enum {
	MODEST_ACCOUNT_VIEW_NAME_COLUMN,
	MODEST_ACCOUNT_VIEW_DISPLAY_NAME_COLUMN,
	MODEST_ACCOUNT_VIEW_IS_ENABLED_COLUMN,
	MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN,
	MODEST_ACCOUNT_VIEW_PROTO_COLUMN,
	MODEST_ACCOUNT_VIEW_LAST_UPDATED_COLUMN,

	MODEST_ACCOUNT_VIEW_COLUMN_NUM
} AccountViewColumns;

typedef struct _ModestAccountViewPrivate ModestAccountViewPrivate;
struct _ModestAccountViewPrivate {
	ModestAccountMgr *account_mgr;

	/* Signal handlers */
	GSList *sig_handlers;
};
#define MODEST_ACCOUNT_VIEW_GET_PRIVATE(o)      (G_TYPE_INSTANCE_GET_PRIVATE((o), \
                                                 MODEST_TYPE_ACCOUNT_VIEW, \
                                                 ModestAccountViewPrivate))
/* globals */
static GtkTreeViewClass *parent_class = NULL;

GType
modest_account_view_get_type (void)
{
	static GType my_type = 0;
	if (!my_type) {
		static const GTypeInfo my_info = {
			sizeof(ModestAccountViewClass),
			NULL,		/* base init */
			NULL,		/* base finalize */
			(GClassInitFunc) modest_account_view_class_init,
			NULL,		/* class finalize */
			NULL,		/* class data */
			sizeof(ModestAccountView),
			1,		/* n_preallocs */
			(GInstanceInitFunc) modest_account_view_init,
			NULL
		};
		my_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
		                                  "ModestAccountView",
		                                  &my_info, 0);
	}
	return my_type;
}

static void
modest_account_view_class_init (ModestAccountViewClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*) klass;

	parent_class            = g_type_class_peek_parent (klass);
	gobject_class->finalize = modest_account_view_finalize;

	g_type_class_add_private (gobject_class, sizeof(ModestAccountViewPrivate));
}

static void
modest_account_view_init (ModestAccountView *obj)
{
 	ModestAccountViewPrivate *priv;
	
	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE(obj);
	
	priv->sig_handlers = NULL;
}

static void
modest_account_view_finalize (GObject *obj)
{
	ModestAccountViewPrivate *priv;

	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE(obj);

	/* Disconnect signals */
	modest_signal_mgr_disconnect_all_and_destroy (priv->sig_handlers);

	if (priv->account_mgr) {	
		g_object_unref (G_OBJECT(priv->account_mgr));
		priv->account_mgr = NULL; 
	}
	
	G_OBJECT_CLASS(parent_class)->finalize (obj);
}

/* Get the string for the last updated time. Result must be g_freed */
static gchar*
get_last_updated_string(ModestAccountMgr* account_mgr, ModestAccountData *account_data)
{
	/* FIXME: let's assume that 'last update' applies to the store account... */
	gchar* last_updated_string;
	time_t last_updated = account_data->store_account->last_updated;
	if (!modest_account_mgr_account_is_busy(account_mgr, account_data->account_name)) {
		if (last_updated > 0) 
			last_updated_string = modest_text_utils_get_display_date(last_updated);
		else
			last_updated_string = g_strdup (_("mcen_va_never"));
	} else 	{
		/* FIXME: There should be a logical name in the UI specs */
		last_updated_string = g_strdup(_("mcen_va_refreshing"));
	}
	return last_updated_string;
}

static void
update_account_view (ModestAccountMgr *account_mgr, ModestAccountView *view)
{
	GSList *account_names, *cursor;
	GtkListStore *model;
		
	model = GTK_LIST_STORE(gtk_tree_view_get_model (GTK_TREE_VIEW(view)));
	
	/* Get the ID of the currently-selected account,
	 * so we can select it again after rebuilding the list.
	 * Note that the name doesn't change even when the display name changes.
	 */
	gchar *selected_name = modest_account_view_get_selected_account (view);

	gtk_list_store_clear (model);

	/* Note: We do not show disabled accounts.
	 * Of course, this means that there is no UI to enable or disable
	 * accounts. That is OK for maemo where no such feature or UI is
	 * specified, so the "enabled" property is used internally to avoid
	 * showing unfinished accounts. If a user-visible "enabled" is
	 * needed in the future, we must use a second property for the
	 * current use instead */
	cursor = account_names = modest_account_mgr_account_names (account_mgr,
		TRUE /* only enabled accounts. */);

	while (cursor) {
		gchar *account_name;
		ModestAccountData *account_data;
		
		account_name = (gchar*)cursor->data;
		
		account_data = modest_account_mgr_get_account_data (account_mgr, account_name);
		if (!account_data) {
			g_printerr ("modest: failed to get account data for %s\n", account_name);
			continue;
		}

		/* don't display accounts without stores */
		if (account_data->store_account) {

			GtkTreeIter iter;
			
			gchar *last_updated_string = get_last_updated_string(account_mgr, account_data);
			
			if (account_data->is_enabled) {
				const gchar *proto_name;

				proto_name = modest_protocol_info_get_transport_store_protocol_name (account_data->store_account->proto);
				gtk_list_store_insert_with_values (
					model, &iter, 0,
					MODEST_ACCOUNT_VIEW_NAME_COLUMN, account_name,
					MODEST_ACCOUNT_VIEW_DISPLAY_NAME_COLUMN, account_data->display_name,
					MODEST_ACCOUNT_VIEW_IS_ENABLED_COLUMN, account_data->is_enabled,
					MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN, account_data->is_default,
					MODEST_ACCOUNT_VIEW_PROTO_COLUMN, proto_name,
					MODEST_ACCOUNT_VIEW_LAST_UPDATED_COLUMN,  last_updated_string,
					-1);
			}
			g_free (last_updated_string);
		}

		modest_account_mgr_free_account_data (account_mgr, account_data);
		cursor = cursor->next;
	}

	modest_account_mgr_free_account_names (account_names);
	account_names = NULL;
	
	/* Try to re-select the same account: */
	if (selected_name) {
		modest_account_view_select_account (view, selected_name);
		g_free (selected_name);
	}
}

static void
on_account_busy_changed(ModestAccountMgr *account_mgr, 
                        const gchar *account_name,
                        gboolean busy, 
                        ModestAccountView *self)
{
	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model (GTK_TREE_VIEW(self)));
	GtkTreeIter iter;
	gboolean found = FALSE;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	do {
		gchar* cur_name;
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 
				   MODEST_ACCOUNT_VIEW_NAME_COLUMN, 
				   &cur_name, -1);

		if (g_str_equal(cur_name, account_name)) {
			ModestAccountData* account_data = 
				modest_account_mgr_get_account_data (account_mgr, account_name);
			if (!account_data) {
				g_free (cur_name);
				return;
			}
			gchar* last_updated_string = get_last_updated_string(account_mgr, account_data);
			gtk_list_store_set(model, &iter, 
					   MODEST_ACCOUNT_VIEW_LAST_UPDATED_COLUMN, last_updated_string,
					   -1);
			g_free (last_updated_string);
			modest_account_mgr_free_account_data (account_mgr, account_data);
			found = TRUE;
		}
		g_free (cur_name);

	} while (!found && gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
}

static void
on_account_inserted (TnyAccountStore *account_store, 
		     TnyAccount *account,
		     gpointer user_data)
{
	ModestAccountView *self;
	ModestAccountViewPrivate *priv;

	g_return_if_fail (MODEST_IS_ACCOUNT_VIEW (user_data));

	self = MODEST_ACCOUNT_VIEW (user_data);
	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE (self);

	/* Do not refresh the view with transport accounts */
	if (TNY_IS_STORE_ACCOUNT (account))
		update_account_view (priv->account_mgr, self);
}

static void
on_account_removed (TnyAccountStore *account_store, 
		    TnyAccount *account,
		    gpointer user_data)
{
	ModestAccountView *self;
	ModestAccountViewPrivate *priv;

	g_return_if_fail (MODEST_IS_ACCOUNT_VIEW (user_data));

	self = MODEST_ACCOUNT_VIEW (user_data);
	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE (self);

	update_account_view (priv->account_mgr, self);
}


static void
on_account_changed (TnyAccountStore *account_store, 
		    TnyAccount *account,
		    gpointer user_data)
{
	ModestAccountView *self = NULL;
	ModestAccountViewPrivate *priv = NULL;

	g_return_if_fail (MODEST_IS_ACCOUNT_VIEW (user_data));
	g_return_if_fail (account);
	g_return_if_fail (TNY_IS_ACCOUNT (account));

	self = MODEST_ACCOUNT_VIEW (user_data);
	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE (self);
	
	/* Update account view */
/* 	update_account_view (priv->account_mgr, self); */
}

static void
on_account_default_toggled (GtkCellRendererToggle *cell_renderer, 
			    gchar *path,
			    ModestAccountView *self)
{
	ModestAccountViewPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *account_name = NULL;

	g_return_if_fail (MODEST_IS_ACCOUNT_VIEW (self));

	/* If it's active then do nothing, no need to reenable it as
	   default account */
	if (gtk_cell_renderer_toggle_get_active (cell_renderer))
		return;

	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE(self);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW(self));	
	gtk_tree_model_get_iter_from_string (model, &iter, path);
	
	gtk_tree_model_get (model, &iter, 
			    MODEST_ACCOUNT_VIEW_NAME_COLUMN, 
			    &account_name, -1);

	/* Set this previously-non-default account as the
	   default. We're not updating here the value of the
	   DEFAULT_COLUMN because we'll do it in the
	   "default_account_changed" signal handler. We do it like
	   this because that way the signal handler is useful also
	   when we're inserting a new account and there is no other
	   one defined, in that case the change of account is provoked
	   by the account mgr and not by a signal toggle.*/
	modest_account_mgr_set_default_account (priv->account_mgr, account_name);

	g_free (account_name);
}

void
bold_if_default_cell_data  (GtkTreeViewColumn *column,  GtkCellRenderer *renderer,
			    GtkTreeModel *tree_model,  GtkTreeIter *iter,  gpointer user_data)
{
	gboolean is_default;
	gtk_tree_model_get (tree_model, iter, MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN,
			    &is_default, -1);
	g_object_set (G_OBJECT(renderer),
		      "weight", is_default ? 800: 400,
		      NULL);
}

static void
init_view (ModestAccountView *self)
{
	ModestAccountViewPrivate *priv;
	GtkCellRenderer *toggle_renderer, *text_renderer;
	GtkListStore *model;
	GtkTreeViewColumn *column;
	
	g_return_if_fail (MODEST_IS_ACCOUNT_VIEW (self));
	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE(self);
		
	model = gtk_list_store_new (6,
				    G_TYPE_STRING,  /* account name */
				    G_TYPE_STRING,  /* account display name */
				    G_TYPE_BOOLEAN, /* is-enabled */
				    G_TYPE_BOOLEAN, /* is-default */
				    G_TYPE_STRING,  /* account proto (pop, imap,...) */
				    G_TYPE_STRING   /* last updated (time_t) */
		); 
		
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (model), MODEST_ACCOUNT_VIEW_DISPLAY_NAME_COLUMN, 
		GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW(self), GTK_TREE_MODEL(model));
	g_object_unref (G_OBJECT (model));

	toggle_renderer = gtk_cell_renderer_toggle_new ();
	text_renderer = gtk_cell_renderer_text_new ();

	/* the is_default column */
	g_object_set (G_OBJECT(toggle_renderer), "activatable", TRUE, "radio", TRUE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(self),
				     gtk_tree_view_column_new_with_attributes (
					     _("mcen_ti_default"), toggle_renderer,
					     "active", MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN, NULL));
					
	/* Disable the Maemo GtkTreeView::allow-checkbox-mode Maemo modification, 
	 * which causes the model column to be updated automatically when the row is clicked.
	 * Making this the default in Maemo's GTK+ is obviously a bug:
	 * https://maemo.org/bugzilla/show_bug.cgi?id=146
	 *
	 * djcb: indeed, they have been removed for post-bora, i added the ifdefs...
       	 */
#ifdef MODEST_HAVE_HILDON0_WIDGETS
	g_object_set(G_OBJECT(self), "allow-checkbox-mode", FALSE, NULL);
	g_object_set(G_OBJECT(toggle_renderer), "checkbox-mode", FALSE, NULL);
#endif /* MODEST_HAVE_HILDON0_WIDGETS */

	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers,
					   G_OBJECT(toggle_renderer), 
					   "toggled", 
					   G_CALLBACK(on_account_default_toggled),
					   self);
	
	/* account name */
	column =  gtk_tree_view_column_new_with_attributes (_("mcen_ti_account"), text_renderer, "text",
							    MODEST_ACCOUNT_VIEW_DISPLAY_NAME_COLUMN, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(self), column);
	gtk_tree_view_column_set_cell_data_func(column, text_renderer, bold_if_default_cell_data,
						NULL, NULL);

	/* last update for this account */
	column =  gtk_tree_view_column_new_with_attributes (_("mcen_ti_lastupdated"), text_renderer,"text",
							    MODEST_ACCOUNT_VIEW_LAST_UPDATED_COLUMN, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(self),column);
	gtk_tree_view_column_set_cell_data_func(column, text_renderer, bold_if_default_cell_data,
						NULL, NULL);
			
	/* Show the column headers,
	 * which does not seem to be the default on Maemo.
	 */			
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(self), TRUE);

	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers, 
					   G_OBJECT (modest_runtime_get_account_store ()),
					   "account_removed",
					   G_CALLBACK(on_account_removed), 
					   self);
	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers, 
					   G_OBJECT (modest_runtime_get_account_store ()),
					   "account_inserted",
					   G_CALLBACK(on_account_inserted), 
					   self);
	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers, 
					   G_OBJECT (modest_runtime_get_account_store ()),
					   "account_changed",
					   G_CALLBACK(on_account_changed), 
					   self);
	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers, 
					   G_OBJECT(priv->account_mgr),
					   "account_busy_changed",
					   G_CALLBACK(on_account_busy_changed), 
					   self);
	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers, 
					   G_OBJECT(priv->account_mgr),
					   "default_account_changed",
					   G_CALLBACK(on_default_account_changed), 
					   self);
	priv->sig_handlers = 
		modest_signal_mgr_connect (priv->sig_handlers, 
					   G_OBJECT(priv->account_mgr),
					   "display_name_changed",
					   G_CALLBACK(on_display_name_changed), 
					   self);
}


ModestAccountView*
modest_account_view_new (ModestAccountMgr *account_mgr)
{
	GObject *obj;
	ModestAccountViewPrivate *priv;
	
	g_return_val_if_fail (account_mgr, NULL);
	
	obj  = g_object_new(MODEST_TYPE_ACCOUNT_VIEW, NULL);
	priv = MODEST_ACCOUNT_VIEW_GET_PRIVATE(obj);
	
	g_object_ref (G_OBJECT (account_mgr));
	priv->account_mgr = account_mgr;

	init_view (MODEST_ACCOUNT_VIEW (obj));
	update_account_view (account_mgr, MODEST_ACCOUNT_VIEW (obj));
	
	return MODEST_ACCOUNT_VIEW (obj);
}

gchar *
modest_account_view_get_selected_account (ModestAccountView *self)
{
	gchar *account_name = NULL;
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (MODEST_IS_ACCOUNT_VIEW (self), NULL);
	
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
	if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 
				    MODEST_ACCOUNT_VIEW_NAME_COLUMN, 
				    &account_name, -1);
	}

	return account_name;
}

/* This allows us to pass more than one piece of data to the signal handler,
 * and get a result: */
typedef struct 
{
		ModestAccountView* self;
		const gchar *account_name;
} ForEachData;

static gboolean
on_model_foreach_select_account(GtkTreeModel *model, 
	GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	ForEachData *state = (ForEachData*)(user_data);
	
	/* Select the item if it has the matching account name: */
	gchar *this_account_name = NULL;
	gtk_tree_model_get (model, iter, 
		MODEST_ACCOUNT_VIEW_NAME_COLUMN, &this_account_name, 
		-1); 
	if(this_account_name && state->account_name 
		&& (strcmp (this_account_name, state->account_name) == 0)) {
		
		GtkTreeSelection *selection = 
			gtk_tree_view_get_selection (GTK_TREE_VIEW (state->self));
		gtk_tree_selection_select_iter (selection, iter);
		
		return TRUE; /* Stop walking the tree. */
	}
	
	return FALSE; /* Keep walking the tree. */
}

static void 
modest_account_view_select_account (ModestAccountView *account_view, 
				    const gchar* account_name)
{	
	/* Create a state instance so we can send two items of data to the signal handler: */
	ForEachData *state = g_new0 (ForEachData, 1);
	state->self = account_view;
	state->account_name = account_name;
	
	GtkTreeModel *model = gtk_tree_view_get_model (
		GTK_TREE_VIEW (account_view));
	gtk_tree_model_foreach (model, 
		on_model_foreach_select_account, state);
		
	g_free (state);
}

static void
on_default_account_changed (ModestAccountMgr *mgr,
			    gpointer user_data)
{
	GtkTreeIter iter;
	gchar *default_account_name;
	GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (user_data));

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	default_account_name = modest_account_mgr_get_default_account (mgr);

	do {
		gboolean is_default;
		gchar *name;

		gtk_tree_model_get (model, &iter, 
				    MODEST_ACCOUNT_VIEW_NAME_COLUMN, &name,
				    MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN, &is_default, 
				    -1);

		/* Update the default account column */
		if ((default_account_name != NULL) && (!strcmp (name, default_account_name)))
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN, TRUE, -1);
		else
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    MODEST_ACCOUNT_VIEW_IS_DEFAULT_COLUMN, FALSE, -1);

		g_free (name);

	} while (gtk_tree_model_iter_next(model, &iter));

	/* Free and force a redraw */
	g_free (default_account_name);
	gtk_widget_queue_draw (GTK_WIDGET (user_data));
}

static void 
on_display_name_changed (ModestAccountMgr *mgr, 
			 const gchar *account,
			 gpointer user_data)
{
	/* Update the view */
	update_account_view (mgr, MODEST_ACCOUNT_VIEW (user_data));
}
