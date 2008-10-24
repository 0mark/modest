/* Copyright (c) 2008, Nokia Corporation
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

#include "modest-details-dialog.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktable.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <tny-msg.h>
#include <tny-header.h>
#include <tny-header-view.h>
#include <tny-folder-store.h>
#include <modest-tny-folder.h>
#include <modest-tny-account.h>
#include <modest-text-utils.h>
#include "modest-hildon2-details-dialog.h"
#include <hildon/hildon-pannable-area.h>

static void modest_hildon2_details_dialog_create_container_default (ModestDetailsDialog *self);


G_DEFINE_TYPE (ModestHildon2DetailsDialog, 
	       modest_hildon2_details_dialog, 
	       MODEST_TYPE_DETAILS_DIALOG);

#define MODEST_HILDON2_DETAILS_DIALOG_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), MODEST_TYPE_DETAILS_DIALOG, ModestHildon2DetailsDialogPrivate))


typedef struct _ModestHildon2DetailsDialogPrivate ModestHildon2DetailsDialogPrivate;

struct _ModestHildon2DetailsDialogPrivate
{
	GtkWidget *props_table;
};

static void
modest_hildon2_details_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (modest_hildon2_details_dialog_parent_class)->finalize (object);
}

static void
modest_hildon2_details_dialog_class_init (ModestHildon2DetailsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ModestHildon2DetailsDialogPrivate));
	object_class->finalize = modest_hildon2_details_dialog_finalize;

	MODEST_DETAILS_DIALOG_CLASS (klass)->create_container_func = 
		modest_hildon2_details_dialog_create_container_default;
}

static void
modest_hildon2_details_dialog_init (ModestHildon2DetailsDialog *self)
{
}

GtkWidget*
modest_hildon2_details_dialog_new_with_header (GtkWindow *parent, 
					       TnyHeader *header)
{
	ModestDetailsDialog *dialog;

	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	g_return_val_if_fail (TNY_IS_HEADER (header), NULL);

	dialog = (ModestDetailsDialog *) (g_object_new (MODEST_TYPE_HILDON2_DETAILS_DIALOG, 
							"transient-for", parent, 
							NULL));

	MODEST_DETAILS_DIALOG_GET_CLASS (dialog)->set_header_func (dialog, header);

	return GTK_WIDGET (dialog);
}

GtkWidget* 
modest_hildon2_details_dialog_new_with_folder  (GtkWindow *parent, 
						TnyFolder *folder)
{
	ModestDetailsDialog *dialog;

	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
	g_return_val_if_fail (TNY_IS_FOLDER (folder), NULL);

	dialog = (ModestDetailsDialog *) (g_object_new (MODEST_TYPE_HILDON2_DETAILS_DIALOG, 
							"transient-for", parent, 
							NULL));

	MODEST_DETAILS_DIALOG_GET_CLASS (dialog)->set_folder_func (dialog, folder);

	return GTK_WIDGET (dialog);
}


static void
modest_hildon2_details_dialog_create_container_default (ModestDetailsDialog *self)
{
	ModestHildon2DetailsDialogPrivate *priv;
	GtkWidget *pannable;

	priv = MODEST_HILDON2_DETAILS_DIALOG_GET_PRIVATE (self);

	gtk_window_set_default_size (GTK_WINDOW (self), 400, 220);

	priv->props_table = gtk_table_new (0, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (priv->props_table), 12);
	gtk_table_set_row_spacings (GTK_TABLE (priv->props_table), 1);

	pannable = g_object_new (HILDON_TYPE_PANNABLE_AREA, "initial-hint", TRUE, NULL);
	hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable), 
						GTK_WIDGET (priv->props_table));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (self)->vbox), pannable);

	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
}
