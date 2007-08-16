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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <string.h>
#include <modest-runtime.h>
#include <modest-tny-folder.h>
#include <modest-tny-msg.h>
#include <modest-tny-account.h>
#include <modest-address-book.h>
#include "modest-error.h"
#include "modest-ui-actions.h"

#include "modest-tny-platform-factory.h"
#include "modest-platform.h"
#include <tny-mime-part.h>
#include <tny-camel-folder.h>
#include <tny-camel-imap-folder.h>
#include <tny-camel-pop-folder.h>

#ifdef MODEST_PLATFORM_MAEMO
#include "maemo/modest-osso-state-saving.h"
#include "maemo/modest-maemo-utils.h"
#include "maemo/modest-hildon-includes.h"
#endif /* MODEST_PLATFORM_MAEMO */

#include "widgets/modest-ui-constants.h"
#include <widgets/modest-main-window.h>
#include <widgets/modest-msg-view-window.h>
#include <widgets/modest-account-view-window.h>
#include <widgets/modest-details-dialog.h>
#include <widgets/modest-attachments-view.h>
#include "widgets/modest-folder-view.h"
#include "widgets/modest-global-settings-dialog.h"
#include "modest-connection-specific-smtp-window.h"
#include "modest-account-mgr-helpers.h"
#include "modest-mail-operation.h"
#include "modest-text-utils.h"

#ifdef MODEST_HAVE_EASYSETUP
#include "easysetup/modest-easysetup-wizard.h"
#endif /* MODEST_HAVE_EASYSETUP */

#include <modest-widget-memory.h>
#include <tny-error.h>
#include <tny-simple-list.h>
#include <tny-msg-view.h>
#include <tny-device.h>
#include <tny-merge-folder.h>

#include <gtkhtml/gtkhtml.h>

typedef struct _GetMsgAsyncHelper {	
	ModestWindow *window;
	ModestMailOperation *mail_op;
	TnyIterator *iter;
	guint num_ops;
	GFunc func;	
	gpointer user_data;
} GetMsgAsyncHelper;

typedef enum _ReplyForwardAction {
	ACTION_REPLY,
	ACTION_REPLY_TO_ALL,
	ACTION_FORWARD
} ReplyForwardAction;

typedef struct _ReplyForwardHelper {
	guint reply_forward_type;
	ReplyForwardAction action;
	gchar *account_name;
	GtkWidget *parent_window;
} ReplyForwardHelper;

typedef struct _PasteAsAttachmentHelper {
	ModestMsgEditWindow *window;
	GtkWidget *banner;
} PasteAsAttachmentHelper;


/*
 * The do_headers_action uses this kind of functions to perform some
 * action to each member of a list of headers
 */
typedef void (*HeadersFunc) (TnyHeader *header, ModestWindow *win, gpointer user_data);

static void     do_headers_action     (ModestWindow *win, 
				       HeadersFunc func,
				       gpointer user_data);

static void     open_msg_cb            (ModestMailOperation *mail_op, 
					TnyHeader *header, 
					TnyMsg *msg,
					gpointer user_data);

static void     reply_forward_cb       (ModestMailOperation *mail_op, 
					TnyHeader *header, 
					TnyMsg *msg,
					gpointer user_data);

static void     reply_forward          (ReplyForwardAction action, ModestWindow *win);

static void     folder_refreshed_cb    (ModestMailOperation *mail_op, 
					TnyFolder *folder, 
					gpointer user_data);

static void     _on_send_receive_progress_changed (ModestMailOperation  *mail_op, 
						   ModestMailOperationState *state,
						   gpointer user_data);

static gboolean
download_uncached_messages (TnyList *header_list, GtkWindow *win,
			    gboolean reply_fwd);


static gint
msgs_move_to_confirmation (GtkWindow *win,
			   TnyFolder *dest_folder,
			   gboolean delete,
			   TnyList *headers);


/* Show the account creation wizard dialog.
 * returns: TRUE if an account was created. FALSE if the user cancelled.
 */
gboolean
modest_run_account_setup_wizard (ModestWindow *win)
{
	gboolean result = FALSE;
	ModestEasysetupWizardDialog *wizard;
	
	if (!win)
		win = modest_window_mgr_get_main_window (modest_runtime_get_window_mgr ());
		
	g_return_val_if_fail (MODEST_IS_WINDOW(win), FALSE);
	
	wizard = modest_easysetup_wizard_dialog_new ();
	gtk_window_set_transient_for (GTK_WINDOW (wizard), GTK_WINDOW (win));
	
	/* Don't make this a modal window, because secondary windows will then 
	 * be unusable, freezing the UI: */
	/* gtk_window_set_modal (GTK_WINDOW (wizard), TRUE); */
	
	gint dialog_response = gtk_dialog_run (GTK_DIALOG (wizard));
	if (dialog_response == GTK_RESPONSE_CANCEL)
		result = FALSE;
	else {
		/* Check whether an account was created: */
		result = modest_account_mgr_has_accounts(modest_runtime_get_account_mgr(), TRUE);
	}
	
	gtk_widget_destroy (GTK_WIDGET (wizard));

	return result;
}


void   
modest_ui_actions_on_about (GtkAction *action, ModestWindow *win)
{
	GtkWidget *about;
	const gchar *authors[] = {
		"Dirk-Jan C. Binnema <dirk-jan.binnema@nokia.com>",
		NULL
	};
	about = gtk_about_dialog_new ();
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG(about), PACKAGE_NAME);
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG(about),PACKAGE_VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG(about),
					_("Copyright (c) 2006, Nokia Corporation\n"
					  "All rights reserved."));
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG(about),
				       _("a modest e-mail client\n\n"
					 "design and implementation: Dirk-Jan C. Binnema\n"
					 "contributions from the fine people at KC and Ig\n"
					 "uses the tinymail email framework written by Philip van Hoof"));
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG(about), authors);
	gtk_about_dialog_set_website (GTK_ABOUT_DIALOG(about), "http://modest.garage.maemo.org");
	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (win));
	gtk_window_set_modal (GTK_WINDOW (about), TRUE);
	
	gtk_dialog_run (GTK_DIALOG (about));
	gtk_widget_destroy(about);
}

/*
 * Gets the list of currently selected messages. If the win is the
 * main window, then it returns a newly allocated list of the headers
 * selected in the header view. If win is the msg view window, then
 * the value returned is a list with just a single header.
 *
 * The caller of this funcion must free the list.
 */
static TnyList *
get_selected_headers (ModestWindow *win)
{
	if (MODEST_IS_MAIN_WINDOW(win)) {
		GtkWidget *header_view;		
		
		header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW(win),
								   MODEST_WIDGET_TYPE_HEADER_VIEW);
		return modest_header_view_get_selected_headers (MODEST_HEADER_VIEW(header_view));
		
	} else if (MODEST_IS_MSG_VIEW_WINDOW (win)) {
		/* for MsgViewWindows, we simply return a list with one element */
		TnyHeader *header;
		TnyList *list = NULL;
		
		header = modest_msg_view_window_get_header (MODEST_MSG_VIEW_WINDOW (win));
		if (header != NULL) {
			list = tny_simple_list_new ();
			tny_list_prepend (list, G_OBJECT(header));
			g_object_unref (G_OBJECT(header));
		}

		return list;

	} else
		return NULL;
}

static void
headers_action_mark_as_read (TnyHeader *header,
			     ModestWindow *win,
			     gpointer user_data)
{
	TnyHeaderFlags flags;

	g_return_if_fail (TNY_IS_HEADER(header));

	flags = tny_header_get_flags (header);
	if (flags & TNY_HEADER_FLAG_SEEN) return;
	tny_header_set_flags (header, TNY_HEADER_FLAG_SEEN);
}

static void
headers_action_mark_as_unread (TnyHeader *header,
			       ModestWindow *win,
			       gpointer user_data)
{
	TnyHeaderFlags flags;

	g_return_if_fail (TNY_IS_HEADER(header));

	flags = tny_header_get_flags (header);
	if (flags & TNY_HEADER_FLAG_SEEN)  {
		tny_header_unset_flags (header, TNY_HEADER_FLAG_SEEN);
	}
}

/** A convenience method, because deleting a message is 
 * otherwise complicated, and it's best to change it in one place 
 * when we change it.
 */
void modest_do_message_delete (TnyHeader *header, ModestWindow *win)
{
	ModestMailOperation *mail_op = NULL;
	mail_op = modest_mail_operation_new (MODEST_MAIL_OPERATION_TYPE_DELETE, 
		win ? G_OBJECT(win) : NULL);
	modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (),
					 mail_op);
	
	/* Always delete. TODO: Move to trash still not supported */
	modest_mail_operation_remove_msg (mail_op, header, FALSE);
	g_object_unref (G_OBJECT (mail_op));
}

static void
headers_action_delete (TnyHeader *header,
		       ModestWindow *win,
		       gpointer user_data)
{
	modest_do_message_delete (header, win);
}

/** After deleing a message that is currently visible in a window, 
 * show the next message from the list, or close the window if there are no more messages.
 **/
void modest_ui_actions_refresh_message_window_after_delete (ModestMsgViewWindow* win)
{
	/* Close msg view window or select next */
	if (modest_msg_view_window_last_message_selected (win) &&
		modest_msg_view_window_first_message_selected (win)) {
		modest_ui_actions_on_close_window (NULL, MODEST_WINDOW (win));
	} else {
		if (!modest_msg_view_window_select_next_message (win)) {
			gboolean ret_value;
			g_signal_emit_by_name (G_OBJECT (win), "delete-event", NULL, &ret_value);
		}
	}
}

void
modest_ui_actions_on_delete_message (GtkAction *action, ModestWindow *win)
{
	TnyList *header_list = NULL;
	TnyIterator *iter = NULL;
	TnyHeader *header = NULL;
	gchar *message = NULL;
	gchar *desc = NULL;
	gint response;
	ModestWindowMgr *mgr;
	GtkWidget *header_view = NULL;

	g_return_if_fail (MODEST_IS_WINDOW(win));
	
	/* Check first if the header view has the focus */
	if (MODEST_IS_MAIN_WINDOW (win)) {
		header_view = 
			modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
							     MODEST_WIDGET_TYPE_HEADER_VIEW);
		if (!gtk_widget_is_focus (header_view))
			return;
	}
	
	/* Get the headers, either from the header view (if win is the main window),
	 * or from the message view window: */
	header_list = get_selected_headers (win);
	if (!header_list) return;
			
	/* Check if any of the headers are already opened, or in the process of being opened */
	if (MODEST_IS_MAIN_WINDOW (win)) {
		gboolean found;
		iter = tny_list_create_iterator (header_list);
		found = FALSE;
		mgr = modest_runtime_get_window_mgr ();
		while (!tny_iterator_is_done (iter) && !found) {
			header = TNY_HEADER (tny_iterator_get_current (iter));
			if (header) {
				found =  modest_window_mgr_find_registered_header (mgr, header, NULL);
				g_object_unref (header);
			}

			tny_iterator_next (iter);
		}
		g_object_unref (iter);

		if (found) {
			gchar *num, *msg;

			num = g_strdup_printf ("%d", tny_list_get_length (header_list));
			msg = g_strdup_printf (_("mcen_nc_unable_to_delete_n_messages"), num);

			modest_platform_run_information_dialog (GTK_WINDOW (win), (const gchar *) msg);
			
			g_free (msg);
			g_free (num);
			g_object_unref (header_list);
			return;
		}
	}

	/* Select message */
	if (tny_list_get_length(header_list) == 1) {
		iter = tny_list_create_iterator (header_list);
		header = TNY_HEADER (tny_iterator_get_current (iter));
		if (header) {
			desc = g_strdup_printf ("%s", tny_header_get_subject (header)); 
			g_object_unref (header);
		}

		g_object_unref (iter);
	}
	message = g_strdup_printf(ngettext("emev_nc_delete_message", "emev_nc_delete_messages", 
					   tny_list_get_length(header_list)), desc);

	/* Confirmation dialog */
	printf("DEBUG: %s\n", __FUNCTION__);	
	response = modest_platform_run_confirmation_dialog (GTK_WINDOW (win),
							    message);
	

	if (response == GTK_RESPONSE_OK) {	
		ModestWindow *main_window = NULL;
		ModestWindowMgr *mgr = NULL;
		GtkTreeModel *model = NULL;
		GtkTreeSelection *sel = NULL;
		GList *sel_list = NULL, *tmp = NULL;
		GtkTreeRowReference *row_reference = NULL;
		GtkTreePath *next_path = NULL;
		TnyFolder *folder = NULL;
		GError *err = NULL;

		/* Find last selected row */			
		if (MODEST_IS_MAIN_WINDOW (win)) {
			model = gtk_tree_view_get_model (GTK_TREE_VIEW (header_view));
			sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (header_view));
			sel_list = gtk_tree_selection_get_selected_rows (sel, &model);
			for (tmp=sel_list; tmp; tmp=tmp->next) {
				if (tmp->next == NULL) {
					next_path = gtk_tree_path_copy((GtkTreePath *) tmp->data);
					gtk_tree_path_next (next_path);
					row_reference = gtk_tree_row_reference_new (model, next_path);
					gtk_tree_path_free (next_path);
				}
			}
		}
		
		/* Remove each header. If it's a view window header_view == NULL */
		do_headers_action (win, headers_action_delete, header_view);

		/* refresh the header view (removing marked-as-deleted)*/
 		modest_header_view_refilter (MODEST_HEADER_VIEW(header_view)); 
		
		if (MODEST_IS_MSG_VIEW_WINDOW (win)) {
			modest_ui_actions_refresh_message_window_after_delete (MODEST_MSG_VIEW_WINDOW (win));
			
			/* Get main window */
			mgr = modest_runtime_get_window_mgr ();
			main_window = modest_window_mgr_get_main_window (mgr);
		}
		else {			
			/* Move cursor to next row */
			main_window = win; 

			/* Select next row */
			if (gtk_tree_row_reference_valid (row_reference)) {
				next_path = gtk_tree_row_reference_get_path (row_reference);
				gtk_tree_selection_select_path (sel, next_path);
				gtk_tree_path_free (next_path);
			}
			if (row_reference != NULL)
				gtk_tree_row_reference_free (row_reference);
		}

		/* Get folder from first header and sync it */
		iter = tny_list_create_iterator (header_list);
		header = TNY_HEADER (tny_iterator_get_current (iter));
		folder = tny_header_get_folder (header);
		if (TNY_IS_CAMEL_IMAP_FOLDER (folder))
/* 			tny_folder_sync_async(folder, FALSE, NULL, NULL, NULL); /\* FALSE --> don't expunge *\/ */
			tny_folder_sync (folder, FALSE, &err); /* FALSE --> don't expunge */
		else if (TNY_IS_CAMEL_POP_FOLDER (folder))
/* 			tny_folder_sync_async(folder, FALSE, NULL, NULL, NULL); /\* TRUE --> dont expunge *\/ */
			tny_folder_sync (folder, TRUE, &err); /* TRUE --> expunge */
		else
			/* local folders */
/* 			tny_folder_sync_async(folder, TRUE, NULL, NULL, NULL); /\* TRUE --> expunge *\/ */
			tny_folder_sync (folder, TRUE, &err); /* TRUE --> expunge */

		if (err != NULL) {
			printf ("DEBUG: %s: Error: code=%d, text=%s\n", __FUNCTION__, err->code, err->message);
			g_error_free(err);
		}

		g_object_unref (header);
		g_object_unref (iter);
		g_object_unref (folder);
		
		/* Update toolbar dimming state */
		modest_ui_actions_check_toolbar_dimming_rules (MODEST_WINDOW (main_window));

		/* Free */
		g_list_foreach (sel_list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (sel_list);
	}

	/* Free*/
	g_free(message);
	g_free(desc);
	g_object_unref (header_list);
}




/* delete either message or folder, based on where we are */
void
modest_ui_actions_on_delete_message_or_folder (GtkAction *action, ModestWindow *win)
{
	g_return_if_fail (MODEST_IS_WINDOW(win));
	
	/* Check first if the header view has the focus */
	if (MODEST_IS_MAIN_WINDOW (win)) {
		GtkWidget *w;
		w = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
							 MODEST_WIDGET_TYPE_FOLDER_VIEW);
		if (gtk_widget_is_focus (w)) {
			modest_ui_actions_on_delete_folder (action, MODEST_MAIN_WINDOW(win));
			return;
		}
	}
	modest_ui_actions_on_delete_message (action, win);
}



void
modest_ui_actions_on_quit (GtkAction *action, ModestWindow *win)
{
#ifdef MODEST_PLATFORM_MAEMO
	modest_osso_save_state();
#endif /* MODEST_PLATFORM_MAEMO */

	g_debug ("closing down, clearing %d item(s) from operation queue",
		 modest_mail_operation_queue_num_elements
		 (modest_runtime_get_mail_operation_queue()));

	/* cancel all outstanding operations */
	modest_mail_operation_queue_cancel_all 
		(modest_runtime_get_mail_operation_queue());
	
	g_debug ("queue has been cleared");

	/* note: when modest-tny-account-store is finalized,
	   it will automatically set all network connections
	   to offline */

	gtk_main_quit ();
}

void
modest_ui_actions_on_close_window (GtkAction *action, ModestWindow *win)
{
	gboolean ret_value;

	g_signal_emit_by_name (G_OBJECT (win), "delete-event", NULL, &ret_value);

/* 	if (MODEST_IS_MSG_VIEW_WINDOW (win)) { */
/* 		gtk_widget_destroy (GTK_WIDGET (win)); */
/* 	} else if (MODEST_IS_MSG_EDIT_WINDOW (win)) { */
/* 		gboolean ret_value; */
/* 		g_signal_emit_by_name (G_OBJECT (win), "delete-event", NULL, &ret_value); */
/* 	} else if (MODEST_IS_WINDOW (win)) { */
/* 		gtk_widget_destroy (GTK_WIDGET (win)); */
/* 	} else { */
/* 		g_return_if_reached (); */
/* 	} */
}

void
modest_ui_actions_on_add_to_contacts (GtkAction *action, ModestWindow *win)
{
	GtkClipboard *clipboard = NULL;
	gchar *selection = NULL;

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	selection = gtk_clipboard_wait_for_text (clipboard);

	/* Question: why is the clipboard being used here? 
	 * It doesn't really make a lot of sense. */

	if (selection)
	{
		modest_address_book_add_address (selection);
		g_free (selection);
	}
}

void
modest_ui_actions_on_accounts (GtkAction *action, ModestWindow *win)
{
	/* This is currently only implemented for Maemo */
#ifdef MODEST_PLATFORM_MAEMO /* Defined in config.h */
	if (!modest_account_mgr_has_accounts(modest_runtime_get_account_mgr(), TRUE)) {
		modest_run_account_setup_wizard (win);
		return;
	} else 	{
		/* Show the list of accounts: */
		GtkDialog *account_win = GTK_DIALOG(modest_account_view_window_new ());
		gtk_window_set_transient_for (GTK_WINDOW (account_win), GTK_WINDOW (win));
		
		/* Don't make this a modal window, because secondary windows will then 
		 * be unusable, freezing the UI: */
		/* gtk_window_set_modal (GTK_WINDOW (account_win), TRUE); */
		modest_maemo_show_dialog_and_forget (GTK_WINDOW (win), account_win); 
	}
#else
	GtkWidget *dialog, *label;
	
	/* Create the widgets */
	
	dialog = gtk_dialog_new_with_buttons ("Message",
					      GTK_WINDOW(win),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_NONE,
					      NULL);
	label = gtk_label_new ("Hello World!");
	
	/* Ensure that the dialog box is destroyed when the user responds. */
	
	g_signal_connect_swapped (dialog, "response", 
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);
	
	/* Add the label, and show everything we've added to the dialog. */
	
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
			   label);
	gtk_widget_show_all (dialog);
#endif /* MODEST_PLATFORM_MAEMO */
}

static void
on_smtp_servers_window_hide (GtkWindow* window, gpointer user_data)
{
	ModestWindow *main_window = MODEST_WINDOW (user_data);
	
	/* Save any changes. */
	modest_connection_specific_smtp_window_save_server_accounts (
			MODEST_CONNECTION_SPECIFIC_SMTP_WINDOW (window), 
			modest_window_get_active_account (main_window));
	gtk_widget_destroy (GTK_WIDGET (window));
}



void
modest_ui_actions_on_smtp_servers (GtkAction *action, ModestWindow *win)
{
	/* This is currently only implemented for Maemo,
	 * because it requires an API (libconic) to detect different connection 
	 * possiblities.
	 */
#ifdef MODEST_PLATFORM_MAEMO /* Defined in config.h */
	
	/* Create the window if necessary: */
	const gchar *active_account_name = modest_window_get_active_account (win);
	
	/* TODO: Dim the menu item (not in the UI spec)? or show a warning,
	 * or show the default account?
	 * If we show the default account then the account name should be shown in 
	 * the window when we show it. */
	if (!active_account_name) {
		g_warning ("%s: No account is active.", __FUNCTION__);
		return;
	}
		
	GtkWidget *specific_window = GTK_WIDGET (modest_connection_specific_smtp_window_new ());
	modest_connection_specific_smtp_window_fill_with_connections (
		MODEST_CONNECTION_SPECIFIC_SMTP_WINDOW (specific_window), 
		modest_runtime_get_account_mgr(), 
		active_account_name);

	/* Show the window: */	
	gtk_window_set_transient_for (GTK_WINDOW (specific_window), GTK_WINDOW (win));
	gtk_window_set_modal (GTK_WINDOW (specific_window), TRUE);
    	gtk_widget_show (specific_window);
    
    	/* Save changes when the window is hidden: */
	g_signal_connect (specific_window, "hide", 
		G_CALLBACK (on_smtp_servers_window_hide), win);
#endif /* MODEST_PLATFORM_MAEMO */
}

void
modest_ui_actions_on_new_msg (GtkAction *action, ModestWindow *win)
{
	ModestWindow *msg_win = NULL;
	TnyMsg *msg = NULL;
	TnyFolder *folder = NULL;
	gchar *account_name = NULL;
	gchar *from_str = NULL;
/* 	GError *err = NULL; */
	TnyAccount *account = NULL;
	ModestWindowMgr *mgr;
	gchar *signature = NULL, *blank_and_signature = NULL;

	/* if there are no accounts yet, just show the wizard */
	if (!modest_account_mgr_has_accounts (modest_runtime_get_account_mgr(), TRUE)) {
			const gboolean created = modest_run_account_setup_wizard (win);
			if (!created)
				return;
	}
	
	account_name = modest_account_mgr_get_default_account (modest_runtime_get_account_mgr ());
	if (!account_name)
		account_name = g_strdup (modest_window_get_active_account (win));
	if (!account_name) {
		g_printerr ("modest: no account found\n");
		goto cleanup;
	}
	
	account = modest_tny_account_store_get_server_account (modest_runtime_get_account_store(),
								       account_name,
								       TNY_ACCOUNT_TYPE_STORE);
	if (!account) {
		g_printerr ("modest: failed to get tnyaccount for '%s'\n", account_name);
		goto cleanup;
	}

	from_str = modest_account_mgr_get_from_string (modest_runtime_get_account_mgr(), account_name);
	if (!from_str) {
		g_printerr ("modest: failed get from string for '%s'\n", account_name);
		goto cleanup;
	}

	if (modest_account_mgr_get_bool (modest_runtime_get_account_mgr (), account_name,
					 MODEST_ACCOUNT_USE_SIGNATURE, FALSE)) {
		signature = modest_account_mgr_get_string (modest_runtime_get_account_mgr (), account_name,
							   MODEST_ACCOUNT_SIGNATURE, FALSE);
		blank_and_signature = g_strconcat ("\n", signature, NULL);
		g_free (signature);
	} else {
		blank_and_signature = g_strdup ("");
	}

	msg = modest_tny_msg_new ("", from_str, "", "", "", blank_and_signature, NULL);
	if (!msg) {
		g_printerr ("modest: failed to create new msg\n");
		goto cleanup;
	}
	
	folder = modest_tny_account_get_special_folder (account, TNY_FOLDER_TYPE_DRAFTS);
	if (!folder) {
		g_printerr ("modest: failed to find Drafts folder\n");
		goto cleanup;
	}
	

	/* Create and register edit window */
	/* This is destroyed by TOOD. */
	msg_win = modest_msg_edit_window_new (msg, account_name, FALSE);
	mgr = modest_runtime_get_window_mgr ();
	modest_window_mgr_register_window (mgr, msg_win);

	if (win)
		gtk_window_set_transient_for (GTK_WINDOW (msg_win),
					      GTK_WINDOW (win));	
	gtk_widget_show_all (GTK_WIDGET (msg_win));

cleanup:
	g_free (account_name);
	g_free (from_str);
	g_free (blank_and_signature);
	if (msg_win)
		g_object_unref (msg_win);
	if (account)
		g_object_unref (G_OBJECT(account));
	if (msg)
		g_object_unref (G_OBJECT(msg));
	if (folder)
		g_object_unref (G_OBJECT(folder));
}

gboolean 
modest_ui_actions_msg_retrieval_check (ModestMailOperation *mail_op,
				       TnyHeader *header,
				       TnyMsg *msg)
{
	ModestMailOperationStatus status;

	/* If there is no message or the operation was not successful */
	status = modest_mail_operation_get_status (mail_op);
	if (!msg || status != MODEST_MAIL_OPERATION_STATUS_SUCCESS) {

		/* Remove the header from the preregistered uids */
		modest_window_mgr_unregister_header (modest_runtime_get_window_mgr (),  
						     header);

		return FALSE;
	}

	return TRUE;
}

static void
open_msg_cb (ModestMailOperation *mail_op, 
	     TnyHeader *header, 
	     TnyMsg *msg, 
	     gpointer user_data)
{
	ModestWindowMgr *mgr = NULL;
	ModestWindow *parent_win = NULL;
	ModestWindow *win = NULL;
	TnyFolderType folder_type = TNY_FOLDER_TYPE_UNKNOWN;
	gchar *account = NULL;
	TnyFolder *folder;
	
	/* Do nothing if there was any problem with the mail
	   operation. The error will be shown by the error_handler of
	   the mail operation */
	if (!modest_ui_actions_msg_retrieval_check (mail_op, header, msg)) {
		return;
	}

	parent_win = (ModestWindow *) modest_mail_operation_get_source (mail_op);
	folder = tny_header_get_folder (header);

	/* Mark header as read */
	headers_action_mark_as_read (header, MODEST_WINDOW(parent_win), NULL);

	/* Get account */
	account = g_strdup (modest_window_get_active_account (MODEST_WINDOW (parent_win)));
	if (!account)
		account = modest_account_mgr_get_default_account (modest_runtime_get_account_mgr());
	
	/* Gets folder type (OUTBOX headers will be opened in edit window */
	if (modest_tny_folder_is_local_folder (folder))
		folder_type = modest_tny_folder_get_local_or_mmc_folder_type (folder);

	/* If the header is in the drafts folder then open the editor,
	   else the message view window */
	if ((folder_type == TNY_FOLDER_TYPE_DRAFTS) ||
	    (folder_type == TNY_FOLDER_TYPE_OUTBOX)) {
		/* we cannot edit without a valid account... */
		if (!modest_account_mgr_has_accounts(modest_runtime_get_account_mgr(), TRUE)) {
			const gboolean created = modest_run_account_setup_wizard(parent_win);
			if (!created)
				goto cleanup;
		}
		win = modest_msg_edit_window_new (msg, account, TRUE);
		
	} else {
		gchar *uid = modest_tny_folder_get_header_unique_id (header);
		
		if (MODEST_IS_MAIN_WINDOW (parent_win)) {
			GtkWidget *header_view;
			GtkTreeSelection *sel;
			GList *sel_list = NULL;
			GtkTreeModel *model;
			
			header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW(parent_win),
									   MODEST_WIDGET_TYPE_HEADER_VIEW);

			sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (header_view));
			sel_list = gtk_tree_selection_get_selected_rows (sel, &model);

			if (sel_list != NULL) {
				GtkTreeRowReference *row_reference;

				row_reference = gtk_tree_row_reference_new (model, (GtkTreePath *) sel_list->data);
				g_list_foreach (sel_list, (GFunc) gtk_tree_path_free, NULL);
				g_list_free (sel_list);
				
				win = modest_msg_view_window_new_with_header_model (msg, 
										    account,
										    (const gchar*) uid,
										    model, 
										    row_reference);
				gtk_tree_row_reference_free (row_reference);
			} else {
				win = modest_msg_view_window_new_for_attachment (msg, account, (const gchar*) uid);
			}
		} else {
			win = modest_msg_view_window_new_for_attachment (msg, account, (const gchar*) uid);
		}
		g_free (uid);
	}
	
	/* Register and show new window */
	if (win != NULL) {
		mgr = modest_runtime_get_window_mgr ();
		modest_window_mgr_register_window (mgr, win);
		g_object_unref (win);
		gtk_window_set_transient_for (GTK_WINDOW (win), GTK_WINDOW (parent_win));
		gtk_widget_show_all (GTK_WIDGET(win));
	}

	/* Update toolbar dimming state */
	if (MODEST_IS_MAIN_WINDOW (parent_win)) {
		modest_ui_actions_check_toolbar_dimming_rules (MODEST_WINDOW (parent_win));
	}

cleanup:
	/* Free */
	g_free(account);
	g_object_unref (parent_win);
	g_object_unref (folder);
}

void
modest_ui_actions_get_msgs_full_error_handler (ModestMailOperation *mail_op,
					       gpointer user_data)
{
	const GError *error;
	GObject *win = modest_mail_operation_get_source (mail_op);

	error = modest_mail_operation_get_error (mail_op);
	printf ("DEBUG: %s: Error: code=%d, text=%s\n", __FUNCTION__, error->code, error->message);
 
	if (error->code == MODEST_MAIL_OPERATION_ERROR_MESSAGE_SIZE_LIMIT) {

		modest_platform_run_information_dialog ((win) ? GTK_WINDOW (win) : NULL,
							error->message);
	} else {
		modest_platform_run_information_dialog ((win) ? GTK_WINDOW (win) : NULL,
							_("mail_ni_ui_folder_get_msg_folder_error"));
	}

	if (win)
		g_object_unref (win);
}

/*
 * This function is used by both modest_ui_actions_on_open and
 * modest_ui_actions_on_header_activated. This way we always do the
 * same when trying to open messages.
 */
static void
_modest_ui_actions_open (TnyList *headers, ModestWindow *win)
{
	ModestWindowMgr *mgr = NULL;
	TnyIterator *iter = NULL;
	ModestMailOperation *mail_op = NULL;
	TnyList *not_opened_headers = NULL;
	TnyHeaderFlags flags = 0;
		
	/* Look if we already have a message view for each header. If
	   true, then remove the header from the list of headers to
	   open */
	mgr = modest_runtime_get_window_mgr ();
	iter = tny_list_create_iterator (headers);
	not_opened_headers = tny_simple_list_new ();

	while (!tny_iterator_is_done (iter)) {

		ModestWindow *window = NULL;
		TnyHeader *header = NULL;
		gboolean found = FALSE;
		
		header = TNY_HEADER (tny_iterator_get_current (iter));
		if (header)
			flags = tny_header_get_flags (header);

		window = NULL;
		found = modest_window_mgr_find_registered_header (mgr, header, &window);
		
		/* Do not open again the message and present the
		   window to the user */
		if (found) {
			if (window)
				gtk_window_present (GTK_WINDOW (window));
			else
				/* the header has been registered already, we don't do
				 * anything but wait for the window to come up*/
				g_debug ("header %p already registered, waiting for window", header);
		} else {
			tny_list_append (not_opened_headers, G_OBJECT (header));
		}

		if (header)
			g_object_unref (header);

		tny_iterator_next (iter);
	}
	g_object_unref (iter);
	iter = NULL;
	
	/* If some messages would have to be downloaded, ask the user to 
	 * make a connection. It's generally easier to do this here (in the mainloop) 
	 * than later in a thread:
	 */
	if (tny_list_get_length (not_opened_headers) > 0) {
		TnyIterator *iter;
		gboolean found = FALSE;

		iter = tny_list_create_iterator (not_opened_headers);
		while (!tny_iterator_is_done (iter) && !found) {
			TnyHeader *header = TNY_HEADER (tny_iterator_get_current (iter));
			if (!(tny_header_get_flags (header) & TNY_HEADER_FLAG_CACHED))
				found = TRUE;
			else
				tny_iterator_next (iter);

			g_object_unref (header);
		}
		g_object_unref (iter);

		if (found && !modest_platform_connect_and_wait (GTK_WINDOW (win), NULL)) {
			g_object_unref (not_opened_headers);
			return;			
		}
	}
	
	/* Register the headers before actually creating the windows: */
	TnyIterator *iter_not_opened = tny_list_create_iterator (not_opened_headers);
	while (!tny_iterator_is_done (iter_not_opened)) {
		TnyHeader *header = TNY_HEADER (tny_iterator_get_current (iter_not_opened));
		if (header) {
			modest_window_mgr_register_header (mgr, header);
			g_object_unref (header);
		}
		
		tny_iterator_next (iter_not_opened);
	}
	g_object_unref (iter_not_opened);
	iter_not_opened = NULL;
	
	/* Open each message */
	if (tny_list_get_length (not_opened_headers) > 0) {
		mail_op = modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_RECEIVE, 
									 G_OBJECT (win), 
									 modest_ui_actions_get_msgs_full_error_handler, 
									 NULL);
		modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_op);
		if (tny_list_get_length (not_opened_headers) > 1) {
			modest_mail_operation_get_msgs_full (mail_op, 
							     not_opened_headers, 
							     open_msg_cb, 
							     NULL, 
							     NULL);
		} else {
			TnyIterator *iter = tny_list_create_iterator (not_opened_headers);
			TnyHeader *header = TNY_HEADER (tny_iterator_get_current (iter));
			modest_mail_operation_get_msg (mail_op, header, open_msg_cb, NULL);
			g_object_unref (header);
			g_object_unref (iter);
		}
		g_object_unref (mail_op);
	}

	/* Clean */
	if (not_opened_headers != NULL)
		g_object_unref (not_opened_headers);
}

void
modest_ui_actions_on_open (GtkAction *action, ModestWindow *win)
{
	TnyList *headers;

	/* Get headers */
	headers = get_selected_headers (win);
	if (!headers)
		return;

	/* Open them */
	_modest_ui_actions_open (headers, win);

	g_object_unref(headers);
}


static void
free_reply_forward_helper (gpointer data)
{
	ReplyForwardHelper *helper;

	helper = (ReplyForwardHelper *) data;
	g_free (helper->account_name);
	g_slice_free (ReplyForwardHelper, helper);
}

static void
reply_forward_cb (ModestMailOperation *mail_op, 
		  TnyHeader *header, 
		  TnyMsg *msg,
		  gpointer user_data)
{
	TnyMsg *new_msg;
	ReplyForwardHelper *rf_helper;
	ModestWindow *msg_win = NULL;
	ModestEditType edit_type;
	gchar *from = NULL;
	TnyAccount *account = NULL;
	ModestWindowMgr *mgr = NULL;
	gchar *signature = NULL;

	/* If there was any error. The mail operation could be NULL,
	   this means that we already have the message downloaded and
	   that we didn't do a mail operation to retrieve it */
	if (mail_op && !modest_ui_actions_msg_retrieval_check (mail_op, header, msg))
		return;
			
	g_return_if_fail (user_data != NULL);
	rf_helper = (ReplyForwardHelper *) user_data;

	from = modest_account_mgr_get_from_string (modest_runtime_get_account_mgr(),
						   rf_helper->account_name);
	if (modest_account_mgr_get_bool (modest_runtime_get_account_mgr(),
					 rf_helper->account_name,
					 MODEST_ACCOUNT_USE_SIGNATURE, FALSE)) {
		signature = modest_account_mgr_get_string (modest_runtime_get_account_mgr (),
							   rf_helper->account_name,
							   MODEST_ACCOUNT_SIGNATURE, FALSE);
	}

	/* Create reply mail */
	switch (rf_helper->action) {
	case ACTION_REPLY:
		new_msg = 
			modest_tny_msg_create_reply_msg (msg, header, from, signature,
							 rf_helper->reply_forward_type,
							 MODEST_TNY_MSG_REPLY_MODE_SENDER);
		break;
	case ACTION_REPLY_TO_ALL:
		new_msg = 
			modest_tny_msg_create_reply_msg (msg, header, from, signature, rf_helper->reply_forward_type,
							 MODEST_TNY_MSG_REPLY_MODE_ALL);
		edit_type = MODEST_EDIT_TYPE_REPLY;
		break;
	case ACTION_FORWARD:
		new_msg = 
			modest_tny_msg_create_forward_msg (msg, from, signature, rf_helper->reply_forward_type);
		edit_type = MODEST_EDIT_TYPE_FORWARD;
		break;
	default:
		g_return_if_reached ();
		return;
	}

	g_free (signature);

	if (!new_msg) {
		g_printerr ("modest: failed to create message\n");
		goto cleanup;
	}

	account = modest_tny_account_store_get_server_account (modest_runtime_get_account_store(),
								       rf_helper->account_name,
								       TNY_ACCOUNT_TYPE_STORE);
	if (!account) {
		g_printerr ("modest: failed to get tnyaccount for '%s'\n", rf_helper->account_name);
		goto cleanup;
	}

	/* Create and register the windows */
	msg_win = modest_msg_edit_window_new (new_msg, rf_helper->account_name, FALSE);
	mgr = modest_runtime_get_window_mgr ();
	modest_window_mgr_register_window (mgr, msg_win);

	if (rf_helper->parent_window != NULL) {
		gdouble parent_zoom;

		parent_zoom = modest_window_get_zoom (MODEST_WINDOW (rf_helper->parent_window));
		modest_window_set_zoom (msg_win, parent_zoom);
	}

	/* Show edit window */
	gtk_widget_show_all (GTK_WIDGET (msg_win));

cleanup:
	if (msg_win)
		g_object_unref (msg_win);
	if (new_msg)
		g_object_unref (G_OBJECT (new_msg));
	if (account)
		g_object_unref (G_OBJECT (account));
/* 	g_object_unref (msg); */
	free_reply_forward_helper (rf_helper);
}

/*
 * Checks a list of headers. If any of them are not currently
 * downloaded (CACHED) then it asks the user for permission to
 * download them.
 *
 * Returns FALSE if the user does not want to download the
 * messages. Returns TRUE if the user allowed the download or if all
 * of them are currently downloaded
 */
static gboolean
download_uncached_messages (TnyList *header_list, 
			    GtkWindow *win,
			    gboolean reply_fwd)
{
	TnyIterator *iter;
	gboolean retval;
	gint uncached_messages = 0;

	iter = tny_list_create_iterator (header_list);
	while (!tny_iterator_is_done (iter)) {
		TnyHeader *header;

		header = TNY_HEADER (tny_iterator_get_current (iter));
		if (header) {
			if (!(tny_header_get_flags (header) & TNY_HEADER_FLAG_CACHED))
				uncached_messages ++;
			g_object_unref (header);
		}

		tny_iterator_next (iter);
	}
	g_object_unref (iter);

	/* Ask for user permission to download the messages */
	retval = TRUE;
	if (uncached_messages > 0) {
		GtkResponseType response;
		if (reply_fwd)
			response = modest_platform_run_confirmation_dialog (GTK_WINDOW (win),
									    _("emev_nc_include_original"));
		else
			response =
				modest_platform_run_confirmation_dialog (GTK_WINDOW (win),
									 ngettext("mcen_nc_get_msg",
										  "mcen_nc_get_msgs",
										  uncached_messages));
		if (response == GTK_RESPONSE_CANCEL)
			retval = FALSE;
		else {
			/* If a download will be necessary, make sure that we have a connection: */
			retval = modest_platform_connect_and_wait(win, NULL);	
		}
	}
	return retval;
}


/*
 * Common code for the reply and forward actions
 */
static void
reply_forward (ReplyForwardAction action, ModestWindow *win)
{
	ModestMailOperation *mail_op = NULL;
	TnyList *header_list = NULL;
	ReplyForwardHelper *rf_helper = NULL;
	guint reply_forward_type;
	gboolean continue_download = TRUE;
	gboolean do_retrieve = TRUE;
	
	g_return_if_fail (MODEST_IS_WINDOW(win));

	/* we need an account when editing */
	if (!modest_account_mgr_has_accounts(modest_runtime_get_account_mgr(), TRUE)) {
		const gboolean created = modest_run_account_setup_wizard (win);
		if (!created)
			return;
	}
	
	header_list = get_selected_headers (win);
	if (!header_list)
		return;

	reply_forward_type = 
		modest_conf_get_int (modest_runtime_get_conf (),
				     (action == ACTION_FORWARD) ? MODEST_CONF_FORWARD_TYPE : MODEST_CONF_REPLY_TYPE,
				     NULL);

	/* Check that the messages have been previously downloaded */
	do_retrieve = (action == ACTION_FORWARD) || (reply_forward_type != MODEST_TNY_MSG_REPLY_TYPE_CITE);
	if (do_retrieve)
		continue_download = download_uncached_messages (header_list, GTK_WINDOW (win), TRUE);
	if (!continue_download) {
		g_object_unref (header_list);
		return;
	}
	
	/* We assume that we can only select messages of the
	   same folder and that we reply all of them from the
	   same account. In fact the interface currently only
	   allows single selection */
	
	/* Fill helpers */
	rf_helper = g_slice_new0 (ReplyForwardHelper);
	rf_helper->reply_forward_type = reply_forward_type;
	rf_helper->action = action;
	rf_helper->account_name = g_strdup (modest_window_get_active_account (win));
	
	if ((win != NULL) && (MODEST_IS_WINDOW (win)))
		rf_helper->parent_window = GTK_WIDGET (win);
	if (!rf_helper->account_name)
		rf_helper->account_name =
			modest_account_mgr_get_default_account (modest_runtime_get_account_mgr());

	if (MODEST_IS_MSG_VIEW_WINDOW(win)) {
		TnyMsg *msg;
		TnyHeader *header;
		/* Get header and message. Do not free them here, the
		   reply_forward_cb must do it */
		msg = modest_msg_view_window_get_message (MODEST_MSG_VIEW_WINDOW(win));
		header = modest_msg_view_window_get_header (MODEST_MSG_VIEW_WINDOW(win));
 		if (!msg || !header) {
			if (msg)
				g_object_unref (msg);
			g_printerr ("modest: no message found\n");
			return;
		} else {
			reply_forward_cb (NULL, header, msg, rf_helper);
		}
		if (header)
			g_object_unref (header);
	} else {
		TnyHeader *header;
		TnyIterator *iter;

		/* Only reply/forward to one message */
		iter = tny_list_create_iterator (header_list);
		header = TNY_HEADER (tny_iterator_get_current (iter));
		g_object_unref (iter);

		if (header) {
			/* Retrieve messages */
			if (do_retrieve) {
				mail_op = modest_mail_operation_new_with_error_handling (
					MODEST_MAIL_OPERATION_TYPE_RECEIVE, 
					G_OBJECT(win),
					modest_ui_actions_get_msgs_full_error_handler, 
					NULL);
				modest_mail_operation_queue_add (
					modest_runtime_get_mail_operation_queue (), mail_op);
				
				modest_mail_operation_get_msg (mail_op,
							       header,
							       reply_forward_cb,
							       rf_helper);
				/* Clean */
				g_object_unref(mail_op);
			} else {
				/* we put a ref here to prevent double unref as the reply
				 * forward callback unrefs the header at its end */
				reply_forward_cb (NULL, header, NULL, rf_helper);
			}


			g_object_unref (header);
		}

	}

	/* Free */
	g_object_unref (header_list);
}

void
modest_ui_actions_on_reply (GtkAction *action, ModestWindow *win)
{
	g_return_if_fail (MODEST_IS_WINDOW(win));

	reply_forward (ACTION_REPLY, win);
}

void
modest_ui_actions_on_forward (GtkAction *action, ModestWindow *win)
{
	g_return_if_fail (MODEST_IS_WINDOW(win));

	reply_forward (ACTION_FORWARD, win);
}

void
modest_ui_actions_on_reply_all (GtkAction *action, ModestWindow *win)
{
	g_return_if_fail (MODEST_IS_WINDOW(win));

	reply_forward (ACTION_REPLY_TO_ALL, win);
}

void 
modest_ui_actions_on_next (GtkAction *action, 
			   ModestWindow *window)
{
	if (MODEST_IS_MAIN_WINDOW (window)) {
		GtkWidget *header_view;

		header_view = modest_main_window_get_child_widget (
				MODEST_MAIN_WINDOW(window),
				MODEST_WIDGET_TYPE_HEADER_VIEW);
		if (!header_view)
			return;
	
		modest_header_view_select_next (
				MODEST_HEADER_VIEW(header_view)); 
	} else if (MODEST_IS_MSG_VIEW_WINDOW (window)) {
		modest_msg_view_window_select_next_message (
				MODEST_MSG_VIEW_WINDOW (window));
	} else {
		g_return_if_reached ();
	}
}

void 
modest_ui_actions_on_prev (GtkAction *action, 
			   ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW(window));

	if (MODEST_IS_MAIN_WINDOW (window)) {
		GtkWidget *header_view;
		header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW(window),
								   MODEST_WIDGET_TYPE_HEADER_VIEW);
		if (!header_view)
			return;
		
		modest_header_view_select_prev (MODEST_HEADER_VIEW(header_view)); 
	} else if (MODEST_IS_MSG_VIEW_WINDOW (window)) {
		modest_msg_view_window_select_previous_message (MODEST_MSG_VIEW_WINDOW (window));
	} else {
		g_return_if_reached ();
	}
}

void 
modest_ui_actions_on_sort (GtkAction *action, 
			   ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW(window));

	if (MODEST_IS_MAIN_WINDOW (window)) {
		GtkWidget *header_view;
		header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW(window),
								   MODEST_WIDGET_TYPE_HEADER_VIEW);
		if (!header_view) {
			modest_platform_information_banner (NULL, NULL, _CS("ckdg_ib_nothing_to_sort"));

			return;
		}

		/* Show sorting dialog */
		modest_platform_run_sort_dialog (GTK_WINDOW (window), MODEST_SORT_HEADERS);	
	}
}

static void
new_messages_arrived (ModestMailOperation *self, 
		      gint new_messages,
		      gpointer user_data)
{
	ModestMainWindow *win = NULL;
	GtkWidget *folder_view = NULL;
	TnyFolderStore *folder = NULL;
	gboolean folder_empty = FALSE;

	g_return_if_fail (MODEST_IS_MAIN_WINDOW (user_data));
	win = MODEST_MAIN_WINDOW (user_data);

	/* Set contents style of headers view */
	if (modest_main_window_get_contents_style (win) == MODEST_MAIN_WINDOW_CONTENTS_STYLE_EMPTY) {
		folder_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win), 
								   MODEST_WIDGET_TYPE_FOLDER_VIEW);
		folder = modest_folder_view_get_selected (MODEST_FOLDER_VIEW(folder_view));		
		

		folder_empty = (tny_folder_get_all_count (TNY_FOLDER (folder)) == 0);
		
		if (!folder_empty)
			modest_main_window_set_contents_style (win,
						       MODEST_MAIN_WINDOW_CONTENTS_STYLE_HEADERS);
	}	

	/* Notify new messages have been downloaded */
	if (new_messages > 0)
		modest_platform_on_new_msg ();
}

/*
 * This function performs the send & receive required actions. The
 * window is used to create the mail operation. Typically it should
 * always be the main window, but we pass it as argument in order to
 * be more flexible.
 */
void
modest_ui_actions_do_send_receive (const gchar *account_name, ModestWindow *win)
{
	gchar *acc_name = NULL;
	ModestMailOperation *mail_op;

	/* If no account name was provided then get the current account, and if
	   there is no current account then pick the default one: */
	if (!account_name) {
		acc_name = g_strdup (modest_window_get_active_account(win));
		if (!acc_name)
			acc_name  = modest_account_mgr_get_default_account (modest_runtime_get_account_mgr());
		if (!acc_name) {
			g_printerr ("modest: cannot get default account\n");
			return;
		}
	} else {
		acc_name = g_strdup (account_name);
	}

	/* Set send/receive operation in progress */	
	modest_main_window_notify_send_receive_initied (MODEST_MAIN_WINDOW(win));

	mail_op = modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_RECEIVE,
								 G_OBJECT (win),
								 modest_ui_actions_send_receive_error_handler,
								 NULL);

	g_signal_connect (G_OBJECT(mail_op), "progress-changed", 
			  G_CALLBACK (_on_send_receive_progress_changed), 
			  win);

	/* Send & receive. */
	/* TODO: The spec wants us to first do any pending deletions, before receiving. */
	/* Receive and then send. The operation is tagged initially as
	   a receive operation because the account update performs a
	   receive and then a send. The operation changes its type
	   internally, so the progress objects will receive the proper
	   progress information */
	modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_op);
	modest_mail_operation_update_account (mail_op, acc_name, new_messages_arrived, win);
	g_object_unref (G_OBJECT (mail_op));
	
	/* Free */
	g_free (acc_name);
}


static void
modest_ui_actions_do_cancel_send (const gchar *account_name,  
				  ModestWindow *win)
{
	TnyTransportAccount *transport_account;
	TnySendQueue *send_queue = NULL;
	GError *error = NULL;

	/* Get transport account */
	transport_account =
		TNY_TRANSPORT_ACCOUNT(modest_tny_account_store_get_server_account
				      (modest_runtime_get_account_store(),
				       account_name,
				       TNY_ACCOUNT_TYPE_TRANSPORT));
	if (!transport_account) {
		g_printerr ("modest: no transport account found for '%s'\n", account_name);
		goto frees;
	}

	/* Get send queue*/
	send_queue = TNY_SEND_QUEUE (modest_runtime_get_send_queue (transport_account));
	if (!TNY_IS_SEND_QUEUE(send_queue)) {
		g_set_error (&error, MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     "modest: could not find send queue for account\n");
	} else {
		/* Keeep messages in outbox folder */
		tny_send_queue_cancel (send_queue, FALSE, &error);
	}	

 frees:
	if (transport_account != NULL) 
		g_object_unref (G_OBJECT (transport_account));
}

static void
modest_ui_actions_cancel_send_all (ModestWindow *win) 
{
	GSList *account_names, *iter;

	account_names = modest_account_mgr_account_names (modest_runtime_get_account_mgr(), 
							  TRUE);

	iter = account_names;
	while (iter) {			
		modest_ui_actions_do_cancel_send ((const char*) iter->data, win);
		iter = g_slist_next (iter);
	}

	modest_account_mgr_free_account_names (account_names);
	account_names = NULL;
}

void
modest_ui_actions_cancel_send (GtkAction *action,  ModestWindow *win)

{
	/* Check if accounts exist */
	gboolean accounts_exist = 
		modest_account_mgr_has_accounts(modest_runtime_get_account_mgr(), TRUE);
	
	/* If not, allow the user to create an account before trying to send/receive. */
	if (!accounts_exist)
		modest_ui_actions_on_accounts (NULL, win);
	
	/* Cancel all sending operaitons */	
	modest_ui_actions_cancel_send_all (win);
}

/*
 * Refreshes all accounts. This function will be used by automatic
 * updates
 */
void
modest_ui_actions_do_send_receive_all (ModestWindow *win)
{
	GSList *account_names, *iter;

	account_names = modest_account_mgr_account_names (modest_runtime_get_account_mgr(), 
							  TRUE);

	iter = account_names;
	while (iter) {			
		modest_ui_actions_do_send_receive ((const char*) iter->data, win);
		iter = g_slist_next (iter);
	}

	modest_account_mgr_free_account_names (account_names);
	account_names = NULL;
}

void 
modest_do_refresh_current_folder(ModestWindow *win)
{
	/* Refresh currently selected folder. Note that if we only
	   want to retreive the headers, then the refresh only will
	   invoke a poke_status over all folders, i.e., only the
	   total/unread count will be updated */
	if (MODEST_IS_MAIN_WINDOW (win)) {
		GtkWidget *header_view, *folder_view;
		TnyFolderStore *folder_store;

		/* Get folder and header view */
		folder_view = 
			modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win), 
							     MODEST_WIDGET_TYPE_FOLDER_VIEW);

		folder_store = modest_folder_view_get_selected (MODEST_FOLDER_VIEW (folder_view));

		if (folder_store && TNY_IS_FOLDER (folder_store)) {
			header_view = 
				modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
								     MODEST_WIDGET_TYPE_HEADER_VIEW);
		
			/* We do not need to set the contents style
			   because it hasn't changed. We also do not
			   need to save the widget status. Just force
			   a refresh */
			modest_header_view_set_folder (MODEST_HEADER_VIEW(header_view),
						       TNY_FOLDER (folder_store),
						       folder_refreshed_cb,
						       MODEST_MAIN_WINDOW (win));
		}
		
		if (folder_store)
			g_object_unref (folder_store);
	}
}


/*
 * Handler of the click on Send&Receive button in the main toolbar
 */
void
modest_ui_actions_on_send_receive (GtkAction *action, ModestWindow *win)
{
	/* Check if accounts exist */
	gboolean accounts_exist = 
		modest_account_mgr_has_accounts(modest_runtime_get_account_mgr(), TRUE);
	
	/* If not, allow the user to create an account before trying to send/receive. */
	if (!accounts_exist)
		modest_ui_actions_on_accounts (NULL, win);

	modest_do_refresh_current_folder (win);
	
	/* Refresh the active account */
	modest_ui_actions_do_send_receive (NULL, win);
}


void
modest_ui_actions_toggle_header_list_view (GtkAction *action, ModestMainWindow *main_window)
{
	ModestConf *conf;
	GtkWidget *header_view;
	
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));

	header_view = modest_main_window_get_child_widget (main_window,
							   MODEST_WIDGET_TYPE_HEADER_VIEW);
	if (!header_view)
		return;

	conf = modest_runtime_get_conf ();
	
	/* what is saved/restored is depending on the style; thus; we save with
	 * old style, then update the style, and restore for this new style
	 */
	modest_widget_memory_save (conf, G_OBJECT(header_view), MODEST_CONF_HEADER_VIEW_KEY);
	
	if (modest_header_view_get_style
	    (MODEST_HEADER_VIEW(header_view)) == MODEST_HEADER_VIEW_STYLE_DETAILS)
		modest_header_view_set_style (MODEST_HEADER_VIEW(header_view),
					      MODEST_HEADER_VIEW_STYLE_TWOLINES);
	else
		modest_header_view_set_style (MODEST_HEADER_VIEW(header_view),
					      MODEST_HEADER_VIEW_STYLE_DETAILS);

	modest_widget_memory_restore (conf, G_OBJECT(header_view),
				      MODEST_CONF_HEADER_VIEW_KEY);
}


void 
modest_ui_actions_on_header_selected (ModestHeaderView *header_view, 
				      TnyHeader *header,
				      ModestMainWindow *main_window)
{
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));
	g_return_if_fail (MODEST_IS_HEADER_VIEW (header_view));
	
	/* in the case the folder is empty, show the empty folder message and focus
	 * folder view */
	if (!header && gtk_widget_is_focus (GTK_WIDGET (header_view))) {
		if (modest_header_view_is_empty (header_view)) {
			TnyFolder *folder = modest_header_view_get_folder (header_view);
			GtkWidget *folder_view = 
				modest_main_window_get_child_widget (main_window,
								     MODEST_WIDGET_TYPE_FOLDER_VIEW);
			if (folder != NULL) 
				modest_folder_view_select_folder (MODEST_FOLDER_VIEW (folder_view), folder, FALSE);
			gtk_widget_grab_focus (GTK_WIDGET (folder_view));
			return;
		}
	}
	/* If no header has been selected then exit */
	if (!header)
		return;

	/* Update focus */
	if (!gtk_widget_is_focus (GTK_WIDGET(header_view)))
	    gtk_widget_grab_focus (GTK_WIDGET(header_view));

	/* Update Main window title */
	if (gtk_widget_is_focus (GTK_WIDGET(header_view))) {
		const gchar *subject = tny_header_get_subject (header);
		if (subject && strlen(subject) > 0)
			gtk_window_set_title (GTK_WINDOW (main_window), subject);
		else
			gtk_window_set_title (GTK_WINDOW (main_window), _("mail_va_no_subject"));
	}

	/* Update toolbar dimming state */
	modest_ui_actions_check_toolbar_dimming_rules (MODEST_WINDOW (main_window));
}

void
modest_ui_actions_on_header_activated (ModestHeaderView *header_view,
				       TnyHeader *header,
				       ModestMainWindow *main_window)
{
	TnyList *headers;

	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));
	
	if (!header)
		return;

	headers = tny_simple_list_new ();
	tny_list_prepend (headers, G_OBJECT (header));

	_modest_ui_actions_open (headers, MODEST_WINDOW (main_window));

	g_object_unref (headers);
}

static void
set_active_account_from_tny_account (TnyAccount *account,
				     ModestWindow *window)
{
	const gchar *server_acc_name = tny_account_get_id (account);
	
	/* We need the TnyAccount provided by the
	   account store because that is the one that
	   knows the name of the Modest account */
	TnyAccount *modest_server_account = modest_server_account = 
		modest_tny_account_store_get_tny_account_by (modest_runtime_get_account_store (),
							     MODEST_TNY_ACCOUNT_STORE_QUERY_ID, 
							     server_acc_name);
	
	const gchar *modest_acc_name = 
		modest_tny_account_get_parent_modest_account_name_for_server_account (modest_server_account);
	modest_window_set_active_account (window, modest_acc_name);
	g_object_unref (modest_server_account);
}


static void
folder_refreshed_cb (ModestMailOperation *mail_op, 
		     TnyFolder *folder, 
		     gpointer user_data)
{
	ModestMainWindow *win = NULL;
	GtkWidget *header_view;
	TnyFolder *current_folder;
	gboolean folder_empty = TRUE;
	gboolean all_marked_as_deleted = TRUE;

	g_return_if_fail (TNY_IS_FOLDER (folder));

	win = MODEST_MAIN_WINDOW (user_data);
	header_view = 
		modest_main_window_get_child_widget(win, MODEST_WIDGET_TYPE_HEADER_VIEW);

	if (header_view) {
		current_folder = modest_header_view_get_folder (MODEST_HEADER_VIEW (header_view));
		if (current_folder != NULL && folder != current_folder) {
			return;
		}
	}

	/* Check if folder is empty and set headers view contents style */
	folder_empty = (tny_folder_get_all_count (folder) == 0);
	all_marked_as_deleted = modest_header_view_is_empty (MODEST_HEADER_VIEW(header_view));
	folder_empty = folder_empty || all_marked_as_deleted ;
	if (folder_empty) {

	printf ("DEBUG: %s: tny_folder_get_all_count() returned 0.\n", __FUNCTION__);
		modest_main_window_set_contents_style (win,
						       MODEST_MAIN_WINDOW_CONTENTS_STYLE_EMPTY);
	} else {
		printf ("DEBUG: %s: tny_folder_get_all_count() returned >0.\n", __FUNCTION__);
	}
}

void 
modest_ui_actions_on_folder_selection_changed (ModestFolderView *folder_view,
					       TnyFolderStore *folder_store, 
					       gboolean selected,
					       ModestMainWindow *main_window)
{
	ModestConf *conf;
	GtkWidget *header_view;

	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));

	header_view = modest_main_window_get_child_widget(main_window,
							  MODEST_WIDGET_TYPE_HEADER_VIEW);
	if (!header_view)
		return;
	
	conf = modest_runtime_get_conf ();

	if (TNY_IS_ACCOUNT (folder_store)) {
		if (selected) {
			/* Update active account */
			set_active_account_from_tny_account (TNY_ACCOUNT (folder_store), MODEST_WINDOW (main_window));
			/* Show account details */
			modest_main_window_set_contents_style (main_window, MODEST_MAIN_WINDOW_CONTENTS_STYLE_DETAILS);
		}
	} else {
		if (TNY_IS_FOLDER (folder_store) && selected) {
			
			/* Update the active account */
			TnyAccount *account = modest_tny_folder_get_account (TNY_FOLDER (folder_store));
			if (account) {
				set_active_account_from_tny_account (account, MODEST_WINDOW (main_window));
				g_object_unref (account);
				account = NULL;
			}

			/* Set the header style by default, it could
			   be changed later by the refresh callback to
			   empty */
			modest_main_window_set_contents_style (main_window, 
							       MODEST_MAIN_WINDOW_CONTENTS_STYLE_HEADERS);

			/* Set folder on header view. This function
			   will call tny_folder_refresh_async so we
			   pass a callback that will be called when
			   finished. We use that callback to set the
			   empty view if there are no messages */
			modest_header_view_set_folder (MODEST_HEADER_VIEW(header_view),
						       TNY_FOLDER (folder_store),
						       folder_refreshed_cb,
						       main_window);
			
			/* Restore configuration. We need to do this
			   *after* the set_folder because the widget
			   memory asks the header view about its
			   folder  */
			modest_widget_memory_restore (modest_runtime_get_conf (), 
						      G_OBJECT(header_view),
						      MODEST_CONF_HEADER_VIEW_KEY);
		} else {
			/* Update the active account */
			modest_window_set_active_account (MODEST_WINDOW (main_window), NULL);
			/* Save only if we're seeing headers */
			if (modest_main_window_get_contents_style (main_window) ==
			    MODEST_MAIN_WINDOW_CONTENTS_STYLE_HEADERS)
				modest_widget_memory_save (conf, G_OBJECT (header_view), 
							   MODEST_CONF_HEADER_VIEW_KEY);
			modest_header_view_clear (MODEST_HEADER_VIEW(header_view));
		}
	}

	/* Update toolbar dimming state */
	modest_ui_actions_check_toolbar_dimming_rules (MODEST_WINDOW (main_window));
}

void 
modest_ui_actions_on_item_not_found (ModestHeaderView *header_view,ModestItemType type,
				     ModestWindow *win)
{
	GtkWidget *dialog;
	gchar *txt, *item;
	gboolean online;

	item = (type == MODEST_ITEM_TYPE_FOLDER) ? "folder" : "message";
	
	online = tny_device_is_online (modest_runtime_get_device());

	if (online) {
		/* already online -- the item is simply not there... */
		dialog = gtk_message_dialog_new (GTK_WINDOW (win),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_OK,
						 _("The %s you selected cannot be found"),
						 item);
		gtk_dialog_run (GTK_DIALOG(dialog));
	} else {
		dialog = gtk_dialog_new_with_buttons (_("Connection requested"),
						      GTK_WINDOW (win),
						      GTK_DIALOG_MODAL,
						      GTK_STOCK_CANCEL,
						      GTK_RESPONSE_REJECT,
						      GTK_STOCK_OK,
						      GTK_RESPONSE_ACCEPT,
						      NULL);
		txt = g_strdup_printf (_("This %s is not available in offline mode.\n"
					 "Do you want to get online?"), item);
		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), 
				    gtk_label_new (txt), FALSE, FALSE, 0);
		gtk_widget_show_all (GTK_WIDGET(GTK_DIALOG(dialog)->vbox));
		g_free (txt);

		gtk_window_set_default_size (GTK_WINDOW(dialog), 300, 300);
		if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			/* TODO: Comment about why is this commented out: */
			/* modest_platform_connect_and_wait (); */
		}
	}
	gtk_widget_destroy (dialog);
}

void
modest_ui_actions_on_msg_link_hover (ModestMsgView *msgview, const gchar* link,
				     ModestWindow *win)
{
	/* g_message ("%s %s", __FUNCTION__, link); */
}	


void
modest_ui_actions_on_msg_link_clicked (ModestMsgView *msgview, const gchar* link,
					ModestWindow *win)
{
	modest_platform_activate_uri (link);
}

void
modest_ui_actions_on_msg_link_contextual (ModestMsgView *msgview, const gchar* link,
					  ModestWindow *win)
{
	modest_platform_show_uri_popup (link);
}

void
modest_ui_actions_on_msg_attachment_clicked (ModestMsgView *msgview, TnyMimePart *mime_part,
					     ModestWindow *win)
{
	modest_msg_view_window_view_attachment (MODEST_MSG_VIEW_WINDOW (win), mime_part);
}

void
modest_ui_actions_on_msg_recpt_activated (ModestMsgView *msgview,
					  const gchar *address,
					  ModestWindow *win)
{
	/* g_message ("%s %s", __FUNCTION__, address); */
}

void
modest_ui_actions_on_save_to_drafts (GtkWidget *widget, ModestMsgEditWindow *edit_window)
{
	TnyTransportAccount *transport_account;
	ModestMailOperation *mail_operation;
	MsgData *data;
	gchar *account_name, *from;
	ModestAccountMgr *account_mgr;
	gchar *info_text = NULL;

	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW(edit_window));
	
	data = modest_msg_edit_window_get_msg_data (edit_window);

	account_mgr = modest_runtime_get_account_mgr();
	account_name = g_strdup(modest_window_get_active_account (MODEST_WINDOW(edit_window)));
	if (!account_name) 
		account_name = modest_account_mgr_get_default_account (account_mgr);
	if (!account_name) {
		g_printerr ("modest: no account found\n");
		modest_msg_edit_window_free_msg_data (edit_window, data);
		return;
	}

	if (!strcmp (account_name, MODEST_LOCAL_FOLDERS_ACCOUNT_ID)) {
		account_name = g_strdup (data->account_name);
	}

	transport_account =
		TNY_TRANSPORT_ACCOUNT(modest_tny_account_store_get_server_account
				      (modest_runtime_get_account_store(),
				       account_name,
				       TNY_ACCOUNT_TYPE_TRANSPORT));
	if (!transport_account) {
		g_printerr ("modest: no transport account found for '%s'\n", account_name);
		g_free (account_name);
		modest_msg_edit_window_free_msg_data (edit_window, data);
		return;
	}
	from = modest_account_mgr_get_from_string (account_mgr, account_name);

	/* Create the mail operation */		
	mail_operation = modest_mail_operation_new (MODEST_MAIL_OPERATION_TYPE_INFO, G_OBJECT(edit_window));
	modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_operation);

	modest_mail_operation_save_to_drafts (mail_operation,
					      transport_account,
					      data->draft_msg,
					      edit_window,
					      from,
					      data->to, 
					      data->cc, 
					      data->bcc,
					      data->subject, 
					      data->plain_body, 
					      data->html_body,
					      data->attachments,
					      data->priority_flags);
	/* Frees */
	g_free (from);
	g_free (account_name);
	g_object_unref (G_OBJECT (transport_account));
	g_object_unref (G_OBJECT (mail_operation));

	modest_msg_edit_window_free_msg_data (edit_window, data);

	info_text = g_strdup_printf (_("mail_va_saved_to_drafts"), _("mcen_me_folder_drafts"));
	modest_platform_information_banner (NULL, NULL, info_text);
	g_free (info_text);
}

/* For instance, when clicking the Send toolbar button when editing a message: */
void
modest_ui_actions_on_send (GtkWidget *widget, ModestMsgEditWindow *edit_window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW(edit_window));

	if (!modest_msg_edit_window_check_names (edit_window, TRUE))
		return;
	
	/* Offer the connection dialog, if necessary: */	
	if (!modest_platform_connect_and_wait (GTK_WINDOW (edit_window), NULL))
		return;
	
	/* FIXME: Code added just for testing. The final version will
	   use the send queue provided by tinymail and some
	   classifier */
	ModestAccountMgr *account_mgr = modest_runtime_get_account_mgr();
	gchar *account_name = g_strdup(modest_window_get_active_account (MODEST_WINDOW(edit_window)));
	if (!account_name) 
		account_name = modest_account_mgr_get_default_account (account_mgr);
		
	if (!account_name) {
		/* Run account setup wizard */
		const gboolean created = modest_run_account_setup_wizard(MODEST_WINDOW(edit_window));
		if (!created)
			return;
	}
	
	MsgData *data = modest_msg_edit_window_get_msg_data (edit_window);

	if (!strcmp (account_name, MODEST_LOCAL_FOLDERS_ACCOUNT_ID)) {
		account_name = g_strdup (data->account_name);
	}
	
	/* Get the currently-active transport account for this modest account: */
	TnyTransportAccount *transport_account =
		TNY_TRANSPORT_ACCOUNT(modest_tny_account_store_get_transport_account_for_open_connection
				      (modest_runtime_get_account_store(),
				       account_name));
	if (!transport_account) {
		/* Run account setup wizard */
		const gboolean created = modest_run_account_setup_wizard(MODEST_WINDOW(edit_window));
		if (!created)
			return;
	}
	
	gchar *from = modest_account_mgr_get_from_string (account_mgr, account_name);

	/* mail content checks and dialogs */
	if (data->subject == NULL || data->subject[0] == '\0') {
		GtkResponseType response;
		response = modest_platform_run_confirmation_dialog (GTK_WINDOW (edit_window),
								    _("mcen_nc_subject_is_empty_send"));
		if (response == GTK_RESPONSE_CANCEL) {
			g_free (account_name);
			return;
		}
	}

	if (data->plain_body == NULL || data->plain_body[0] == '\0') {
		GtkResponseType response;
		gchar *note_message;
		gchar *note_subject = data->subject;
		if (note_subject == NULL || note_subject[0] == '\0')
			note_subject = _("mail_va_no_subject");
		note_message = g_strdup_printf (_("emev_ni_ui_smtp_message_null"), note_subject);
		response = modest_platform_run_confirmation_dialog (GTK_WINDOW (edit_window),
								    note_message);
		g_free (note_message);
		if (response == GTK_RESPONSE_CANCEL) {
			g_free (account_name);
			return;
		}
	}

	modest_platform_information_banner (NULL, NULL, _("mcen_ib_outbox_waiting_to_be_sent"));

	/* Create the mail operation */
	ModestMailOperation *mail_operation = modest_mail_operation_new (MODEST_MAIL_OPERATION_TYPE_SEND, G_OBJECT(edit_window));
	modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_operation);

	modest_mail_operation_send_new_mail (mail_operation,
					     transport_account,
					     data->draft_msg,
					     from,
					     data->to, 
					     data->cc, 
					     data->bcc,
					     data->subject, 
					     data->plain_body, 
					     data->html_body,
					     data->attachments,
					     data->priority_flags);
					     
	/* Free data: */
	g_free (from);
	g_free (account_name);
	g_object_unref (G_OBJECT (transport_account));
	g_object_unref (G_OBJECT (mail_operation));

	modest_msg_edit_window_free_msg_data (edit_window, data);
	modest_msg_edit_window_set_sent (edit_window, TRUE);

	/* Save settings and close the window: */
	modest_ui_actions_on_close_window (NULL, MODEST_WINDOW (edit_window));
}

void 
modest_ui_actions_on_toggle_bold (GtkToggleAction *action,
				  ModestMsgEditWindow *window)
{
	ModestMsgEditFormatState *format_state = NULL;

	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW (window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	format_state = modest_msg_edit_window_get_format_state (window);
	g_return_if_fail (format_state != NULL);

	format_state->bold = gtk_toggle_action_get_active (action);
	modest_msg_edit_window_set_format_state (window, format_state);
	g_free (format_state);
	
}

void 
modest_ui_actions_on_toggle_italics (GtkToggleAction *action,
				     ModestMsgEditWindow *window)
{
	ModestMsgEditFormatState *format_state = NULL;

	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW(window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	format_state = modest_msg_edit_window_get_format_state (window);
	g_return_if_fail (format_state != NULL);

	format_state->italics = gtk_toggle_action_get_active (action);
	modest_msg_edit_window_set_format_state (window, format_state);
	g_free (format_state);
	
}

void 
modest_ui_actions_on_toggle_bullets (GtkToggleAction *action,
				     ModestMsgEditWindow *window)
{
	ModestMsgEditFormatState *format_state = NULL;

	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW (window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	format_state = modest_msg_edit_window_get_format_state (window);
	g_return_if_fail (format_state != NULL);

	format_state->bullet = gtk_toggle_action_get_active (action);
	modest_msg_edit_window_set_format_state (window, format_state);
	g_free (format_state);
	
}

void 
modest_ui_actions_on_change_justify (GtkRadioAction *action,
				     GtkRadioAction *selected,
				     ModestMsgEditWindow *window)
{
	ModestMsgEditFormatState *format_state = NULL;
	GtkJustification value;

	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW(window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	value = gtk_radio_action_get_current_value (selected);

	format_state = modest_msg_edit_window_get_format_state (window);
	g_return_if_fail (format_state != NULL);

	format_state->justification = value;
	modest_msg_edit_window_set_format_state (window, format_state);
	g_free (format_state);
}

void 
modest_ui_actions_on_select_editor_color (GtkAction *action,
					  ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_ACTION (action));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW(window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	modest_msg_edit_window_select_color (window);
}

void 
modest_ui_actions_on_select_editor_background_color (GtkAction *action,
						     ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_ACTION (action));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW(window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	modest_msg_edit_window_select_background_color (window);
}

void 
modest_ui_actions_on_insert_image (GtkAction *action,
				   ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_ACTION (action));

	if (modest_msg_edit_window_get_format (MODEST_MSG_EDIT_WINDOW(window)) == MODEST_MSG_EDIT_FORMAT_TEXT)
		return;

	modest_msg_edit_window_insert_image (window);
}

void 
modest_ui_actions_on_attach_file (GtkAction *action,
				  ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_ACTION (action));

	modest_msg_edit_window_offer_attach_file (window);
}

void 
modest_ui_actions_on_remove_attachments (GtkAction *action,
					 ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	g_return_if_fail (GTK_IS_ACTION (action));

	modest_msg_edit_window_remove_attachments (window, NULL);
}

static void
modest_ui_actions_new_folder_error_handler (ModestMailOperation *mail_op,
                                            gpointer user_data)
{
	ModestMainWindow *window = MODEST_MAIN_WINDOW (user_data);
	const GError *error = modest_mail_operation_get_error (mail_op);

	if(error)
	{
		modest_platform_information_banner (GTK_WIDGET (window), NULL,
	        	                            modest_mail_operation_get_error (mail_op)->message);
	}
}

static void
modest_ui_actions_create_folder(GtkWidget *parent_window,
                                GtkWidget *folder_view)
{
	TnyFolderStore *parent_folder;

	parent_folder = modest_folder_view_get_selected (MODEST_FOLDER_VIEW(folder_view));
	
	if (parent_folder) {
		gboolean finished = FALSE;
		gint result;
		gchar *folder_name = NULL, *suggested_name = NULL;
		const gchar *proto_str = NULL;
		TnyAccount *account;

		if (TNY_IS_ACCOUNT (parent_folder))
			account = g_object_ref (parent_folder);
		else
			account = tny_folder_get_account (TNY_FOLDER (parent_folder));
		proto_str = tny_account_get_proto (TNY_ACCOUNT (account));

		if (proto_str && modest_protocol_info_get_transport_store_protocol (proto_str) ==
		    MODEST_PROTOCOL_STORE_POP) {
			finished = TRUE;
			hildon_banner_show_information (NULL, NULL, _("mail_in_ui_folder_create_error"));
		}
		g_object_unref (account);

		/* Run the new folder dialog */
		while (!finished) {
			result = modest_platform_run_new_folder_dialog (GTK_WINDOW (parent_window),
									parent_folder,
									suggested_name,
									&folder_name);

			g_free (suggested_name);
			suggested_name = NULL;

			if (result == GTK_RESPONSE_REJECT) {
				finished = TRUE;
			} else {
				ModestMailOperation *mail_op;
				TnyFolder *new_folder = NULL;

				mail_op  = modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_INFO,
								                          G_OBJECT(parent_window),
											  modest_ui_actions_new_folder_error_handler,
											  parent_window);

				modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), 
								 mail_op);
				new_folder = modest_mail_operation_create_folder (mail_op,
										  parent_folder,
										  (const gchar *) folder_name);
				if (new_folder) {
					modest_folder_view_select_folder (MODEST_FOLDER_VIEW(folder_view), 
									  new_folder, TRUE);

					g_object_unref (new_folder);
					finished = TRUE;
				}
				g_object_unref (mail_op);
			}

			suggested_name = folder_name;
			folder_name = NULL;
		}

		g_object_unref (parent_folder);
	}
}

void 
modest_ui_actions_on_new_folder (GtkAction *action, ModestMainWindow *main_window)
{
	GtkWidget *folder_view;
	
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));

	folder_view = modest_main_window_get_child_widget (main_window,
							   MODEST_WIDGET_TYPE_FOLDER_VIEW);
	if (!folder_view)
		return;

	modest_ui_actions_create_folder (GTK_WIDGET (main_window), folder_view);
}

static void
modest_ui_actions_rename_folder_error_handler (ModestMailOperation *mail_op,
					       gpointer user_data)
{
	ModestMainWindow *window = MODEST_MAIN_WINDOW (user_data);
	const GError *error = NULL;
	const gchar *message = NULL;
	
	/* Get error message */
	error = modest_mail_operation_get_error (mail_op);
	if (error != NULL && error->message != NULL) {
		message = error->message;
	} else {
		message = _("!!! FIXME: Unable to rename");
	}
	
	modest_platform_information_banner (GTK_WIDGET (window), NULL,
	                                    message);
}

void 
modest_ui_actions_on_rename_folder (GtkAction *action,
				     ModestMainWindow *main_window)
{
	TnyFolderStore *folder;
	GtkWidget *folder_view;
	GtkWidget *header_view;	

	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));

	folder_view = modest_main_window_get_child_widget (main_window,
							   MODEST_WIDGET_TYPE_FOLDER_VIEW);
	if (!folder_view)
		return;

	header_view = modest_main_window_get_child_widget (main_window,
							   MODEST_WIDGET_TYPE_HEADER_VIEW);
	
	if (!header_view)
		return;

	folder = modest_folder_view_get_selected (MODEST_FOLDER_VIEW(folder_view));
	if (!folder)
		return;

	/* Offer the connection dialog if necessary: */
	if (!modest_platform_connect_and_wait_if_network_folderstore (NULL, folder)) {
		g_object_unref (G_OBJECT (folder));
		return;
	}

	
	if (TNY_IS_FOLDER (folder)) {
		gchar *folder_name;
		gint response;
		const gchar *current_name;

		current_name = tny_folder_get_name (TNY_FOLDER (folder));
		response = modest_platform_run_rename_folder_dialog (GTK_WINDOW (main_window), NULL,
								     current_name, &folder_name);

		if (response == GTK_RESPONSE_ACCEPT && strlen (folder_name) > 0) {
			ModestMailOperation *mail_op;

			mail_op = 
				modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_INFO,
									       G_OBJECT(main_window),
									       modest_ui_actions_rename_folder_error_handler,
									       main_window);

			modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (),
							 mail_op);

			modest_header_view_clear (MODEST_HEADER_VIEW (header_view));
			
			modest_folder_view_select_folder (MODEST_FOLDER_VIEW(folder_view),
							  TNY_FOLDER(folder), TRUE);

			modest_header_view_clear ((ModestHeaderView *) header_view);
 
			modest_mail_operation_rename_folder (mail_op,
							     TNY_FOLDER (folder),
							     (const gchar *) folder_name);

			g_object_unref (mail_op);
			g_free (folder_name);
		}
	}
	g_object_unref (folder);
}

static void
modest_ui_actions_delete_folder_error_handler (ModestMailOperation *mail_op,
					       gpointer user_data)
{
	GObject *win = modest_mail_operation_get_source (mail_op);

	modest_platform_run_information_dialog ((win) ? GTK_WINDOW (win) : NULL,
						_("mail_in_ui_folder_delete_error"));
	g_object_unref (win);
}

static void
delete_folder (ModestMainWindow *main_window, gboolean move_to_trash) 
{
	TnyFolderStore *folder;
	GtkWidget *folder_view;
	gint response;
	gchar *message;
	
	g_return_if_fail (MODEST_IS_MAIN_WINDOW (main_window));

	folder_view = modest_main_window_get_child_widget (main_window,
							   MODEST_WIDGET_TYPE_FOLDER_VIEW);
	if (!folder_view)
		return;

	folder = modest_folder_view_get_selected (MODEST_FOLDER_VIEW (folder_view));

	/* Show an error if it's an account */
	if (!TNY_IS_FOLDER (folder)) {
		modest_platform_run_information_dialog (GTK_WINDOW (main_window),
							_("mail_in_ui_folder_delete_error"));
		g_object_unref (G_OBJECT (folder));
		return ;
	}

	/* Offer the connection dialog if necessary: */
	if (!modest_platform_connect_and_wait_if_network_folderstore (NULL, folder)) {
		g_object_unref (G_OBJECT (folder));
		return;
	}

	/* Ask the user */	
	message =  g_strdup_printf (_("mcen_nc_delete_folder_text"), 
				    tny_folder_get_name (TNY_FOLDER (folder)));
	response = modest_platform_run_confirmation_dialog (GTK_WINDOW (main_window),
							    (const gchar *) message);
	g_free (message);

	if (response == GTK_RESPONSE_OK) {
		ModestMailOperation *mail_op = 
			modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_DELETE, 
								       G_OBJECT(main_window),
								       modest_ui_actions_delete_folder_error_handler,
								       NULL);

		modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (),
						 mail_op);
		modest_mail_operation_remove_folder (mail_op, TNY_FOLDER (folder), move_to_trash);
		g_object_unref (G_OBJECT (mail_op));
	}

	g_object_unref (G_OBJECT (folder));
}

void 
modest_ui_actions_on_delete_folder (GtkAction *action,
				     ModestMainWindow *main_window)
{
	GtkWidget *folder_view;
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));

	delete_folder (main_window, FALSE);
	folder_view = modest_main_window_get_child_widget (main_window,
							   MODEST_WIDGET_TYPE_FOLDER_VIEW);
	if (!folder_view)
		return;
	modest_folder_view_select_first_inbox_or_local (MODEST_FOLDER_VIEW (folder_view));
}

void 
modest_ui_actions_on_move_folder_to_trash_folder (GtkAction *action, ModestMainWindow *main_window)
{
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));
	
	delete_folder (main_window, TRUE);
}


static void
show_error (GtkWidget *parent_widget, const gchar* text)
{
	hildon_banner_show_information(parent_widget, NULL, text);
	
#if 0
	GtkDialog *dialog = GTK_DIALOG (hildon_note_new_information (parent_window, text)); */
	/*
	  GtkDialog *dialog = GTK_DIALOG (gtk_message_dialog_new (parent_window,
	  (GtkDialogFlags)0,
	  GTK_MESSAGE_ERROR,
	  GTK_BUTTONS_OK,
	  text ));
	*/
		 
	gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
#endif
}

void
modest_ui_actions_on_password_requested (TnyAccountStore *account_store, 
					 const gchar* server_account_name,
					 gchar **username,
					 gchar **password, 
					 gboolean *cancel, 
					 gboolean *remember,
					 ModestMainWindow *main_window)
{
	g_return_if_fail(server_account_name);
	/* printf("DEBUG: %s: server_account_name=%s\n", __FUNCTION__, server_account_name); */
	
	/* Initalize output parameters: */
	if (cancel)
		*cancel = FALSE;
		
	if (remember)
		*remember = TRUE;
		
#ifdef MODEST_PLATFORM_MAEMO
	/* Maemo uses a different (awkward) button order,
	 * It should probably just use gtk_alternative_dialog_button_order ().
	 */
	GtkWidget *dialog = gtk_dialog_new_with_buttons (_("mail_ti_password_protected"),
					      NULL,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_REJECT,
					      NULL);
#else
	GtkWidget *dialog = gtk_dialog_new_with_buttons (_("mail_ti_password_protected"),
					      NULL,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_REJECT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_ACCEPT,
					      NULL);
#endif /* MODEST_PLATFORM_MAEMO */

	gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(main_window));
	
	gchar *server_name = modest_server_account_get_hostname (
		modest_runtime_get_account_mgr(), server_account_name);
	if (!server_name) {/* This happened once, though I don't know why. murrayc. */
		g_warning("%s: Could not get server name for server account '%s'", __FUNCTION__, server_account_name);
		*cancel = TRUE;
		return;
	}
	
	/* This causes a warning because the logical ID has no %s in it, 
	 * though the translation does, but there is not much we can do about that: */
	gchar *txt = g_strdup_printf (_("mail_ia_password_info"), server_name);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), gtk_label_new(txt),
			    FALSE, FALSE, 0);
	g_free (txt);
	g_free (server_name);
	server_name = NULL;

	/* username: */
	gchar *initial_username = modest_server_account_get_username (
		modest_runtime_get_account_mgr(), server_account_name);
	
	GtkWidget *entry_username = gtk_entry_new ();
	if (initial_username)
		gtk_entry_set_text (GTK_ENTRY (entry_username), initial_username);
	/* Dim this if a connection has ever succeeded with this username,
	 * as per the UI spec: */
	const gboolean username_known = 
		modest_server_account_get_username_has_succeeded(
			modest_runtime_get_account_mgr(), server_account_name);
	gtk_widget_set_sensitive (entry_username, !username_known);
	
#ifdef MODEST_PLATFORM_MAEMO
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (entry_username), HILDON_GTK_INPUT_MODE_FULL);
	
	/* Create a size group to be used by all captions.
	 * Note that HildonCaption does not create a default size group if we do not specify one.
	 * We use GTK_SIZE_GROUP_HORIZONTAL, so that the widths are the same. */
	GtkSizeGroup *sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	
	GtkWidget *caption = hildon_caption_new (sizegroup, 
		_("mail_fi_username"), entry_username, NULL, HILDON_CAPTION_MANDATORY);
	gtk_widget_show (entry_username);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, 
		FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (caption);
#else 
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), entry_username,
			    TRUE, FALSE, 0);
#endif /* MODEST_PLATFORM_MAEMO */	
			    
	/* password: */
	GtkWidget *entry_password = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY(entry_password), FALSE);
	/* gtk_entry_set_invisible_char (GTK_ENTRY(entry_password), "*"); */
	
#ifdef MODEST_PLATFORM_MAEMO
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (entry_password), 
		HILDON_GTK_INPUT_MODE_FULL | HILDON_GTK_INPUT_MODE_INVISIBLE);
	
	caption = hildon_caption_new (sizegroup, 
		_("mail_fi_password"), entry_password, NULL, HILDON_CAPTION_MANDATORY);
	gtk_widget_show (entry_password);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, 
		FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (caption);
	g_object_unref (sizegroup);
#else 
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), entry_password,
			    TRUE, FALSE, 0);
#endif /* MODEST_PLATFORM_MAEMO */	
			    	
/* This is not in the Maemo UI spec:
	remember_pass_check = gtk_check_button_new_with_label (_("Remember password"));
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), remember_pass_check,
			    TRUE, FALSE, 0);
*/

	gtk_widget_show_all (GTK_WIDGET(GTK_DIALOG(dialog)->vbox));
	
	if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		if (username) {
			*username = g_strdup (gtk_entry_get_text (GTK_ENTRY(entry_username)));
			
			modest_server_account_set_username (
				 modest_runtime_get_account_mgr(), server_account_name, 
				 *username);
				 
			const gboolean username_was_changed = 
				(strcmp (*username, initial_username) != 0);
			if (username_was_changed) {
				g_warning ("%s: tinymail does not yet support changing the "
					"username in the get_password() callback.\n", __FUNCTION__);
			}
		}
			
		if (password) {
			*password = g_strdup (gtk_entry_get_text (GTK_ENTRY(entry_password)));
			
			/* We do not save the password in the configuration, 
			 * because this function is only called for passwords that should 
			 * not be remembered:
			modest_server_account_set_password (
				 modest_runtime_get_account_mgr(), server_account_name, 
				 *password);
			*/
		}
		
		if (cancel)
			*cancel   = FALSE;
			
	} else {
		show_error(GTK_WIDGET (main_window), _("mail_ib_login_cancelled"));
		
		if (username)
			*username = NULL;
			
		if (password)
			*password = NULL;
			
		if (cancel)
			*cancel   = TRUE;
	}

/* This is not in the Maemo UI spec:
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (remember_pass_check)))
		*remember = TRUE;
	else
		*remember = FALSE;
*/

	gtk_widget_destroy (dialog);
	
	/* printf ("DEBUG: %s: cancel=%d\n", __FUNCTION__, *cancel); */
}

void
modest_ui_actions_on_cut (GtkAction *action,
			  ModestWindow *window)
{
	GtkWidget *focused_widget;
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	focused_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (GTK_IS_EDITABLE (focused_widget)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE(focused_widget));
		gtk_clipboard_set_can_store (clipboard, NULL, 0);
		gtk_clipboard_store (clipboard);
	} else if (GTK_IS_TEXT_VIEW (focused_widget)) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (focused_widget));
		gtk_text_buffer_cut_clipboard (buffer, clipboard, TRUE);
		gtk_clipboard_set_can_store (clipboard, NULL, 0);
		gtk_clipboard_store (clipboard);
	} else if (MODEST_IS_HEADER_VIEW (focused_widget)) {
		modest_header_view_cut_selection (MODEST_HEADER_VIEW (focused_widget));
	} else if (MODEST_IS_FOLDER_VIEW (focused_widget)) {
 		modest_folder_view_cut_selection (MODEST_FOLDER_VIEW (focused_widget));
	}
}

void
modest_ui_actions_on_copy (GtkAction *action,
			   ModestWindow *window)
{
	GtkClipboard *clipboard;
	GtkWidget *focused_widget;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	focused_widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_LABEL (focused_widget)) {
		gtk_clipboard_set_text (clipboard, gtk_label_get_text (GTK_LABEL (focused_widget)), -1);
		gtk_clipboard_set_can_store (clipboard, NULL, 0);
		gtk_clipboard_store (clipboard);
	} else if (GTK_IS_EDITABLE (focused_widget)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE(focused_widget));
		gtk_clipboard_set_can_store (clipboard, NULL, 0);
		gtk_clipboard_store (clipboard);
	} else if (GTK_IS_HTML (focused_widget)) {
		gtk_html_copy (GTK_HTML (focused_widget));
		gtk_clipboard_set_can_store (clipboard, NULL, 0);
		gtk_clipboard_store (clipboard);
	} else if (GTK_IS_TEXT_VIEW (focused_widget)) {
		GtkTextBuffer *buffer;
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (focused_widget));
		gtk_text_buffer_copy_clipboard (buffer, clipboard);
		gtk_clipboard_set_can_store (clipboard, NULL, 0);
		gtk_clipboard_store (clipboard);
	} else if (MODEST_IS_HEADER_VIEW (focused_widget)) {
		TnyList *header_list = modest_header_view_get_selected_headers (MODEST_HEADER_VIEW (focused_widget));
		TnyIterator *iter = tny_list_create_iterator (header_list);
		TnyHeader *header = TNY_HEADER (tny_iterator_get_current (iter));
		
		gboolean ask = FALSE;
		if (header) {
			TnyFolder *folder = tny_header_get_folder (header);
			TnyAccount *account = tny_folder_get_account (folder);
			const gchar *proto_str = tny_account_get_proto (TNY_ACCOUNT (account));
			/* If it's POP then ask */
			ask = (modest_protocol_info_get_transport_store_protocol (proto_str) == 
		       		MODEST_PROTOCOL_STORE_POP) ? TRUE : FALSE;
			g_object_unref (account);
			g_object_unref (folder);
			g_object_unref (header);
		}

		g_object_unref (iter);
		
		/* Check that the messages have been previously downloaded */
		gboolean continue_download = TRUE;
		if (ask)
			continue_download = download_uncached_messages (header_list, GTK_WINDOW (window), FALSE);
		if (continue_download)
			modest_header_view_copy_selection (MODEST_HEADER_VIEW (focused_widget));
		g_object_unref (header_list);
	} else if (MODEST_IS_FOLDER_VIEW (focused_widget)) {
 		modest_folder_view_copy_selection (MODEST_FOLDER_VIEW (focused_widget));
	}    

	/* Show information banner */
	modest_platform_information_banner (NULL, NULL, _CS("ecoc_ib_edwin_copied"));
	
}

void
modest_ui_actions_on_undo (GtkAction *action,
			   ModestWindow *window)
{
	ModestEmailClipboard *clipboard = NULL;

	if (MODEST_IS_MSG_EDIT_WINDOW (window)) {
		modest_msg_edit_window_undo (MODEST_MSG_EDIT_WINDOW (window));
	} else if (MODEST_IS_MAIN_WINDOW (window)) {
		/* Clear clipboard source */
		clipboard = modest_runtime_get_email_clipboard ();
		modest_email_clipboard_clear (clipboard); 		
	}
	else {
		g_return_if_reached ();
	}
}

void
modest_ui_actions_on_redo (GtkAction *action,
			   ModestWindow *window)
{
	if (MODEST_IS_MSG_EDIT_WINDOW (window)) {
		modest_msg_edit_window_redo (MODEST_MSG_EDIT_WINDOW (window));
	}
	else {
		g_return_if_reached ();
	}
}


static void
paste_msgs_cb (const GObject *object, gpointer user_data)
{
	g_return_if_fail (MODEST_IS_MAIN_WINDOW (object));
	g_return_if_fail (GTK_IS_WIDGET (user_data));
	
	/* destroy information note */
	gtk_widget_destroy (GTK_WIDGET(user_data));
}

static void
paste_as_attachment_free (gpointer data)
{
	PasteAsAttachmentHelper *helper = (PasteAsAttachmentHelper *) data;

	gtk_widget_destroy (helper->banner);
	g_object_unref (helper->banner);
	g_free (helper);
}

static void
paste_msg_as_attachment_cb (ModestMailOperation *mail_op,
			    TnyHeader *header,
			    TnyMsg *msg,
			    gpointer userdata)
{
	PasteAsAttachmentHelper *helper = (PasteAsAttachmentHelper *) userdata;
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (helper->window));

	if (msg == NULL)
		return;

	modest_msg_edit_window_add_part (MODEST_MSG_EDIT_WINDOW (helper->window), TNY_MIME_PART (msg));
	
}

void
modest_ui_actions_on_paste (GtkAction *action,
			    ModestWindow *window)
{
	GtkWidget *focused_widget = NULL;
	GtkWidget *inf_note = NULL;
	ModestMailOperation *mail_op = NULL;

	focused_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (GTK_IS_EDITABLE (focused_widget)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE(focused_widget));
	} else if (GTK_IS_TEXT_VIEW (focused_widget)) {
		ModestEmailClipboard *e_clipboard = NULL;
		e_clipboard = modest_runtime_get_email_clipboard ();
		if (modest_email_clipboard_cleared (e_clipboard)) {
			GtkTextBuffer *buffer;
			GtkClipboard *clipboard;

			clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (focused_widget));
			gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
		} else if (MODEST_IS_MSG_EDIT_WINDOW (window)) {
			ModestMailOperation *mail_op;
			TnyFolder *src_folder;
			TnyList *data;
			gboolean delete;
			PasteAsAttachmentHelper *helper = g_new0 (PasteAsAttachmentHelper, 1);
			helper->window = MODEST_MSG_EDIT_WINDOW (window);
			helper->banner = modest_platform_animation_banner (GTK_WIDGET (window), NULL,
									   _CS("ckct_nw_pasting"));
			modest_email_clipboard_get_data (e_clipboard, &src_folder, &data, &delete);
			mail_op = modest_mail_operation_new (MODEST_MAIL_OPERATION_TYPE_RECEIVE, 
							     G_OBJECT (window));
			if (helper->banner != NULL) {
				g_object_ref (G_OBJECT (helper->banner));
				gtk_window_set_modal (GTK_WINDOW (helper->banner), FALSE);
				gtk_widget_show (GTK_WIDGET (helper->banner));
			}

			if (data != NULL) {
				modest_mail_operation_get_msgs_full (mail_op, 
								     data,
								     (GetMsgAsyncUserCallback) paste_msg_as_attachment_cb,
								     helper,
								     paste_as_attachment_free);
			}
		}
	} else if (MODEST_IS_FOLDER_VIEW (focused_widget)) {
		ModestEmailClipboard *clipboard = NULL;
		TnyFolder *src_folder = NULL;
		TnyFolderStore *folder_store = NULL;
		TnyList *data = NULL;		
		gboolean delete = FALSE;
		
		/* Check clipboard source */
		clipboard = modest_runtime_get_email_clipboard ();
		if (modest_email_clipboard_cleared (clipboard)) 
			return;
		
		/* Get elements to paste */
		modest_email_clipboard_get_data (clipboard, &src_folder, &data, &delete);

		/* Create a new mail operation */
		mail_op = modest_mail_operation_new (MODEST_MAIL_OPERATION_TYPE_RECEIVE, G_OBJECT(window));
		
		/* Get destination folder */
		folder_store = modest_folder_view_get_selected (MODEST_FOLDER_VIEW (focused_widget));

		/* transfer messages  */
		if (data != NULL) {
			gint response = 0;

			/* Ask for user confirmation */
			response = msgs_move_to_confirmation (GTK_WINDOW (window), 
							      TNY_FOLDER (folder_store), 
							      delete,
							      data);
			
			if (response == GTK_RESPONSE_OK) {
				/* Launch notification */
				inf_note = modest_platform_animation_banner (GTK_WIDGET (window), NULL, 
									     _CS("ckct_nw_pasting"));
				if (inf_note != NULL)  {
					gtk_window_set_modal (GTK_WINDOW(inf_note), FALSE);
					gtk_widget_show (GTK_WIDGET(inf_note));
				}

				modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_op);
				modest_mail_operation_xfer_msgs (mail_op, 
								 data,
								 TNY_FOLDER (folder_store),
								 delete,
								 paste_msgs_cb,
								 inf_note);				
			} else {
				g_object_unref (mail_op);
			}
			
		} else if (src_folder != NULL) {			
			/* Launch notification */
			inf_note = modest_platform_animation_banner (GTK_WIDGET (window), NULL, 
								     _CS("ckct_nw_pasting"));
			if (inf_note != NULL)  {
				gtk_window_set_modal (GTK_WINDOW(inf_note), FALSE);
				gtk_widget_show (GTK_WIDGET(inf_note));
			}
			
			modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_op);
			modest_mail_operation_xfer_folder (mail_op, 
							   src_folder,
							   folder_store,
							   delete,
							   paste_msgs_cb,
							   inf_note);
		}

		/* Free */
		if (data != NULL) 
			g_object_unref (data);
		if (src_folder != NULL) 
			g_object_unref (src_folder);
		if (folder_store != NULL) 
			g_object_unref (folder_store);
	}
}


void
modest_ui_actions_on_select_all (GtkAction *action,
				 ModestWindow *window)
{
	GtkWidget *focused_widget;

	focused_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (MODEST_IS_ATTACHMENTS_VIEW (focused_widget)) {
		modest_attachments_view_select_all (MODEST_ATTACHMENTS_VIEW (focused_widget));
	} else if (GTK_IS_LABEL (focused_widget)) {
		gtk_label_select_region (GTK_LABEL (focused_widget), 0, -1);
	} else if (GTK_IS_EDITABLE (focused_widget)) {
		gtk_editable_select_region (GTK_EDITABLE(focused_widget), 0, -1);
	} else if (GTK_IS_TEXT_VIEW (focused_widget)) {
		GtkTextBuffer *buffer;
		GtkTextIter start, end;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (focused_widget));
		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_select_range (buffer, &start, &end);
	} else if (GTK_IS_HTML (focused_widget)) {
		gtk_html_select_all (GTK_HTML (focused_widget));
	} else if (MODEST_IS_MAIN_WINDOW (window)) {
		GtkWidget *header_view = focused_widget;
 		GtkTreeSelection *selection = NULL;
		
		if (!(MODEST_IS_HEADER_VIEW (focused_widget)))
			header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (window),
									   MODEST_WIDGET_TYPE_HEADER_VIEW);
				
		/* Select all messages */
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(header_view));
		gtk_tree_selection_select_all (selection);

		/* Set focuse on header view */
		gtk_widget_grab_focus (header_view);
	}

}

void
modest_ui_actions_on_mark_as_read (GtkAction *action,
				   ModestWindow *window)
{	
	g_return_if_fail (MODEST_IS_WINDOW(window));
		
	/* Mark each header as read */
	do_headers_action (window, headers_action_mark_as_read, NULL);
}

void
modest_ui_actions_on_mark_as_unread (GtkAction *action,
				     ModestWindow *window)
{	
	g_return_if_fail (MODEST_IS_WINDOW(window));
		
	/* Mark each header as read */
	do_headers_action (window, headers_action_mark_as_unread, NULL);
}

void
modest_ui_actions_on_change_zoom (GtkRadioAction *action,
				  GtkRadioAction *selected,
				  ModestWindow *window)
{
	gint value;

	value = gtk_radio_action_get_current_value (selected);
	if (MODEST_IS_WINDOW (window)) {
		modest_window_set_zoom (MODEST_WINDOW (window), ((gdouble)value)/100);
	}
}

void     modest_ui_actions_msg_edit_on_change_priority (GtkRadioAction *action,
							GtkRadioAction *selected,
							ModestWindow *window)
{
	TnyHeaderFlags flags;
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));

	flags = gtk_radio_action_get_current_value (selected);
	modest_msg_edit_window_set_priority_flags (MODEST_MSG_EDIT_WINDOW (window), flags);
}

void     modest_ui_actions_msg_edit_on_change_file_format (GtkRadioAction *action,
							   GtkRadioAction *selected,
							   ModestWindow *window)
{
	gint file_format;

	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));

	file_format = gtk_radio_action_get_current_value (selected);
	modest_msg_edit_window_set_file_format (MODEST_MSG_EDIT_WINDOW (window), file_format);
}


void     
modest_ui_actions_on_zoom_plus (GtkAction *action,
				ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	modest_window_zoom_plus (MODEST_WINDOW (window));
}

void     
modest_ui_actions_on_zoom_minus (GtkAction *action,
				 ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	modest_window_zoom_minus (MODEST_WINDOW (window));
}

void     
modest_ui_actions_on_toggle_fullscreen    (GtkToggleAction *toggle,
					   ModestWindow *window)
{
	ModestWindowMgr *mgr;
	gboolean fullscreen, active;
	g_return_if_fail (MODEST_IS_WINDOW (window));

	mgr = modest_runtime_get_window_mgr ();

	active = (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (toggle)))?1:0;
	fullscreen = modest_window_mgr_get_fullscreen_mode (mgr);

	if (active != fullscreen) {
		modest_window_mgr_set_fullscreen_mode (mgr, active);
		gtk_window_present (GTK_WINDOW (window));
	}
}

void
modest_ui_actions_on_change_fullscreen (GtkAction *action,
					ModestWindow *window)
{
	ModestWindowMgr *mgr;
	gboolean fullscreen;

	g_return_if_fail (MODEST_IS_WINDOW (window));

	mgr = modest_runtime_get_window_mgr ();
	fullscreen = modest_window_mgr_get_fullscreen_mode (mgr);
	modest_window_mgr_set_fullscreen_mode (mgr, !fullscreen);

	gtk_window_present (GTK_WINDOW (window));
}

/* 
 * Used by modest_ui_actions_on_details to call do_headers_action 
 */
static void
headers_action_show_details (TnyHeader *header, 
			     ModestWindow *window,
			     gpointer user_data)

{
	GtkWidget *dialog;
	
	/* Create dialog */
	dialog = modest_details_dialog_new_with_header (GTK_WINDOW (window), header);

	/* Run dialog */
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

/*
 * Show the folder details in a ModestDetailsDialog widget
 */
static void
show_folder_details (TnyFolder *folder, 
		     GtkWindow *window)
{
	GtkWidget *dialog;
	
	/* Create dialog */
	dialog = modest_details_dialog_new_with_folder (window, folder);

	/* Run dialog */
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

/*
 * Show the header details in a ModestDetailsDialog widget
 */
void     
modest_ui_actions_on_details (GtkAction *action, 
			      ModestWindow *win)
{
	TnyList * headers_list;
	TnyIterator *iter;
	TnyHeader *header;		

	if (MODEST_IS_MSG_VIEW_WINDOW (win)) {
		TnyMsg *msg;

		msg = modest_msg_view_window_get_message (MODEST_MSG_VIEW_WINDOW (win));
		if (!msg)
			return;
		g_object_unref (msg);		

		headers_list = get_selected_headers (win);
		if (!headers_list)
			return;

		iter = tny_list_create_iterator (headers_list);

		header = TNY_HEADER (tny_iterator_get_current (iter));
		if (header) {
			headers_action_show_details (header, win, NULL);
			g_object_unref (header);
		}

		g_object_unref (iter);
		g_object_unref (headers_list);

	} else if (MODEST_IS_MAIN_WINDOW (win)) {
		GtkWidget *folder_view, *header_view;

		/* Check which widget has the focus */
		folder_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
								    MODEST_WIDGET_TYPE_FOLDER_VIEW);
		if (gtk_widget_is_focus (folder_view)) {
			TnyFolderStore *folder_store
				= modest_folder_view_get_selected (MODEST_FOLDER_VIEW (folder_view));
			if (!folder_store) {
				g_warning ("%s: No item was selected.\n", __FUNCTION__);
				return;	
			}
			/* Show only when it's a folder */
			/* This function should not be called for account items, 
			 * because we dim the menu item for them. */
			if (TNY_IS_FOLDER (folder_store)) {
				show_folder_details (TNY_FOLDER (folder_store), GTK_WINDOW (win));
			}

			g_object_unref (folder_store);

		} else {
			header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
									   MODEST_WIDGET_TYPE_HEADER_VIEW);
			/* Show details of each header */
			do_headers_action (win, headers_action_show_details, header_view);
		}
	}
}

void     
modest_ui_actions_on_toggle_show_cc (GtkToggleAction *toggle,
				     ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));

	modest_msg_edit_window_show_cc (window, gtk_toggle_action_get_active (toggle));
}

void     
modest_ui_actions_on_toggle_show_bcc (GtkToggleAction *toggle,
				      ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));

	modest_msg_edit_window_show_bcc (window, gtk_toggle_action_get_active (toggle));
}

void
modest_ui_actions_toggle_folders_view (GtkAction *action, 
				       ModestMainWindow *main_window)
{
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(main_window));

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
		modest_main_window_set_style (main_window, MODEST_MAIN_WINDOW_STYLE_SPLIT);
	else
		modest_main_window_set_style (main_window, MODEST_MAIN_WINDOW_STYLE_SIMPLE);
}

void 
modest_ui_actions_on_toggle_toolbar (GtkToggleAction *toggle, 
				     ModestWindow *window)
{
	gboolean active, fullscreen = FALSE;
	ModestWindowMgr *mgr;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (toggle));

	/* Check if we want to toggle the toolbar vuew in fullscreen
	   or normal mode */
	if (!strcmp (gtk_action_get_name (GTK_ACTION (toggle)), 
		     "ViewShowToolbarFullScreen")) {
		fullscreen = TRUE;
	}

	/* Toggle toolbar */
	mgr = modest_runtime_get_window_mgr ();
	modest_window_mgr_show_toolbars (mgr, active, fullscreen);
}

void     
modest_ui_actions_msg_edit_on_select_font (GtkAction *action,
					   ModestMsgEditWindow *window)
{
	modest_msg_edit_window_select_font (window);
}

void
modest_ui_actions_on_folder_display_name_changed (ModestFolderView *folder_view,
						  const gchar *display_name,
						  GtkWindow *window)
{
	/* Do not change the application name if the widget has not
	   the focus. This callback could be called even if the folder
	   view has not the focus, because the handled signal could be
	   emitted when the folder view is redrawn */
	if (gtk_widget_is_focus (GTK_WIDGET (folder_view))) {
		if (display_name)
			gtk_window_set_title (window, display_name);
		else
			gtk_window_set_title (window, " ");
	}
}

void
modest_ui_actions_on_select_contacts (GtkAction *action, ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	modest_msg_edit_window_select_contacts (window);
}

void
modest_ui_actions_on_check_names (GtkAction *action, ModestMsgEditWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));
	modest_msg_edit_window_check_names (window, FALSE);
}

static void
create_move_to_dialog_on_new_folder(GtkWidget *button, gpointer user_data)
{
	modest_ui_actions_create_folder (gtk_widget_get_toplevel (button),
	                                 GTK_WIDGET (user_data));
}

static GtkWidget*
create_move_to_dialog (GtkWindow *win,
		       GtkWidget *folder_view,
		       GtkWidget **tree_view)
{
	GtkWidget *dialog, *scroll;
	GtkWidget *new_button;

	dialog = gtk_dialog_new_with_buttons (_("mcen_ti_moveto_folders_title"),
					      GTK_WINDOW (win),
					      GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT,
	                                      NULL);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	/* We do this manually so GTK+ does not associate a response ID for
	 * the button. */
	new_button = gtk_button_new_from_stock (GTK_STOCK_NEW);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->action_area), new_button, FALSE, FALSE, 0);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

	/* Create scrolled window */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy  (GTK_SCROLLED_WINDOW (scroll),
					 GTK_POLICY_AUTOMATIC,
					 GTK_POLICY_AUTOMATIC);

	/* Create folder view */
	*tree_view = modest_platform_create_folder_view (NULL);

	g_signal_connect (G_OBJECT (new_button), "clicked", G_CALLBACK(create_move_to_dialog_on_new_folder), *tree_view);

	/* It could happen that we're trying to move a message from a
	   window (msg window for example) after the main window was
	   closed, so we can not just get the model of the folder
	   view */
	if (MODEST_IS_FOLDER_VIEW (folder_view))
		gtk_tree_view_set_model (GTK_TREE_VIEW (*tree_view),
					 gtk_tree_view_get_model (GTK_TREE_VIEW (folder_view)));
	else
		modest_folder_view_update_model (MODEST_FOLDER_VIEW (*tree_view), 
						 TNY_ACCOUNT_STORE (modest_runtime_get_account_store ()));

	modest_folder_view_show_non_move_folders (MODEST_FOLDER_VIEW (*tree_view), FALSE);
	
	gtk_container_add (GTK_CONTAINER (scroll), *tree_view);

	/* Add scroll to dialog */
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), 
			    scroll, TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_WIDGET(GTK_DIALOG(dialog)->vbox));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 300);

	return dialog;
}

/*
 * Returns TRUE if at least one of the headers of the list belongs to
 * a message that has been fully retrieved.
 */
static gboolean
has_retrieved_msgs (TnyList *list)
{
	TnyIterator *iter;
	gboolean found = FALSE;

	iter = tny_list_create_iterator (list);
	while (!tny_iterator_is_done (iter) && !found) {
		TnyHeader *header;
		TnyHeaderFlags flags = 0;

		header = TNY_HEADER (tny_iterator_get_current (iter));
		if (header) {
			flags = tny_header_get_flags (header);
			if (flags & TNY_HEADER_FLAG_CACHED)
/* 			if (!(flags & TNY_HEADER_FLAG_PARTIAL)) */
				found = TRUE;

			g_object_unref (header);
		}

		if (!found)
			tny_iterator_next (iter);
	}
	g_object_unref (iter);

	return found;
}

/*
 * Shows a confirmation dialog to the user when we're moving messages
 * from a remote server to the local storage. Returns the dialog
 * response. If it's other kind of movement the it always returns
 * GTK_RESPONSE_OK
 */
static gint
msgs_move_to_confirmation (GtkWindow *win,
			   TnyFolder *dest_folder,
			   gboolean delete,
			   TnyList *headers)
{
	gint response = GTK_RESPONSE_OK;

	/* If the destination is a local folder (or MMC folder )*/
	if (!modest_tny_folder_is_remote_folder (dest_folder)) {
/* 	if (modest_tny_folder_is_local_folder (dest_folder)) { */
		TnyFolder *src_folder = NULL;
		TnyIterator *iter = NULL;
		TnyHeader *header = NULL;

		/* Get source folder */
		iter = tny_list_create_iterator (headers);
		header = TNY_HEADER (tny_iterator_get_current (iter));
		if (header) {
			src_folder = tny_header_get_folder (header);
			g_object_unref (header);
		}

		g_object_unref (iter);

		/* if no src_folder, message may be an attahcment */
		if (src_folder == NULL) 
			return GTK_RESPONSE_CANCEL;

		/* If the source is a remote folder */
/* 		if (!modest_tny_folder_is_local_folder (src_folder)) { */
		if (modest_tny_folder_is_remote_folder (src_folder)) {
			const gchar *message = NULL;
			gboolean cached = has_retrieved_msgs (headers);
			if (cached) 
				message = ngettext ("mcen_nc_move_retrieve", "mcen_nc_move_retrieves",
						    tny_list_get_length (headers));
			else 
				message = ngettext ("mcen_nc_move_header", "mcen_nc_move_headers",
						    tny_list_get_length (headers));
			
			if (cached && !delete)	
				response = GTK_RESPONSE_OK;
			else
				response = modest_platform_run_confirmation_dialog (GTK_WINDOW (win),
										    (const gchar *) message);
		}
		
		g_object_unref (src_folder);
	}
	
	return response;
}



static void
transfer_msgs_from_viewer_cb (const GObject *object, gpointer user_data)
{
	ModestMsgViewWindow *self = NULL;

	g_return_if_fail (MODEST_IS_MSG_VIEW_WINDOW (object));
	self = MODEST_MSG_VIEW_WINDOW (object);
	
	if (!modest_msg_view_window_select_next_message (self))
		if (!modest_msg_view_window_select_previous_message (self))
			/* No more messages to view, so close this window */
			modest_ui_actions_on_close_window (NULL, MODEST_WINDOW(self));
}

void
modest_ui_actions_move_folder_error_handler (ModestMailOperation *mail_op, 
					     gpointer user_data)
{
	GObject *win = modest_mail_operation_get_source (mail_op);
	const GError *error = NULL;
	const gchar *message = NULL;
	
	/* Get error message */
	error = modest_mail_operation_get_error (mail_op);
	if (error != NULL && error->message != NULL) {
		message = error->message;
	} else {
		message = _("mail_in_ui_folder_move_target_error");
	}
	
	/* Show notification dialog */
	modest_platform_run_information_dialog ((win) ? GTK_WINDOW (win) : NULL, message);
	g_object_unref (win);
}

void
modest_ui_actions_send_receive_error_handler (ModestMailOperation *mail_op, 
					      gpointer user_data)
{
	GObject *win = modest_mail_operation_get_source (mail_op);
	const GError *error = modest_mail_operation_get_error (mail_op);

	g_return_if_fail (error != NULL);
	if (error->message != NULL)		
		g_printerr ("modest: %s\n", error->message);
	else
		g_printerr ("modest: unkonw error on send&receive operation");

	/* Show error message */
/* 	if (modest_mail_operation_get_id (mail_op) == MODEST_MAIL_OPERATION_TYPE_RECEIVE) */
/* 		modest_platform_run_information_dialog ((win) ? GTK_WINDOW (win) : NULL, */
/* 							_CS("sfil_ib_unable_to_receive")); */
/* 	else  */
/* 		modest_platform_run_information_dialog ((win) ? GTK_WINDOW (win) : NULL, */
/* 							_CS("sfil_ib_unable_to_send")); */
	g_object_unref (win);
}

static void
open_msg_for_purge_cb (ModestMailOperation *mail_op, 
		       TnyHeader *header, 
		       TnyMsg *msg, 
		       gpointer user_data)
{
	TnyList *parts;
	TnyIterator *iter;
	gint pending_purges = 0;
	gboolean some_purged = FALSE;
	ModestWindow *win = MODEST_WINDOW (user_data);
	ModestWindowMgr *mgr = modest_runtime_get_window_mgr ();

	/* If there was any error */
	if (!modest_ui_actions_msg_retrieval_check (mail_op, header, msg)) {
		modest_window_mgr_unregister_header (mgr, header);
		return;
	}

	/* Once the message has been retrieved for purging, we check if
	 * it's all ok for purging */

	parts = tny_simple_list_new ();
	tny_mime_part_get_parts (TNY_MIME_PART (msg), parts);
	iter = tny_list_create_iterator (parts);

	while (!tny_iterator_is_done (iter)) {
		TnyMimePart *part;
		part = TNY_MIME_PART (tny_iterator_get_current (iter));
		if (part && (tny_mime_part_is_attachment (part) || TNY_IS_MSG (part))) {
			if (tny_mime_part_is_purged (part))
				some_purged = TRUE;
			else
				pending_purges++;
		}

		if (part)
			g_object_unref (part);

		tny_iterator_next (iter);
	}

	if (pending_purges>0) {
		gint response;
		response = modest_platform_run_confirmation_dialog (GTK_WINDOW (win),_("mcen_nc_purge_file_text_inbox"));

		if (response == GTK_RESPONSE_OK) {
			modest_platform_information_banner (NULL, NULL, _("mcen_ib_removing_attachment"));
			tny_iterator_first (iter);
			while (!tny_iterator_is_done (iter)) {
				TnyMimePart *part;
				
				part = TNY_MIME_PART (tny_iterator_get_current (iter));
				if (part && (tny_mime_part_is_attachment (part) || TNY_IS_MSG (part)))
					tny_mime_part_set_purged (part);

				if (part)
					g_object_unref (part);

				tny_iterator_next (iter);
			}
			
			tny_msg_rewrite_cache (msg);
		}
	} else {
		modest_platform_information_banner (NULL, NULL, _("mail_ib_attachment_already_purged"));
	}

	/* remove attachments */
	tny_iterator_first (iter);
	while (!tny_iterator_is_done (iter)) {
		TnyMimePart *part;
			
		part = TNY_MIME_PART (tny_iterator_get_current (iter));
		if (part) {
			/* One for the reference given by tny_iterator_get_current(): */
			g_object_unref (part);

			/* TODO: Is this meant to remove the attachment by doing another unref()? 
			 * Otherwise, this seems useless. */
		}

		tny_iterator_next (iter);
	}
	modest_window_mgr_unregister_header (mgr, header);

	g_object_unref (iter);
	g_object_unref (parts);
}

static void
modest_ui_actions_on_main_window_remove_attachments (GtkAction *action,
						     ModestMainWindow *win)
{
	GtkWidget *header_view;
	TnyList *header_list;
	TnyIterator *iter;
	TnyHeader *header;
	TnyHeaderFlags flags;
	ModestWindow *msg_view_window =  NULL;
	gboolean found;

	g_return_if_fail (MODEST_IS_MAIN_WINDOW (win));

	header_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
							   MODEST_WIDGET_TYPE_HEADER_VIEW);

	header_list = modest_header_view_get_selected_headers (MODEST_HEADER_VIEW (header_view));

	if (tny_list_get_length (header_list) == 1) {
		iter = tny_list_create_iterator (header_list);
		header = TNY_HEADER (tny_iterator_get_current (iter));
		g_object_unref (iter);
	} else {
		return;
	}

	found = modest_window_mgr_find_registered_header (modest_runtime_get_window_mgr (),
							  header, &msg_view_window);
	flags = tny_header_get_flags (header);
	if (!(flags & TNY_HEADER_FLAG_CACHED))
		return;
	if (found) {
		if (msg_view_window != NULL) 
			modest_msg_view_window_remove_attachments (MODEST_MSG_VIEW_WINDOW (msg_view_window), TRUE);
		else {
			/* do nothing; uid was registered before, so window is probably on it's way */
			g_warning ("debug: header %p has already been registered", header);
		}
	} else {
		ModestMailOperation *mail_op = NULL;
		modest_window_mgr_register_header (modest_runtime_get_window_mgr (), header);
		mail_op = modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_RECEIVE,
									 G_OBJECT (win),
									 modest_ui_actions_get_msgs_full_error_handler,
									 NULL);
		modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_op);
		modest_mail_operation_get_msg (mail_op, header, open_msg_for_purge_cb, win);
		
		g_object_unref (mail_op);
	}
	if (header)
		g_object_unref (header);
	if (header_list)
		g_object_unref (header_list);
}

/**
 * Utility function that transfer messages from both the main window
 * and the msg view window when using the "Move to" dialog
 */
static void
modest_ui_actions_xfer_messages_from_move_to (TnyFolderStore *dst_folder,
					      ModestWindow *win)
{
	TnyList *headers = NULL;
	gint response = 0;
	TnyAccount *dst_account = NULL;
	const gchar *proto_str = NULL;
	gboolean dst_is_pop = FALSE;

	if (!TNY_IS_FOLDER (dst_folder)) {
		modest_platform_information_banner (GTK_WIDGET (win),
						    NULL,
						    _CS("ckdg_ib_unable_to_move_to_current_location"));
		return;
	}

	dst_account = tny_folder_get_account (TNY_FOLDER (dst_folder));
	proto_str = tny_account_get_proto (dst_account);
	dst_is_pop = (modest_protocol_info_get_transport_store_protocol (proto_str) == 
		      MODEST_PROTOCOL_STORE_POP);
	g_object_unref (dst_account);

	/* Get selected headers */
	headers = get_selected_headers (MODEST_WINDOW (win));

	if (dst_is_pop) {
		modest_platform_information_banner (GTK_WIDGET (win),
						    NULL,
						    ngettext("mail_in_ui_folder_move_target_error",
							     "mail_in_ui_folder_move_targets_error",
							     tny_list_get_length (headers)));
		g_object_unref (headers);
		return;
	}

	/* Ask for user confirmation */
	response = msgs_move_to_confirmation (GTK_WINDOW (win), 
					      TNY_FOLDER (dst_folder), 
					      TRUE,
					      headers);

	/* Transfer messages */
	if (response == GTK_RESPONSE_OK) {
		ModestMailOperation *mail_op = 
			modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_RECEIVE, 
								       G_OBJECT(win),
								       modest_ui_actions_move_folder_error_handler,
								       NULL);
		modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), 
						 mail_op);

		modest_mail_operation_xfer_msgs (mail_op, 
						 headers,
						 TNY_FOLDER (dst_folder),
						 TRUE,
						 (MODEST_IS_MSG_VIEW_WINDOW (win)) ? transfer_msgs_from_viewer_cb : NULL,
						 NULL);

		g_object_unref (G_OBJECT (mail_op));
	}
	g_object_unref (headers);
}


/*
 * UI handler for the "Move to" action when invoked from the
 * ModestMainWindow
 */
static void 
modest_ui_actions_on_main_window_move_to (GtkAction *action, 
					  GtkWidget *folder_view,
					  TnyFolderStore *dst_folder,
					  ModestMainWindow *win)
{
	GtkWidget *header_view = NULL;
	ModestMailOperation *mail_op = NULL;
	TnyFolderStore *src_folder;

	g_return_if_fail (MODEST_IS_MAIN_WINDOW (win));

	/* Get the source folder */
	src_folder = modest_folder_view_get_selected (MODEST_FOLDER_VIEW (folder_view));
	
	/* Offer the connection dialog if necessary, if the source folder is in a networked account: */
	if (!modest_platform_connect_and_wait_if_network_folderstore (GTK_WINDOW (win), 
								      src_folder))
		goto end;

	/* Get header view */
	header_view = 
		modest_main_window_get_child_widget (win, MODEST_WIDGET_TYPE_HEADER_VIEW);

	/* Get folder or messages to transfer */
	if (gtk_widget_is_focus (folder_view)) {

		/* Allow only to transfer folders to the local root folder */
		if (TNY_IS_ACCOUNT (dst_folder) && 
		    !MODEST_IS_TNY_LOCAL_FOLDERS_ACCOUNT (dst_folder))
			goto end;
		
		/* Clean folder on header view before moving it */
		modest_header_view_clear (MODEST_HEADER_VIEW (header_view)); 

		if (TNY_IS_FOLDER (src_folder)) {
			mail_op = 
				modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_RECEIVE, 
									       G_OBJECT(win),
									       modest_ui_actions_move_folder_error_handler,
									       NULL);
			modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), 
							 mail_op);

			modest_mail_operation_xfer_folder (mail_op, 
							   TNY_FOLDER (src_folder),
							   dst_folder,
							   TRUE, NULL, NULL);
			/* Unref mail operation */
			g_object_unref (G_OBJECT (mail_op));
		} else {
			g_warning ("%s: src_folder is not a TnyFolder.\n", __FUNCTION__);	
		}
	} else if (gtk_widget_is_focus (header_view)) {
		/* Transfer messages */
		modest_ui_actions_xfer_messages_from_move_to (dst_folder, MODEST_WINDOW (win));
	}
	
 end:
    if (src_folder)
    	g_object_unref (src_folder);
}


/*
 * UI handler for the "Move to" action when invoked from the
 * ModestMsgViewWindow
 */
static void 
modest_ui_actions_on_msg_view_window_move_to (GtkAction *action, 
					      TnyFolderStore *dst_folder,
					      ModestMsgViewWindow *win)
{
	TnyHeader *header = NULL;
	TnyFolder *src_folder;

	/* Create header list */
	header = modest_msg_view_window_get_header (MODEST_MSG_VIEW_WINDOW (win));		
	src_folder = tny_header_get_folder(header);
	g_object_unref (header);

	/* Transfer the message */
	if (modest_platform_connect_and_wait_if_network_folderstore (NULL, TNY_FOLDER_STORE (src_folder)))
		modest_ui_actions_xfer_messages_from_move_to (dst_folder, MODEST_WINDOW (win));

	g_object_unref (src_folder);
}

void 
modest_ui_actions_on_move_to (GtkAction *action, 
			      ModestWindow *win)
{
	GtkWidget *dialog = NULL, *folder_view = NULL, *tree_view = NULL;
	gint result = 0;
	TnyFolderStore *dst_folder = NULL;
	ModestMainWindow *main_window;

	g_return_if_fail (MODEST_IS_MAIN_WINDOW (win) ||
			  MODEST_IS_MSG_VIEW_WINDOW (win));

	/* Get the main window if exists */
	if (MODEST_IS_MAIN_WINDOW (win))
		main_window = MODEST_MAIN_WINDOW (win);
	else
		main_window = 
			MODEST_MAIN_WINDOW (modest_window_mgr_get_main_window (modest_runtime_get_window_mgr ()));

	/* Get the folder view widget if exists */
	if (main_window)
		folder_view = modest_main_window_get_child_widget (main_window,
								   MODEST_WIDGET_TYPE_FOLDER_VIEW);
	else
		folder_view = NULL;

	/* Create and run the dialog */
	dialog = create_move_to_dialog (GTK_WINDOW (win), folder_view, &tree_view);
	modest_folder_view_select_first_inbox_or_local (MODEST_FOLDER_VIEW (tree_view));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	result = gtk_dialog_run (GTK_DIALOG(dialog));
	g_object_ref (tree_view);
	gtk_widget_destroy (dialog);

	if (result != GTK_RESPONSE_ACCEPT)
		return;

	dst_folder = modest_folder_view_get_selected (MODEST_FOLDER_VIEW (tree_view));
	/* Offer the connection dialog if necessary: */
	if (modest_platform_connect_and_wait_if_network_folderstore (GTK_WINDOW (win), 
								      dst_folder)) {

		/* Do window specific stuff */
		if (MODEST_IS_MAIN_WINDOW (win))
			modest_ui_actions_on_main_window_move_to (action,
								  folder_view,
								  dst_folder,
								  MODEST_MAIN_WINDOW (win));
		else
			modest_ui_actions_on_msg_view_window_move_to (action,
								      dst_folder,
								      MODEST_MSG_VIEW_WINDOW (win));
	}
	if (dst_folder)
		g_object_unref (dst_folder);
}

/*
 * Calls #HeadersFunc for each header already selected in the main
 * window or the message currently being shown in the msg view window
 */
static void
do_headers_action (ModestWindow *win, 
		   HeadersFunc func,
		   gpointer user_data)
{
	TnyList *headers_list = NULL;
	TnyIterator *iter = NULL;
	TnyHeader *header = NULL;
	TnyFolder *folder = NULL;

	/* Get headers */
	headers_list = get_selected_headers (win);
	if (!headers_list)
		return;

	/* Get the folder */
	iter = tny_list_create_iterator (headers_list);
	header = TNY_HEADER (tny_iterator_get_current (iter));
	if (header) {
		folder = tny_header_get_folder (header);
		g_object_unref (header);
	}

	/* Call the function for each header */
	while (!tny_iterator_is_done (iter)) {
		header = TNY_HEADER (tny_iterator_get_current (iter));
		func (header, win, user_data);
		g_object_unref (header);
		tny_iterator_next (iter);
	}

	/* Trick: do a poke status in order to speed up the signaling
	   of observers */
	tny_folder_poke_status (folder);

	/* Frees */
	g_object_unref (folder);
	g_object_unref (iter);
	g_object_unref (headers_list);
}

void 
modest_ui_actions_view_attachment (GtkAction *action,
				   ModestWindow *window)
{
	if (MODEST_IS_MSG_VIEW_WINDOW (window)) {
		modest_msg_view_window_view_attachment (MODEST_MSG_VIEW_WINDOW (window), NULL);
	} else {
		/* not supported window for this action */
		g_return_if_reached ();
	}
}

void
modest_ui_actions_save_attachments (GtkAction *action,
				    ModestWindow *window)
{
	if (MODEST_IS_MSG_VIEW_WINDOW (window)) {
		modest_msg_view_window_save_attachments (MODEST_MSG_VIEW_WINDOW (window), NULL);
	} else {
		/* not supported window for this action */
		g_return_if_reached ();
	}
}

void
modest_ui_actions_remove_attachments (GtkAction *action,
				      ModestWindow *window)
{
	if (MODEST_IS_MAIN_WINDOW (window)) {
		modest_ui_actions_on_main_window_remove_attachments (action, MODEST_MAIN_WINDOW (window));
	} else if (MODEST_IS_MSG_VIEW_WINDOW (window)) {
		modest_msg_view_window_remove_attachments (MODEST_MSG_VIEW_WINDOW (window), FALSE);
	} else {
		/* not supported window for this action */
		g_return_if_reached ();
	}
}

void 
modest_ui_actions_on_settings (GtkAction *action, 
			       ModestWindow *win)
{
	GtkWidget *dialog;

	dialog = modest_platform_get_global_settings_dialog ();
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (win));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show_all (dialog);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

void 
modest_ui_actions_on_help (GtkAction *action, 
			   ModestWindow *win)
{
	const gchar *help_id = NULL;

	if (MODEST_IS_MAIN_WINDOW (win)) {
		const gchar *action_name;
		action_name = gtk_action_get_name (action);

		if (!strcmp (action_name, "FolderViewCSMHelp") ||
		    !strcmp (action_name, "HeaderViewCSMHelp")) {
			GtkWidget *folder_view;
			TnyFolderStore *folder_store;
			/* Get selected folder */
			folder_view = modest_main_window_get_child_widget (MODEST_MAIN_WINDOW (win),
									   MODEST_WIDGET_TYPE_FOLDER_VIEW);
			folder_store = modest_folder_view_get_selected (MODEST_FOLDER_VIEW (folder_view));

			/* Switch help_id */
			if (TNY_IS_FOLDER (folder_store)) {
				switch (modest_tny_folder_guess_folder_type (TNY_FOLDER (folder_store))) {
				case TNY_FOLDER_TYPE_NORMAL:
					help_id = "applications_email_managefolders";
					break;
				case TNY_FOLDER_TYPE_INBOX:
					help_id = "applications_email_inbox";
					break;
				case TNY_FOLDER_TYPE_OUTBOX:
					help_id = "applications_email_outbox";
					break;
				case TNY_FOLDER_TYPE_SENT:
					help_id = "applications_email_sent";
					break;
				case TNY_FOLDER_TYPE_DRAFTS:
					help_id = "applications_email_drafts";
					break;
				case TNY_FOLDER_TYPE_ARCHIVE:
					help_id = "applications_email_managefolders";
					break;
				default:
					help_id = "applications_email_managefolders";
				}
			} else {
				help_id = "applications_email_mainview";	
			}
			g_object_unref (folder_store);
		} else {
			help_id = "applications_email_mainview";	
		}
	} else if (MODEST_IS_MSG_VIEW_WINDOW (win)) {
		help_id = "applications_email_viewer";
	} else if (MODEST_IS_MSG_EDIT_WINDOW (win))
		help_id = "applications_email_editor";

	modest_platform_show_help (GTK_WINDOW (win), help_id);
}

void 
modest_ui_actions_on_retrieve_msg_contents (GtkAction *action,
					    ModestWindow *window)
{
	ModestMailOperation *mail_op;
	TnyList *headers;

	/* Get headers */
	headers = get_selected_headers (window);
	if (!headers)
		return;

	/* Create mail operation */
	mail_op = modest_mail_operation_new_with_error_handling (MODEST_MAIL_OPERATION_TYPE_RECEIVE, 
								 G_OBJECT (window),
								 modest_ui_actions_get_msgs_full_error_handler, 
								 NULL);
	modest_mail_operation_queue_add (modest_runtime_get_mail_operation_queue (), mail_op);
	modest_mail_operation_get_msgs_full (mail_op, headers, NULL, NULL, NULL);

	/* Frees */
	g_object_unref (headers);
	g_object_unref (mail_op);
}

void
modest_ui_actions_on_email_menu_activated (GtkAction *action,
					  ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));
	
	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_edit_menu_activated (GtkAction *action,
					  ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_view_menu_activated (GtkAction *action,
					  ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_tools_menu_activated (GtkAction *action,
					  ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_attachment_menu_activated (GtkAction *action,
					  ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_toolbar_csm_menu_activated (GtkAction *action,
						 ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_folder_view_csm_menu_activated (GtkAction *action,
						     ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_on_header_view_csm_menu_activated (GtkAction *action,
						     ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestMenuDimmingRules");	
}

void
modest_ui_actions_check_toolbar_dimming_rules (ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	/* Update dimmed */	
	modest_window_check_dimming_rules_group (window, "ModestToolbarDimmingRules");	
}

void
modest_ui_actions_on_search_messages (GtkAction *action, ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_WINDOW (window));

	modest_platform_show_search_messages (GTK_WINDOW (window));
}

void     
modest_ui_actions_on_open_addressbook (GtkAction *action, ModestWindow *win)
{
	g_return_if_fail (MODEST_IS_WINDOW (win));
	modest_platform_show_addressbook (GTK_WINDOW (win));
}


void
modest_ui_actions_on_toggle_find_in_page (GtkToggleAction *action,
					  ModestWindow *window)
{
	g_return_if_fail (MODEST_IS_MSG_EDIT_WINDOW (window));

	modest_msg_edit_window_toggle_find_toolbar (MODEST_MSG_EDIT_WINDOW (window), gtk_toggle_action_get_active (action));
}

static void 
_on_send_receive_progress_changed (ModestMailOperation  *mail_op, 
				   ModestMailOperationState *state,
				   gpointer user_data)
{
	g_return_if_fail (MODEST_IS_MAIN_WINDOW(user_data));

	/* Set send/receive operation finished */	
	if (state->status != MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS)
		modest_main_window_notify_send_receive_completed (MODEST_MAIN_WINDOW(user_data));
	
}


