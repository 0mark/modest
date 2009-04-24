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

#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif /*DBUS_API_SUBJECT_TO_CHANGE*/

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h> /* for strlen */
#include <modest-runtime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <tny-fs-stream.h>
#include <tny-camel-account.h>
#include <tny-status.h>
#include <tny-camel-transport-account.h>
#include <tny-camel-imap-store-account.h>
#include <tny-camel-pop-store-account.h>
#include "modest-hildon-includes.h"

#include <modest-defs.h>
#include "modest-maemo-utils.h"
#include "modest-text-utils.h"
#include "modest-platform.h"
#include "modest-ui-constants.h"
#include <hildon/hildon-picker-dialog.h>

/* Label child of a captioned */
#define CAPTIONED_LABEL_CHILD "captioned-label"


static osso_context_t *__osso_context = NULL; /* urgh global */

osso_context_t *
modest_maemo_utils_get_osso_context (void)
{
	if (!__osso_context) 
		g_warning ("%s: __osso_context == NULL", __FUNCTION__);

	return __osso_context;
}

void
modest_maemo_utils_set_osso_context (osso_context_t *osso_context)
{
	g_return_if_fail (osso_context);
	__osso_context = osso_context;
}

static void
get_properties_cb (DBusPendingCall *call,
		   void *user_data)
{
	DBusMessageIter iter;
	DBusMessageIter dict_iter;
	DBusMessageIter dict_entry_iter;
	DBusError err;
	gchar *bt_name = NULL;
	DBusMessage *message;
	int key_type, array_type, msg_type;

	if ( !dbus_pending_call_get_completed ( call ) )
		g_warning ("%s: Call not completed!", __FUNCTION__);

	message = dbus_pending_call_steal_reply ( call );

	if (message == NULL)
		g_warning ("%s: Message is NULL", __FUNCTION__);


	dbus_error_init(&err);
	if (dbus_set_error_from_message (&err, message)) {
		g_warning ("%s: %s", __FUNCTION__, err.message);
	}


	/* Get msg type */
	dbus_message_iter_init (message, &iter);
	msg_type = dbus_message_iter_get_arg_type (&iter);
	dbus_message_iter_recurse (&iter, &dict_iter);

	while ((array_type = dbus_message_iter_get_arg_type (&dict_iter)) == DBUS_TYPE_DICT_ENTRY) {

		dbus_message_iter_recurse (&dict_iter, &dict_entry_iter);

		while ((key_type = dbus_message_iter_get_arg_type (&dict_entry_iter)) == DBUS_TYPE_STRING) {
			DBusMessageIter dict_entry_content_iter;
			char *key;
			char *value;
			int dict_entry_type;
			int dict_entry_content_type;

			dbus_message_iter_get_basic (&dict_entry_iter, &key);
			dbus_message_iter_next (&dict_entry_iter);
			dict_entry_type = dbus_message_iter_get_arg_type (&dict_entry_iter);
			dbus_message_iter_recurse (&dict_entry_iter, &dict_entry_content_iter);
			dict_entry_content_type = dbus_message_iter_get_arg_type (&dict_entry_content_iter);

			if (dict_entry_content_type == DBUS_TYPE_STRING) {
				dbus_message_iter_get_basic ( &dict_entry_content_iter, &value );

				if (strcmp (key, "Name") == 0 ) {
					g_debug ("-------------Name %s", value);
					bt_name = value;
					break;
				}
			}
			dbus_message_iter_next (&dict_entry_iter);
		}

		if (key_type != DBUS_TYPE_INVALID)
			break;

		dbus_message_iter_next (&dict_iter);
	}

	/* Save device name */
	if (bt_name) {
		g_debug ("Setting the device name %s", bt_name);
		modest_conf_set_string (modest_runtime_get_conf(),
					MODEST_CONF_DEVICE_NAME, bt_name,
					NULL);
	}
}

static void
get_default_adapter_cb (DBusPendingCall *call,
			void *user_data)
{
	DBusMessage *message;
	DBusMessageIter iter;
	gchar* path = NULL;

	message = dbus_pending_call_steal_reply(call);

	// Todo extract msg info from here
	dbus_message_iter_init ( message, &iter );

	dbus_message_iter_get_basic (&iter, &path);
	if (path != NULL) {
		DBusConnection *conn;
		DBusMessage *adapterMsg = dbus_message_new_method_call("org.bluez", path,
								       "org.bluez.Adapter",
								       "GetProperties");
		DBusPendingCall *call = NULL;

		conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);

		if (conn && dbus_connection_send_with_reply(conn, adapterMsg, &call, -1) ) {
			g_debug ("Getting the properties");
			dbus_pending_call_set_notify(call, get_properties_cb, NULL, NULL);
			dbus_pending_call_block(call);
			dbus_pending_call_unref(call);
		}
	} else {
		g_warning ("Failed to get the default bluetooth adapter");
	}
}

void
modest_maemo_utils_get_device_name (void)
{
	static DBusConnection *conn = NULL;
	DBusMessage *request;
	DBusError error;
	DBusPendingCall *call = NULL;

	dbus_error_init (&error);
	if (!conn) {
		conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
		if (!conn) {
			g_printerr ("modest: cannot get on the dbus: %s: %s\n",
				    error.name, error.message);
			dbus_error_free (&error);
			return;
		}
	}

	/* Get the default adapter */
	request = dbus_message_new_method_call("org.bluez", "/" ,
					       "org.bluez.Manager",
					       "DefaultAdapter");

	if (dbus_connection_send_with_reply(conn, request, &call, -1)) {
		g_debug ("Getting the default adapter");
		dbus_pending_call_set_notify(call, get_default_adapter_cb, NULL, NULL);
		dbus_pending_call_block(call);
		dbus_pending_call_unref(call);
	}
}

void
modest_maemo_utils_setup_images_filechooser (GtkFileChooser *chooser)
{
	GtkFileFilter *file_filter;
	GList *image_mimetypes_list;
	GList *node;
	gchar *conf_folder;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

	conf_folder = modest_conf_get_string (modest_runtime_get_conf (), MODEST_CONF_LATEST_INSERT_IMAGE_PATH, NULL);
	if (conf_folder && conf_folder[0] != '\0') {
		gtk_file_chooser_set_current_folder_uri (chooser, conf_folder);
	} else {
		gchar *images_folder;
		/* Set the default folder to images folder */
		images_folder = g_build_filename (g_getenv (MODEST_MAEMO_UTILS_MYDOCS_ENV),
						  MODEST_MAEMO_UTILS_DEFAULT_IMAGE_FOLDER, NULL);
		gtk_file_chooser_set_current_folder (chooser, images_folder);
		g_free (images_folder);
	}
	g_free (conf_folder);

	/* Set the images mime filter */
	file_filter = gtk_file_filter_new ();
#ifdef MODEST_HAVE_HILDON0_WIDGETS
	image_mimetypes_list = osso_mime_get_mime_types_for_category (OSSO_MIME_CATEGORY_IMAGES);
#else
	image_mimetypes_list = hildon_mime_get_mime_types_for_category (HILDON_MIME_CATEGORY_IMAGES);
#endif
	for (node = image_mimetypes_list; node != NULL; node = g_list_next (node)) {
		gtk_file_filter_add_mime_type (file_filter, node->data);
	}
	gtk_file_chooser_set_filter (chooser, file_filter);
#ifdef MODEST_HAVE_HILDON0_WIDGETS
	osso_mime_types_list_free (image_mimetypes_list);
#else
	hildon_mime_types_list_free (image_mimetypes_list);
#endif

}

void
modest_maemo_set_thumbable_scrollbar (GtkScrolledWindow *win, 
				      gboolean thumbable)
{
	g_return_if_fail (GTK_IS_SCROLLED_WINDOW(win));
#ifdef MODEST_HAVE_HILDON1_WIDGETS		
	hildon_helper_set_thumb_scrollbar (win, thumbable);
#endif /* MODEST_HAVE_HILDON1_WIDGETS */
}

GtkWidget *
modest_maemo_utils_get_manager_menubar_as_menu (GtkUIManager *manager,
						const gchar *item_name)
{
	GtkWidget *new_menu;
	GtkWidget *menubar;
	GList *children, *iter;

	menubar = gtk_ui_manager_get_widget (manager, item_name);
	new_menu = gtk_menu_new ();

	children = gtk_container_get_children (GTK_CONTAINER (menubar));
	for (iter = children; iter != NULL; iter = g_list_next (iter)) {
		GtkWidget *menu;

		menu = GTK_WIDGET (iter->data);
		gtk_widget_reparent (menu, new_menu);
	}
	
	g_list_free (children);

	return new_menu;
}

/**
 * modest_maemo_utils_create_captioned:
 * @title_size_group: a #GtkSizeGroup
 * @value_size_group: a #GtkSizeGroup
 * @title: a string
 * @control: a #GtkWidget
 *
 * this creates a widget (a #GtkHBox) with a control, and a label
 * (@string) captioning it. It also uses the proper size groups for title
 * and control.
 *
 * Returns: a widget containing the control and a proper label.
 */
GtkWidget *
modest_maemo_utils_create_captioned    (GtkSizeGroup *title_size_group,
					GtkSizeGroup *value_size_group,
					const gchar *title,
					gboolean use_markup,
					GtkWidget *control)
{
	return modest_maemo_utils_create_captioned_with_size_type (title_size_group,
								   value_size_group,
								   title,
								   use_markup,
								   control,
								   0);
}

/**
 * modest_maemo_utils_create_captioned_with_size_type:
 * @title_size_group: a #GtkSizeGroup
 * @value_size_group: a #GtkSizeGroup
 * @title: a string
 * @control: a #GtkWidget
 * @size_type: a #HildonSizeType
 *
 * this creates a widget (a #GtkHBox) with a control, and a label
 * (@string) captioning it. It also uses the proper size groups for title
 * and control.
 *
 * Returns: a widget containing the control and a proper label.
 */
GtkWidget *
modest_maemo_utils_create_captioned_with_size_type    (GtkSizeGroup *title_size_group,
						       GtkSizeGroup *value_size_group,
						       const gchar *title,
						       gboolean use_markup,
						       GtkWidget *control,
						       HildonSizeType size_type)
{
 	GtkWidget *label;
	GtkWidget *align;
	GtkWidget *box;
  
	if (use_markup) {
		label = gtk_label_new (NULL);
		gtk_label_set_markup (GTK_LABEL (label), title);
	} else {
		label = gtk_label_new (title);
	}
	align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, MODEST_MARGIN_DOUBLE, MODEST_MARGIN_DOUBLE);

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	hildon_gtk_widget_set_theme_size (label, HILDON_SIZE_FINGER_HEIGHT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_widget_show (align);
	box = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (align), label);
	gtk_box_pack_start (GTK_BOX (box), align, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), control, TRUE, TRUE, 0);
	if (title_size_group)
		gtk_size_group_add_widget (title_size_group, label);
	if (value_size_group)
		gtk_size_group_add_widget (value_size_group, control);

	hildon_gtk_widget_set_theme_size (control, size_type);

	g_object_set_data (G_OBJECT (box), CAPTIONED_LABEL_CHILD, label);

	return box;
}

/**
 * modest_maemo_utils_captioned_set_label:
 * @captioned: a #GtkWidget built as captioned
 * @new_label: a string
 * @use_markup: a #gboolean
 *
 * set a new label for the captioned
 */
void
modest_maemo_utils_captioned_set_label (GtkWidget *captioned,
					const gchar *new_label,
					gboolean use_markup)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_WIDGET (captioned));

	label = g_object_get_data (G_OBJECT (captioned), CAPTIONED_LABEL_CHILD);
	g_return_if_fail (GTK_IS_LABEL (label));

	if (use_markup) {
		gtk_label_set_markup (GTK_LABEL (label), new_label);
	} else {
		gtk_label_set_text (GTK_LABEL (label), new_label);
	}
}

/**
 * modest_maemo_utils_captioned_get_label_widget:
 * @captioned: a #GtkWidget built as captioned
 *
 * obtains the label widget for the captioned
 *
 * Returns: a #GtkLabel
 */
GtkWidget *
modest_maemo_utils_captioned_get_label_widget (GtkWidget *captioned)
{
	GtkWidget *label;

	g_return_val_if_fail (GTK_IS_WIDGET (captioned), NULL);

	label = g_object_get_data (G_OBJECT (captioned), CAPTIONED_LABEL_CHILD);
	g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

	return label;

}

/**
 * modest_maemo_utils_set_hbutton_layout:
 * @title_sizegroup: a #GtkSizeGroup, or %NULL
 * @value_sizegroup: a #GtkSizeGroup, or %NULL
 * @title: a string
 * @button: a #HildonButton
 *
 * Configures the alignment and layout of @button. If @title_sizegroup is provided,
 * the title will be aligned to the left using it. If @value_sizegroup is provided,
 * the value will be aligned to the left using it. It also sets the title
 * of the button.
 *
 * The alignment is left for the title and for the value.
 */
void
modest_maemo_utils_set_hbutton_layout (GtkSizeGroup *title_sizegroup, 
				       GtkSizeGroup *value_sizegroup,
				       const gchar *title, 
				       GtkWidget *button)
{
	hildon_button_set_title (HILDON_BUTTON (button), title);
	if (title_sizegroup)
		hildon_button_add_title_size_group (HILDON_BUTTON (button), title_sizegroup);
	if (value_sizegroup)
		hildon_button_add_value_size_group (HILDON_BUTTON (button), value_sizegroup);
	hildon_button_set_alignment (HILDON_BUTTON (button), 0.0, 0.5, 1.0, 0.0);
	hildon_button_set_title_alignment (HILDON_BUTTON (button), 0.0, 0.5);
	hildon_button_set_value_alignment (HILDON_BUTTON (button), 0.0, 0.5);
}

void
modest_maemo_utils_set_vbutton_layout (GtkSizeGroup *sizegroup, 
				       const gchar *title, 
				       GtkWidget *button)
{
	hildon_button_set_title (HILDON_BUTTON (button), title);
	if (sizegroup) {
		hildon_button_add_title_size_group (HILDON_BUTTON (button), sizegroup);
		hildon_button_add_value_size_group (HILDON_BUTTON (button), sizegroup);
	}
	hildon_button_set_alignment (HILDON_BUTTON (button), 0.0, 0.5, 1.0, 0.0);
	hildon_button_set_title_alignment (HILDON_BUTTON (button), 0.0, 0.5);
	hildon_button_set_value_alignment (HILDON_BUTTON (button), 0.0, 0.5);
}

GtkWidget *
modest_maemo_utils_create_group_box (const gchar *label_text, GtkWidget *contents)
{
	GtkWidget *label;
	GtkWidget *box;

	label = gtk_label_new (label_text);
	gtk_widget_show (label);

	box = gtk_vbox_new (FALSE, MODEST_MARGIN_HALF);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), contents, TRUE, TRUE, 0);
	gtk_widget_show (box);

	return box;
}

static gboolean match_all (TnyList *list, GObject *item, gpointer match_data)
{
	return TRUE;
}

gboolean
modest_maemo_utils_select_attachments (GtkWindow *window, TnyList *att_list, gboolean include_msgs)
{
	GtkTreeModel *model;
	TnyIterator *iterator;
	GtkWidget *selector;
	GtkCellRenderer *renderer;
	GtkWidget *dialog;
	gint response;
	gboolean result = TRUE;
	gint attachments_added = 0;

	model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_OBJECT));
	for (iterator = tny_list_create_iterator (att_list);
	     !tny_iterator_is_done (iterator);
	     tny_iterator_next (iterator)) {
		GtkTreeIter iter;
		TnyMimePart *part;
		gchar *filename = NULL;

		part = (TnyMimePart *) tny_iterator_get_current (iterator);

		/* Ignore purged attachments and messages if ignore is
		   set to TRUE */
		if (!(tny_mime_part_is_purged (part) ||
		      (TNY_IS_MSG (part) && !include_msgs))) {

			if (TNY_IS_MSG (part)) {
				TnyHeader *header = tny_msg_get_header (TNY_MSG (part));
				filename = tny_header_dup_subject (header);
				if ((filename == NULL) || (filename[0] == '\0')) {
					g_free (filename);
					filename = g_strdup (_("mail_va_no_subject"));
				}
				g_object_unref (header);
			} else {
				filename = g_strdup (tny_mime_part_get_filename (part));
			}
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, filename, 1, part, -1);
			attachments_added ++;
			g_free (filename);
			g_object_unref (part);
		}
	}

	selector = GTK_WIDGET (hildon_touch_selector_new ());
	renderer = gtk_cell_renderer_text_new ();
	hildon_touch_selector_append_column (HILDON_TOUCH_SELECTOR (selector), model, renderer,
					     "text", 0, NULL);
	hildon_touch_selector_set_column_selection_mode (HILDON_TOUCH_SELECTOR (selector), 
							 HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE);

	dialog = hildon_picker_dialog_new (window);
	gtk_window_set_title (GTK_WINDOW (dialog), (attachments_added > 1)?
			      _("mcen_ti_select_attachments_title"):_("mcen_ti_select_attachment_title"));
	hildon_picker_dialog_set_selector (HILDON_PICKER_DIALOG (dialog), HILDON_TOUCH_SELECTOR (selector));
	hildon_picker_dialog_set_done_label (HILDON_PICKER_DIALOG (dialog), _HL("wdgt_bd_done"));

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_OK) {
		GList *selected_rows, *node;

		tny_list_remove_matches (att_list, match_all, NULL);
		selected_rows = hildon_touch_selector_get_selected_rows (HILDON_TOUCH_SELECTOR (selector), 0);
		for (node = selected_rows; node != NULL; node = g_list_next (node)) {
			GtkTreePath *path;
			GObject *selected;
			GtkTreeIter iter;

			path = (GtkTreePath *) node->data;
			gtk_tree_model_get_iter (model, &iter, path);
			gtk_tree_model_get (model, &iter, 1, &selected, -1);
			tny_list_append (att_list, selected);
		}
		if (tny_list_get_length (att_list) == 0)
			result = FALSE;
	} else {
		result = FALSE;
	}

	gtk_widget_destroy (dialog);

	g_object_unref (model);

	return result;
}
