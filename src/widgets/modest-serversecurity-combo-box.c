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


#include "modest-serversecurity-combo-box.h"
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

G_DEFINE_TYPE (ModestServersecurityComboBox, modest_serversecurity_combo_box, GTK_TYPE_COMBO_BOX);

#define SERVERSECURITY_COMBO_BOX_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), MODEST_TYPE_SERVERSECURITY_COMBO_BOX, ModestServersecurityComboBoxPrivate))

typedef struct _ModestServersecurityComboBoxPrivate ModestServersecurityComboBoxPrivate;

struct _ModestServersecurityComboBoxPrivate
{
	GtkTreeModel *model;
	ModestProtocol protocol;
};

static void
modest_serversecurity_combo_box_get_property (GObject *object, guint property_id,
															GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
modest_serversecurity_combo_box_set_property (GObject *object, guint property_id,
															const GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
modest_serversecurity_combo_box_dispose (GObject *object)
{
	if (G_OBJECT_CLASS (modest_serversecurity_combo_box_parent_class)->dispose)
		G_OBJECT_CLASS (modest_serversecurity_combo_box_parent_class)->dispose (object);
}

static void
modest_serversecurity_combo_box_finalize (GObject *object)
{
	ModestServersecurityComboBoxPrivate *priv = SERVERSECURITY_COMBO_BOX_GET_PRIVATE (object);

	g_object_unref (G_OBJECT (priv->model));

	G_OBJECT_CLASS (modest_serversecurity_combo_box_parent_class)->finalize (object);
}

static void
modest_serversecurity_combo_box_class_init (ModestServersecurityComboBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ModestServersecurityComboBoxPrivate));

	object_class->get_property = modest_serversecurity_combo_box_get_property;
	object_class->set_property = modest_serversecurity_combo_box_set_property;
	object_class->dispose = modest_serversecurity_combo_box_dispose;
	object_class->finalize = modest_serversecurity_combo_box_finalize;
}

enum MODEL_COLS {
	MODEL_COL_NAME = 0, /* a string */
	MODEL_COL_ID = 1 /* an int. */
};

static void
modest_serversecurity_combo_box_init (ModestServersecurityComboBox *self)
{
	ModestServersecurityComboBoxPrivate *priv = SERVERSECURITY_COMBO_BOX_GET_PRIVATE (self);

	/* Create a tree model for the combo box,
	 * with a string for the name, and an ID for the serversecurity.
	 * This must match our MODEL_COLS enum constants.
	 */
	priv->model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT));

	/* Setup the combo box: */
	GtkComboBox *combobox = GTK_COMBO_BOX (self);
	gtk_combo_box_set_model (combobox, priv->model);

	/* Serversecurity column:
	 * The ID model column in not shown in the view. */
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer, 
	"text", MODEL_COL_NAME, NULL);
	
	/* The application must call modest_serversecurity_combo_box_fill().
	 */
}

ModestServersecurityComboBox*
modest_serversecurity_combo_box_new (void)
{
	return g_object_new (MODEST_TYPE_SERVERSECURITY_COMBO_BOX, NULL);
}

/* Fill the combo box with appropriate choices.
 * #combobox: The combo box.
 * @protocol: IMAP or POP.
 */
void modest_serversecurity_combo_box_fill (ModestServersecurityComboBox *combobox, ModestProtocol protocol)
{	
	ModestServersecurityComboBoxPrivate *priv = SERVERSECURITY_COMBO_BOX_GET_PRIVATE (combobox);
	priv->protocol = protocol; /* Remembered for later. */
	
	/* Remove any existing rows: */
	GtkListStore *liststore = GTK_LIST_STORE (priv->model);
	gtk_list_store_clear (liststore);
	
	GtkTreeIter iter;
	gtk_list_store_append (liststore, &iter);
	gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_CONNECTION_NORMAL, MODEL_COL_NAME, _("mcen_fi_advsetup_other_security_none"), -1);
	
	/* Select the None item: */
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
	
	gtk_list_store_append (liststore, &iter);
	gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_CONNECTION_TLS, MODEL_COL_NAME, _("mcen_fi_advsetup_other_security_normal"), -1);
	
	/* Add security choices with protocol-specific names, as in the UI spec:
	 * (Note: Changing the title seems pointless. murrayc) */
	if(protocol == MODEST_PROTOCOL_STORE_POP) {
		gtk_list_store_append (liststore, &iter);
		gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_CONNECTION_SSL, MODEL_COL_NAME, _("mcen_fi_advsetup_other_security_securepop3s"), -1);
	} else if(protocol == MODEST_PROTOCOL_STORE_IMAP) {
		gtk_list_store_append (liststore, &iter);
		gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_CONNECTION_SSL, MODEL_COL_NAME, _("mcen_fi_advsetup_other_security_secureimap4"), -1);
	} else if(protocol == MODEST_PROTOCOL_TRANSPORT_SMTP) {
		gtk_list_store_append (liststore, &iter);
		gtk_list_store_set (liststore, &iter, MODEL_COL_ID, (gint)MODEST_PROTOCOL_CONNECTION_SSL, MODEL_COL_NAME, _("mcen_fi_advsetup_other_security_ssl"), -1);
	}
}

static gint get_port_for_security (ModestProtocol protocol, ModestConnectionProtocol security)
{
	/* See the UI spec, section Email Wizards, Incoming Details [MSG-WIZ001]: */
	gint result = 0;

	/* Get the default port number for this protocol with this security: */
	if(protocol == MODEST_PROTOCOL_STORE_POP) {
		if ((security ==  MODEST_PROTOCOL_CONNECTION_NORMAL) || (security ==  MODEST_PROTOCOL_CONNECTION_TLS))
			result = 110;
		else if (security ==  MODEST_PROTOCOL_CONNECTION_SSL)
			result = 995;
	} else if (protocol == MODEST_PROTOCOL_STORE_IMAP) {
		if ((security ==  MODEST_PROTOCOL_CONNECTION_NORMAL) || (security ==  MODEST_PROTOCOL_CONNECTION_TLS))
			result = 143;
		else if (security ==  MODEST_PROTOCOL_CONNECTION_SSL)
			result = 993;
	} else if (protocol == MODEST_PROTOCOL_TRANSPORT_SMTP) {
		if ((security ==  MODEST_PROTOCOL_CONNECTION_NORMAL) || (security ==  MODEST_PROTOCOL_CONNECTION_TLS))
			result = 25;
		else if (security ==  MODEST_PROTOCOL_CONNECTION_SSL)
			result = 465;
	}

	return result;
}

/**
 * Returns the selected serversecurity, 
 * or MODEST_PROTOCOL_CONNECTION_NORMAL if no serversecurity was selected.
 */
ModestConnectionProtocol
modest_serversecurity_combo_box_get_active_serversecurity (ModestServersecurityComboBox *combobox)
{
	GtkTreeIter active;
	const gboolean found = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &active);
	if (found) {
		ModestServersecurityComboBoxPrivate *priv = SERVERSECURITY_COMBO_BOX_GET_PRIVATE (combobox);

		ModestConnectionProtocol serversecurity = MODEST_PROTOCOL_CONNECTION_NORMAL;
		gtk_tree_model_get (priv->model, &active, MODEL_COL_ID, &serversecurity, -1);
		return serversecurity;	
	}

	return MODEST_PROTOCOL_CONNECTION_NORMAL; /* Failed. */
}

/**
 * Returns the default port to be used for the selected serversecurity, 
 * or 0 if no serversecurity was selected.
 */
gint
modest_serversecurity_combo_box_get_active_serversecurity_port (ModestServersecurityComboBox *combobox)
{
	ModestServersecurityComboBoxPrivate *priv = SERVERSECURITY_COMBO_BOX_GET_PRIVATE (combobox);
	
	const ModestConnectionProtocol security = modest_serversecurity_combo_box_get_active_serversecurity 
		(combobox);
	return get_port_for_security (priv->protocol, security);
}
	
/* This allows us to pass more than one piece of data to the signal handler,
 * and get a result: */
typedef struct 
{
		ModestServersecurityComboBox* self;
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
 * Selects the specified serversecurity, 
 * or MODEST_PROTOCOL_UNKNOWN if no serversecurity was selected.
 */
gboolean
modest_serversecurity_combo_box_set_active_serversecurity (ModestServersecurityComboBox *combobox,
							   ModestConnectionProtocol serversecurity)
{
	ModestServersecurityComboBoxPrivate *priv = SERVERSECURITY_COMBO_BOX_GET_PRIVATE (combobox);
	
	/* Create a state instance so we can send two items of data to the signal handler: */
	ForEachData *state = g_new0 (ForEachData, 1);
	state->self = combobox;
	state->id = serversecurity;
	state->found = FALSE;
	
	/* Look at each item, and select the one with the correct ID: */
	gtk_tree_model_foreach (priv->model, &on_model_foreach_select_id, state);

	const gboolean result = state->found;
	
	/* Free the state instance: */
	g_free(state);
	
	return result;
}

