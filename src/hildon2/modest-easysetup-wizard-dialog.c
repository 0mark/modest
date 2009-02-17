
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
 
#include <config.h>
#include "modest-easysetup-wizard-dialog.h"
#include <glib/gi18n.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkseparator.h>
#include "modest-country-picker.h"
#include "modest-provider-picker.h"
#include "modest-servertype-picker.h"
#include "widgets/modest-validating-entry.h"
#include "modest-text-utils.h"
#include "modest-conf.h"
#include "modest-defs.h"
#include "modest-account-mgr.h"
#include "modest-signal-mgr.h"
#include "modest-account-mgr-helpers.h"
#include "modest-runtime.h" /* For modest_runtime_get_account_mgr(). */
#include "modest-connection-specific-smtp-window.h"
#include "widgets/modest-ui-constants.h"
#include "widgets/modest-default-account-settings-dialog.h"
#include "widgets/modest-easysetup-wizard-page.h"
#include "modest-maemo-utils.h"
#include "modest-utils.h"
#include "modest-hildon-includes.h"
#include "modest-maemo-security-options-view.h"
#include "modest-account-protocol.h"

/* Include config.h so that _() works: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_DEFINE_TYPE (ModestEasysetupWizardDialog, modest_easysetup_wizard_dialog, MODEST_TYPE_WIZARD_DIALOG);

#define MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
   						    MODEST_TYPE_EASYSETUP_WIZARD_DIALOG, \
						    ModestEasysetupWizardDialogPrivate))

#define LABELS_WIDTH 480
#define DIALOG_WIDTH LABELS_WIDTH + MODEST_MARGIN_DOUBLE

typedef struct _ModestEasysetupWizardDialogPrivate ModestEasysetupWizardDialogPrivate;


typedef enum {
	MODEST_EASYSETUP_WIZARD_DIALOG_INCOMING_CHANGED = 0x01,
	MODEST_EASYSETUP_WIZARD_DIALOG_OUTGOING_CHANGED = 0x02
} ModestEasysetupWizardDialogServerChanges;

struct _ModestEasysetupWizardDialogPrivate
{
	ModestPresets *presets;

	/* Remember what fields the user edited manually to not prefill them
	 * again. */
	ModestEasysetupWizardDialogServerChanges server_changes;
	
	/* Check if the user changes a field to show a confirmation dialog */
	gboolean dirty;

	/* If we have a pending load of settings or not. */
	gboolean pending_load_settings;
	
	/* Used by derived widgets to query existing accounts,
	 * and to create new accounts: */
	ModestAccountMgr *account_manager;
	ModestAccountSettings *settings;
	
	/* notebook pages: */
	GtkWidget *page_welcome;
	
	GtkWidget *page_account_details;
	GtkWidget *account_country_picker;
	GtkWidget *account_serviceprovider_picker;
	GtkWidget *entry_account_title;
	
	GtkWidget *page_user_details;
	GtkWidget *entry_user_name;
	GtkWidget *entry_user_username;
	GtkWidget *entry_user_password;
	GtkWidget *entry_user_email;
	
	GtkWidget *page_complete_easysetup;
	
	GtkWidget *page_custom_incoming;
	GtkWidget *incoming_servertype_picker;
	GtkWidget *caption_incoming;
	GtkWidget *entry_incomingserver;

	/* Security widgets */
	GtkWidget *incoming_security;
	GtkWidget *outgoing_security;

	GtkWidget *page_custom_outgoing;
	GtkWidget *entry_outgoingserver;
	GtkWidget *checkbox_outgoing_smtp_specific;
	GtkWidget *button_outgoing_smtp_servers;
	
	GtkWidget *page_complete_customsetup;

	ModestProtocolType last_plugin_protocol_selected;
	GSList *missing_data_signals;
};

static void save_to_settings (ModestEasysetupWizardDialog *self);
static void real_enable_buttons (ModestWizardDialog *dialog, gboolean enable_next);
static GList* check_for_supported_auth_methods (ModestEasysetupWizardDialog* self);
static gboolean check_has_supported_auth_methods(ModestEasysetupWizardDialog* self);

static gboolean
on_delete_event (GtkWidget *widget,
		 GdkEvent *event,
		 ModestEasysetupWizardDialog *wizard)
{
	gtk_dialog_response (GTK_DIALOG (wizard), GTK_RESPONSE_CANCEL);
	return TRUE;
}

static void
on_easysetup_changed(GtkWidget* widget, ModestEasysetupWizardDialog* wizard)
{
	ModestEasysetupWizardDialogPrivate* priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(wizard);
	g_return_if_fail (priv != NULL);
	priv->dirty = TRUE;
}

static void
modest_easysetup_wizard_dialog_dispose (GObject *object)
{
	if (G_OBJECT_CLASS (modest_easysetup_wizard_dialog_parent_class)->dispose)
		G_OBJECT_CLASS (modest_easysetup_wizard_dialog_parent_class)->dispose (object);
}

static void
modest_easysetup_wizard_dialog_finalize (GObject *object)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (object);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	
	if (priv->account_manager)
		g_object_unref (G_OBJECT (priv->account_manager));
		
	if (priv->presets)
		modest_presets_destroy (priv->presets);

	if (priv->settings)
		g_object_unref (priv->settings);

	if (priv->missing_data_signals) {
		modest_signal_mgr_disconnect_all_and_destroy (priv->missing_data_signals);
		priv->missing_data_signals = NULL;
	}
	 	
	G_OBJECT_CLASS (modest_easysetup_wizard_dialog_parent_class)->finalize (object);
}

static void
create_subsequent_easysetup_pages (ModestEasysetupWizardDialog *self);

static void
set_default_custom_servernames(ModestEasysetupWizardDialog *dialog);

static void on_servertype_selector_changed(HildonTouchSelector *selector, gint column, gpointer user_data);

static gint 
get_port_from_protocol (ModestProtocolType server_type,
			gboolean alternate_port)
{
	int server_port = 0;
	ModestProtocol *protocol;
	ModestProtocolRegistry *protocol_registry;

	protocol_registry = modest_runtime_get_protocol_registry ();
	protocol = modest_protocol_registry_get_protocol_by_type (protocol_registry, server_type);

	if (alternate_port) {
		server_port = modest_account_protocol_get_alternate_port (MODEST_ACCOUNT_PROTOCOL (protocol));
	} else {
		server_port = modest_account_protocol_get_port (MODEST_ACCOUNT_PROTOCOL (protocol));
	}
	return server_port;
}

static void
invoke_enable_buttons_vfunc (ModestEasysetupWizardDialog *wizard_dialog)
{
	ModestWizardDialogClass *klass = MODEST_WIZARD_DIALOG_GET_CLASS (wizard_dialog);
	
	/* Call the vfunc, which may be overridden by derived classes: */
	if (klass->enable_buttons) {
		GtkNotebook *notebook = NULL;
		g_object_get (wizard_dialog, "wizard-notebook", &notebook, NULL);
		
		const gint current_page_num = gtk_notebook_get_current_page (notebook);
		if (current_page_num == -1)
			return;
			
		GtkWidget* current_page_widget = gtk_notebook_get_nth_page (notebook, current_page_num);
		(*(klass->enable_buttons))(MODEST_WIZARD_DIALOG (wizard_dialog), current_page_widget);
	}
}

static void
on_caption_entry_changed (GtkEditable *editable, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	g_assert(self);
	invoke_enable_buttons_vfunc(self);
}

static void
on_caption_combobox_changed (GtkComboBox *widget, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	g_assert(self);
	invoke_enable_buttons_vfunc(self);
}

static void
on_picker_button_value_changed (HildonPickerButton *widget, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	g_assert(self);
	invoke_enable_buttons_vfunc(self);
}

static void
on_serviceprovider_picker_button_value_changed (HildonPickerButton *widget, gpointer user_data)
{
	gchar* default_account_name_start;
	gchar* default_account_name;
	ModestEasysetupWizardDialog *self;
	ModestEasysetupWizardDialogPrivate *priv;
	ModestProviderPickerIdType provider_id_type;

	self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);

	on_picker_button_value_changed (widget, user_data);

	provider_id_type = modest_provider_picker_get_active_id_type (
		MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker));
	if (provider_id_type == MODEST_PROVIDER_PICKER_ID_OTHER) {
		default_account_name_start = g_strdup (_("mcen_ia_emailsetup_defaultname"));
	} else {
		GtkWidget *selector;

		selector = GTK_WIDGET (hildon_picker_button_get_selector (HILDON_PICKER_BUTTON (widget)));
		default_account_name_start = 
			g_strdup (hildon_touch_selector_get_current_text (HILDON_TOUCH_SELECTOR (selector)));
		
	}
	default_account_name = modest_account_mgr_get_unused_account_display_name (
		priv->account_manager, default_account_name_start);
	g_free (default_account_name_start);
	default_account_name_start = NULL;

	hildon_entry_set_text (HILDON_ENTRY (priv->entry_account_title), default_account_name);
	g_free (default_account_name);
}

/** This is a convenience function to create a caption containing a mandatory widget.
 * When the widget is edited, the enable_buttons() vfunc will be called.
 */
static GtkWidget* 
create_captioned (ModestEasysetupWizardDialog *self,
		  GtkSizeGroup *title_size_group,
		  GtkSizeGroup *value_size_group,
		  const gchar *value,
		  gboolean use_markup,
		  GtkWidget *control)
{

	GtkWidget *result;
	result = modest_maemo_utils_create_captioned (title_size_group, value_size_group,
						      value, use_markup, control);

	/* Connect to the appropriate changed signal for the widget, 
	 * so we can ask for the prev/next buttons to be enabled/disabled appropriately:
	 */
	if (GTK_IS_ENTRY (control)) {
		g_signal_connect (G_OBJECT (control), "changed",
				  G_CALLBACK (on_caption_entry_changed), self);
		
	}
	else if (GTK_IS_COMBO_BOX (control)) {
		g_signal_connect (G_OBJECT (control), "changed",
				  G_CALLBACK (on_caption_combobox_changed), self);
	}
	 
	return result;
}
           
static GtkWidget*
create_page_welcome (ModestEasysetupWizardDialog *self)
{
	GtkWidget *box = gtk_vbox_new (FALSE, MODEST_MARGIN_NONE);
	GtkWidget *label = gtk_label_new(_("mcen_ia_emailsetup_intro"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	/* So that it is not truncated: */
	gtk_widget_set_size_request (label, LABELS_WIDTH, -1);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_widget_show (GTK_WIDGET (box));
	return GTK_WIDGET (box);
}

static void
on_account_country_selector_changed (HildonTouchSelector *widget, gint column, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	g_assert(self);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	
	priv->dirty = TRUE;
	
	/* Fill the providers picker, based on the selected country: */
	if (priv->presets != NULL) {
		gint mcc = modest_country_picker_get_active_country_mcc (
			MODEST_COUNTRY_PICKER (priv->account_country_picker));
		modest_provider_picker_fill (
			MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker), priv->presets, mcc);
	}
}

static void
update_user_email_from_provider (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv; 
	gchar* provider_id;
	gchar* domain_name = NULL;

	g_assert(self);
	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);

	/* Fill the providers combo, based on the selected country: */
	provider_id = modest_provider_picker_get_active_provider_id (
		MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker));
	
	if(provider_id)
		domain_name = modest_presets_get_domain (priv->presets, provider_id);
	
	if(!domain_name)
		domain_name = g_strdup (MODEST_EXAMPLE_EMAIL_ADDRESS);
		
	if (priv->entry_user_email)
		hildon_entry_set_text (HILDON_ENTRY (priv->entry_user_email), domain_name);
		
	g_free (domain_name);
	
	g_free (provider_id);
}

static void
on_account_serviceprovider_selector_changed (HildonTouchSelector *widget, gint column, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	g_assert(self);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	
	priv->dirty = TRUE;

	update_user_email_from_provider (self);
}

static void
on_entry_max (ModestValidatingEntry *self, gpointer user_data)
{
	modest_platform_information_banner (GTK_WIDGET (self), NULL,
					    _CS("ckdg_ib_maximum_characters_reached"));
}

static GtkWidget*
create_page_account_details (ModestEasysetupWizardDialog *self)
{
	GtkWidget *caption;
	GtkWidget *box = gtk_vbox_new (FALSE, MODEST_MARGIN_NONE);
	GtkWidget *label = gtk_label_new(_("mcen_ia_accountdetails"));
	ModestEasysetupWizardDialogPrivate* priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_set_size_request (label, LABELS_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (label);

	/* Create a size group to be used by all captions.
	 * Note that HildonCaption does not create a default size group if we do not specify one.
	 * We use GTK_SIZE_GROUP_HORIZONTAL, so that the widths are the same. */
	GtkSizeGroup* title_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* value_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* The country widgets: */
	priv->account_country_picker = GTK_WIDGET (modest_country_picker_new (MODEST_EDITABLE_SIZE,
									      HILDON_BUTTON_ARRANGEMENT_HORIZONTAL));
	modest_maemo_utils_set_hbutton_layout (title_sizegroup, value_sizegroup, 
					       _("mcen_fi_country"), priv->account_country_picker);
	g_signal_connect (G_OBJECT (priv->account_country_picker), "value-changed",
			  G_CALLBACK (on_picker_button_value_changed), self);
	gtk_box_pack_start (GTK_BOX (box), priv->account_country_picker, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (priv->account_country_picker);
	
	GtkWidget *separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (separator);
            
	/* The service provider widgets: */	
	priv->account_serviceprovider_picker = GTK_WIDGET (modest_provider_picker_new (MODEST_EDITABLE_SIZE,
										       HILDON_BUTTON_ARRANGEMENT_HORIZONTAL));
	modest_maemo_utils_set_hbutton_layout (title_sizegroup, value_sizegroup,
					       _("mcen_fi_serviceprovider"), 
					       priv->account_serviceprovider_picker);
	g_signal_connect (G_OBJECT (priv->account_serviceprovider_picker), "value-changed",
			  G_CALLBACK (on_serviceprovider_picker_button_value_changed), self);
	gtk_box_pack_start (GTK_BOX (box), priv->account_serviceprovider_picker, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (priv->account_serviceprovider_picker);
	
	/* The description widgets: */	
	priv->entry_account_title = GTK_WIDGET (modest_validating_entry_new ());
	g_signal_connect(G_OBJECT(priv->entry_account_title), "changed",
			 G_CALLBACK(on_easysetup_changed), self);
	/* Do use auto-capitalization: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_account_title), 
					 HILDON_GTK_INPUT_MODE_FULL | HILDON_GTK_INPUT_MODE_AUTOCAP);
	
	/* Set a default account title, choosing one that does not already exist: */
	/* Note that this is irrelevant to the non-user visible name, which we will create later. */
	gchar* default_account_name_start = g_strdup (_("mcen_ia_emailsetup_defaultname"));
	gchar* default_account_name = modest_account_mgr_get_unused_account_display_name (
		priv->account_manager, default_account_name_start);
	g_free (default_account_name_start);
	default_account_name_start = NULL;
	
	hildon_entry_set_text (HILDON_ENTRY (priv->entry_account_title), default_account_name);
	g_free (default_account_name);
	default_account_name = NULL;

	caption = create_captioned (self, title_sizegroup, value_sizegroup, _("mcen_fi_account_title"), FALSE,
				    priv->entry_account_title);
	gtk_widget_show (priv->entry_account_title);
	gtk_box_pack_start (GTK_BOX (box), caption, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (caption);
	
	/* Prevent the use of some characters in the account title, 
	 * as required by our UI specification: */
	GList *list_prevent = NULL;
	list_prevent = g_list_append (list_prevent, "\\");
	list_prevent = g_list_append (list_prevent, "/");
	list_prevent = g_list_append (list_prevent, ":");
	list_prevent = g_list_append (list_prevent, "*");
	list_prevent = g_list_append (list_prevent, "?");
	list_prevent = g_list_append (list_prevent, "\""); /* The UI spec mentions “, but maybe means ", maybe both. */
	list_prevent = g_list_append (list_prevent, "“");
	list_prevent = g_list_append (list_prevent, "<"); 
	list_prevent = g_list_append (list_prevent, ">"); 
	list_prevent = g_list_append (list_prevent, "|");
	list_prevent = g_list_append (list_prevent, "^"); 	
	modest_validating_entry_set_unallowed_characters (
	 	MODEST_VALIDATING_ENTRY (priv->entry_account_title), list_prevent);
	g_list_free (list_prevent);
	list_prevent = NULL;
	modest_validating_entry_set_func(MODEST_VALIDATING_ENTRY(priv->entry_account_title),
																	 modest_utils_on_entry_invalid_character, self);
	
	/* Set max length as in the UI spec:
	 * The UI spec seems to want us to show a dialog if we hit the maximum. */
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry_account_title), 64);
	modest_validating_entry_set_max_func (MODEST_VALIDATING_ENTRY (priv->entry_account_title), 
					      on_entry_max, self);
	
	gtk_widget_show (GTK_WIDGET (box));

	g_object_unref (title_sizegroup);
	g_object_unref (value_sizegroup);
	
	return GTK_WIDGET (box);
}

static GtkWidget*
create_page_user_details (ModestEasysetupWizardDialog *self)
{
	GtkSizeGroup* title_sizegroup;
	GtkSizeGroup* value_sizegroup;
	GtkWidget *box;
	ModestEasysetupWizardDialogPrivate *priv;

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	
	/* Create a size group to be used by all captions.
	 * Note that HildonCaption does not create a default size group if we do not specify one.
	 * We use GTK_SIZE_GROUP_HORIZONTAL, so that the widths are the same. */
	box = gtk_vbox_new (FALSE, MODEST_MARGIN_NONE);
	title_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	value_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	 
	/* The name widgets: (use auto cap) */
	priv->entry_user_name = GTK_WIDGET (modest_validating_entry_new ());
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_user_name), 
					 HILDON_GTK_INPUT_MODE_FULL | HILDON_GTK_INPUT_MODE_AUTOCAP);
	
	/* Set max length as in the UI spec:
	 * The UI spec seems to want us to show a dialog if we hit the maximum. */
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry_user_name), 64);
	modest_validating_entry_set_max_func (MODEST_VALIDATING_ENTRY (priv->entry_user_name), 
					      on_entry_max, self);
	GtkWidget *caption = create_captioned (self, title_sizegroup, value_sizegroup,
					       _("mcen_li_emailsetup_name"), FALSE, priv->entry_user_name);
	g_signal_connect(G_OBJECT(priv->entry_user_name), "changed", 
			 G_CALLBACK(on_easysetup_changed), self);
	gtk_widget_show (priv->entry_user_name);
	gtk_box_pack_start (GTK_BOX (box), caption, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (caption);
	
	/* Prevent the use of some characters in the name, 
	 * as required by our UI specification: */
	GList *list_prevent = NULL;
	list_prevent = g_list_append (list_prevent, "<");
	list_prevent = g_list_append (list_prevent, ">");
	modest_validating_entry_set_unallowed_characters (
	 	MODEST_VALIDATING_ENTRY (priv->entry_user_name), list_prevent);
	modest_validating_entry_set_func(MODEST_VALIDATING_ENTRY(priv->entry_user_name),
		modest_utils_on_entry_invalid_character, self);
	g_list_free (list_prevent);
	
	/* The username widgets: */	
	priv->entry_user_username = GTK_WIDGET (modest_validating_entry_new ());
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_user_username), 
					 HILDON_GTK_INPUT_MODE_FULL);
	caption = create_captioned (self, title_sizegroup, value_sizegroup, _("mail_fi_username"), FALSE,
				    priv->entry_user_username);
	gtk_widget_show (priv->entry_user_username);
	gtk_box_pack_start (GTK_BOX (box), caption, FALSE, FALSE, MODEST_MARGIN_HALF);
	g_signal_connect(G_OBJECT(priv->entry_user_username), "changed", 
			 G_CALLBACK(on_easysetup_changed), self);
	gtk_widget_show (caption);
	
	/* Prevent the use of some characters in the username, 
	 * as required by our UI specification: */
	modest_validating_entry_set_func(MODEST_VALIDATING_ENTRY(priv->entry_user_username),
		modest_utils_on_entry_invalid_character, self);
	
	/* Set max length as in the UI spec:
	 * The UI spec seems to want us to show a dialog if we hit the maximum. */
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry_user_username), 64);
	modest_validating_entry_set_max_func (MODEST_VALIDATING_ENTRY (priv->entry_user_username), 
					      on_entry_max, self);
	
	/* The password widgets: */	
	priv->entry_user_password = hildon_entry_new (MODEST_EDITABLE_SIZE);
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_user_password), 
					 HILDON_GTK_INPUT_MODE_FULL | HILDON_GTK_INPUT_MODE_INVISIBLE);
	gtk_entry_set_visibility (GTK_ENTRY (priv->entry_user_password), FALSE);
	/* gtk_entry_set_invisible_char (GTK_ENTRY (priv->entry_user_password), '*'); */
	caption = create_captioned (self, title_sizegroup, value_sizegroup,
				    _("mail_fi_password"), FALSE, priv->entry_user_password);
	g_signal_connect(G_OBJECT(priv->entry_user_password), "changed", 
			 G_CALLBACK(on_easysetup_changed), self);
	gtk_widget_show (priv->entry_user_password);
	gtk_box_pack_start (GTK_BOX (box), caption, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (caption);
	
	/* The email address widgets: */	
	priv->entry_user_email = GTK_WIDGET (modest_validating_entry_new ());
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_user_email), HILDON_GTK_INPUT_MODE_FULL);
	caption = create_captioned (self, title_sizegroup, value_sizegroup,
				    _("mcen_li_emailsetup_email_address"), FALSE, priv->entry_user_email);
	update_user_email_from_provider (self);
	gtk_widget_show (priv->entry_user_email);
	gtk_box_pack_start (GTK_BOX (box), caption, FALSE, FALSE, MODEST_MARGIN_HALF);
	g_signal_connect(G_OBJECT(priv->entry_user_email), "changed", 
			 G_CALLBACK(on_easysetup_changed), self);
	gtk_widget_show (caption);
	
	/* Set max length as in the UI spec:
	 * The UI spec seems to want us to show a dialog if we hit the maximum. */
	gtk_entry_set_max_length (GTK_ENTRY (priv->entry_user_email), 64);
	modest_validating_entry_set_max_func (MODEST_VALIDATING_ENTRY (priv->entry_user_email), 
					      on_entry_max, self);
	
	
	gtk_widget_show (GTK_WIDGET (box));
	g_object_unref (title_sizegroup);
	g_object_unref (value_sizegroup);
	
	return GTK_WIDGET (box);
}

static GtkWidget* 
create_page_complete_easysetup (ModestEasysetupWizardDialog *self)
{
	GtkWidget *box = gtk_vbox_new (FALSE, MODEST_MARGIN_NONE);
	
	GtkWidget *label = gtk_label_new(_("mcen_ia_emailsetup_setup_complete"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_set_size_request (label, LABELS_WIDTH, -1);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	/* The documentation for gtk_label_set_line_wrap() says that we must 
	 * call gtk_widget_set_size_request() with a hard-coded width, 
	 * though I wonder why gtk_label_set_max_width_chars() isn't enough. */
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	label = gtk_label_new (_("mcen_ia_easysetup_complete"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_set_size_request (label, LABELS_WIDTH, -1);
	
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	gtk_widget_show (GTK_WIDGET (box));
	return GTK_WIDGET (box);
}

/** Change the caption title for the incoming server, 
 * as specified in the UI spec:
 */
static void 
update_incoming_server_title (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate* priv; 
	ModestProtocolType protocol_type; 
	ModestProtocolRegistry *protocol_registry;

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	protocol_registry = modest_runtime_get_protocol_registry ();

	protocol_type = modest_servertype_picker_get_active_servertype (
		MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker));

	/* This could happen when the combo box has still no active iter */
	if (protocol_type != MODEST_PROTOCOL_REGISTRY_TYPE_INVALID) {
		gchar* incomingserver_title;
		const gchar *protocol_display_name;
		ModestProtocol *protocol;

		protocol = modest_protocol_registry_get_protocol_by_type (protocol_registry, 
									  protocol_type);
		protocol_display_name = modest_protocol_get_display_name (protocol);

		incomingserver_title = g_strconcat (_("mcen_li_emailsetup_servertype"), "\n<small>(",
						    protocol_display_name, ")</small>", NULL);

		modest_maemo_utils_captioned_set_label (priv->caption_incoming, incomingserver_title, TRUE);

		g_free(incomingserver_title);
	}
}

/** Change the caption title for the incoming server, 
 * as specified in the UI spec:
 */
static void 
update_incoming_server_security_choices (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	ModestServertypePicker *server_type_picker;
	ModestProtocolType protocol_type;
	ModestSecurityOptionsView *view;

	server_type_picker = 
		MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker);
	protocol_type = 
		modest_servertype_picker_get_active_servertype (server_type_picker);
	
	/* Fill the combo with appropriately titled choices for all
	   those protocols */
	view = MODEST_SECURITY_OPTIONS_VIEW (priv->incoming_security);
	modest_security_options_view_set_server_type (view, protocol_type);
}

static void 
on_servertype_selector_changed(HildonTouchSelector *selector, gint column, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	ModestServertypePicker *picker;
	ModestProtocolType protocol_type;

	priv->dirty = TRUE;
	
	/* Update title */
	update_incoming_server_title (self);

	/* Update security options if needed */
	picker = MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker);
	protocol_type = modest_servertype_picker_get_active_servertype (picker);
	update_incoming_server_security_choices (self);
	gtk_widget_show (priv->incoming_security);

	set_default_custom_servernames (self);
}

static void 
on_entry_incoming_servername_changed(GtkEntry *entry, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	priv->dirty = TRUE;
	priv->server_changes |= MODEST_EASYSETUP_WIZARD_DIALOG_INCOMING_CHANGED;
}

static GtkWidget* 
create_page_custom_incoming (ModestEasysetupWizardDialog *self)
{
	ModestProtocolRegistry *protocol_registry;
	ModestEasysetupWizardDialogPrivate* priv; 
	GtkWidget *box; 
	GtkWidget *pannable;
	GtkWidget *label;
	GtkSizeGroup *title_sizegroup;
	GtkSizeGroup *value_sizegroup;

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	protocol_registry = modest_runtime_get_protocol_registry ();

	box = gtk_vbox_new (FALSE, MODEST_MARGIN_NONE);
	pannable = hildon_pannable_area_new ();

	/* Show note that account type cannot be changed in future: */
	label = gtk_label_new (_("mcen_ia_emailsetup_account_type"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_set_size_request (label, LABELS_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	/* Create a size group to be used by all captions.
	 * Note that HildonCaption does not create a default size group if we do not specify one.
	 * We use GTK_SIZE_GROUP_HORIZONTAL, so that the widths are the same. */
	title_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	value_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	 
	/* The incoming server widgets: */
	priv->incoming_servertype_picker = GTK_WIDGET (modest_servertype_picker_new (MODEST_EDITABLE_SIZE,
										     HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
										     TRUE));
	hildon_button_set_title (HILDON_BUTTON (priv->incoming_servertype_picker), _("mcen_li_emailsetup_type"));
	g_signal_connect (G_OBJECT (priv->incoming_servertype_picker), "value-changed",
			  G_CALLBACK (on_picker_button_value_changed), self);
	gtk_box_pack_start (GTK_BOX (box), priv->incoming_servertype_picker, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (priv->incoming_servertype_picker);
	
	priv->entry_incomingserver = hildon_entry_new (MODEST_EDITABLE_SIZE);
	g_signal_connect(G_OBJECT(priv->entry_incomingserver), "changed", G_CALLBACK(on_easysetup_changed), self);
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_incomingserver), HILDON_GTK_INPUT_MODE_FULL);
	set_default_custom_servernames (self);

	/* The caption title will be updated in update_incoming_server_title().
	 * so this default text will never be seen: */
	priv->caption_incoming = create_captioned (self, title_sizegroup, value_sizegroup,
						   "This will be removed", 
						   FALSE, priv->entry_incomingserver);
	update_incoming_server_title (self);
	gtk_widget_show (priv->entry_incomingserver);
	gtk_box_pack_start (GTK_BOX (box), priv->caption_incoming, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (priv->caption_incoming);
	
	/* Change the caption title when the servertype changes, 
	 * as in the UI spec: */
	g_signal_connect (G_OBJECT (hildon_picker_button_get_selector (HILDON_PICKER_BUTTON (priv->incoming_servertype_picker))), 
			  "changed",
			  G_CALLBACK (on_servertype_selector_changed), self);

	/* Remember when the servername was changed manually: */
	g_signal_connect (G_OBJECT (priv->entry_incomingserver), "changed",
	                  G_CALLBACK (on_entry_incoming_servername_changed), self);

	/* The secure connection widgets. These are only valid for
	   protocols with security */	
	priv->incoming_security = 
		modest_maemo_security_options_view_new (MODEST_SECURITY_OPTIONS_INCOMING,
							FALSE, title_sizegroup, value_sizegroup);
	gtk_box_pack_start (GTK_BOX (box), priv->incoming_security, 
			    FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show_all (priv->incoming_security);

	/* Set default selection */
	modest_servertype_picker_set_active_servertype (
		MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker), 
		MODEST_PROTOCOLS_STORE_POP);

	hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable), box);
	gtk_container_set_focus_vadjustment (GTK_CONTAINER (box),
					     hildon_pannable_area_get_vadjustment (HILDON_PANNABLE_AREA (pannable)));
	gtk_widget_show (GTK_WIDGET (box));
	gtk_widget_show (pannable);

	g_object_unref (title_sizegroup);
	g_object_unref (value_sizegroup);

	return GTK_WIDGET (pannable);
}

static void
on_check_button_changed (HildonCheckButton *button, gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET (user_data);
	
	/* Enable the widget only if the check button is active: */
	const gboolean enable = hildon_check_button_get_active (button);
	gtk_widget_set_sensitive (widget, enable);
}

/* Make the sensitivity of a widget depend on a check button.
 */
static void
enable_widget_for_checkbutton (GtkWidget *widget, HildonCheckButton* button)
{
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (on_check_button_changed), widget);
	
	/* Set the starting sensitivity: */
	on_check_button_changed (button, widget);
}

static void
on_button_outgoing_smtp_servers (GtkButton *button, gpointer user_data)
{
	ModestEasysetupWizardDialog * self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	ModestEasysetupWizardDialogPrivate* priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	GtkWidget *specific_window;
	
	/* We set dirty here because setting it depending on the connection specific dialog
	seems overkill */
	priv->dirty = TRUE;
	
	/* Create the window, if necessary: */
	specific_window = (GtkWidget *) modest_connection_specific_smtp_window_new ();
	modest_connection_specific_smtp_window_fill_with_connections (MODEST_CONNECTION_SPECIFIC_SMTP_WINDOW (specific_window), priv->account_manager);

	/* Show the window */
	modest_window_mgr_set_modal (modest_runtime_get_window_mgr (), GTK_WINDOW (specific_window), GTK_WINDOW (self));
	gtk_widget_show (specific_window);
}

static void 
on_entry_outgoing_servername_changed (GtkEntry *entry, gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	priv->server_changes |= MODEST_EASYSETUP_WIZARD_DIALOG_OUTGOING_CHANGED;
}

static GtkWidget* 
create_page_custom_outgoing (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv;
	gchar *smtp_caption_label;
	GtkWidget *pannable;
	GtkWidget *box = gtk_vbox_new (FALSE, MODEST_MARGIN_NONE);

	pannable = hildon_pannable_area_new ();
	
	/* Create a size group to be used by all captions.
	 * Note that HildonCaption does not create a default size group if we do not specify one.
	 * We use GTK_SIZE_GROUP_HORIZONTAL, so that the widths are the same. */
	GtkSizeGroup *title_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup *value_sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	 
	/* The outgoing server widgets: */
	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	priv->entry_outgoingserver = hildon_entry_new (MODEST_EDITABLE_SIZE);
	g_signal_connect (G_OBJECT (priv->entry_outgoingserver), "changed",
                  G_CALLBACK (on_easysetup_changed), self);
	/* Auto-capitalization is the default, so let's turn it off: */
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry_outgoingserver), HILDON_GTK_INPUT_MODE_FULL);
	smtp_caption_label = g_strconcat (_("mcen_li_emailsetup_smtp"), "\n<small>(SMTP)</small>", NULL);
	GtkWidget *caption = create_captioned (self, title_sizegroup, value_sizegroup,
					       smtp_caption_label, TRUE, priv->entry_outgoingserver);
	g_free (smtp_caption_label);
	gtk_widget_show (priv->entry_outgoingserver);
	gtk_box_pack_start (GTK_BOX (box), caption, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (caption);
	set_default_custom_servernames (self);

	/* The secure connection widgets. These are only valid for
	   protocols with security */	
	priv->outgoing_security = 
		modest_maemo_security_options_view_new (MODEST_SECURITY_OPTIONS_OUTGOING,
							FALSE, title_sizegroup, value_sizegroup);
	gtk_box_pack_start (GTK_BOX (box), priv->outgoing_security, 
			    FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (priv->outgoing_security);

	GtkWidget *separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, MODEST_MARGIN_HALF);
	gtk_widget_show (separator);

	/* connection-specific checkbox: */
	priv->checkbox_outgoing_smtp_specific = hildon_check_button_new (HILDON_SIZE_FINGER_HEIGHT);
	hildon_check_button_set_active (HILDON_CHECK_BUTTON (priv->checkbox_outgoing_smtp_specific), 
					FALSE);
	gtk_button_set_label (GTK_BUTTON (priv->checkbox_outgoing_smtp_specific),
			      _("mcen_fi_advsetup_connection_smtp"));
	gtk_button_set_alignment (GTK_BUTTON (priv->checkbox_outgoing_smtp_specific),
				  0.0, 0.5);
	g_signal_connect (G_OBJECT (priv->checkbox_outgoing_smtp_specific), "toggled",
                  G_CALLBACK (on_easysetup_changed), self);

	gtk_widget_show (priv->checkbox_outgoing_smtp_specific);
	gtk_box_pack_start (GTK_BOX (box), priv->checkbox_outgoing_smtp_specific,
			    FALSE, FALSE, MODEST_MARGIN_HALF);

	/* Connection-specific SMTP-Severs Edit button: */
	priv->button_outgoing_smtp_servers = gtk_button_new_with_label (_("mcen_bd_advsetup_optional_smtp"));
	hildon_gtk_widget_set_theme_size (priv->button_outgoing_smtp_servers, 
					  HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH);	
	gtk_widget_show (priv->button_outgoing_smtp_servers);
	gtk_box_pack_start (GTK_BOX (box), priv->button_outgoing_smtp_servers, 
			    FALSE, FALSE, MODEST_MARGIN_HALF);

	/* Only enable the button when the checkbox is checked: */
	enable_widget_for_checkbutton (priv->button_outgoing_smtp_servers, 
				       HILDON_CHECK_BUTTON (priv->checkbox_outgoing_smtp_specific));

	g_signal_connect (G_OBJECT (priv->button_outgoing_smtp_servers), "clicked",
			  G_CALLBACK (on_button_outgoing_smtp_servers), self);

	g_signal_connect (G_OBJECT (priv->entry_outgoingserver), "changed",
	                  G_CALLBACK (on_entry_outgoing_servername_changed), self);


	hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable), box);
	gtk_container_set_focus_vadjustment (GTK_CONTAINER (box),
					     hildon_pannable_area_get_vadjustment (HILDON_PANNABLE_AREA (pannable)));
	gtk_widget_show (GTK_WIDGET (box));
	gtk_widget_show (pannable);

	g_object_unref (title_sizegroup);
	g_object_unref (value_sizegroup);
	
	return GTK_WIDGET (pannable);
}

static gboolean
show_advanced_edit(gpointer user_data)
{
	ModestEasysetupWizardDialog * self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	gint response;
	
	/* Show the Account Settings window: */
	ModestAccountSettingsDialog *dialog = modest_default_account_settings_dialog_new ();
	if (priv->pending_load_settings) {
		save_to_settings (self);
		priv->pending_load_settings = FALSE;
	}
	modest_account_settings_dialog_load_settings (dialog, priv->settings);
	
	modest_window_mgr_set_modal (modest_runtime_get_window_mgr (), GTK_WINDOW (dialog), GTK_WINDOW (self));
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (GTK_WIDGET (dialog));
	
	return FALSE; /* Do not call this timeout callback again. */
}

static void
on_button_edit_advanced_settings (GtkButton *button, gpointer user_data)
{
	ModestEasysetupWizardDialog * self = MODEST_EASYSETUP_WIZARD_DIALOG (user_data);
	
	/* Show the Account Settings window: */
	show_advanced_edit(self);
}
static GtkWidget* 
create_page_complete_custom (ModestEasysetupWizardDialog *self)
{
	GtkWidget *box = gtk_vbox_new (FALSE, MODEST_MARGIN_DEFAULT);
	GtkWidget *label = gtk_label_new(_("mcen_ia_emailsetup_setup_complete"));
	GtkWidget *button_edit = gtk_button_new_with_label (_("mcen_fi_advanced_settings"));
	hildon_gtk_widget_set_theme_size (button_edit, HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	label = gtk_label_new (_("mcen_ia_customsetup_complete"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	
	gtk_widget_show (button_edit);
	gtk_box_pack_start (GTK_BOX (box), button_edit, FALSE, FALSE, MODEST_MARGIN_HALF);
	
	g_signal_connect (G_OBJECT (button_edit), "clicked", 
			  G_CALLBACK (on_button_edit_advanced_settings), self);
	
	gtk_widget_show (GTK_WIDGET (box));
	return GTK_WIDGET (box);
}


/*
 */
static void 
on_response (ModestWizardDialog *wizard_dialog,
	     gint response_id,
	     gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (wizard_dialog);

	invoke_enable_buttons_vfunc (self);
}

static void 
on_response_before (ModestWizardDialog *wizard_dialog,
                    gint response_id,
                    gpointer user_data)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (wizard_dialog);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(wizard_dialog);
	if (response_id == GTK_RESPONSE_CANCEL) {
		/* This is mostly copied from
		 * src/maemo/modest-account-settings-dialog.c */
		if (priv->dirty) {
			GtkDialog *dialog = GTK_DIALOG (hildon_note_new_confirmation (GTK_WINDOW (self), 
				_("imum_nc_wizard_confirm_lose_changes")));
			/* TODO: These button names will be ambiguous, and not
			 * specified in the UI specification. */

			const gint dialog_response = gtk_dialog_run (dialog);
			gtk_widget_destroy (GTK_WIDGET (dialog));

			if (dialog_response != GTK_RESPONSE_OK) {
				/* Don't let the dialog close */
				g_signal_stop_emission_by_name (wizard_dialog, "response");
			}
		}
	}
}

typedef struct IdleData {
	ModestEasysetupWizardDialog *dialog;
	ModestPresets *presets;
} IdleData;

static gboolean
presets_idle (gpointer userdata)
{
	IdleData *idle_data = (IdleData *) userdata;
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (idle_data->dialog);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);

	g_assert (idle_data->presets);

	gdk_threads_enter ();

	priv->presets = idle_data->presets;

	if (MODEST_IS_COUNTRY_PICKER (priv->account_country_picker)) {
/* 		gint mcc = get_default_country_code(); */
		gint mcc;
		/* Fill the combo in an idle call, as it takes a lot of time */
		modest_country_picker_load_data(
			MODEST_COUNTRY_PICKER (priv->account_country_picker));
		/* connect to country picker's changed signal, so we can fill the provider picker: */
		g_signal_connect (G_OBJECT (hildon_picker_button_get_selector 
					    (HILDON_PICKER_BUTTON (priv->account_country_picker))),
				  "changed",
				  G_CALLBACK (on_account_country_selector_changed), self);
            
		modest_country_picker_set_active_country_locale (
			MODEST_COUNTRY_PICKER (priv->account_country_picker));
		mcc = modest_country_picker_get_active_country_mcc (
		        MODEST_COUNTRY_PICKER (priv->account_country_picker));
		modest_provider_picker_fill (
			MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker),
			priv->presets, mcc);
		/* connect to providers picker's changed signal, so we can fill the email address: */
		g_signal_connect (G_OBJECT (hildon_picker_button_get_selector 
					    (HILDON_PICKER_BUTTON (priv->account_serviceprovider_picker))),
				  "changed",
				  G_CALLBACK (on_account_serviceprovider_selector_changed), self);
		
		modest_provider_picker_set_others_provider (MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker));
	}

	priv->dirty = FALSE;

	g_object_unref (idle_data->dialog);
	g_free (idle_data);

	gdk_threads_leave ();

	return FALSE;
}

static gpointer
presets_loader (gpointer userdata)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (userdata);
	ModestPresets *presets = NULL;
	IdleData *idle_data;

	const gchar* path  = NULL;
	const gchar* path1 = MODEST_PROVIDER_DATA_FILE;
	const gchar* path2 = MODEST_MAEMO_PROVIDER_DATA_FILE;
	
	if (access(path1, R_OK) == 0) 
		path = path1;
	else if (access(path2, R_OK) == 0)
		path = path2;
	else {
		g_warning ("%s: neither '%s' nor '%s' is a readable provider data file",
			   __FUNCTION__, path1, path2);
		return NULL;
	}

	presets = modest_presets_new (path);
	if (!presets) {
		g_warning ("%s: failed to parse '%s'", __FUNCTION__, path);
		return NULL;
	}
	
	idle_data = g_new0 (IdleData, 1);
	idle_data->dialog = self;
	idle_data->presets = presets;
	
	g_idle_add (presets_idle, idle_data);	

	return NULL;
}

static void
modest_easysetup_wizard_dialog_append_page (GtkNotebook *notebook,
					    GtkWidget *page,
					    const gchar *label)
{
	gint index;
	/* Append page and set attributes */
	index = gtk_notebook_append_page (notebook, page, gtk_label_new (label));
	gtk_container_child_set (GTK_CONTAINER (notebook), page,
				 "tab-expand", TRUE, "tab-fill", TRUE,
				 NULL);
	gtk_widget_show (page);
}

static void
init_user_page (ModestEasysetupWizardDialogPrivate *priv)
{
	priv->page_user_details = NULL;
	priv->entry_user_name = NULL;
	priv->entry_user_username = NULL;
	priv->entry_user_password = NULL;
	priv->entry_user_email = NULL;
}

static void
init_incoming_page (ModestEasysetupWizardDialogPrivate *priv)
{
	priv->page_custom_incoming = NULL;
	priv->incoming_servertype_picker = NULL;
	priv->caption_incoming = NULL;
	priv->entry_incomingserver = NULL;
	priv->entry_user_email = NULL;
	priv->incoming_security = NULL;
}

static void
init_outgoing_page (ModestEasysetupWizardDialogPrivate *priv)
{
	priv->page_custom_outgoing = NULL;
	priv->entry_outgoingserver = NULL;
	priv->checkbox_outgoing_smtp_specific = NULL;
	priv->button_outgoing_smtp_servers = NULL;
	priv->outgoing_security = NULL;
}

static void
modest_easysetup_wizard_dialog_init (ModestEasysetupWizardDialog *self)
{
	gtk_container_set_border_width (GTK_CONTAINER (self), MODEST_MARGIN_HALF);
	
	/* Create the notebook to be used by the ModestWizardDialog base class:
	 * Each page of the notebook will be a page of the wizard: */
	GtkNotebook *notebook = GTK_NOTEBOOK (gtk_notebook_new());
	gtk_widget_set_size_request (GTK_WIDGET (notebook), -1, MODEST_DIALOG_WINDOW_MAX_HEIGHT);
	
	/* Set the notebook used by the ModestWizardDialog base class: */
	g_object_set (G_OBJECT(self), "wizard-notebook", notebook, NULL);
    
	/* Set the wizard title:
	 * The actual window title will be a combination of this and the page's tab label title. */
	g_object_set (G_OBJECT(self), "wizard-name", _("mcen_ti_emailsetup"), NULL);

	/* Read in the information about known service providers: */
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	
	/* The server fields did not have been manually changed yet */
	priv->server_changes = 0;
	priv->pending_load_settings = TRUE;

	/* Get the account manager object, 
	 * so we can check for existing accounts,
	 * and create new accounts: */
	priv->account_manager = modest_runtime_get_account_mgr ();
	g_object_ref (priv->account_manager);
	
	/* Initialize fields */
	priv->page_welcome = create_page_welcome (self);
	priv->page_account_details = create_page_account_details (self);

	init_user_page (priv);
	init_incoming_page (priv);
	init_outgoing_page (priv);

	priv->page_complete_easysetup = NULL;       
	priv->page_complete_customsetup = NULL;
	priv->last_plugin_protocol_selected = MODEST_PROTOCOL_REGISTRY_TYPE_INVALID;
	priv->missing_data_signals = NULL;

	/* Add the common pages */
	modest_easysetup_wizard_dialog_append_page (notebook, priv->page_welcome, 
						    _("mcen_ti_emailsetup_welcome"));
	modest_easysetup_wizard_dialog_append_page (notebook, priv->page_account_details, 
						    _("mcen_ti_accountdetails"));
		
	/* Connect to the dialog's response signal so we can enable/disable buttons 
	 * for the newly-selected page, because the prev/next buttons cause response to be emitted.
	 * Note that we use g_signal_connect_after() instead of g_signal_connect()
	 * so that we can be enable/disable after ModestWizardDialog has done its own 
	 * enabling/disabling of buttons.
	 * 
	 * HOWEVER, this doesn't work because ModestWizardDialog's response signal handler 
	 * does g_signal_stop_emission_by_name(), stopping our signal handler from running.
	 * 
	 * It's not enough to connect to the notebook's switch-page signal, because 
	 * ModestWizardDialog's "response" signal handler enables the buttons itself, 
	 * _after_ switching the page (understandably).
	 * (Note that if we had, if we used g_signal_connect() instead of g_signal_connect_after()
	 * then gtk_notebook_get_current_page() would return an incorrect value.)
	 */
	g_signal_connect_after (G_OBJECT (self), "response",
				G_CALLBACK (on_response), self);

	/* This is to show a confirmation dialog when the user hits cancel */
	g_signal_connect (G_OBJECT (self), "response",
	                  G_CALLBACK (on_response_before), self);

	g_signal_connect (G_OBJECT (self), "delete-event",
			  G_CALLBACK (on_delete_event), self);

	/* Reset dirty, because there was no user input until now */
	priv->dirty = FALSE;
	
	/* When this window is shown, hibernation should not be possible, 
	 * because there is no sensible way to save the state: */
	modest_window_mgr_prevent_hibernation_while_window_is_shown (
		modest_runtime_get_window_mgr (), GTK_WINDOW (self)); 

	/* Load provider presets */
	g_object_ref (self);
	g_thread_create (presets_loader, self, FALSE, NULL);

	priv->settings = modest_account_settings_new ();
}

ModestEasysetupWizardDialog*
modest_easysetup_wizard_dialog_new (void)
{	
	
	return g_object_new (MODEST_TYPE_EASYSETUP_WIZARD_DIALOG, NULL);
}

static void 
create_subsequent_customsetup_pages (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv;
	GtkNotebook *notebook = NULL;

	g_object_get (self, "wizard-notebook", &notebook, NULL);
	g_assert(notebook);

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);

	if (!priv->page_user_details) {
		priv->page_user_details = create_page_user_details (self);
	}			

	/* Create the custom pages: */
	if(!(priv->page_custom_incoming)) {
		priv->page_custom_incoming = create_page_custom_incoming (self);
	}
		
	/* TODO: only if needed */
	if(!(priv->page_custom_outgoing)) {
		priv->page_custom_outgoing = create_page_custom_outgoing (self);
	}
	
	if(!(priv->page_complete_customsetup)) {
		priv->page_complete_customsetup = create_page_complete_custom (self);
	}

	if (!gtk_widget_get_parent (GTK_WIDGET (priv->page_user_details)))
		modest_easysetup_wizard_dialog_append_page (notebook, priv->page_user_details,
							    _("mcen_ti_emailsetup_userdetails"));

	if (!gtk_widget_get_parent (GTK_WIDGET (priv->page_custom_incoming)))
		modest_easysetup_wizard_dialog_append_page (notebook, priv->page_custom_incoming, 
							    _("mcen_ti_emailsetup_incomingdetails"));
	
	if (!gtk_widget_get_parent (GTK_WIDGET (priv->page_custom_outgoing)))		
		modest_easysetup_wizard_dialog_append_page (notebook, priv->page_custom_outgoing, 
							    _("mcen_ti_emailsetup_outgoingdetails"));
		
	if (!gtk_widget_get_parent (GTK_WIDGET (priv->page_complete_customsetup)))
		modest_easysetup_wizard_dialog_append_page (notebook, priv->page_complete_customsetup, 
							    _("mcen_ti_emailsetup_complete"));
			
	/* This is unnecessary with GTK+ 2.10: */
	modest_wizard_dialog_force_title_update (MODEST_WIZARD_DIALOG(self));
}
	
static void 
create_subsequent_easysetup_pages (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv;
	GtkNotebook *notebook = NULL;

	g_object_get (self, "wizard-notebook", &notebook, NULL);
	g_assert(notebook);
	
	/* Create the easysetup-specific pages: */
	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	if(!priv->page_complete_easysetup)
		priv->page_complete_easysetup = create_page_complete_easysetup (self);

	if (!gtk_widget_get_parent (GTK_WIDGET (priv->page_complete_easysetup)))
		modest_easysetup_wizard_dialog_append_page (notebook, priv->page_complete_easysetup, 
							    _("mcen_ti_emailsetup_complete"));
			
	/* This is unnecessary with GTK+ 2.10: */
	modest_wizard_dialog_force_title_update (MODEST_WIZARD_DIALOG(self));
}

/* */
static void
remove_non_common_tabs (GtkNotebook *notebook, 
			gboolean remove_user_details) 
{
	gint starting_tab;
	/* The first 2 tabs are the common ones (welcome tab and the
	   providers tab), so we always remove starting from the
	   end */

	starting_tab = (remove_user_details) ? 2 : 3;
	while (gtk_notebook_get_n_pages (notebook) > starting_tab) 
		gtk_notebook_remove_page (notebook, -1); 
}

static void
on_missing_mandatory_data (ModestAccountProtocol *protocol,
			   gboolean missing,
			   gpointer user_data)
{
	real_enable_buttons (MODEST_WIZARD_DIALOG (user_data), !missing);
}

/* After the user details page,
 * the following pages depend on whether "Other" was chosen 
 * in the provider combobox on the account page
 */
static void 
create_subsequent_pages (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv;
	ModestProviderPicker *picker;
        ModestProviderPickerIdType id_type;
	GtkNotebook *notebook;

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	picker = MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker);
	id_type = modest_provider_picker_get_active_id_type (picker);
	g_object_get (self, "wizard-notebook", &notebook, NULL);

	if (id_type == MODEST_PROVIDER_PICKER_ID_OTHER) {
		/* "Other..." was selected: */

		/* If we come from a rollbacked easysetup */
		if (priv->page_complete_easysetup) {
			remove_non_common_tabs (notebook, FALSE);
			priv->page_complete_easysetup = NULL;
		} 
		
		/* If we come from a rollbacked plugin protocol setup */
		if (priv->last_plugin_protocol_selected != MODEST_PROTOCOL_REGISTRY_TYPE_INVALID) {
			remove_non_common_tabs (notebook, TRUE);
			priv->last_plugin_protocol_selected = MODEST_PROTOCOL_REGISTRY_TYPE_INVALID;
			modest_signal_mgr_disconnect_all_and_destroy (priv->missing_data_signals);
			priv->missing_data_signals = NULL;
		}

		create_subsequent_customsetup_pages (self);
	} else {
		/* If we come from a rollbacked custom setup */
		if (priv->page_custom_incoming) {
			remove_non_common_tabs (notebook, TRUE);
			init_user_page (priv);
			init_incoming_page (priv);
			init_outgoing_page (priv);
			init_user_page (priv);
			priv->page_complete_customsetup = NULL;
		}

		/* It's a pluggable protocol and not a provider with presets */
		if (id_type == MODEST_PROVIDER_PICKER_ID_PLUGIN_PROTOCOL) {
			ModestProtocol *protocol;
			gchar *proto_name;
			ModestProtocolType proto_type;

			
			/* If we come from a rollbacked easy setup */
			if (priv->last_plugin_protocol_selected == 
			    MODEST_PROTOCOL_REGISTRY_TYPE_INVALID &&
			    priv->page_complete_easysetup) {
				remove_non_common_tabs (notebook, TRUE);
				init_user_page (priv);
				priv->page_complete_easysetup = NULL;
			}
			
			proto_name = modest_provider_picker_get_active_provider_id (picker);
			protocol = modest_protocol_registry_get_protocol_by_name (modest_runtime_get_protocol_registry (),
										  MODEST_PROTOCOL_REGISTRY_PROVIDER_PROTOCOLS,
										  proto_name);
			proto_type = modest_protocol_get_type_id (protocol);

			if (protocol && MODEST_IS_ACCOUNT_PROTOCOL (protocol) &&
			    proto_type != priv->last_plugin_protocol_selected) {
				ModestPairList *tabs;
				GSList *tmp;
				gboolean first_page = TRUE;

				/* Remember the last selected plugin protocol */
				priv->last_plugin_protocol_selected = proto_type;

				/* Get tabs */
				tabs = modest_account_protocol_get_easysetupwizard_tabs (MODEST_ACCOUNT_PROTOCOL (protocol));
				tmp = (GSList *) tabs;
				while (tmp) {
					ModestPair *pair = (ModestPair *) tmp->data;
					modest_easysetup_wizard_dialog_append_page (notebook,
										    GTK_WIDGET (pair->second),
										    (const gchar *) pair->first);
					if (first_page) {
						gtk_container_set_focus_child (GTK_CONTAINER (notebook),
									       GTK_WIDGET (pair->second));
						first_page = FALSE;
					}

					/* Connect signals */
					priv->missing_data_signals = 
						modest_signal_mgr_connect (priv->missing_data_signals, 
									   G_OBJECT (pair->second), 
									   "missing-mandatory-data",
									   G_CALLBACK (on_missing_mandatory_data), 
									   self);

					g_free (pair->first);
					tmp = g_slist_next (tmp);
					/* Critical: if you don't show the page then the dialog will ignore it */
/* 					gtk_widget_show (GTK_WIDGET (pair->second)); */
				}
				modest_pair_list_free (tabs);
			}
			g_free (proto_name);
		} else {
			if (priv->last_plugin_protocol_selected != 
			    MODEST_PROTOCOL_REGISTRY_TYPE_INVALID) {
				remove_non_common_tabs (notebook, TRUE);
				init_user_page (priv);
				priv->page_complete_easysetup = NULL;
				priv->last_plugin_protocol_selected = MODEST_PROTOCOL_REGISTRY_TYPE_INVALID;
				modest_signal_mgr_disconnect_all_and_destroy (priv->missing_data_signals);
				priv->missing_data_signals = NULL;
			}
			if (!priv->page_user_details) {
				priv->page_user_details = create_page_user_details (self);
				modest_easysetup_wizard_dialog_append_page (notebook, 
									    priv->page_user_details,
									    _("mcen_ti_emailsetup_userdetails"));
			}
		}
		
		/* Create the easysetup pages: */
		create_subsequent_easysetup_pages (self);
	}
}


static gchar*
util_get_default_servername_from_email_address (const gchar* email_address, ModestProtocolType protocol_type)
{
	const gchar* hostname = NULL;
	gchar* at;
	gchar* domain;
	ModestProtocolRegistry *protocol_registry;
	ModestProtocol *protocol;

	if (!email_address)
		return NULL;
	
	at = g_utf8_strchr (email_address, -1, '@');
	if (!at || (g_utf8_strlen (at, -1) < 2))
		return NULL;
		
	domain = g_utf8_next_char (at);
	if(!domain)
		return NULL;

	protocol_registry = modest_runtime_get_protocol_registry ();
	protocol = modest_protocol_registry_get_protocol_by_type (protocol_registry, protocol_type);
		
	if (modest_protocol_registry_protocol_type_has_tag (protocol_registry, protocol_type, MODEST_PROTOCOL_REGISTRY_REMOTE_STORE_PROTOCOLS) ||
	    modest_protocol_registry_protocol_type_has_tag (protocol_registry, protocol_type, MODEST_PROTOCOL_REGISTRY_TRANSPORT_PROTOCOLS)) {
		hostname = modest_protocol_get_name (protocol);
	}
	
	if (!hostname)
		return NULL;
		
	return g_strdup_printf ("%s.%s", hostname, domain);
}

static void 
set_default_custom_servernames (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);

	if (!priv->entry_incomingserver)
		return;
		
	/* Set a default domain for the server, based on the email address,
	 * if no server name was already specified.
	 */
	if (priv->entry_user_email
	    && ((priv->server_changes & MODEST_EASYSETUP_WIZARD_DIALOG_INCOMING_CHANGED) == 0)) {
		const ModestProtocolType protocol_type = modest_servertype_picker_get_active_servertype (
			MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker));

		/* This could happen when the combo box has still no active iter */
		if (protocol_type != MODEST_PROTOCOL_REGISTRY_TYPE_INVALID) {
			const gchar* email_address = hildon_entry_get_text (HILDON_ENTRY(priv->entry_user_email));	
			gchar* servername = util_get_default_servername_from_email_address (email_address, 
											    protocol_type);

			/* Do not set the INCOMING_CHANGED flag because of this edit */
			g_signal_handlers_block_by_func (G_OBJECT (priv->entry_incomingserver), G_CALLBACK (on_entry_incoming_servername_changed), self);
			hildon_entry_set_text (HILDON_ENTRY (priv->entry_incomingserver), servername);
			g_signal_handlers_unblock_by_func (G_OBJECT (priv->entry_incomingserver), G_CALLBACK (on_entry_incoming_servername_changed), self);
			
			g_free (servername);
		}
	}
	
	/* Set a default domain for the server, based on the email address,
	 * if no server name was already specified.
	 */
	if (!priv->entry_outgoingserver)
		return;
		
	if (priv->entry_user_email
	    && ((priv->server_changes & MODEST_EASYSETUP_WIZARD_DIALOG_OUTGOING_CHANGED) == 0)) {
		const gchar* email_address = hildon_entry_get_text (HILDON_ENTRY(priv->entry_user_email));
		
		gchar* servername = util_get_default_servername_from_email_address (email_address, MODEST_PROTOCOLS_TRANSPORT_SMTP);

		/* Do not set the OUTGOING_CHANGED flag because of this edit */
		g_signal_handlers_block_by_func (G_OBJECT (priv->entry_outgoingserver), G_CALLBACK (on_entry_outgoing_servername_changed), self);
		hildon_entry_set_text (HILDON_ENTRY (priv->entry_outgoingserver), servername);
		g_signal_handlers_unblock_by_func (G_OBJECT (priv->entry_outgoingserver), G_CALLBACK (on_entry_outgoing_servername_changed), self);

		g_free (servername);
	}
}

static gchar*
get_entered_account_title (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv;
	const gchar* account_title;

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE(self);
	account_title = hildon_entry_get_text (HILDON_ENTRY (priv->entry_account_title));

	if (!account_title || (g_utf8_strlen (account_title, -1) == 0)) {
		return NULL;
	} else {
		/* Strip it of whitespace at the start and end: */
		gchar *result = g_strdup (account_title);
		result = g_strstrip (result);
		
		if (!result)
			return NULL;
			
		if (g_utf8_strlen (result, -1) == 0) {
			g_free (result);
			return NULL;	
		}
		
		return result;
	}
}

static gboolean
on_before_next (ModestWizardDialog *dialog, GtkWidget *current_page, GtkWidget *next_page)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (dialog);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	ModestProtocolRegistry *protocol_registry;

	protocol_registry = modest_runtime_get_protocol_registry ();

	/* if are browsing pages previous to the last one, then we have pending settings in
	 * this wizard */
	if (next_page != NULL)
		priv->pending_load_settings = TRUE;
	
	/* Do extra validation that couldn't be done for every key press,
	 * either because it was too slow,
	 * or because it requires interaction:
	 */
	if (current_page == priv->page_account_details) {	
		/* Check that the title is not already in use: */
		gchar* account_title = get_entered_account_title (self);
		if (!account_title)
			return FALSE;
			
		/* Aavoid a clash with an existing display name: */
		const gboolean name_in_use = modest_account_mgr_account_with_display_name_exists (
			priv->account_manager, account_title);
		g_free (account_title);

		if (name_in_use) {
			/* Warn the user via a dialog: */
			hildon_banner_show_information(NULL, NULL, _("mail_ib_account_name_already_existing"));
            
			return FALSE;
		}

		/* Make sure that the subsequent pages are appropriate for the provider choice. */
		create_subsequent_pages (self);

	} else if (current_page == priv->page_user_details) {
		/* Check that the email address is valud: */
		const gchar* email_address = hildon_entry_get_text (HILDON_ENTRY (priv->entry_user_email));
		if ((!email_address) || (g_utf8_strlen (email_address, -1) == 0))
			return FALSE;
			
		if (!modest_text_utils_validate_email_address (email_address, NULL)) {
			/* Warn the user via a dialog: */
			hildon_banner_show_information (NULL, NULL, _("mcen_ib_invalid_email"));
                                             
			/* Return focus to the email address entry: */
			gtk_widget_grab_focus (priv->entry_user_email);
			gtk_editable_select_region (GTK_EDITABLE (priv->entry_user_email), 0, -1);

			return FALSE;
		}
	}
	  
	if (next_page == priv->page_custom_incoming) {
		set_default_custom_servernames (self);
	} else if (next_page == priv->page_custom_outgoing) {
		set_default_custom_servernames (self);

		/* Check if the server supports secure authentication */
		if (modest_security_options_view_auth_check (MODEST_SECURITY_OPTIONS_VIEW (priv->incoming_security)))
			if (!check_has_supported_auth_methods (self))
				return FALSE;
		gtk_widget_show (priv->outgoing_security);
	}
	
	/* If this is the last page, and this is a click on Finish, 
	 * then attempt to create the dialog.
	 */
	if(!next_page && 
	   current_page != priv->page_account_details) /* This is NULL when this is a click on Finish. */
	{
		if (priv->pending_load_settings) {
			save_to_settings (self);
		}

		/* We check if there's already another account with the same configuration */
		if (modest_account_mgr_check_already_configured_account (priv->account_manager, priv->settings)) {
			modest_platform_information_banner (NULL, NULL, _("mail_ib_setting_failed"));
			return FALSE;
		}

		modest_account_mgr_add_account_from_settings (priv->account_manager, priv->settings);
	}
	
	
	return TRUE;
}

static gboolean entry_is_empty (GtkWidget *entry)
{
	if (!entry)
		return FALSE;
		
	const gchar* text = hildon_entry_get_text (HILDON_ENTRY (entry));
	if ((!text) || (g_utf8_strlen (text, -1) == 0))
		return TRUE;
	else {
		/* Strip it of whitespace at the start and end: */
		gchar *stripped = g_strdup (text);
		stripped = g_strstrip (stripped);
		
		if (!stripped)
			return TRUE;
			
		const gboolean result = (g_utf8_strlen (stripped, -1) == 0);
		
		g_free (stripped);
		return result;
	}
}

static void
real_enable_buttons (ModestWizardDialog *dialog, gboolean enable_next)
{		
	GtkNotebook *notebook = NULL;
	gboolean is_finish_tab;
	GtkWidget *current;
	ModestEasysetupWizardDialogPrivate *priv;

	/* Get data */
	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (dialog);
	g_object_get (dialog, "wizard-notebook", &notebook, NULL);
    	
	/* Disable the Finish button until we are on the last page,
	 * because HildonWizardDialog enables this for all but the
	 * first page */
	current = gtk_notebook_get_nth_page (notebook, gtk_notebook_get_current_page (notebook));
	is_finish_tab = ((current == priv->page_complete_easysetup) ||
			 (current == priv->page_complete_customsetup));

	if (is_finish_tab) {
		/* Disable Next on the last page no matter what the
		   argument say */
		enable_next = FALSE;
	} else {
		/* Not the last one */
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   MODEST_WIZARD_DIALOG_FINISH,
						   FALSE);
	}
    	
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   MODEST_WIZARD_DIALOG_NEXT,
					   enable_next);
}

static void
on_enable_buttons (ModestWizardDialog *dialog, GtkWidget *current_page)
{
	ModestEasysetupWizardDialog *self = MODEST_EASYSETUP_WIZARD_DIALOG (dialog);
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	
	gboolean enable_next = TRUE;
	if (current_page == priv->page_welcome) {
		enable_next = TRUE;
	} else if (current_page == priv->page_account_details) {
		/* The account details title is mandatory: */
		if (entry_is_empty(priv->entry_account_title))
			enable_next = FALSE;
	} else if (current_page == priv->page_user_details) {	
		/* The user details username is mandatory: */
		if (entry_is_empty(priv->entry_user_username))
			enable_next = FALSE;
			
		/* The user details email address is mandatory: */
		if (enable_next && entry_is_empty (priv->entry_user_email))
			enable_next = FALSE;
	} else if (current_page == priv->page_custom_incoming) {
		/* The custom incoming server is mandatory: */
		if (entry_is_empty(priv->entry_incomingserver))
			enable_next = FALSE;
	} else if (MODEST_IS_EASYSETUP_WIZARD_PAGE (current_page)) {
		enable_next = !modest_easysetup_wizard_page_validate (
		       MODEST_EASYSETUP_WIZARD_PAGE (current_page));
	}
			
	/* Enable/disable buttons */ 
	real_enable_buttons (dialog, enable_next);
}

static void
modest_easysetup_wizard_dialog_class_init (ModestEasysetupWizardDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (ModestEasysetupWizardDialogPrivate));


	object_class->dispose = modest_easysetup_wizard_dialog_dispose;
	object_class->finalize = modest_easysetup_wizard_dialog_finalize;
	
	/* Provide a vfunc implementation so we can decide 
	 * when to enable/disable the prev/next buttons.
	 */
	ModestWizardDialogClass *base_klass = (ModestWizardDialogClass*)(klass);
	base_klass->before_next = on_before_next;
	base_klass->enable_buttons = on_enable_buttons;
}

/**
 * save_to_settings:
 * @self: a #ModestEasysetupWizardDialog
 *
 * takes information from all the wizard and stores it in settings
 */
static void
save_to_settings (ModestEasysetupWizardDialog *self)
{
	ModestEasysetupWizardDialogPrivate *priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	guint special_port;
	gchar *provider_id = NULL;
	gchar* display_name;
	const gchar *username, *password;
	gchar *store_hostname, *transport_hostname;
	guint store_port, transport_port;
	ModestProtocolRegistry *protocol_registry;
	ModestProtocolType store_protocol, transport_protocol;
	ModestProtocolType store_security, transport_security;
	ModestProtocolType store_auth_protocol, transport_auth_protocol;
	ModestServerAccountSettings *store_settings, *transport_settings;
	const gchar *fullname, *email_address;
	ModestProviderPicker *picker;
	ModestProviderPickerIdType id_type;

	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	picker = MODEST_PROVIDER_PICKER (priv->account_serviceprovider_picker);
	protocol_registry = modest_runtime_get_protocol_registry ();

	/* Get details from the specified presets: */
	id_type = modest_provider_picker_get_active_id_type (picker);
	provider_id = modest_provider_picker_get_active_provider_id (picker);
		
	/* Let the plugin save the settings. We do a return in order
	   to save an indentation level */
	if (id_type == MODEST_PROVIDER_PICKER_ID_PLUGIN_PROTOCOL) {
		ModestProtocol *protocol;

		protocol = modest_protocol_registry_get_protocol_by_name (
		       modest_runtime_get_protocol_registry (),
		       MODEST_PROTOCOL_REGISTRY_PROVIDER_PROTOCOLS,
		       provider_id);

		if (protocol && MODEST_IS_ACCOUNT_PROTOCOL (protocol)) {
			gint n_pages, i = 0;
			GtkNotebook *notebook;
			GList *wizard_pages = NULL;

			g_object_get (self, "wizard-notebook", &notebook, NULL);
			n_pages = gtk_notebook_get_n_pages (notebook);
			for (i = 0; i < n_pages; i++) {
				GtkWidget *page = gtk_notebook_get_nth_page (notebook, i);
				if (MODEST_IS_EASYSETUP_WIZARD_PAGE (page))
					wizard_pages  = g_list_append (wizard_pages, page);
			}
			modest_account_protocol_save_wizard_settings (MODEST_ACCOUNT_PROTOCOL (protocol),
								      wizard_pages,
								      priv->settings);
			g_list_free (wizard_pages);
		} else {
			g_warning ("The selected protocol is a plugin protocol "//
				   "but it's not a ModestAccountProtocol");
		}

		g_free (provider_id);
		return;
	}

	/* username and password (for both incoming and outgoing): */
	username = hildon_entry_get_text (HILDON_ENTRY (priv->entry_user_username));
	password = hildon_entry_get_text (HILDON_ENTRY (priv->entry_user_password));

	store_settings = modest_account_settings_get_store_settings (priv->settings);
	transport_settings = modest_account_settings_get_transport_settings (priv->settings);

	/* Incoming server: */
	/* Note: We need something as default for the transport store protocol values, 
	 * or modest_account_mgr_add_server_account will fail. */
	store_port = 0;
	store_protocol = MODEST_PROTOCOL_REGISTRY_TYPE_INVALID;
	store_security = MODEST_PROTOCOLS_CONNECTION_NONE;
	store_auth_protocol = MODEST_PROTOCOLS_AUTH_NONE;

	if (provider_id) {
		ModestProtocolType store_provider_server_type;
		gboolean store_provider_use_alternate_port;
		/* Use presets: */
		store_hostname = modest_presets_get_server (priv->presets, provider_id, 
							    TRUE /* store */);
		
		store_provider_server_type = modest_presets_get_info_server_type (priv->presets,
									 provider_id, 
									 TRUE /* store */);
		store_security  = modest_presets_get_info_server_security (priv->presets,
										    provider_id, 
										    TRUE /* store */);
		store_auth_protocol  = modest_presets_get_info_server_auth (priv->presets,
										     provider_id, 
										     TRUE /* store */);
		store_provider_use_alternate_port  = modest_presets_get_info_server_use_alternate_port (priv->presets,
													provider_id, 
													TRUE /* store */);

		/* We don't check for SMTP here as that is impossible for an incoming server. */
		if (store_provider_server_type == MODEST_PROTOCOL_REGISTRY_TYPE_INVALID)
			store_protocol = MODEST_PROTOCOLS_STORE_POP;
		else
			store_protocol = store_provider_server_type;

		/* we check if there is a *special* port */
		special_port = modest_presets_get_port (priv->presets, provider_id, TRUE /* incoming */);
		if (special_port != 0) {
			store_port = special_port;
		} else {
			gboolean use_alternate_port = FALSE;
			if (modest_protocol_registry_protocol_type_is_secure (modest_runtime_get_protocol_registry (),
									      store_security))
				use_alternate_port = TRUE;
			store_port = get_port_from_protocol(store_provider_server_type, use_alternate_port);
		}

		modest_server_account_settings_set_security_protocol (store_settings, 
								      store_security);
		modest_server_account_settings_set_auth_protocol (store_settings, 
								  store_auth_protocol);
		if (store_port != 0)
			modest_server_account_settings_set_port (store_settings, store_port);
	} else {
		/* Use custom pages because no preset was specified: */
		store_hostname = g_strdup (hildon_entry_get_text (HILDON_ENTRY (priv->entry_incomingserver) ));		
		store_protocol = modest_servertype_picker_get_active_servertype (
			MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker));		

		modest_security_options_view_save_settings (
				    MODEST_SECURITY_OPTIONS_VIEW (priv->incoming_security),
				    priv->settings);
	}

	/* now we store the common store account settings */
	modest_server_account_settings_set_hostname (store_settings, store_hostname);
	modest_server_account_settings_set_username (store_settings, username);
	modest_server_account_settings_set_password (store_settings, password);
	modest_server_account_settings_set_protocol (store_settings, store_protocol);

	g_object_unref (store_settings);
	g_free (store_hostname);
	
	/* Outgoing server: */
	transport_hostname = NULL;
	transport_protocol = MODEST_PROTOCOL_REGISTRY_TYPE_INVALID;
	transport_security = MODEST_PROTOCOLS_CONNECTION_NONE;
	transport_auth_protocol = MODEST_PROTOCOLS_AUTH_NONE;
	transport_port = 0;
	
	if (provider_id) {
		ModestProtocolType transport_provider_server_type;
		ModestProtocolType transport_provider_security;

		/* Use presets */
		transport_hostname = modest_presets_get_server (priv->presets, provider_id, 
								FALSE /* transport */);
			
		transport_provider_server_type = modest_presets_get_info_server_type (priv->presets,
										      provider_id, 
										      FALSE /* transport */);		
		transport_provider_security = modest_presets_get_info_server_security (priv->presets, 
										       provider_id, 
										       FALSE /* transport */);

		/* Note: We need something as default, or modest_account_mgr_add_server_account will fail. */
		transport_protocol = transport_provider_server_type;
		transport_security = transport_provider_security;
		if (transport_security == MODEST_PROTOCOLS_CONNECTION_SSL) {
			/* printf("DEBUG: %s: using secure SMTP\n", __FUNCTION__); */
			/* we check if there is a *special* port */
			special_port = modest_presets_get_port (priv->presets, provider_id,
								FALSE /* transport */);
			if (special_port != 0)
				transport_port = special_port;
			else 
				transport_port = 465;
			transport_auth_protocol = MODEST_PROTOCOLS_AUTH_PASSWORD;
		} else {
			/* printf("DEBUG: %s: using non-secure SMTP\n", __FUNCTION__); */
			transport_auth_protocol = MODEST_PROTOCOLS_AUTH_NONE;
		}

		modest_server_account_settings_set_security_protocol (transport_settings, 
								      transport_security);
		modest_server_account_settings_set_auth_protocol (transport_settings, 
								  transport_auth_protocol);
		if (transport_port != 0)
			modest_server_account_settings_set_port (transport_settings, 
								 transport_port);
	} else {
		ModestProtocolRegistry *registry;
		ModestProtocol *store_proto;

		registry = modest_runtime_get_protocol_registry ();
		/* Use custom pages because no preset was specified: */
		transport_hostname = g_strdup (hildon_entry_get_text (HILDON_ENTRY (priv->entry_outgoingserver) ));

		store_proto = modest_protocol_registry_get_protocol_by_type (registry, 
									     store_protocol);

		if (transport_protocol == MODEST_PROTOCOL_REGISTRY_TYPE_INVALID) {
			/* fallback to SMTP if none was specified */
			g_warning ("No transport protocol was specified for store %d (%s)",
				   modest_protocol_get_type_id (store_proto),
				   modest_protocol_get_display_name (store_proto));
			transport_protocol = MODEST_PROTOCOLS_TRANSPORT_SMTP;
		}

		modest_security_options_view_save_settings (
				    MODEST_SECURITY_OPTIONS_VIEW (priv->outgoing_security),
				    priv->settings);
	}

	/* now we store the common transport account settings */
	modest_server_account_settings_set_hostname (transport_settings, transport_hostname);
	modest_server_account_settings_set_username (transport_settings, username);
	modest_server_account_settings_set_password (transport_settings, password);
	modest_server_account_settings_set_protocol (transport_settings, transport_protocol);

	g_object_unref (transport_settings);
	g_free (transport_hostname);
	
	fullname = hildon_entry_get_text (HILDON_ENTRY (priv->entry_user_name));
	email_address = hildon_entry_get_text (HILDON_ENTRY (priv->entry_user_email));
	modest_account_settings_set_fullname (priv->settings, fullname);
	modest_account_settings_set_email_address (priv->settings, email_address);
	/* we don't set retrieve type to preserve advanced settings if
	   any. By default account settings are set to headers only */
	
	/* Save the connection-specific SMTP server accounts. */
        modest_account_settings_set_use_connection_specific_smtp 
		(priv->settings, 
		 hildon_check_button_get_active(HILDON_CHECK_BUTTON(priv->checkbox_outgoing_smtp_specific)));

	display_name = get_entered_account_title (self);
	modest_account_settings_set_display_name (priv->settings, display_name);
	g_free (display_name);
	g_free (provider_id);
}

static GList*
check_for_supported_auth_methods (ModestEasysetupWizardDialog* self)
{
	GError *error = NULL;
	ModestProtocolType protocol_type;
	const gchar* hostname;
	const gchar* username;
	ModestProtocolType security_protocol_incoming_type;
	ModestProtocolRegistry *registry;
	int port_num;
	GList *list_auth_methods;
	ModestEasysetupWizardDialogPrivate *priv;
	
	priv = MODEST_EASYSETUP_WIZARD_DIALOG_GET_PRIVATE (self);
	registry = modest_runtime_get_protocol_registry ();
	protocol_type = modest_servertype_picker_get_active_servertype (
		MODEST_SERVERTYPE_PICKER (priv->incoming_servertype_picker));
	hostname = gtk_entry_get_text(GTK_ENTRY(priv->entry_incomingserver));
	username = gtk_entry_get_text(GTK_ENTRY(priv->entry_user_username));
	security_protocol_incoming_type = modest_security_options_view_get_connection_protocol
		(MODEST_SECURITY_OPTIONS_VIEW (priv->incoming_security));
	port_num = get_port_from_protocol(protocol_type, FALSE);
	list_auth_methods = modest_utils_get_supported_secure_authentication_methods (protocol_type, hostname, port_num,
										      username, GTK_WINDOW (self), &error);

	if (list_auth_methods) {
		/* TODO: Select the correct method */
		GList* list = NULL;
		GList* method;
		for (method = list_auth_methods; method != NULL; method = g_list_next(method)) {
			ModestProtocolType auth_protocol_type = (ModestProtocolType) (GPOINTER_TO_INT(method->data));
			if (modest_protocol_registry_protocol_type_has_tag (registry, auth_protocol_type,
									    MODEST_PROTOCOL_REGISTRY_SECURE_PROTOCOLS)) {
				list = g_list_append(list, GINT_TO_POINTER(auth_protocol_type));
			}
		}

		g_list_free(list_auth_methods);

		if (list)
			return list;
	}

	if(error == NULL || error->domain != modest_utils_get_supported_secure_authentication_error_quark() ||
			error->code != MODEST_UTILS_GET_SUPPORTED_SECURE_AUTHENTICATION_ERROR_CANCELED)
	{
		modest_platform_information_banner (GTK_WIDGET(self), NULL,
						    _("mcen_ib_unableto_discover_auth_methods"));
	}

	if(error != NULL)
		g_error_free(error);

	return NULL;
}

static gboolean 
check_has_supported_auth_methods(ModestEasysetupWizardDialog* self)
{
	GList* methods = check_for_supported_auth_methods(self);
	if (!methods)
	{
		return FALSE;
	}

	g_list_free(methods);
	return TRUE;
}

