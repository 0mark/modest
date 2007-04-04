/* Copyright (c) 2007, Nokia Corporation
 * All rights reserved.
 *
 */

#include "modest-easysetup-servertype-combo-box.h"
#include <gtk/gtkliststore.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h> /* For memcpy() */

/* Include config.h so that _() works: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_DEFINE_TYPE (EasysetupServertypeComboBox, easysetup_servertype_combo_box, GTK_TYPE_COMBO_BOX);

#define SERVERTYPE_COMBO_BOX_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), EASYSETUP_TYPE_SERVERTYPE_COMBO_BOX, EasysetupServertypeComboBoxPrivate))

typedef struct _EasysetupServertypeComboBoxPrivate EasysetupServertypeComboBoxPrivate;

struct _EasysetupServertypeComboBoxPrivate
{
	GtkTreeModel *model;
};

static void
easysetup_servertype_combo_box_get_property (GObject *object, guint property_id,
															GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
easysetup_servertype_combo_box_set_property (GObject *object, guint property_id,
															const GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
easysetup_servertype_combo_box_dispose (GObject *object)
{
	if (G_OBJECT_CLASS (easysetup_servertype_combo_box_parent_class)->dispose)
		G_OBJECT_CLASS (easysetup_servertype_combo_box_parent_class)->dispose (object);
}

static void
easysetup_servertype_combo_box_finalize (GObject *object)
{
	EasysetupServertypeComboBoxPrivate *priv = SERVERTYPE_COMBO_BOX_GET_PRIVATE (object);

	g_object_unref (G_OBJECT (priv->model));

	G_OBJECT_CLASS (easysetup_servertype_combo_box_parent_class)->finalize (object);
}

static void
easysetup_servertype_combo_box_class_init (EasysetupServertypeComboBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EasysetupServertypeComboBoxPrivate));

	object_class->get_property = easysetup_servertype_combo_box_get_property;
	object_class->set_property = easysetup_servertype_combo_box_set_property;
	object_class->dispose = easysetup_servertype_combo_box_dispose;
	object_class->finalize = easysetup_servertype_combo_box_finalize;
}

enum MODEL_COLS {
	MODEL_COL_NAME = 0, /* a string */
	MODEL_COL_ID = 1 /* an int. */
};

static void
easysetup_servertype_combo_box_fill (EasysetupServertypeComboBox *combobox);

static void
easysetup_servertype_combo_box_init (EasysetupServertypeComboBox *self)
{
	EasysetupServertypeComboBoxPrivate *priv = SERVERTYPE_COMBO_BOX_GET_PRIVATE (self);

	/* Create a tree model for the combo box,
	 * with a string for the name, and an ID for the servertype.
	 * This must match our MODEL_COLS enum constants.
	 */
	priv->model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT));

	/* Setup the combo box: */
	GtkComboBox *combobox = GTK_COMBO_BOX (self);
	gtk_combo_box_set_model (combobox, priv->model);

	/* Servertype column:
	 * The ID model column in not shown in the view. */
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer, 
	"text", MODEL_COL_NAME, NULL);
	
	easysetup_servertype_combo_box_fill (self);
}

EasysetupServertypeComboBox*
easysetup_servertype_combo_box_new (void)
{
	return g_object_new (EASYSETUP_TYPE_SERVERTYPE_COMBO_BOX, NULL);
}

void easysetup_servertype_combo_box_fill (EasysetupServertypeComboBox *combobox)
{	
	EasysetupServertypeComboBoxPrivate *priv = SERVERTYPE_COMBO_BOX_GET_PRIVATE (combobox);
	
	/* Remove any existing rows: */
	GtkListStore *liststore = GTK_LIST_STORE (priv->model);
	gtk_list_store_clear (liststore);
	
	GtkTreeIter iter;
	gtk_list_store_append (liststore, &iter);
	gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_STORE_POP, MODEL_COL_NAME, _("mail_fi_emailtype_pop3"), -1);
	
	/* Select the POP item: */
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
	
	gtk_list_store_append (liststore, &iter);
	gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_STORE_IMAP, MODEL_COL_NAME, _("mail_fi_emailtype_imap"), -1);
}

/**
 * Returns the selected servertype, 
 * or MODEST_PROTOCOL_UNKNOWN if no servertype was selected.
 */
ModestProtocol
easysetup_servertype_combo_box_get_active_servertype (EasysetupServertypeComboBox *combobox)
{
	GtkTreeIter active;
	const gboolean found = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &active);
	if (found) {
		EasysetupServertypeComboBoxPrivate *priv = SERVERTYPE_COMBO_BOX_GET_PRIVATE (combobox);

		ModestProtocol servertype = MODEST_PROTOCOL_UNKNOWN;
		gtk_tree_model_get (priv->model, &active, MODEL_COL_ID, &servertype, -1);
		return servertype;	
	}

	return MODEST_PROTOCOL_UNKNOWN; /* Failed. */
}

/* This allows us to pass more than one piece of data to the signal handler,
 * and get a result: */
typedef struct 
{
		EasysetupServertypeComboBox* self;
		gint id;
		gboolean found;
} ForEachData;

static gboolean
on_model_foreach_select_id(GtkTreeModel *model, 
	GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	ForEachData *state = (ForEachData*)(user_data);
	
	/* Select the item if it has the matching ID: */
	guint id = 0;
	gtk_tree_model_get (model, iter, MODEL_COL_ID, &id, -1); 
	if(id == state->id) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (state->self), iter);
		
		state->found = TRUE;
		return TRUE; /* Stop walking the tree. */
	}
	
	return FALSE; /* Keep walking the tree. */
}

/**
 * Selects the specified servertype, 
 * or MODEST_PROTOCOL_UNKNOWN if no servertype was selected.
 */
gboolean
easysetup_servertype_combo_box_set_active_servertype (EasysetupServertypeComboBox *combobox, ModestProtocol servertype)
{
	EasysetupServertypeComboBoxPrivate *priv = SERVERTYPE_COMBO_BOX_GET_PRIVATE (combobox);
	
	/* Create a state instance so we can send two items of data to the signal handler: */
	ForEachData *state = g_new0 (ForEachData, 1);
	state->self = combobox;
	state->id = servertype;
	state->found = FALSE;
	
	/* Look at each item, and select the one with the correct ID: */
	gtk_tree_model_foreach (priv->model, &on_model_foreach_select_id, state);

	const gboolean result = state->found;
	
	/* Free the state instance: */
	g_free(state);
	
	return result;
}

