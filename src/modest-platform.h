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

#ifndef __MODEST_PLATFORM_H__
#define __MODEST_PLATFORM_H__

#include <tny-device.h>
#include "widgets/modest-window.h"
#include "widgets/modest-folder-view.h"

G_BEGIN_DECLS

typedef enum _ModestConfirmationDialogType {
	MODEST_CONFIRMATION_DELETE_FOLDER,
} ModestConfirmationDialogType;

typedef enum _ModestSortDialogType {
	MODEST_SORT_HEADERS,
} ModestSortDialogType;

/**
 * modest_platform_platform_init:
 *
 * platform specific initialization function
 * 
 * Returns: TRUE if succeeded, FALSE otherwise
 */
gboolean modest_platform_init (void);


/**
 * modest_platform_get_new_device:
 *
 * platform specific initialization function
 * 
 * Returns: TRUE if succeeded, FALSE otherwise
 */
TnyDevice*  modest_platform_get_new_device (void);


/**
 * modest_platform_get_file_icon_name:
 * @name: the name of the file, or NULL
 * @mime_type: the mime-type, or NULL
 * @effective_mime_type: out-param which receives the 'effective mime-type', ie., the mime type
 * that will be used. May be NULL if you're not interested in this. Note: the returned string
 * is newly allocated, and should be g_free'd when done with it.
 *
 * this function gets the icon for the file, based on the file name and/or the mime type,
 * using the following strategy:
 * (1) if mime_type != NULL and mime_type != application/octet-stream, find the
 *     the icon name for this mime type
 * (2) otherwise, guess the icon type from the file name, and then goto (1)
 *
 * Returns: the icon name
 */
gchar*  modest_platform_get_file_icon_name (const gchar* name, const gchar* mime_type,
					    gchar **effective_mime_type);

/**
 * modest_platform_activate_uri:
 * @uri: the uri to activate
 *
 * This function activates an URI
 *
 * Returns: %TRUE if successful, %FALSE if not.
 **/
gboolean modest_platform_activate_uri (const gchar *uri);

/**
 * modest_platform_activate_file:
 * @path: the path to activate
 * @mime_type: the mime type of the path, or %NULL to guess
 *
 * This function activates a file
 *
 * Returns: %TRUE if successful, %FALSE if not.
 **/
gboolean modest_platform_activate_file (const gchar *path, const gchar *mime_type);

/**
 * modest_platform_show_uri_popup:
 * @uri: an URI with the string
 *
 * This function show the popup of actions for an URI
 *
 * Returns: %TRUE if successful, %FALSE if not.
 **/
gboolean modest_platform_show_uri_popup (const gchar *uri);

/**
 * modest_platform_get_icon:
 * @name: the name of the icon
 *
 * this function returns an icon, or NULL in case of error 
 */
GdkPixbuf* modest_platform_get_icon (const gchar *name);


/**
 * modest_platform_get_app_name:
 *
 * this function returns the name of the application. Do not modify.
 */
const gchar* modest_platform_get_app_name (void);


/**
 * modest_platform_run_new_folder_dialog:
 * @parent_window: a #GtkWindow
 * @parent: the parent of the new folder
 * @suggested_name: the suggested name for the new folder
 * @folder_name: the folder name selected by the user for the new folder
 * 
 * runs a "new folder" confirmation dialog. The dialog will suggest a
 * folder name which depends of the platform if the #suggested_name
 * parametter is NULL. If the user input a valid folder name it's
 * returned in the #folder_name attribute.
 * 
 * Returns: the #GtkResponseType returned by the dialog
 **/
gint      modest_platform_run_new_folder_dialog        (GtkWindow *parent_window,
							TnyFolderStore *parent,
							gchar *suggested_name,
							gchar **folder_name);

/**
 * modest_platform_run_confirmation_dialog:
 * @parent_window: the parent #GtkWindow of the dialog
 * @message: the message to show to the user
 * 
 * runs a confirmation dialog
 * 
 * Returns: GTK_RESPONSE_OK or GTK_RESPONSE_CANCEL
 **/
gint      modest_platform_run_confirmation_dialog      (GtkWindow *parent_window,
							const gchar *message);


/**
 * modest_platform_run_information_dialog:
 * @parent_window: the parent #GtkWindow of the dialog
 * @message: the message to show
 * 
 * shows an information dialog
 **/
void      modest_platform_run_information_dialog       (GtkWindow *parent_window,
							const gchar *message);

/**
 * modest_platform_run_sort_dialog:
 * @parent_window: the parent #GtkWindow of the dialog
 * @type: the sort dialog type.
 * 
 * shows a sort dialog
 **/
void      modest_platform_run_sort_dialog       (GtkWindow *parent_window, 
						 ModestSortDialogType type);
		
/*
 * modest_platform_connect_and_wait:
 * @parent_window: the parent #GtkWindow for any interactive or progress feedback UI.
 * @return value: Whether a connection was made.
 * 
 * Attempts to make a connection, possibly showing interactive UI to achieve this.
 * This will return TRUE immediately if a connection is already open.
 * Otherwise, this function blocks until the connection attempt has either succeded or failed.
 */		
gboolean modest_platform_connect_and_wait (GtkWindow *parent_window);

/**
 * modest_platform_set_update_interval:
 * @minutes: The number of minutes between updates, or 0 for no updates.
 * 
 * Set the number of minutes between automatic updates of email accounts.
 * The platform will cause the send/receive action to happen repeatedly.
 **/
gboolean modest_platform_set_update_interval (guint minutes);

/**
 * modest_platform_get_global_settings_dialog:
 * @void: 
 * 
 * returns the global settings dialog
 * 
 * Return value: a new #ModestGlobalSettingsDialog dialog
 **/
GtkWidget* modest_platform_get_global_settings_dialog (void);

void modest_platform_on_new_msg (void);


/**
 * modest_platform_show_help:
 * @parent_window: 
 * @help_id: the help topic id to be shown in the help dialog
 * 
 * shows the application help dialog
 **/
void modest_platform_show_help (GtkWindow *parent_window, 
				const gchar *help_id);

/**
 * modest_platform_show_search_messages:
 * @parent_window: window the dialog will be child of
 *
 * shows the search messages dialog
 **/
void modest_platform_show_search_messages (GtkWindow *parent_window);

/**
 * modest_platform_show_addressbook:
 * @parent_window: window the dialog will be child of
 *
 * shows the addressbook
 **/
void modest_platform_show_addressbook (GtkWindow *parent_window);


GtkWidget* modest_platform_create_folder_view (TnyFolderStoreQuery *query);

G_END_DECLS

#endif /* __MODEST_PLATFORM_UTILS_H__ */
