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

#include <config.h>
//#include <glib/gi18n-lib.h>

#include <string.h>
#include <gtk/gtk.h>
#include <modest-text-utils.h>
#include <modest-compact-mail-header-view.h>
#include <modest-tny-folder.h>
#include <modest-ui-constants.h>
#ifdef MODEST_TOOLKIT_HILDON2
#include <hildon/hildon-gtk.h>
#endif

static GObjectClass *parent_class = NULL;

typedef struct _ModestCompactMailHeaderViewPriv ModestCompactMailHeaderViewPriv;

struct _ModestCompactMailHeaderViewPriv
{
	GtkWidget    *headers_vbox;
	GtkWidget    *subject_box;

	GtkWidget    *fromto_label;
	GtkWidget    *fromto_contents;
	GtkWidget    *priority_icon;
	GtkWidget    *details_label;
	GtkWidget    *date_label;
	GtkWidget    *subject_label;

	GtkSizeGroup *labels_size_group;

	gboolean     is_outgoing;
	gboolean     is_draft;
	gchar        *first_address;
	
	TnyHeader    *header;
	TnyHeaderFlags priority_flags;
};

#define MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE(o)	\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), MODEST_TYPE_COMPACT_MAIL_HEADER_VIEW, ModestCompactMailHeaderViewPriv))

/* GObject */
static void modest_compact_mail_header_view_class_init (ModestCompactMailHeaderViewClass *klass);
static void modest_compact_mail_header_view_instance_init (GTypeInstance *instance, gpointer g_class);
static void modest_compact_mail_header_view_finalize (GObject *object);

/* TnyHeaderView interface */
static void tny_header_view_init (gpointer g, gpointer iface_data);
static void modest_compact_mail_header_view_set_header (TnyHeaderView *self, TnyHeader *header);
static void modest_compact_mail_header_view_update_is_outgoing (TnyHeaderView *self);
static void modest_compact_mail_header_view_set_header_default (TnyHeaderView *self, TnyHeader *header);
static void modest_compact_mail_header_view_clear (TnyHeaderView *self);
static void modest_compact_mail_header_view_clear_default (TnyHeaderView *self);

/* ModestMailHeaderView interface */
static void modest_mail_header_view_init (gpointer g, gpointer iface_data);
static TnyHeaderFlags modest_compact_mail_header_view_get_priority (ModestMailHeaderView *self);
static TnyHeaderFlags modest_compact_mail_header_view_get_priority_default (ModestMailHeaderView *headers_view);
static void modest_compact_mail_header_view_set_priority (ModestMailHeaderView *self, TnyHeaderFlags flags);
static void modest_compact_mail_header_view_set_priority_default (ModestMailHeaderView *headers_view,
								   TnyHeaderFlags flags);
static const GtkWidget *modest_compact_mail_header_view_add_custom_header (ModestMailHeaderView *self,
									    const gchar *label,
									    GtkWidget *custom_widget,
									    gboolean with_expander,
									    gboolean start);
static const GtkWidget *modest_compact_mail_header_view_add_custom_header_default (ModestMailHeaderView *self,
										    const gchar *label,
										    GtkWidget *custom_widget,
										    gboolean with_expander,
										    gboolean start);

/* internal */
static void on_notify_style (GObject *obj, GParamSpec *spec, gpointer userdata);
static void on_details_button_clicked  (GtkButton *button, gpointer userdata);
static void on_fromto_button_clicked  (GtkButton *button, gpointer userdata);
static void update_style (ModestCompactMailHeaderView *self);
static void set_date_time (ModestCompactMailHeaderView *compact_mail_header, time_t date);
static void fill_address (ModestCompactMailHeaderView *self);

static void
set_date_time (ModestCompactMailHeaderView *compact_mail_header, time_t date)
{
	const guint BUF_SIZE = 64; 
	gchar date_buf [BUF_SIZE];

	ModestCompactMailHeaderViewPriv *priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (compact_mail_header);

	modest_text_utils_strftime (date_buf, BUF_SIZE, "%x %X", date);

	gtk_label_set_text (GTK_LABEL (priv->date_label), date_buf);

}

/* static void */
/* activate_recpt (GtkWidget *recpt_view, const gchar *address, gpointer user_data) */
/* { */
/* 	ModestCompactMailHeaderView * view = MODEST_COMPACT_MAIL_HEADER_VIEW (user_data); */

/* 	g_signal_emit (G_OBJECT (view), signals[RECPT_ACTIVATED_SIGNAL], 0, address); */
/* } */


static void 
modest_compact_mail_header_view_set_header (TnyHeaderView *self, TnyHeader *header)
{
	MODEST_COMPACT_MAIL_HEADER_VIEW_GET_CLASS (self)->set_header_func (self, header);
	return;
}

static void
modest_compact_mail_header_view_update_is_outgoing (TnyHeaderView *self)
{
	ModestCompactMailHeaderViewPriv *priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);
	TnyFolder *folder = NULL;
     
	priv->is_outgoing = FALSE;

	if (priv->header == NULL)
		return;
	
	folder = tny_header_get_folder (priv->header);

	if (folder) {
		TnyFolderType folder_type = tny_folder_get_folder_type (folder);

		switch (folder_type) {
		case TNY_FOLDER_TYPE_DRAFTS:
		case TNY_FOLDER_TYPE_OUTBOX:
		case TNY_FOLDER_TYPE_SENT:
			priv->is_outgoing = TRUE;
			break;
		default:
			priv->is_outgoing = FALSE;
		}
		priv->is_draft = (folder_type == TNY_FOLDER_TYPE_DRAFTS);

		g_object_unref (folder);
	}
}

static void 
modest_compact_mail_header_view_set_header_default (TnyHeaderView *self, TnyHeader *header)
{
	ModestCompactMailHeaderViewPriv *priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);

	if (header)
		g_assert (TNY_IS_HEADER (header));

	if (G_LIKELY (priv->header))
 		g_object_unref (G_OBJECT (priv->header));
	priv->header = NULL;

	if (header && G_IS_OBJECT (header))
	{
		gchar *subject;
		time_t date_to_show;

		g_object_ref (G_OBJECT (header)); 
		priv->header = header;

		modest_compact_mail_header_view_update_is_outgoing (self);

		subject = tny_header_dup_subject (header);

		if (subject && (subject[0] != '\0'))
			gtk_label_set_text (GTK_LABEL (priv->subject_label), subject);
		else
			gtk_label_set_text (GTK_LABEL (priv->subject_label), _("mail_va_no_subject"));

		if (priv->is_outgoing && priv->is_draft) {
			date_to_show = tny_header_get_date_sent (header);
		} else {
			date_to_show = tny_header_get_date_received (header);
		}
		set_date_time (MODEST_COMPACT_MAIL_HEADER_VIEW (self), date_to_show);

		fill_address (MODEST_COMPACT_MAIL_HEADER_VIEW (self));

		g_free (subject);
	}

	modest_mail_header_view_set_priority (MODEST_MAIL_HEADER_VIEW (self), 0);
	update_style (MODEST_COMPACT_MAIL_HEADER_VIEW (self));
	gtk_widget_show_all (GTK_WIDGET (self));

	return;
}

static void 
modest_compact_mail_header_view_clear (TnyHeaderView *self)
{
	MODEST_COMPACT_MAIL_HEADER_VIEW_GET_CLASS (self)->clear_func (self);
	return;
}

static void 
modest_compact_mail_header_view_clear_default (TnyHeaderView *self)
{
	ModestCompactMailHeaderViewPriv *priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);

	if (G_LIKELY (priv->header))
		g_object_unref (G_OBJECT (priv->header));
	priv->header = NULL;

	gtk_label_set_text (GTK_LABEL (priv->fromto_label), "");
	gtk_label_set_text (GTK_LABEL (priv->fromto_contents), "");

	gtk_widget_hide (GTK_WIDGET(self));

	return;
}

static const GtkWidget *
modest_compact_mail_header_view_add_custom_header (ModestMailHeaderView *self,
						    const gchar *label,
						    GtkWidget *custom_widget,
						    gboolean with_expander,
						    gboolean start)
{
	return MODEST_COMPACT_MAIL_HEADER_VIEW_GET_CLASS (self)->add_custom_header_func (self, label,
											 custom_widget, with_expander,
											 start);
}

static const GtkWidget *
modest_compact_mail_header_view_add_custom_header_default (ModestMailHeaderView *header_view,
							   const gchar *label,
							   GtkWidget *custom_widget,
							   gboolean with_expander,
							   gboolean start)
{
	ModestCompactMailHeaderViewPriv *priv;
	g_return_val_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (header_view), NULL);
	GtkWidget *hbox;
	GtkWidget *label_field;

	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (header_view);
	hbox = gtk_hbox_new (FALSE, 12);
	label_field = gtk_label_new (NULL);
	gtk_label_set_text (GTK_LABEL (label_field), label);
	gtk_misc_set_alignment (GTK_MISC (label_field), 1.0, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), label_field, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), custom_widget, TRUE, TRUE, 0);
	gtk_size_group_add_widget (priv->labels_size_group, label_field);

	if (start)
		gtk_box_pack_start (GTK_BOX (priv->headers_vbox), hbox, FALSE, FALSE, 0);
	else
		gtk_box_pack_end (GTK_BOX (priv->headers_vbox), hbox, FALSE, FALSE, 0);

	update_style (MODEST_COMPACT_MAIL_HEADER_VIEW (header_view));
	return hbox;
}

/**
 * modest_compact_mail_header_view_new:
 *
 * Return value: a new #ModestHeaderView instance implemented for Gtk+
 **/
TnyHeaderView*
modest_compact_mail_header_view_new ()
{
	ModestCompactMailHeaderViewPriv *priv;
	ModestCompactMailHeaderView *self = g_object_new (MODEST_TYPE_COMPACT_MAIL_HEADER_VIEW, NULL);

	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);
	return TNY_HEADER_VIEW (self);
}

static void
modest_compact_mail_header_view_instance_init (GTypeInstance *instance, gpointer g_class)
{
	ModestCompactMailHeaderView *self = (ModestCompactMailHeaderView *)instance;
	ModestCompactMailHeaderViewPriv *priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);
	GtkWidget *first_hbox, *second_hbox, *vbox, *main_vbox;
	GtkWidget *details_button, *fromto_button;
	PangoAttrList *attr_list;

	priv->header = NULL;
	priv->first_address = NULL;

	main_vbox = gtk_vbox_new (FALSE, 0);
	vbox = gtk_vbox_new (FALSE, 0);
	first_hbox = gtk_hbox_new (FALSE, MODEST_MARGIN_HALF);

	priv->subject_box = gtk_hbox_new (FALSE, MODEST_MARGIN_HALF);

	/* We set here the style for widgets using standard text color. For
	 * widgets with secondary text color, we set them in update_style,
	 * as we want to track the style changes and update the color properly */

	priv->subject_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->subject_label), 0.0, 1.0);
	gtk_label_set_ellipsize (GTK_LABEL (priv->subject_label), PANGO_ELLIPSIZE_END);
	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_scale_new (PANGO_SCALE_LARGE));
	gtk_label_set_attributes (GTK_LABEL (priv->subject_label), attr_list);
	pango_attr_list_unref (attr_list);

	details_button = gtk_button_new ();
	priv->details_label = gtk_label_new (_("mcen_ti_message_properties"));
	gtk_misc_set_alignment (GTK_MISC (priv->details_label), 1.0, 0.5);
	gtk_container_add (GTK_CONTAINER (details_button), priv->details_label);
	gtk_button_set_relief (GTK_BUTTON (details_button), GTK_RELIEF_NONE);

	gtk_box_pack_end (GTK_BOX (priv->subject_box), priv->subject_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (first_hbox), priv->subject_box, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (first_hbox), details_button, FALSE, FALSE, 0);

	second_hbox = gtk_hbox_new (FALSE, MODEST_MARGIN_HALF);

	priv->fromto_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->fromto_label), 0.0, 1.0);

	priv->fromto_contents = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->fromto_contents), 0.0, 1.0);
	gtk_label_set_ellipsize (GTK_LABEL (priv->fromto_contents), PANGO_ELLIPSIZE_END);
	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_underline_new (PANGO_UNDERLINE_SINGLE));
	gtk_label_set_attributes (GTK_LABEL (priv->fromto_contents), attr_list);
	pango_attr_list_unref (attr_list);
	fromto_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (fromto_button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (fromto_button), priv->fromto_contents);

	priv->date_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->date_label), 1.0, 1.0);
	gtk_misc_set_padding (GTK_MISC (priv->date_label), MODEST_MARGIN_DEFAULT, 0);

	gtk_box_pack_start (GTK_BOX (second_hbox), priv->fromto_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (second_hbox), fromto_button, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (second_hbox), priv->date_label, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), first_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), second_hbox, FALSE, FALSE, 0);

	priv->labels_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (priv->labels_size_group, priv->fromto_label);
	
	update_style (self);

	g_signal_connect (G_OBJECT (self), "notify::style", G_CALLBACK (on_notify_style), (gpointer) self);

 	g_signal_connect (G_OBJECT (details_button), "clicked", G_CALLBACK (on_details_button_clicked), instance);
	g_signal_connect (G_OBJECT (fromto_button), "clicked", G_CALLBACK (on_fromto_button_clicked), instance);

	gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);

	priv->headers_vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->headers_vbox), NULL);
	g_object_ref (priv->headers_vbox);

	gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_vbox), priv->headers_vbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (self), main_vbox, TRUE, TRUE, 0);

	gtk_container_set_border_width (GTK_CONTAINER (self), MODEST_MARGIN_DOUBLE);

	gtk_widget_show_all (main_vbox);

	priv->is_outgoing = FALSE;
	priv->is_draft = FALSE;

	return;
}

static void
modest_compact_mail_header_view_finalize (GObject *object)
{
	ModestCompactMailHeaderView *self = (ModestCompactMailHeaderView *)object;	
	ModestCompactMailHeaderViewPriv *priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);

	if (G_LIKELY (priv->header))
		g_object_unref (G_OBJECT (priv->header));
	priv->header = NULL;

	if (G_LIKELY (priv->headers_vbox))
		g_object_unref (G_OBJECT (priv->headers_vbox));

	priv->headers_vbox = NULL;

	g_object_unref (priv->labels_size_group);

	(*parent_class->finalize) (object);

	return;
}

static void
tny_header_view_init (gpointer g, gpointer iface_data)
{
	TnyHeaderViewIface *klass = (TnyHeaderViewIface *)g;

	klass->set_header = modest_compact_mail_header_view_set_header;
	klass->clear = modest_compact_mail_header_view_clear;

	return;
}

static void
modest_mail_header_view_init (gpointer g, gpointer iface_data)
{
	ModestMailHeaderViewIface *klass = (ModestMailHeaderViewIface *)g;

	klass->get_priority = modest_compact_mail_header_view_get_priority;
	klass->set_priority = modest_compact_mail_header_view_set_priority;
	klass->add_custom_header = modest_compact_mail_header_view_add_custom_header;

	return;
}

static void 
modest_compact_mail_header_view_class_init (ModestCompactMailHeaderViewClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);
	object_class = (GObjectClass*) klass;

	klass->set_header_func = modest_compact_mail_header_view_set_header_default;
	klass->clear_func = modest_compact_mail_header_view_clear_default;
	klass->set_priority_func = modest_compact_mail_header_view_set_priority_default;
	klass->get_priority_func = modest_compact_mail_header_view_get_priority_default;
	klass->add_custom_header_func = modest_compact_mail_header_view_add_custom_header_default;
	object_class->finalize = modest_compact_mail_header_view_finalize;

	g_type_class_add_private (object_class, sizeof (ModestCompactMailHeaderViewPriv));

	return;
}

GType 
modest_compact_mail_header_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0))
	{
		static const GTypeInfo info = 
		{
		  sizeof (ModestCompactMailHeaderViewClass),
		  NULL,   /* base_init */
		  NULL,   /* base_finalize */
		  (GClassInitFunc) modest_compact_mail_header_view_class_init,   /* class_init */
		  NULL,   /* class_finalize */
		  NULL,   /* class_data */
		  sizeof (ModestCompactMailHeaderView),
		  0,      /* n_preallocs */
		  modest_compact_mail_header_view_instance_init    /* instance_init */
		};

		static const GInterfaceInfo tny_header_view_info = 
		{
		  (GInterfaceInitFunc) tny_header_view_init, /* interface_init */
		  NULL,         /* interface_finalize */
		  NULL          /* interface_data */
		};

		static const GInterfaceInfo modest_mail_header_view_info = 
		{
		  (GInterfaceInitFunc) modest_mail_header_view_init, /* interface_init */
		  NULL,         /* interface_finalize */
		  NULL          /* interface_data */
		};

		type = g_type_register_static (GTK_TYPE_HBOX,
			"ModestCompactMailHeaderView",
			&info, 0);

		g_type_add_interface_static (type, TNY_TYPE_HEADER_VIEW, 
			&tny_header_view_info);
		g_type_add_interface_static (type, MODEST_TYPE_MAIL_HEADER_VIEW, 
			&modest_mail_header_view_info);

	}

	return type;
}

static TnyHeaderFlags
modest_compact_mail_header_view_get_priority (ModestMailHeaderView *self)
{
	return MODEST_COMPACT_MAIL_HEADER_VIEW_GET_CLASS (self)->get_priority_func (self);
}

static TnyHeaderFlags
modest_compact_mail_header_view_get_priority_default (ModestMailHeaderView *headers_view)
{
	ModestCompactMailHeaderViewPriv *priv;

	g_return_val_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (headers_view), 0);
	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (headers_view);

	return priv->priority_flags;
}

static void 
modest_compact_mail_header_view_set_priority (ModestMailHeaderView *self, TnyHeaderFlags flags)
{
	MODEST_COMPACT_MAIL_HEADER_VIEW_GET_CLASS (self)->set_priority_func (self, flags);
	return;
}

static void
modest_compact_mail_header_view_set_priority_default (ModestMailHeaderView *headers_view,
						      TnyHeaderFlags flags)
{
	ModestCompactMailHeaderViewPriv *priv;

	g_return_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (headers_view));
	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (headers_view);

	priv->priority_flags = flags & TNY_HEADER_FLAG_PRIORITY_MASK ;

	if (priv->priority_flags == TNY_HEADER_FLAG_NORMAL_PRIORITY) {
		if (priv->priority_icon != NULL) {
			gtk_widget_destroy (priv->priority_icon);
			priv->priority_icon = NULL;
		}
	} else if (priv->priority_flags == TNY_HEADER_FLAG_HIGH_PRIORITY) {
		priv->priority_icon = gtk_image_new_from_icon_name ("qgn_list_messaging_high", GTK_ICON_SIZE_MENU);
		gtk_box_pack_start (GTK_BOX (priv->subject_box), priv->priority_icon, FALSE, FALSE, 0);
		gtk_widget_show (priv->priority_icon);
	} else if (priv->priority_flags == TNY_HEADER_FLAG_LOW_PRIORITY) {
		priv->priority_icon = gtk_image_new_from_icon_name ("qgn_list_messaging_low", GTK_ICON_SIZE_MENU);
		gtk_box_pack_start (GTK_BOX (priv->subject_box), priv->priority_icon, FALSE, FALSE, 0);
		gtk_widget_show (priv->priority_icon);
	}
}

static void 
on_notify_style (GObject *obj, GParamSpec *spec, gpointer userdata)
{
	if (strcmp ("style", spec->name) == 0) {
		update_style (MODEST_COMPACT_MAIL_HEADER_VIEW (obj));
		gtk_widget_queue_draw (GTK_WIDGET (obj));
	} 
}

/* This method updates the color (and other style settings) of widgets using secondary text color,
 * tracking the gtk style */
static void
update_style (ModestCompactMailHeaderView *self)
{
	ModestCompactMailHeaderViewPriv *priv;
	GdkColor style_color;
	PangoColor color;
	PangoAttrList *attr_list;
	GSList *custom_widgets;

	g_return_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (self));
	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);

	if (gtk_style_lookup_color (GTK_WIDGET (self)->style, "SecondaryTextColor", &style_color)) {
		color.red = style_color.red;
		color.green = style_color.green;
		color.blue = style_color.blue;
	} else {
		pango_color_parse (&color, "grey");
	}

	/* style of from:/to: label */
	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_foreground_new (color.red, color.green, color.blue));
	gtk_label_set_attributes (GTK_LABEL (priv->fromto_label), attr_list);
	pango_attr_list_unref (attr_list);

	/* style of details label in details button */
	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_foreground_new (color.red, color.green, color.blue));
	pango_attr_list_insert (attr_list, pango_attr_scale_new (PANGO_SCALE_SMALL));
	pango_attr_list_insert (attr_list, pango_attr_underline_new (PANGO_UNDERLINE_SINGLE));
	gtk_label_set_attributes (GTK_LABEL (priv->details_label), attr_list);
	pango_attr_list_unref (attr_list);

	/* style of date time label */
	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_foreground_new (color.red, color.green, color.blue));
	pango_attr_list_insert (attr_list, pango_attr_scale_new (PANGO_SCALE_SMALL));
	gtk_label_set_attributes (GTK_LABEL (priv->date_label), attr_list);
	pango_attr_list_unref (attr_list);

	/* set style of custom headers */
	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_foreground_new (color.red, color.green, color.blue));
	custom_widgets = gtk_size_group_get_widgets (priv->labels_size_group);
	while (custom_widgets) {		
		gtk_label_set_attributes (GTK_LABEL (custom_widgets->data), attr_list);
		custom_widgets = g_slist_next (custom_widgets);
	}
	pango_attr_list_unref (attr_list);
}

static void
fill_address (ModestCompactMailHeaderView *self)
{
	ModestCompactMailHeaderViewPriv *priv;
	gchar *recipients;
	const gchar *label;
	GSList *recipient_list;
	
	g_return_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (self));
	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);

	if (priv->is_outgoing) {
		label = _("mail_va_to");
		recipients = tny_header_dup_to (TNY_HEADER (priv->header));
	} else {
		label = _("mail_va_from");
		recipients = tny_header_dup_from (TNY_HEADER (priv->header));
	}

	g_free (priv->first_address);
	recipient_list = modest_text_utils_split_addresses_list (recipients);
	if (recipient_list == NULL) {
		priv->first_address = NULL;
	} else {
		gchar *first_recipient;

		first_recipient = (gchar *) recipient_list->data;
		priv->first_address = first_recipient?g_strdup (first_recipient):NULL;
	}
	g_slist_foreach (recipient_list, (GFunc) g_free, NULL);
	g_slist_free (recipient_list);

	gtk_label_set_text (GTK_LABEL (priv->fromto_label), label);
	if (recipients)
		gtk_label_set_text (GTK_LABEL (priv->fromto_contents), priv->first_address);

	g_free (recipients);
}

static void 
on_fromto_button_clicked (GtkButton *button,
			  gpointer userdata)
{
	ModestCompactMailHeaderViewPriv *priv;
	ModestCompactMailHeaderView *self = (ModestCompactMailHeaderView *) userdata;

	g_return_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (self));
	priv = MODEST_COMPACT_MAIL_HEADER_VIEW_GET_PRIVATE (self);

	if (priv->first_address) {
		g_signal_emit_by_name (G_OBJECT (self), "recpt-activated", priv->first_address);
	}
}

static void 
on_details_button_clicked (GtkButton *button,
			   gpointer userdata)
{
	ModestCompactMailHeaderView *self = (ModestCompactMailHeaderView *) userdata;

	g_return_if_fail (MODEST_IS_COMPACT_MAIL_HEADER_VIEW (self));

	g_signal_emit_by_name (G_OBJECT (self), "show-details");
}