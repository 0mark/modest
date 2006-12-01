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

#include "modest-mail-operation.h"
/* include other impl specific header files */
#include <string.h>
#include <stdarg.h>
#include <tny-mime-part.h>
#include <tny-store-account.h>
#include <tny-folder-store.h>
#include <tny-folder-store-query.h>
#include <tny-camel-msg.h>
#include <tny-camel-header.h>
#include <tny-camel-stream.h>
#include <tny-camel-mime-part.h>
#include <tny-simple-list.h>
#include <camel/camel-stream-mem.h>
#include <glib/gi18n.h>

#include "modest-text-utils.h"
#include "modest-tny-msg-actions.h"
#include "modest-tny-platform-factory.h"

/* 'private'/'protected' functions */
static void modest_mail_operation_class_init (ModestMailOperationClass *klass);
static void modest_mail_operation_init       (ModestMailOperation *obj);
static void modest_mail_operation_finalize   (GObject *obj);

#define MODEST_ERROR modest_error_quark ()

typedef enum _ModestMailOperationErrorCode ModestMailOperationErrorCode;
enum _ModestMailOperationErrorCode {
        MODEST_MAIL_OPERATION_ERROR_BAD_ACCOUNT,
        MODEST_MAIL_OPERATION_ERROR_MISSING_PARAMETER,

	MODEST_MAIL_OPERATION_NUM_ERROR_CODES
};

typedef struct
{
	ModestMailOperation *mail_op;
	GCallback            cb;
	gpointer             user_data;
} AsyncHelper;

static void       set_error          (ModestMailOperation *mail_operation, 
				      ModestMailOperationErrorCode error_code,
				      const gchar *fmt, ...);
static void       status_update_cb   (TnyFolder *folder, 
				      const gchar *what, 
				      gint status, 
				      gpointer user_data);
static void       folder_refresh_cb  (TnyFolder *folder, 
				      gboolean cancelled,
				      GError **err,
				      gpointer user_data);
static void       add_attachments    (TnyMsg *msg, 
				      const GList *attachments_list);


static TnyMimePart *         add_body_part    (TnyMsg *msg, 
					       const gchar *body,
					       const gchar *content_type, 
					       gboolean has_attachments);


static void modest_mail_operation_xfer_folder (ModestMailOperation *mail_op,
					       TnyFolder *folder,
					       TnyFolderStore *parent,
					       gboolean delete_original);

static void modest_mail_operation_xfer_msg    (ModestMailOperation *mail_op,
					       TnyHeader *header, 
					       TnyFolder *folder, 
					       gboolean delete_original);

static TnyFolder * modest_mail_operation_find_trash_folder (ModestMailOperation *mail_op,
							    TnyStoreAccount *store_account);


enum _ModestMailOperationSignals 
{
	PROGRESS_CHANGED_SIGNAL,

	NUM_SIGNALS
};

typedef struct _ModestMailOperationPrivate ModestMailOperationPrivate;
struct _ModestMailOperationPrivate {
	guint                      done;
	guint                      total;
	GMutex                    *cb_lock;
	ModestMailOperationStatus  status;
	GError                    *error;
};
#define MODEST_MAIL_OPERATION_GET_PRIVATE(o)      (G_TYPE_INSTANCE_GET_PRIVATE((o), \
                                                   MODEST_TYPE_MAIL_OPERATION, \
                                                   ModestMailOperationPrivate))

/* some utility functions */
static char * get_content_type(const gchar *s);
static gboolean is_ascii(const gchar *s);

/* globals */
static GObjectClass *parent_class = NULL;

static guint signals[NUM_SIGNALS] = {0};

GType
modest_mail_operation_get_type (void)
{
	static GType my_type = 0;
	if (!my_type) {
		static const GTypeInfo my_info = {
			sizeof(ModestMailOperationClass),
			NULL,		/* base init */
			NULL,		/* base finalize */
			(GClassInitFunc) modest_mail_operation_class_init,
			NULL,		/* class finalize */
			NULL,		/* class data */
			sizeof(ModestMailOperation),
			1,		/* n_preallocs */
			(GInstanceInitFunc) modest_mail_operation_init,
			NULL
		};
		my_type = g_type_register_static (G_TYPE_OBJECT,
		                                  "ModestMailOperation",
		                                  &my_info, 0);
	}
	return my_type;
}

static void
modest_mail_operation_class_init (ModestMailOperationClass *klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass*) klass;

	parent_class            = g_type_class_peek_parent (klass);
	gobject_class->finalize = modest_mail_operation_finalize;

	g_type_class_add_private (gobject_class, sizeof(ModestMailOperationPrivate));

	/* signal definitions go here, e.g.: */
 	signals[PROGRESS_CHANGED_SIGNAL] = 
		g_signal_new ("progress_changed",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ModestMailOperationClass, progress_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
modest_mail_operation_init (ModestMailOperation *obj)
{
	ModestMailOperationPrivate *priv;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(obj);

	priv->status  = MODEST_MAIL_OPERATION_STATUS_INVALID;
	priv->error   = NULL;
	priv->done    = 0;
	priv->total   = 0;
	priv->cb_lock = g_mutex_new ();
}

static void
modest_mail_operation_finalize (GObject *obj)
{
	ModestMailOperationPrivate *priv;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(obj);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	g_mutex_free (priv->cb_lock);

	G_OBJECT_CLASS(parent_class)->finalize (obj);
}

ModestMailOperation*
modest_mail_operation_new (void)
{
	return MODEST_MAIL_OPERATION(g_object_new(MODEST_TYPE_MAIL_OPERATION, NULL));
}


void
modest_mail_operation_send_mail (ModestMailOperation *mail_op,
				 TnyTransportAccount *transport_account,
				 TnyMsg* msg)
{
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (mail_op));
	g_return_if_fail (TNY_IS_TRANSPORT_ACCOUNT (transport_account));

	tny_transport_account_send (transport_account, msg, NULL); /* FIXME */
}

void
modest_mail_operation_send_new_mail (ModestMailOperation *mail_op,
				     TnyTransportAccount *transport_account,
				     const gchar *from,
				     const gchar *to,
				     const gchar *cc,
				     const gchar *bcc,
				     const gchar *subject,
				     const gchar *body,
				     const GList *attachments_list)
{
	TnyMsg *new_msg;
	TnyHeader *header;
	gchar *content_type;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (mail_op));
	g_return_if_fail (TNY_IS_TRANSPORT_ACCOUNT (transport_account));

	/* Check parametters */
	if (to == NULL) {
		set_error (mail_op,
			   MODEST_MAIL_OPERATION_ERROR_MISSING_PARAMETER,
			   _("Error trying to send a mail. You need to set almost one a recipient"));
		return;
	}

	/* Create new msg */
	new_msg          = TNY_MSG (tny_camel_msg_new ());
	header           = TNY_HEADER (tny_camel_header_new ());

	/* WARNING: set the header before assign values to it */
	tny_msg_set_header (new_msg, header);
	tny_header_set_from (TNY_HEADER (header), from);
	tny_header_set_to (TNY_HEADER (header), to);
	tny_header_set_cc (TNY_HEADER (header), cc);
	tny_header_set_bcc (TNY_HEADER (header), bcc);
	tny_header_set_subject (TNY_HEADER (header), subject);

	content_type = get_content_type(body);

	/* Add the body of the new mail */	
	add_body_part (new_msg, body, (const gchar *) content_type,
		       (attachments_list == NULL) ? FALSE : TRUE);

	/* Add attachments */
	add_attachments (new_msg, attachments_list);

	/* Send mail */	
	tny_transport_account_send (transport_account, new_msg, NULL); /* FIXME */

	/* Clean */
	g_object_unref (header);
	g_object_unref (new_msg);
	g_free(content_type);
}

/**
 * modest_mail_operation_create_forward_mail:
 * @msg: a valid #TnyMsg instance
 * @forward_type: the type of forwarded message
 * 
 * creates a forwarded message from an existing one
 * 
 * Returns: a new #TnyMsg, or NULL in case of error
 **/
TnyMsg* 
modest_mail_operation_create_forward_mail (TnyMsg *msg, 
					   const gchar *from,
					   ModestMailOperationForwardType forward_type)
{
	TnyMsg *new_msg;
	TnyHeader *new_header, *header;
	gchar *new_subject, *new_body, *content_type;
	TnyMimePart *text_body_part = NULL;
	GList *attachments_list;
	TnyList *parts;
	TnyIterator *iter;

	/* Create new objects */
	new_msg          = TNY_MSG (tny_camel_msg_new ());
	new_header       = TNY_HEADER (tny_camel_header_new ());

	header = tny_msg_get_header (msg);

	/* Fill the header */
	tny_msg_set_header (new_msg, new_header);
	tny_header_set_from (new_header, from);

	/* Change the subject */
	new_subject = (gchar *) modest_text_utils_derived_subject (tny_header_get_subject(header),
								   _("Fwd:"));
	tny_header_set_subject (new_header, (const gchar *) new_subject);
	g_free (new_subject);

	/* Get body from original msg */
	new_body = (gchar *) modest_tny_msg_actions_find_body (msg, FALSE);
	if (!new_body) {
		g_object_unref (new_msg);
		return NULL;
	}
	content_type = get_content_type(new_body);

	/* Create the list of attachments */
	parts = TNY_LIST (tny_simple_list_new());
	tny_mime_part_get_parts (TNY_MIME_PART (msg), parts);
	iter = tny_list_create_iterator (parts);
	attachments_list = NULL;

	while (!tny_iterator_is_done(iter)) {
		TnyMimePart *part;

		part = TNY_MIME_PART (tny_iterator_get_current (iter));
		if (tny_mime_part_is_attachment (part))
			attachments_list = g_list_prepend (attachments_list, part);

		tny_iterator_next (iter);
	}

	/* Add attachments */
	add_attachments (new_msg, attachments_list);

	switch (forward_type) {
		TnyMimePart *attachment_part;
		gchar *inlined_text;

	case MODEST_MAIL_OPERATION_FORWARD_TYPE_INLINE:
		/* Prepend "Original message" text */
		inlined_text = (gchar *) 
			modest_text_utils_inlined_text (tny_header_get_from (header),
							tny_header_get_date_sent (header),
							tny_header_get_to (header),
							tny_header_get_subject (header),
							(const gchar*) new_body);
		g_free (new_body);
		new_body = inlined_text;

		/* Add body part */
		add_body_part (new_msg, new_body, 
			       (const gchar *) content_type, 
			       (tny_list_get_length (parts) > 0) ? TRUE : FALSE);

		break;
	case MODEST_MAIL_OPERATION_FORWARD_TYPE_ATTACHMENT:
		attachment_part = add_body_part (new_msg, new_body, 
						 (const gchar *) content_type, TRUE);

		/* Set the subject as the name of the attachment */
		tny_mime_part_set_filename (attachment_part, tny_header_get_subject (header));
		
		break;
	}

	/* Clean */
	if (attachments_list) g_list_free (attachments_list);
	g_object_unref (parts);
	if (text_body_part) g_free (text_body_part);
	g_free (content_type);
	g_free (new_body);

	return new_msg;
}

/**
 * modest_mail_operation_create_reply_mail:
 * @msg: a valid #TnyMsg instance
 * @reply_type: the format of the new message
 * @reply_mode: the mode of reply, to the sender only, to a mail list or to all
 * 
 * creates a new message to reply to an existing one
 * 
 * Returns: Returns: a new #TnyMsg, or NULL in case of error
 **/
TnyMsg* 
modest_mail_operation_create_reply_mail (TnyMsg *msg, 
					 const gchar *from,
					 ModestMailOperationReplyType reply_type,
					 ModestMailOperationReplyMode reply_mode)
{
	TnyMsg *new_msg;
	TnyHeader *new_header, *header;
	gchar *new_subject, *new_body, *content_type, *quoted;
	TnyMimePart *text_body_part;

	/* Create new objects */
	new_msg          = TNY_MSG (tny_camel_msg_new ());
	new_header       = TNY_HEADER (tny_camel_header_new ());
	header           = tny_msg_get_header (msg);

	/* Fill the header */
	tny_msg_set_header (new_msg, new_header);
	tny_header_set_to (new_header, tny_header_get_from (header));
	tny_header_set_from (new_header, from);

	switch (reply_mode) {
		gchar *new_cc = NULL;
		const gchar *cc = NULL, *bcc = NULL;
		GString *tmp = NULL;

	case MODEST_MAIL_OPERATION_REPLY_MODE_SENDER:
		/* Do not fill neither cc nor bcc */
		break;
	case MODEST_MAIL_OPERATION_REPLY_MODE_LIST:
		/* TODO */
		break;
	case MODEST_MAIL_OPERATION_REPLY_MODE_ALL:
		/* Concatenate to, cc and bcc */
		cc = tny_header_get_cc (header);
		bcc = tny_header_get_bcc (header);

		tmp = g_string_new (tny_header_get_to (header));
		if (cc)  g_string_append_printf (tmp, ",%s",cc);
		if (bcc) g_string_append_printf (tmp, ",%s",bcc);

		/* Remove my own address from the cc list */
		new_cc = (gchar *) 
			modest_text_utils_remove_address ((const gchar *) tmp->str, 
							  (const gchar *) from);
		/* FIXME: remove also the mails from the new To: */
		tny_header_set_cc (new_header, new_cc);

		/* Clean */
		g_string_free (tmp, TRUE);
		g_free (new_cc);
		break;
	}

	/* Change the subject */
	new_subject = (gchar*) modest_text_utils_derived_subject (tny_header_get_subject(header),
								  _("Re:"));
	tny_header_set_subject (new_header, (const gchar *) new_subject);
	g_free (new_subject);

	/* Get body from original msg */
	new_body = (gchar*) modest_tny_msg_actions_find_body (msg, FALSE);
	if (!new_body) {
		g_object_unref (new_msg);
		return NULL;
	}
	content_type = get_content_type(new_body);

	switch (reply_type) {
		gchar *cited_text;

	case MODEST_MAIL_OPERATION_REPLY_TYPE_CITE:
		/* Prepend "Original message" text */
		cited_text = (gchar *) modest_text_utils_cited_text (tny_header_get_from (header),
								     tny_header_get_date_sent (header),
								     (const gchar*) new_body);
		g_free (new_body);
		new_body = cited_text;
		break;
	case MODEST_MAIL_OPERATION_REPLY_TYPE_QUOTE:
		/* FIXME: replace 80 with a value from ModestConf */
		quoted = (gchar*) modest_text_utils_quote (new_body, 
							   tny_header_get_from (header),
							   tny_header_get_date_sent (header),
							   80);
		g_free (new_body);
		new_body = quoted;
		break;
	}
	/* Add body part */
	text_body_part = add_body_part (new_msg, new_body, 
					(const gchar *) content_type, TRUE);

	/* Clean */
/* 	g_free (text_body_part); */
	g_free (content_type);
	g_free (new_body);

	return new_msg;
}

static void
status_update_cb (TnyFolder *folder, const gchar *what, gint status, gpointer user_data) 
{
	g_print ("%s status: %d\n", what, status);
}

static void
folder_refresh_cb (TnyFolder *folder, gboolean cancelled, GError **err, gpointer user_data)
{
	AsyncHelper *helper = NULL;
	ModestMailOperation *mail_op = NULL;
	ModestMailOperationPrivate *priv = NULL;

	helper = (AsyncHelper *) user_data;
	mail_op = MODEST_MAIL_OPERATION (helper->mail_op);
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(mail_op);

	g_mutex_lock (priv->cb_lock);

	priv->done++;

	if (cancelled)
		priv->status = MODEST_MAIL_OPERATION_STATUS_CANCELLED;
	else
		g_signal_emit (G_OBJECT (mail_op), signals[PROGRESS_CHANGED_SIGNAL], 0, NULL);

	g_mutex_unlock (priv->cb_lock);

	if (priv->done == priv->total) {
		((ModestUpdateAccountCallback) (helper->cb)) (mail_op, helper->user_data);
		g_slice_free (AsyncHelper, helper);
	}
}

void
modest_mail_operation_update_account (ModestMailOperation *mail_op,
				      TnyStoreAccount *store_account,
				      ModestUpdateAccountCallback callback,
				      gpointer user_data)
{
	ModestMailOperationPrivate *priv;
	TnyList *folders;
	TnyIterator *ifolders;
	TnyFolder *cur_folder;
	TnyFolderStoreQuery *query;
	AsyncHelper *helper;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (mail_op));
	g_return_if_fail (TNY_IS_STORE_ACCOUNT(store_account));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(mail_op);

	/* Get subscribed folders */
    	folders = TNY_LIST (tny_simple_list_new ());
	query = tny_folder_store_query_new ();
	tny_folder_store_query_add_item (query, NULL, TNY_FOLDER_STORE_QUERY_OPTION_SUBSCRIBED);
	tny_folder_store_get_folders (TNY_FOLDER_STORE (store_account),
				      folders, query, NULL); /* FIXME */
	g_object_unref (query);
	
	ifolders = tny_list_create_iterator (folders);
	priv->total = tny_list_get_length (folders);
	priv->done = 0;

	gint i =0;
	/* Async refresh folders */	
	for (tny_iterator_first (ifolders); 
	     !tny_iterator_is_done (ifolders); 
	     tny_iterator_next (ifolders)) {
		
		cur_folder = TNY_FOLDER (tny_iterator_get_current (ifolders));
		helper = g_slice_new0 (AsyncHelper);
		helper->mail_op = mail_op;
		helper->user_data = user_data;
		helper->cb = callback;
		tny_folder_refresh_async (cur_folder, folder_refresh_cb,
					  status_update_cb, helper);
	}
	
	g_object_unref (ifolders);
}

ModestMailOperationStatus
modest_mail_operation_get_status (ModestMailOperation *mail_op)
{
	ModestMailOperationPrivate *priv;

/* 	g_return_val_if_fail (mail_op, MODEST_MAIL_OPERATION_STATUS_INVALID); */
/* 	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (mail_op),  */
/* 			      MODEST_MAIL_OPERATION_STATUS_INVALID); */

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (mail_op);
	return priv->status;
}

const GError *
modest_mail_operation_get_error (ModestMailOperation *mail_op)
{
	ModestMailOperationPrivate *priv;

/* 	g_return_val_if_fail (mail_op, NULL); */
/* 	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (mail_op), NULL); */

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (mail_op);
	return priv->error;
}

void 
modest_mail_operation_cancel (ModestMailOperation *mail_op)
{
	/* TODO */
}

guint 
modest_mail_operation_get_task_done (ModestMailOperation *mail_op)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (mail_op), 0);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (mail_op);
	return priv->done;
}

guint 
modest_mail_operation_get_task_total (ModestMailOperation *mail_op)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (mail_op), 0);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (mail_op);
	return priv->total;
}

/* ******************************************************************* */
/* ************************** STORE  ACTIONS ************************* */
/* ******************************************************************* */


TnyFolder *
modest_mail_operation_create_folder (ModestMailOperation *mail_op,
				     TnyFolderStore *parent,
				     const gchar *name)
{
	g_return_val_if_fail (TNY_IS_FOLDER_STORE (parent), NULL);
	g_return_val_if_fail (name, NULL);

	TnyFolder *new_folder = NULL;
	TnyStoreAccount *store_account;

	/* Create the folder */
	new_folder = tny_folder_store_create_folder (parent, name, NULL); /* FIXME */
	if (!new_folder) 
		return NULL;

	/* Subscribe to folder */
	if (!tny_folder_is_subscribed (new_folder)) {
		store_account = tny_folder_get_account (TNY_FOLDER (parent));
		tny_store_account_subscribe (store_account, new_folder);
		g_object_unref (G_OBJECT (store_account));
	}

	return new_folder;
}

void
modest_mail_operation_remove_folder (ModestMailOperation *mail_op,
				     TnyFolder *folder,
				     gboolean remove_to_trash)
{
	TnyFolderStore *folder_store;

	g_return_if_fail (TNY_IS_FOLDER (folder));

	/* Get folder store */
	folder_store = TNY_FOLDER_STORE (tny_folder_get_account (folder));

	/* Delete folder or move to trash */
	if (remove_to_trash) {
		TnyFolder *trash_folder;

		trash_folder = modest_mail_operation_find_trash_folder (mail_op,
									TNY_STORE_ACCOUNT (folder_store));

		/* TODO: error_handling */
		modest_mail_operation_move_folder (mail_op, 
						   folder, 
						   TNY_FOLDER_STORE (trash_folder));
	} else {
		tny_folder_store_remove_folder (folder_store, folder, NULL); /* FIXME */
		g_object_unref (G_OBJECT (folder));
	}

	/* Free instances */
	g_object_unref (G_OBJECT (folder_store));
}

void
modest_mail_operation_rename_folder (ModestMailOperation *mail_op,
				     TnyFolder *folder,
				     const gchar *name)
{
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (mail_op));
	g_return_if_fail (TNY_IS_FOLDER_STORE (folder));

	/* FIXME: better error handling */
	if (strrchr (name, '/') != NULL)
		return;

	/* Rename. Camel handles folder subscription/unsubscription */
	tny_folder_set_name (folder, name, NULL); /* FIXME */
 }

void
modest_mail_operation_move_folder (ModestMailOperation *mail_op,
				   TnyFolder *folder,
				   TnyFolderStore *parent)
{
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (mail_op));
	g_return_if_fail (TNY_IS_FOLDER_STORE (parent));
	g_return_if_fail (TNY_IS_FOLDER (folder));
	
	modest_mail_operation_xfer_folder (mail_op, folder, parent, TRUE);
}

void
modest_mail_operation_copy_folder (ModestMailOperation *mail_op,
				   TnyFolder *folder,
				   TnyFolderStore *parent)
{
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (mail_op));
	g_return_if_fail (TNY_IS_FOLDER_STORE (parent));
	g_return_if_fail (TNY_IS_FOLDER (folder));

	modest_mail_operation_xfer_folder (mail_op, folder, parent, FALSE);
}

static void
modest_mail_operation_xfer_folder (ModestMailOperation *mail_op,
				   TnyFolder *folder,
				   TnyFolderStore *parent,
				   gboolean delete_original)
{
	const gchar *folder_name;
	TnyFolder *dest_folder, *child;
	TnyIterator *iter;
	TnyList *folders, *headers;

	g_return_if_fail (TNY_IS_FOLDER (folder));
	g_return_if_fail (TNY_IS_FOLDER_STORE (parent));

	/* Create the destination folder */
	folder_name = tny_folder_get_name (folder);
	dest_folder = modest_mail_operation_create_folder (mail_op, 
							   parent, folder_name);

	/* Transfer messages */
	headers = TNY_LIST (tny_simple_list_new ());
 	tny_folder_get_headers (folder, headers, FALSE, NULL); /* FIXME */
	tny_folder_transfer_msgs (folder, headers, dest_folder, delete_original, NULL); /* FIXME */

	/* Recurse children */
	folders = TNY_LIST (tny_simple_list_new ());
	tny_folder_store_get_folders (TNY_FOLDER_STORE (folder), folders, NULL, NULL ); /* FIXME */
	iter = tny_list_create_iterator (folders);

	while (!tny_iterator_is_done (iter)) {

		child = TNY_FOLDER (tny_iterator_get_current (iter));
		modest_mail_operation_xfer_folder (mail_op, child,
						   TNY_FOLDER_STORE (dest_folder),
						   delete_original);
		tny_iterator_next (iter);
	}

	/* Delete source folder (if needed) */
	if (delete_original)
		modest_mail_operation_remove_folder (mail_op, folder, FALSE);

	/* Clean up */
	g_object_unref (G_OBJECT (dest_folder));
	g_object_unref (G_OBJECT (headers));
	g_object_unref (G_OBJECT (folders));
	g_object_unref (G_OBJECT (iter));
}

static TnyFolder *
modest_mail_operation_find_trash_folder (ModestMailOperation *mail_op,
					 TnyStoreAccount *store_account)
{
	TnyList *folders;
	TnyIterator *iter;
	gboolean found;
	/*TnyFolderStoreQuery *query;*/
	TnyFolder *trash_folder;

	/* Look for Trash folder */
	folders = TNY_LIST (tny_simple_list_new ());
	tny_folder_store_get_folders (TNY_FOLDER_STORE (store_account),
				      folders, NULL, NULL); /* FIXME */
	iter = tny_list_create_iterator (folders);

	found = FALSE;
	while (!tny_iterator_is_done (iter) && !found) {

		trash_folder = TNY_FOLDER (tny_iterator_get_current (iter));
		if (tny_folder_get_folder_type (trash_folder) == TNY_FOLDER_TYPE_TRASH)
			found = TRUE;
		else
			tny_iterator_next (iter);
	}

	/* Clean up */
	g_object_unref (G_OBJECT (folders));
	g_object_unref (G_OBJECT (iter));

	/* TODO: better error handling management */
	if (!found) 
		return NULL;
	else
		return trash_folder;
}

/* ******************************************************************* */
/* **************************  MSG  ACTIONS  ************************* */
/* ******************************************************************* */

void 
modest_mail_operation_copy_msg (ModestMailOperation *mail_op,
				TnyHeader *header, 
				TnyFolder *folder)
{
	g_return_if_fail (TNY_IS_HEADER (header));
	g_return_if_fail (TNY_IS_FOLDER (folder));

	modest_mail_operation_xfer_msg (mail_op, header, folder, FALSE);
}

void 
modest_mail_operation_move_msg (ModestMailOperation *mail_op,
				TnyHeader *header, 
				TnyFolder *folder)
{
	g_return_if_fail (TNY_IS_HEADER (header));
	g_return_if_fail (TNY_IS_FOLDER (folder));

	modest_mail_operation_xfer_msg (mail_op, header, folder, TRUE);
}

void 
modest_mail_operation_remove_msg (ModestMailOperation *mail_op,
				  TnyHeader *header,
				  gboolean remove_to_trash)
{
	TnyFolder *folder;

	g_return_if_fail (TNY_IS_HEADER (header));

	folder = tny_header_get_folder (header);

	/* Delete or move to trash */
	if (remove_to_trash) {
		TnyFolder *trash_folder;
		TnyStoreAccount *store_account;

		store_account = TNY_STORE_ACCOUNT (tny_folder_get_account (folder));
		trash_folder = modest_mail_operation_find_trash_folder (mail_op, store_account);

		modest_mail_operation_move_msg (mail_op, header, trash_folder);

		g_object_unref (G_OBJECT (store_account));
	} else {
		tny_folder_remove_msg (folder, header, NULL); /* FIXME */
		tny_folder_expunge (folder, NULL); /* FIXME */
	}

	/* Free */
	g_object_unref (folder);
}

static void
modest_mail_operation_xfer_msg (ModestMailOperation *mail_op,
				TnyHeader *header, 
				TnyFolder *folder, 
				gboolean delete_original)
{
	TnyFolder *src_folder;
	TnyList *headers;

	src_folder = tny_header_get_folder (header);
	headers = tny_simple_list_new ();

	/* Move */
	tny_list_prepend (headers, G_OBJECT (header));
	tny_folder_transfer_msgs (src_folder, headers, folder, delete_original, NULL); /* FIXME */

	/* Free */
	g_object_unref (headers);
	g_object_unref (folder);
}


/* ******************************************************************* */
/* ************************* UTILIY FUNCTIONS ************************ */
/* ******************************************************************* */
static gboolean
is_ascii(const gchar *s)
{
	while (s[0]) {
		if (s[0] & 128 || s[0] < 32)
			return FALSE;
		s++;
	}
	return TRUE;
}

static char *
get_content_type(const gchar *s)
{
	GString *type;
	
	type = g_string_new("text/plain");
	if (!is_ascii(s)) {
		if (g_utf8_validate(s, -1, NULL)) {
			g_string_append(type, "; charset=\"utf-8\"");
		} else {
			/* it should be impossible to reach this, but better safe than sorry */
			g_warning("invalid utf8 in message");
			g_string_append(type, "; charset=\"latin1\"");
		}
	}
	return g_string_free(type, FALSE);
}

static GQuark 
modest_error_quark (void)
{
	static GQuark err_q = 0;
	
	if (err_q == 0)
		err_q = g_quark_from_static_string ("modest-error-quark");
	
	return err_q;
}


static void 
set_error (ModestMailOperation *mail_op, 
	   ModestMailOperationErrorCode error_code,
	   const gchar *fmt, ...)
{
	ModestMailOperationPrivate *priv;
	GError* error;
	va_list args;
	gchar* orig;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(mail_op);

	va_start (args, fmt);

	orig = g_strdup_vprintf(fmt, args);
	error = g_error_new (MODEST_ERROR, error_code, orig);

	va_end (args);

	if (priv->error)
		g_object_unref (priv->error);

	priv->error = error;
	priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
}

static void
add_attachments (TnyMsg *msg, const GList *attachments_list)
{
	GList *pos;
	TnyMimePart *attachment_part, *old_attachment;
	const gchar *attachment_content_type;
	const gchar *attachment_filename;
	TnyStream *attachment_stream;

	for (pos = (GList *)attachments_list; pos; pos = pos->next) {

		old_attachment = pos->data;
		attachment_filename = tny_mime_part_get_filename (old_attachment);
		attachment_stream = tny_mime_part_get_stream (old_attachment);
		attachment_part = TNY_MIME_PART (tny_camel_mime_part_new (camel_mime_part_new()));
		
		attachment_content_type = tny_mime_part_get_content_type (old_attachment);
				 
		tny_mime_part_construct_from_stream (attachment_part,
						     attachment_stream,
						     attachment_content_type);
		tny_stream_reset (attachment_stream);
		
		tny_mime_part_set_filename (attachment_part, attachment_filename);
		
		tny_mime_part_add_part (TNY_MIME_PART (msg), attachment_part);
/* 		g_object_unref (attachment_part); */
	}
}


static TnyMimePart *
add_body_part (TnyMsg *msg, 
	       const gchar *body,
	       const gchar *content_type,
	       gboolean has_attachments)
{
	TnyMimePart *text_body_part = NULL;
	TnyStream *text_body_stream;

	/* Create the stream */
	text_body_stream = TNY_STREAM (tny_camel_stream_new
				       (camel_stream_mem_new_with_buffer
					(body, strlen(body))));

	/* Create body part if needed */
	if (has_attachments)
		text_body_part = 
			TNY_MIME_PART (tny_camel_mime_part_new (camel_mime_part_new()));
	else
		text_body_part = TNY_MIME_PART(msg);

	/* Construct MIME part */
	tny_stream_reset (text_body_stream);
	tny_mime_part_construct_from_stream (text_body_part,
					     text_body_stream,
					     content_type);
	tny_stream_reset (text_body_stream);

	/* Add part if needed */
	if (has_attachments) {
		tny_mime_part_add_part (TNY_MIME_PART (msg), text_body_part);
		g_object_unref (G_OBJECT(text_body_part));
	}

	/* Clean */
	g_object_unref (text_body_stream);

	return text_body_part;
}
