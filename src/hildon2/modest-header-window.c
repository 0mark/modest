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

#include <modest-header-window.h>
#include <modest-osso-state-saving.h>
#include <libosso.h>
#include <hildon/hildon-pannable-area.h>
#include <hildon/hildon-edit-toolbar.h>
#include <modest-window-mgr.h>
#include <modest-window-priv.h>
#include <modest-signal-mgr.h>
#include <modest-runtime.h>
#include <modest-platform.h>
#include <modest-maemo-utils.h>
#include <modest-icon-names.h>
#include <modest-ui-constants.h>
#include <modest-account-mgr.h>
#include <modest-account-mgr-helpers.h>
#include <modest-defs.h>
#include <modest-widget-memory.h>
#include <modest-ui-actions.h>
#include <modest-platform.h>
#include <modest-text-utils.h>
#include <hildon/hildon-button.h>
#include <hildon/hildon-program.h>
#include <hildon/hildon-banner.h>
#include "modest-ui-dimming-rules.h"
#include "modest-ui-dimming-manager.h"

typedef enum {
	CONTENTS_STATE_NONE = 0,
	CONTENTS_STATE_EMPTY = 1,
	CONTENTS_STATE_HEADERS = 2
} ContentsState;

typedef enum {
	EDIT_MODE_COMMAND_NONE = 0,
	EDIT_MODE_COMMAND_MOVE = 1,
	EDIT_MODE_COMMAND_DELETE = 2,
} EditModeCommand;

typedef struct _ModestHeaderWindowPrivate ModestHeaderWindowPrivate;
struct _ModestHeaderWindowPrivate {

	GtkWidget *header_view;
	GtkWidget *empty_view;
	GtkWidget *contents_view;
	GtkWidget *top_vbox;
	GtkWidget *edit_toolbar;

	/* state bar */
	ContentsState contents_state;
	gboolean edit_mode;
	EditModeCommand edit_command;

	TnyFolder *folder;

	/* autoscroll */
	gboolean autoscroll;

	/* banners */
	GtkWidget *updating_banner;
	guint updating_banner_timeout;

	/* signals */
	GSList *sighandlers;
	gulong queue_change_handler;

	/* Display state */
	osso_display_state_t display_state;

	/* progress hint */
	gboolean progress_hint;
};
#define MODEST_HEADER_WINDOW_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE((o), \
									  MODEST_TYPE_HEADER_WINDOW, \
									  ModestHeaderWindowPrivate))

/* 'private'/'protected' functions */
static void modest_header_window_class_init  (ModestHeaderWindowClass *klass);
static void modest_header_window_init        (ModestHeaderWindow *obj);
static void modest_header_window_finalize    (GObject *obj);

static void connect_signals (ModestHeaderWindow *self);
static void modest_header_window_disconnect_signals (ModestWindow *self);

static gboolean on_zoom_minus_plus_not_implemented (ModestWindow *window);
static gboolean modest_header_window_toggle_menu (HildonWindow *window,
						  guint button,
						  guint32 time);
static void add_to_menu (ModestHeaderWindow *self,
			 HildonAppMenu *menu,
			 gchar *label,
			 GCallback callback,
			 ModestDimmingRulesGroup *group,
			 GCallback dimming_callback);
static void setup_menu (ModestHeaderWindow *self,
			ModestDimmingRulesGroup *group);
static GtkWidget *create_empty_view (void);
static GtkWidget *create_header_view (ModestWindow *progress_window,
				      TnyFolder *folder);

static void update_view (ModestHeaderWindow *self,
			 TnyFolderChange *change);
static void set_contents_state (ModestHeaderWindow *window, 
				ContentsState state);

static void on_msg_count_changed (ModestHeaderView *header_view,
				  TnyFolder *folder,
				  TnyFolderChange *change,
				  ModestHeaderWindow *header_window);
static void on_header_activated (ModestHeaderView *header_view,
				 TnyHeader *header,
				 GtkTreePath *path,
				 ModestHeaderWindow *header_window);
static void on_updating_msg_list (ModestHeaderView *header_view,
				  gboolean starting,
				  gpointer user_data);
static void set_edit_mode (ModestHeaderWindow *self,
			   EditModeCommand command);
static void edit_toolbar_button_clicked (HildonEditToolbar *toolbar,
					 ModestHeaderWindow *self);
static void edit_toolbar_arrow_clicked (HildonEditToolbar *toolbar,
					ModestHeaderWindow *self);
static void set_delete_edit_mode (GtkButton *button,
				  ModestHeaderWindow *self);
static void set_moveto_edit_mode (GtkButton *button,
				  ModestHeaderWindow *self);
static gboolean on_expose_event(GtkTreeView *header_view,
				GdkEventExpose *event,
				gpointer user_data);
static void on_vertical_movement (HildonPannableArea *area,
				  HildonMovementDirection direction,
				  gdouble x, gdouble y, gpointer user_data);
static void set_progress_hint    (ModestHeaderWindow *self, 
				  gboolean enabled);
static void on_queue_changed    (ModestMailOperationQueue *queue,
				 ModestMailOperation *mail_op,
				 ModestMailOperationQueueNotification type,
				 ModestHeaderWindow *self);
static void modest_header_window_show_toolbar   (ModestWindow *window,
						 gboolean show_toolbar);


/* globals */
static GtkWindowClass *parent_class = NULL;

#define EMPTYVIEW_XALIGN 0.5
#define EMPTYVIEW_YALIGN 0.0
#define EMPTYVIEW_XSPACE 1.0
#define EMPTYVIEW_YSPACE 0.0



/************************************************************************/

GType
modest_header_window_get_type (void)
{
	static GType my_type = 0;
	if (!my_type) {
		static const GTypeInfo my_info = {
			sizeof(ModestHeaderWindowClass),
			NULL,		/* base init */
			NULL,		/* base finalize */
			(GClassInitFunc) modest_header_window_class_init,
			NULL,		/* class finalize */
			NULL,		/* class data */
			sizeof(ModestHeaderWindow),
			1,		/* n_preallocs */
			(GInstanceInitFunc) modest_header_window_init,
			NULL
		};
		my_type = g_type_register_static (MODEST_TYPE_WINDOW,
		                                  "ModestHeaderWindow",
		                                  &my_info, 0);
	}
	return my_type;
}

static void
modest_header_window_class_init (ModestHeaderWindowClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*) klass;
	ModestWindowClass *modest_window_class = (ModestWindowClass *) klass;
	HildonWindowClass *hildon_window_class = (HildonWindowClass *) klass;

	parent_class            = g_type_class_peek_parent (klass);
	gobject_class->finalize = modest_header_window_finalize;

	g_type_class_add_private (gobject_class, sizeof(ModestHeaderWindowPrivate));
	
	hildon_window_class->toggle_menu = modest_header_window_toggle_menu;

	modest_window_class->zoom_minus_func = on_zoom_minus_plus_not_implemented;
	modest_window_class->zoom_plus_func = on_zoom_minus_plus_not_implemented;
	modest_window_class->show_toolbar_func = modest_header_window_show_toolbar;
	modest_window_class->disconnect_signals_func = modest_header_window_disconnect_signals;
}

static void
modest_header_window_init (ModestHeaderWindow *obj)
{
	ModestHeaderWindowPrivate *priv;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(obj);

	priv->sighandlers = NULL;
	priv->display_state = OSSO_DISPLAY_ON;
	
	priv->header_view = NULL;
	priv->empty_view = NULL;
	priv->top_vbox = NULL;
	priv->edit_mode = FALSE;
	priv->edit_command = EDIT_MODE_COMMAND_NONE;
	priv->edit_toolbar = NULL;
	priv->contents_view = NULL;
	priv->contents_state = CONTENTS_STATE_NONE;
	priv->folder = NULL;
	priv->updating_banner = NULL;
	priv->updating_banner_timeout = 0;
	priv->autoscroll = TRUE;
	priv->progress_hint = FALSE;
	priv->queue_change_handler = 0;
	
	modest_window_mgr_register_help_id (modest_runtime_get_window_mgr(),
					    GTK_WINDOW(obj),
					    "applications_email_headerview");
}

static void
modest_header_window_finalize (GObject *obj)
{
	ModestHeaderWindowPrivate *priv;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(obj);

	g_object_unref (priv->header_view);
	g_object_unref (priv->empty_view);
	g_object_unref (priv->folder);

	/* Sanity check: shouldn't be needed, the window mgr should
	   call this function before */
	modest_header_window_disconnect_signals (MODEST_WINDOW (obj));	

	if (priv->updating_banner_timeout > 0) {
		g_source_remove (priv->updating_banner_timeout);
		priv->updating_banner_timeout = 0;
	}
	if (priv->updating_banner) {
		gtk_widget_destroy (priv->updating_banner);
		priv->updating_banner = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize (obj);
}

static void
modest_header_window_disconnect_signals (ModestWindow *self)
{	
	ModestHeaderWindowPrivate *priv;	
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);

	if (g_signal_handler_is_connected (G_OBJECT (modest_runtime_get_mail_operation_queue ()), 
					   priv->queue_change_handler))
		g_signal_handler_disconnect (G_OBJECT (modest_runtime_get_mail_operation_queue ()), 
					     priv->queue_change_handler);

	modest_signal_mgr_disconnect_all_and_destroy (priv->sighandlers);
	priv->sighandlers = NULL;

}

static void
connect_signals (ModestHeaderWindow *self)
{	
	ModestHeaderWindowPrivate *priv;
	
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);

	/* header view */

	priv->sighandlers = 
		modest_signal_mgr_connect (priv->sighandlers,G_OBJECT(priv->header_view), 
					   "msg_count_changed",
					   G_CALLBACK(on_msg_count_changed), self);
	priv->sighandlers =
		modest_signal_mgr_connect (priv->sighandlers, G_OBJECT (priv->header_view),
					   "header-activated",
					   G_CALLBACK (on_header_activated), self);
	priv->sighandlers = 
		modest_signal_mgr_connect (priv->sighandlers,
					   G_OBJECT (priv->header_view), 
					   "updating-msg-list",
					   G_CALLBACK (on_updating_msg_list), 
					   self);
	priv->sighandlers =
		modest_signal_mgr_connect (priv->sighandlers,
					   G_OBJECT (priv->header_view),
					   "expose-event",
					   G_CALLBACK (on_expose_event),
					   self);

	priv->sighandlers =
		modest_signal_mgr_connect (priv->sighandlers,
					   G_OBJECT (priv->contents_view), 
					   "vertical-movement", 
					   G_CALLBACK (on_vertical_movement), 
					   self);

	/* Mail Operation Queue */
	priv->queue_change_handler =
		g_signal_connect (G_OBJECT (modest_runtime_get_mail_operation_queue ()),
				  "queue-changed",
				  G_CALLBACK (on_queue_changed),
				  self);
}

static void 
osso_display_event_cb (osso_display_state_t state, 
		       gpointer data)
{
	ModestHeaderWindowPrivate *priv = MODEST_HEADER_WINDOW_GET_PRIVATE (data);

	priv->display_state = state;

	/* Stop blinking if the screen becomes on */
	if (priv->display_state == OSSO_DISPLAY_ON)
		modest_platform_remove_new_mail_notifications (TRUE);
}

static GtkWidget *
create_header_view (ModestWindow *progress_window, TnyFolder *folder)
{
	GtkWidget *header_view;

	header_view  = modest_header_view_new (NULL, MODEST_HEADER_VIEW_STYLE_TWOLINES);
	modest_header_view_set_folder (MODEST_HEADER_VIEW (header_view), folder, 
				       TRUE, progress_window, NULL, NULL);
	modest_widget_memory_restore (modest_runtime_get_conf (), G_OBJECT(header_view),
				      MODEST_CONF_HEADER_VIEW_KEY);

	return header_view;
}

static GtkWidget *
create_empty_view (void)
{
	GtkWidget *label = NULL;
	GtkWidget *align = NULL;

	align = gtk_alignment_new(EMPTYVIEW_XALIGN, EMPTYVIEW_YALIGN, EMPTYVIEW_XSPACE, EMPTYVIEW_YSPACE);
	label = gtk_label_new (_("mcen_ia_nomessages"));
	gtk_widget_show (label);
	gtk_widget_show (align);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);	
	gtk_container_add (GTK_CONTAINER (align), label);

	return align;
}

static void
on_vertical_movement (HildonPannableArea *area,
		      HildonMovementDirection direction,
		      gdouble x, gdouble y, gpointer user_data)
{
	ModestHeaderWindow *self = (ModestHeaderWindow *) user_data;
	ModestHeaderWindowPrivate *priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	priv->autoscroll = FALSE;
}


ModestWindow *
modest_header_window_new (TnyFolder *folder)
{
	ModestHeaderWindow *self = NULL;	
	ModestHeaderWindowPrivate *priv = NULL;
	HildonProgram *app;
	GdkPixbuf *window_icon;
	ModestWindowPrivate *parent_priv = NULL;
	ModestDimmingRulesGroup *menu_rules_group = NULL;
	
	self  = MODEST_HEADER_WINDOW(g_object_new(MODEST_TYPE_HEADER_WINDOW, NULL));
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);
	parent_priv = MODEST_WINDOW_GET_PRIVATE(self);

	parent_priv->ui_dimming_manager = modest_ui_dimming_manager_new();
	menu_rules_group = modest_dimming_rules_group_new (MODEST_DIMMING_RULES_MENU, FALSE);

	priv->folder = g_object_ref (folder);

	priv->contents_view = hildon_pannable_area_new ();

	priv->header_view  = create_header_view (MODEST_WINDOW (self), folder);
	priv->empty_view = create_empty_view ();
	g_object_ref (priv->header_view);
	g_object_ref (priv->empty_view);
	setup_menu (self, menu_rules_group);

	modest_ui_dimming_manager_insert_rules_group (parent_priv->ui_dimming_manager, menu_rules_group);
	g_object_unref (menu_rules_group);

        priv->top_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (priv->top_vbox), priv->contents_view, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (self), priv->top_vbox);

	gtk_widget_show (priv->contents_view);
	gtk_widget_show (priv->top_vbox);

	connect_signals (MODEST_HEADER_WINDOW (self));

	update_view (self, NULL);

	/* Load previous osso state, for instance if we are being restored from 
	 * hibernation:  */
	modest_osso_load_state ();

	/* Get device name */
	modest_maemo_utils_get_device_name ();

	app = hildon_program_get_instance ();
	hildon_program_add_window (app, HILDON_WINDOW (self));
	
	/* Set window icon */
	window_icon = modest_platform_get_icon (MODEST_APP_ICON, MODEST_ICON_SIZE_BIG);
	if (window_icon) {
		gtk_window_set_icon (GTK_WINDOW (self), window_icon);
		g_object_unref (window_icon);
	}

	/* Listen for changes in the screen, we don't want to show a
	   led pattern when the display is on for example */
	osso_hw_set_display_event_cb (modest_maemo_utils_get_osso_context (),
				      osso_display_event_cb,
				      self); 

	/* Dont't restore settings here, 
	 * because it requires a gtk_widget_show(), 
	 * and we don't want to do that until later,
	 * so that the UI is not visible for non-menu D-Bus activation.
	 */

	return MODEST_WINDOW(self);
}

static gboolean
on_zoom_minus_plus_not_implemented (ModestWindow *window)
{
	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW (window), FALSE);

	hildon_banner_show_information (NULL, NULL, dgettext("hildon-common-strings", "ckct_ib_cannot_zoom_here"));
	return FALSE;

}

gboolean
modest_header_window_screen_is_on (ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv = NULL;

	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW(self), FALSE);

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);
	
	return (priv->display_state == OSSO_DISPLAY_ON) ? TRUE : FALSE;
}

ModestHeaderView *
modest_header_window_get_header_view (ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv = NULL;

	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW(self), FALSE);

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);
	
	return MODEST_HEADER_VIEW (priv->header_view);
}

static void add_to_menu (ModestHeaderWindow *self,
			 HildonAppMenu *menu,
			 gchar *label,
			 GCallback callback,
			 ModestDimmingRulesGroup *dimming_group,
			 GCallback dimming_callback)
{
	GtkWidget *button;

	button = gtk_button_new_with_label (label);
	g_signal_connect_after (G_OBJECT (button), "clicked",
				callback, (gpointer) self);
	modest_dimming_rules_group_add_widget_rule (dimming_group,
						    button,
						    dimming_callback,
						    MODEST_WINDOW (self));
	hildon_app_menu_append (menu, GTK_BUTTON (button));
}

static void setup_menu (ModestHeaderWindow *self, ModestDimmingRulesGroup *group)
{
	ModestHeaderWindowPrivate *priv = NULL;
	GtkWidget *app_menu;

	g_return_if_fail (MODEST_IS_HEADER_WINDOW(self));

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	app_menu = hildon_app_menu_new ();

	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_inbox_moveto"),
		     G_CALLBACK (set_moveto_edit_mode),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_delete));
	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_inbox_delete"),
		     G_CALLBACK (set_delete_edit_mode),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_move_to));
	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_inbox_messagedetails"),
		     G_CALLBACK (modest_ui_actions_on_details),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_details));
	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_inbox_sort"),
		     G_CALLBACK (modest_ui_actions_on_sort),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_sort));
	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_viewer_newemail"),
		     G_CALLBACK (modest_ui_actions_on_new_msg),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_new_msg));
	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_inbox_sendandreceive"),
		     G_CALLBACK (modest_ui_actions_on_send_receive),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_send_receive));
	add_to_menu (self, HILDON_APP_MENU (app_menu), _("mcen_me_outbox_cancelsend"),
		     G_CALLBACK (modest_ui_actions_cancel_send),
		     group, G_CALLBACK (modest_ui_dimming_rules_on_cancel_sending_all));

	hildon_stackable_window_set_main_menu (HILDON_STACKABLE_WINDOW (self), 
					       HILDON_APP_MENU (app_menu));
}

static gboolean 
modest_header_window_toggle_menu (HildonWindow *window,
				  guint button,
				  guint32 time)
{
	modest_ui_actions_check_menu_dimming_rules (MODEST_WINDOW (window));

	return HILDON_WINDOW_CLASS (parent_class)->toggle_menu (window, button, time);
}


static void 
update_view (ModestHeaderWindow *self,
	     TnyFolderChange *change)
{
	ModestHeaderWindowPrivate *priv = NULL;
	gboolean refilter = FALSE;
	gboolean folder_empty = FALSE;
	gboolean all_marked_as_deleted = FALSE;

	g_return_if_fail (MODEST_IS_HEADER_WINDOW(self));
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);
	g_return_if_fail (priv->folder);

	if (change != NULL) {
		TnyFolderChangeChanged changed;

		changed = tny_folder_change_get_changed (change);
		/* If something changes */
		if ((changed) & TNY_FOLDER_CHANGE_CHANGED_ALL_COUNT)
			folder_empty = (((guint) tny_folder_change_get_new_all_count (change)) == 0);
		else
			folder_empty = (((guint) tny_folder_get_all_count (TNY_FOLDER (priv->folder))) == 0);

		if ((changed) & TNY_FOLDER_CHANGE_CHANGED_EXPUNGED_HEADERS)
			refilter = TRUE;
	} else {
		folder_empty = (((guint) tny_folder_get_all_count (TNY_FOLDER (priv->folder))) == 0);
	}

	/* Check if all messages are marked to be deleted */
	all_marked_as_deleted = modest_header_view_is_empty (MODEST_HEADER_VIEW (priv->header_view));
	folder_empty = folder_empty || all_marked_as_deleted;

	/* Set style of headers view */
	set_contents_state (self, folder_empty?CONTENTS_STATE_EMPTY:CONTENTS_STATE_HEADERS);

	if (refilter)
		modest_header_view_refilter (MODEST_HEADER_VIEW (priv->header_view));
}

static void 
set_contents_state (ModestHeaderWindow *self, 
		    ContentsState state)
{
	ModestHeaderWindowPrivate *priv = NULL;

	g_return_if_fail (MODEST_IS_HEADER_WINDOW(self));
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	if (priv->contents_state == state)
		return;

	/* Remove from container the old content */
	switch (priv->contents_state) {
	case CONTENTS_STATE_EMPTY:
		gtk_container_remove (GTK_CONTAINER (priv->contents_view), priv->empty_view);
		break;
	case CONTENTS_STATE_HEADERS:
		gtk_container_remove (GTK_CONTAINER (priv->contents_view), priv->header_view);
		break;
	case CONTENTS_STATE_NONE:
		break;
	}

	/* Add the new content */
	switch (state) {
	case CONTENTS_STATE_EMPTY:
		gtk_container_add (GTK_CONTAINER (priv->contents_view), priv->empty_view);
		gtk_widget_show (priv->empty_view);
		break;
	case CONTENTS_STATE_HEADERS:
		gtk_container_add (GTK_CONTAINER (priv->contents_view), priv->header_view);
		gtk_widget_show (priv->header_view);
		break;
	case CONTENTS_STATE_NONE:
		break;
	}
	priv->contents_state = state;
	
}

static void
on_msg_count_changed (ModestHeaderView *header_view,
		      TnyFolder *folder,
		      TnyFolderChange *change,
		      ModestHeaderWindow *header_window)
{	
	g_return_if_fail (MODEST_IS_HEADER_WINDOW (header_window));
	
	update_view (MODEST_HEADER_WINDOW (header_window), change);
}

static void 
on_header_activated (ModestHeaderView *header_view,
		     TnyHeader *header,
		     GtkTreePath *path,
		     ModestHeaderWindow *header_window)
{
	modest_ui_actions_on_header_activated (header_view, header, path, MODEST_WINDOW (header_window));
}

static void
updating_banner_destroyed (gpointer data,
			   GObject *where_the_object_was)
{
	ModestHeaderWindowPrivate *priv = NULL;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (data);

	priv->updating_banner = NULL;
}

static gboolean
show_updating_banner (gpointer user_data)
{
	ModestHeaderWindowPrivate *priv = NULL;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (user_data);

	if (priv->updating_banner == NULL) {

		/* We're outside the main lock */
		gdk_threads_enter ();
		priv->updating_banner = 
			modest_platform_animation_banner (GTK_WIDGET (user_data), NULL,
							  _CS ("ckdg_pb_updating"));

		/* We need this because banners in Maemo could be
		   destroyed by dialogs so we need to properly update
		   our reference to it */
		g_object_weak_ref (G_OBJECT (priv->updating_banner),
				   updating_banner_destroyed,
				   user_data);
		gdk_threads_leave ();
	}

	/* Remove timeout */
	priv->updating_banner_timeout = 0;
	return FALSE;
}

/**
 * We use this function to show/hide a progress banner showing
 * "Updating" while the header view is being filled. We're not showing
 * it unless the update takes more than 2 seconds
 *
 * If starting = TRUE then the refresh is starting, otherwise it means
 * that is has just finished
 */
static void 
on_updating_msg_list (ModestHeaderView *header_view,
		      gboolean starting,
		      gpointer user_data)
{
	ModestHeaderWindowPrivate *priv = NULL;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (user_data);
	
	/* Remove old timeout */
	if (priv->updating_banner_timeout > 0) {
		g_source_remove (priv->updating_banner_timeout);
		priv->updating_banner_timeout = 0;
	}

	/* Create a new timeout */
	if (starting) {
		priv->updating_banner_timeout = 
			g_timeout_add (2000, show_updating_banner, user_data);
	} else {
		/* Remove the banner if exists */
		if (priv->updating_banner) {
			gtk_widget_destroy (priv->updating_banner);
			priv->updating_banner = NULL;
		}
	}
}

static void
set_edit_mode (ModestHeaderWindow *self,
	       EditModeCommand command)
{
	ModestHeaderWindowPrivate *priv;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);
	if (priv->edit_toolbar) {
		gtk_widget_destroy (priv->edit_toolbar);
		priv->edit_toolbar = NULL;
	}

	if (command == EDIT_MODE_COMMAND_NONE) {
		if (priv->edit_mode) {
			priv->edit_mode = FALSE;
			priv->edit_command = command;
			g_object_set (G_OBJECT (priv->header_view), 
				      "hildon-ui-mode", HILDON_UI_MODE_NORMAL, 
				      NULL);
			gtk_widget_queue_resize (priv->header_view);
			gtk_window_unfullscreen (GTK_WINDOW (self));
		}
	} else {
		if (!priv->edit_mode) {
			GtkTreeSelection *selection;

			g_object_set (G_OBJECT (priv->header_view),
				      "hildon-ui-mode", HILDON_UI_MODE_EDIT,
				      NULL);
			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->header_view));
			gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
			gtk_tree_selection_unselect_all (selection);

			priv->edit_mode = TRUE;
			priv->edit_command = command;

			/* Setup toolbar */
			priv->edit_toolbar = hildon_edit_toolbar_new ();
			switch (command) {
			case EDIT_MODE_COMMAND_DELETE:
				hildon_edit_toolbar_set_label (HILDON_EDIT_TOOLBAR (priv->edit_toolbar),
							       _("TODO: Select messages to delete"));
				hildon_edit_toolbar_set_button_label (HILDON_EDIT_TOOLBAR (priv->edit_toolbar),
								      _("TODO: Delete"));
				break;
			case EDIT_MODE_COMMAND_MOVE:
				hildon_edit_toolbar_set_label (HILDON_EDIT_TOOLBAR (priv->edit_toolbar),
							       _("TODO: Select messages to move"));
				hildon_edit_toolbar_set_button_label (HILDON_EDIT_TOOLBAR (priv->edit_toolbar),
								      _("TODO: Move"));
				break;
			case EDIT_MODE_COMMAND_NONE:
				g_assert ("Shouldn't reach");
			}
			gtk_box_pack_start (GTK_BOX (priv->top_vbox), priv->edit_toolbar, FALSE, FALSE, 0);
			g_signal_connect (G_OBJECT (priv->edit_toolbar), "button-clicked",
					  G_CALLBACK (edit_toolbar_button_clicked), (gpointer) self);
			g_signal_connect (G_OBJECT (priv->edit_toolbar), "arrow-clicked",
					  G_CALLBACK (edit_toolbar_arrow_clicked), (gpointer) self);
			gtk_widget_show (priv->edit_toolbar);

			gtk_widget_queue_resize (priv->header_view);
			gtk_window_fullscreen (GTK_WINDOW (self));
		}
	}
}

static void
edit_toolbar_button_clicked (HildonEditToolbar *toolbar,
			     ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	switch (priv->edit_command) {
	case EDIT_MODE_COMMAND_DELETE:
		if (modest_ui_actions_on_edit_mode_delete_message (MODEST_WINDOW (self)))
			set_edit_mode (self, EDIT_MODE_COMMAND_NONE);
		break;
	case EDIT_MODE_COMMAND_MOVE:
		modest_ui_actions_on_move_to (NULL, MODEST_WINDOW (self));
		set_edit_mode (self, EDIT_MODE_COMMAND_NONE);
		break;
	case EDIT_MODE_COMMAND_NONE:
			g_assert_not_reached ();
	}
}

static void
edit_toolbar_arrow_clicked (HildonEditToolbar *toolbar,
			    ModestHeaderWindow *self)
{
	set_edit_mode (self, EDIT_MODE_COMMAND_NONE);
}

static void
set_delete_edit_mode (GtkButton *button,
		      ModestHeaderWindow *self)
{
	set_edit_mode (self, EDIT_MODE_COMMAND_DELETE);
}

static void
set_moveto_edit_mode (GtkButton *button,
		    ModestHeaderWindow *self)
{
	set_edit_mode (self, EDIT_MODE_COMMAND_MOVE);
}

static gboolean 
on_expose_event(GtkTreeView *header_view,
		GdkEventExpose *event,
		gpointer user_data)
{
	ModestHeaderWindow *self = (ModestHeaderWindow *) user_data;
	ModestHeaderWindowPrivate *priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW (self), FALSE);

	if (priv->autoscroll)
		hildon_pannable_area_jump_to (HILDON_PANNABLE_AREA (priv->contents_view), 0.0, 0.0);

	return FALSE;
}

static gboolean
set_toolbar_transfer_mode (ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv = NULL;
	
	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW (self), FALSE);

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);

	set_progress_hint (self, TRUE);
	
	return FALSE;
}

static void 
set_progress_hint (ModestHeaderWindow *self, 
		   gboolean enabled)
{
	ModestWindowPrivate *parent_priv;
	ModestHeaderWindowPrivate *priv;

	g_return_if_fail (MODEST_IS_HEADER_WINDOW (self));

	parent_priv = MODEST_WINDOW_GET_PRIVATE(self);
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);
			
	/* Sets current progress hint */
	priv->progress_hint = enabled;

	if (GTK_WIDGET_VISIBLE (self)) {
		hildon_gtk_window_set_progress_indicator (GTK_WINDOW (self), enabled?1:0);
	}

}

gboolean 
modest_header_window_toolbar_on_transfer_mode     (ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv= NULL; 

	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW (self), FALSE);
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	return priv->progress_hint;
}

static void
modest_header_window_show_toolbar (ModestWindow *self,
				   gboolean show_toolbar)
{
	ModestHeaderWindowPrivate *priv = NULL;
	ModestWindowPrivate *parent_priv;

	parent_priv = MODEST_WINDOW_GET_PRIVATE(self);
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);

	if (modest_header_window_transfer_mode_enabled (MODEST_HEADER_WINDOW (self))) 
		set_progress_hint (MODEST_HEADER_WINDOW (self), TRUE);
	else
		set_progress_hint (MODEST_HEADER_WINDOW (self), FALSE);
}

gboolean 
modest_header_window_transfer_mode_enabled (ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv;
	
	g_return_val_if_fail (MODEST_IS_HEADER_WINDOW (self), FALSE);	
	priv = MODEST_HEADER_WINDOW_GET_PRIVATE(self);

	return priv->progress_hint;
}

static void 
on_mail_operation_started (ModestMailOperation *mail_op,
			   gpointer user_data)
{
	ModestHeaderWindow *self;
	ModestMailOperationTypeOperation op_type;
	GObject *source = NULL;

	self = MODEST_HEADER_WINDOW (user_data);
	op_type = modest_mail_operation_get_type_operation (mail_op);
	source = modest_mail_operation_get_source(mail_op);
	if (G_OBJECT (self) == source) {
		set_toolbar_transfer_mode(self);
	}
	g_object_unref (source);
}

static void 
on_mail_operation_finished (ModestMailOperation *mail_op,
			    gpointer user_data)
{
	ModestHeaderWindow *self;
	ModestMailOperationTypeOperation op_type;
	
	self = MODEST_HEADER_WINDOW (user_data);
	op_type = modest_mail_operation_get_type_operation (mail_op);
	
	/* If no more operations are being observed, NORMAL mode is enabled again */
	if (modest_mail_operation_queue_num_elements (modest_runtime_get_mail_operation_queue ()) == 0) {
		set_progress_hint (self, FALSE);
	}
	
	modest_ui_actions_check_menu_dimming_rules (MODEST_WINDOW (self));
}

static void
on_queue_changed (ModestMailOperationQueue *queue,
		  ModestMailOperation *mail_op,
		  ModestMailOperationQueueNotification type,
		  ModestHeaderWindow *self)
{
	ModestHeaderWindowPrivate *priv;

	priv = MODEST_HEADER_WINDOW_GET_PRIVATE (self);

	/* If this operations was created by another window, do nothing */
	if (!modest_mail_operation_is_mine (mail_op, G_OBJECT(self))) 
		return;

	if (type == MODEST_MAIL_OPERATION_QUEUE_OPERATION_ADDED) {
		priv->sighandlers = modest_signal_mgr_connect (priv->sighandlers,
							       G_OBJECT (mail_op),
							       "operation-started",
							       G_CALLBACK (on_mail_operation_started),
							       self);
		priv->sighandlers = modest_signal_mgr_connect (priv->sighandlers,
							       G_OBJECT (mail_op),
							       "operation-finished",
							       G_CALLBACK (on_mail_operation_finished),
							       self);
	} else if (type == MODEST_MAIL_OPERATION_QUEUE_OPERATION_REMOVED) {
		priv->sighandlers = modest_signal_mgr_disconnect (priv->sighandlers,
								  G_OBJECT (mail_op),
								  "operation-started");
		priv->sighandlers = modest_signal_mgr_disconnect (priv->sighandlers,
								  G_OBJECT (mail_op),
								  "operation-finished");
	}
}
