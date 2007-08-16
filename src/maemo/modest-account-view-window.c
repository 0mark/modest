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
#include <gtk/gtk.h>

#include <widgets/modest-account-view-window.h>
#include <widgets/modest-account-view.h>

#include <modest-runtime.h>
#include <modest-account-mgr-helpers.h>
#include <string.h>
#include "modest-tny-platform-factory.h"
#include "maemo/easysetup/modest-easysetup-wizard.h"
#include "maemo/modest-account-settings-dialog.h"
#include <maemo/modest-maemo-utils.h>
#include "widgets/modest-ui-constants.h"

/* 'private'/'protected' functions */
static void                            modest_account_view_window_class_init   (ModestAccountViewWindowClass *klass);
static void                            modest_account_view_window_init         (ModestAccountViewWindow *obj);
static void                            modest_account_view_window_finalize     (GObject *obj);

/* list my signals */
enum {
	/* MY_SIGNAL_1, */
	/* MY_SIGNAL_2, */
	LAST_SIGNAL
};

typedef struct _ModestAccountViewWindowPrivate ModestAccountViewWindowPrivate;
struct _ModestAccountViewWindowPrivate {
	GtkWidget           *new_button;
	GtkWidget           *edit_button;
	GtkWidget           *delete_button;
	GtkWidget	    *close_button;
	ModestAccountView   *account_view;
	
	/* We remember this so that we only open one at a time: */
	ModestEasysetupWizardDialog *wizard;
};
#define MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(o)      (G_TYPE_INSTANCE_GET_PRIVATE((o), \
                                                        MODEST_TYPE_ACCOUNT_VIEW_WINDOW, \
                                                        ModestAccountViewWindowPrivate))
/* globals */
static GtkDialogClass *parent_class = NULL;

/* uncomment the following if you have defined any signals */
/* static guint signals[LAST_SIGNAL] = {0}; */

GType
modest_account_view_window_get_type (void)
{
	static GType my_type = 0;
	if (!my_type) {
		static const GTypeInfo my_info = {
			sizeof(ModestAccountViewWindowClass),
			NULL,		/* base init */
			NULL,		/* base finalize */
			(GClassInitFunc) modest_account_view_window_class_init,
			NULL,		/* class finalize */
			NULL,		/* class data */
			sizeof(ModestAccountViewWindow),
			1,		/* n_preallocs */
			(GInstanceInitFunc) modest_account_view_window_init,
			NULL
		};
		my_type = g_type_register_static (GTK_TYPE_DIALOG,
		                                  "ModestAccountViewWindow",
		                                  &my_info, 0);
	}
	return my_type;
}

static void
modest_account_view_window_class_init (ModestAccountViewWindowClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*) klass;

	parent_class            = g_type_class_peek_parent (klass);
	gobject_class->finalize = modest_account_view_window_finalize;

	g_type_class_add_private (gobject_class, sizeof(ModestAccountViewWindowPrivate));

	/* signal definitions go here, e.g.: */
/* 	signals[MY_SIGNAL_1] = */
/* 		g_signal_new ("my_signal_1",....); */
/* 	signals[MY_SIGNAL_2] = */
/* 		g_signal_new ("my_signal_2",....); */
/* 	etc. */
}

static void
modest_account_view_window_finalize (GObject *obj)
{
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(obj);
	
	if (priv->wizard) {
		gtk_widget_destroy (GTK_WIDGET (priv->wizard));
		priv->wizard = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize (obj);
}


static void
on_selection_changed (GtkTreeSelection *sel, ModestAccountViewWindow *self)
{
	ModestAccountViewWindowPrivate *priv;
	GtkTreeModel                   *model;
	GtkTreeIter                     iter;
	gboolean                        has_selection;
	gchar                          *account_name;
	gchar                          *default_account_name;
	
	priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);

	has_selection =
		gtk_tree_selection_get_selected (sel, &model, &iter);

	gtk_widget_set_sensitive (priv->edit_button, has_selection);
	gtk_widget_set_sensitive (priv->delete_button, has_selection);	

	account_name = modest_account_view_get_selected_account (priv->account_view);
	default_account_name = modest_account_mgr_get_default_account(
		modest_runtime_get_account_mgr());

	g_free (account_name);
	g_free (default_account_name);
}

/** Check whether any connections are active, and cancel them if 
 * the user wishes.
 * Returns TRUE is there was no problem, 
 * or if an operation was cancelled so we can continue.
 * Returns FALSE if the user chose to cancel his request instead.
 */
static gboolean
check_for_active_acount (ModestAccountViewWindow *self, const gchar* account_name)
{
	/* Check whether any connections are active, and cancel them if 
	 * the user wishes.
	 */
	ModestAccountMgr* mgr = modest_runtime_get_account_mgr ();
	ModestMailOperationQueue* queue = modest_runtime_get_mail_operation_queue();
	if (modest_account_mgr_account_is_busy(mgr, account_name)) {
		GtkWidget *note = hildon_note_new_confirmation (GTK_WINDOW (self), 
			_("emev_nc_disconnect_account"));
		const int response = gtk_dialog_run (GTK_DIALOG(note));
		gtk_widget_destroy (note);
		if (response == GTK_RESPONSE_OK) {
			/* FIXME: We should only cancel those of this account */
			modest_mail_operation_queue_cancel_all(queue);
			return TRUE;
		}
		else
			return FALSE;
	}
	
	return TRUE;
}

static void
on_delete_button_clicked (GtkWidget *button, ModestAccountViewWindow *self)
{
	ModestAccountViewWindowPrivate *priv;
	ModestAccountMgr *account_mgr;
	
	
	priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);

	account_mgr = modest_runtime_get_account_mgr();	
	gchar *account_name = modest_account_view_get_selected_account (priv->account_view);
	if(!account_name)
		return;
		
	if (account_name) {
		gchar *account_title = modest_account_mgr_get_display_name(account_mgr, account_name);
		
		if (check_for_active_acount (self, account_name)) {
			/* The warning text depends on the account type: */
			gchar *txt = NULL;	
			if (modest_account_mgr_get_store_protocol (account_mgr, account_name) 
				== MODEST_PROTOCOL_STORE_POP) {
				txt = g_strdup_printf (_("emev_nc_delete_mailbox"), 
					account_title);
			} else {
				txt = g_strdup_printf (_("emev_nc_delete_mailboximap"), 
					account_title);
			}
			
			GtkDialog *dialog = GTK_DIALOG (hildon_note_new_confirmation (GTK_WINDOW (self), 
				txt));
			gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (self));
			g_free (txt);
			txt = NULL;
	
			if (gtk_dialog_run (dialog) == GTK_RESPONSE_OK) {
				/* Remove account. If it succeeds then it also removes
				   the account from the ModestAccountView: */
				  
				gboolean is_default = FALSE;
				gchar *default_account_name = modest_account_mgr_get_default_account (account_mgr);
				if (default_account_name && (strcmp (default_account_name, account_name) == 0))
					is_default = TRUE;
				g_free (default_account_name);
					
				gboolean removed = modest_account_mgr_remove_account (account_mgr, account_name);
				if (!removed) {
					g_warning ("%s: modest_account_mgr_remove_account() failed.\n", __FUNCTION__);
				}
			}
			gtk_widget_destroy (GTK_WIDGET (dialog));
			g_free (account_title);
		}
		
		g_free (account_name);
	}
}

static void
on_edit_button_clicked (GtkWidget *button, ModestAccountViewWindow *self)
{
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);
	
	gchar* account_name = modest_account_view_get_selected_account (priv->account_view);
	if (!account_name)
		return;
		
	/* Check whether any connections are active, and cancel them if 
	 * the user wishes.
	 */
	if (check_for_active_acount (self, account_name)) {
		
		/* Show the Account Settings window: */
		ModestAccountSettingsDialog *dialog = modest_account_settings_dialog_new ();
		modest_account_settings_dialog_set_account_name (dialog, account_name);
		
		modest_maemo_show_dialog_and_forget (GTK_WINDOW (self), GTK_DIALOG (dialog));
	}
	
	g_free (account_name);
}

static void
on_wizard_response (GtkDialog *dialog, gint response, gpointer user_data)
{
	ModestAccountViewWindow *self = MODEST_ACCOUNT_VIEW_WINDOW (user_data);
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);
	
	/* The response has already been handled by the wizard dialog itself,
	 * creating the new account.
	 */
	 
	/* Destroy the dialog: */
	if (priv->wizard) {
		gtk_widget_destroy (GTK_WIDGET (priv->wizard));
		priv->wizard = NULL;
	}
}

static void
on_new_button_clicked (GtkWidget *button, ModestAccountViewWindow *self)
{
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);
	
	/* Show the easy-setup wizard: */
	
	if (priv->wizard) {
		/* Just show the existing window: */
		gtk_window_present (GTK_WINDOW (priv->wizard));
	} else {
		/* Create and show the dialog: */
		priv->wizard = modest_easysetup_wizard_dialog_new ();
		
		gtk_window_set_transient_for (GTK_WINDOW (priv->wizard), GTK_WINDOW (self));
		
		/* Destroy the dialog when it is closed: */
		g_signal_connect (G_OBJECT (priv->wizard), "response", G_CALLBACK (on_wizard_response), self);
		gtk_widget_show (GTK_WIDGET (priv->wizard));
	}
}


static void
on_close_button_clicked (GtkWidget *button, gpointer user_data)
{		
	ModestAccountViewWindow *self = MODEST_ACCOUNT_VIEW_WINDOW (user_data);

	gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
}



static GtkWidget*
button_box_new (ModestAccountViewWindow *self)
{
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);
	
	GtkWidget *button_box = gtk_hbutton_box_new ();
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (button_box), 6);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), 
				   GTK_BUTTONBOX_START);
	
	priv->new_button     = gtk_button_new_from_stock(_("mcen_bd_new"));
	priv->edit_button = gtk_button_new_with_label(_("mcen_bd_edit"));
	priv->delete_button  = gtk_button_new_from_stock(_("mcen_bd_delete"));
	priv->close_button    = gtk_button_new_from_stock(_("mcen_bd_close"));
	
	g_signal_connect (G_OBJECT(priv->new_button), "clicked",
			  G_CALLBACK(on_new_button_clicked),
			  self);
	g_signal_connect (G_OBJECT(priv->delete_button), "clicked",
			  G_CALLBACK(on_delete_button_clicked),
			  self);
	g_signal_connect (G_OBJECT(priv->edit_button), "clicked",
			  G_CALLBACK(on_edit_button_clicked),
			  self);
	g_signal_connect (G_OBJECT(priv->close_button), "clicked",
			  G_CALLBACK(on_close_button_clicked),
			  self);
	
	gtk_box_pack_start (GTK_BOX(button_box), priv->new_button, FALSE, FALSE,2);
	gtk_box_pack_start (GTK_BOX(button_box), priv->edit_button, FALSE, FALSE,2);
	gtk_box_pack_start (GTK_BOX(button_box), priv->delete_button, FALSE, FALSE,2);
	gtk_box_pack_start (GTK_BOX(button_box), priv->close_button, FALSE, FALSE,2);

	gtk_widget_set_sensitive (priv->edit_button, FALSE);
	gtk_widget_set_sensitive (priv->delete_button, FALSE);	

	gtk_widget_show_all (button_box);
	return button_box;
}


static GtkWidget*
window_vbox_new (ModestAccountViewWindow *self)
{
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(self);

	GtkWidget *main_vbox     = gtk_vbox_new (FALSE, 6);
	GtkWidget *main_hbox     = gtk_hbox_new (FALSE, 6);
	
	priv->account_view = modest_account_view_new (modest_runtime_get_account_mgr());
	gtk_widget_set_size_request (GTK_WIDGET(priv->account_view), 300, 400);
	gtk_widget_show (GTK_WIDGET (priv->account_view));

	GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW(priv->account_view));
	g_signal_connect (G_OBJECT(sel), "changed",  G_CALLBACK(on_selection_changed),
			  self);
			  
	GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), MODEST_MARGIN_DEFAULT);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, 
		GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (priv->account_view));
	gtk_widget_show (GTK_WIDGET (scrolled_window));
	
	gtk_box_pack_start (GTK_BOX(main_hbox), GTK_WIDGET(scrolled_window), TRUE, TRUE, 2);
	
	gtk_box_pack_start (GTK_BOX(main_vbox), main_hbox, TRUE, TRUE, 2);
	gtk_widget_show (GTK_WIDGET (main_hbox));
	gtk_widget_show (GTK_WIDGET (main_vbox));

	return main_vbox;
}


static void
modest_account_view_window_init (ModestAccountViewWindow *obj)
{
/*
	ModestAccountViewWindowPrivate *priv = MODEST_ACCOUNT_VIEW_WINDOW_GET_PRIVATE(obj);
*/		
	gtk_box_pack_start (GTK_BOX((GTK_DIALOG (obj)->vbox)), GTK_WIDGET (window_vbox_new (obj)), 
		TRUE, TRUE, 2);
	
	gtk_box_pack_start (GTK_BOX((GTK_DIALOG (obj)->action_area)), GTK_WIDGET (button_box_new (obj)), 
		TRUE, TRUE, 2);

	gtk_window_set_title (GTK_WINDOW (obj), _("mcen_ti_emailsetup_accounts"));
}


GtkWidget*
modest_account_view_window_new (void)
{
	GObject *obj = g_object_new(MODEST_TYPE_ACCOUNT_VIEW_WINDOW, NULL);
	
	return GTK_WIDGET(obj);
}
