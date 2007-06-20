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

#include <string.h>
#include <stdarg.h>
#include <tny-mime-part.h>
#include <tny-store-account.h>
#include <tny-folder-store.h>
#include <tny-folder-store-query.h>
#include <tny-camel-stream.h>
#include <tny-camel-pop-store-account.h>
#include <tny-simple-list.h>
#include <tny-send-queue.h>
#include <tny-status.h>
#include <tny-folder-observer.h>
#include <camel/camel-stream-mem.h>
#include <glib/gi18n.h>
#include "modest-platform.h"
#include <modest-tny-account.h>
#include <modest-tny-send-queue.h>
#include <modest-runtime.h>
#include "modest-text-utils.h"
#include "modest-tny-msg.h"
#include "modest-tny-folder.h"
#include "modest-tny-platform-factory.h"
#include "modest-marshal.h"
#include "modest-error.h"
#include "modest-mail-operation.h"

#define KB 1024

/* 'private'/'protected' functions */
static void modest_mail_operation_class_init (ModestMailOperationClass *klass);
static void modest_mail_operation_init       (ModestMailOperation *obj);
static void modest_mail_operation_finalize   (GObject *obj);

static void     get_msg_cb (TnyFolder *folder, 
			    gboolean cancelled, 
			    TnyMsg *msg, 
			    GError **err, 
			    gpointer user_data);

static void     get_msg_status_cb (GObject *obj,
				   TnyStatus *status,  
				   gpointer user_data);

static void     modest_mail_operation_notify_end (ModestMailOperation *self);

static gboolean did_a_cancel = FALSE;

enum _ModestMailOperationSignals 
{
	PROGRESS_CHANGED_SIGNAL,

	NUM_SIGNALS
};

typedef struct _ModestMailOperationPrivate ModestMailOperationPrivate;
struct _ModestMailOperationPrivate {
	TnyAccount                 *account;
	gchar										 *account_name;
	guint                      done;
	guint                      total;
	GObject                   *source;
	GError                    *error;
	ErrorCheckingUserCallback  error_checking;
	gpointer                   error_checking_user_data;
	ModestMailOperationStatus  status;	
	ModestMailOperationTypeOperation op_type;
};

#define MODEST_MAIL_OPERATION_GET_PRIVATE(o)      (G_TYPE_INSTANCE_GET_PRIVATE((o), \
                                                   MODEST_TYPE_MAIL_OPERATION, \
                                                   ModestMailOperationPrivate))

#define CHECK_EXCEPTION(priv, new_status)  if (priv->error) {\
                                                   priv->status = new_status;\
                                               }

typedef struct _GetMsgAsyncHelper {	
	ModestMailOperation *mail_op;
	TnyHeader *header;
	GetMsgAsyncUserCallback user_callback;	
	gpointer user_data;
} GetMsgAsyncHelper;

typedef struct _RefreshAsyncHelper {	
	ModestMailOperation *mail_op;
	RefreshAsyncUserCallback user_callback;	
	gpointer user_data;
} RefreshAsyncHelper;

typedef struct _XFerMsgAsyncHelper
{
	ModestMailOperation *mail_op;
	TnyList *headers;
	TnyFolder *dest_folder;
	XferMsgsAsynUserCallback user_callback;	
	gpointer user_data;
} XFerMsgAsyncHelper;

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

	/**
	 * ModestMailOperation::progress-changed
	 * @self: the #MailOperation that emits the signal
	 * @user_data: user data set when the signal handler was connected
	 *
	 * Emitted when the progress of a mail operation changes
	 */
 	signals[PROGRESS_CHANGED_SIGNAL] = 
		g_signal_new ("progress-changed",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (ModestMailOperationClass, progress_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static void
modest_mail_operation_init (ModestMailOperation *obj)
{
	ModestMailOperationPrivate *priv;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(obj);

	priv->account        = NULL;
	priv->status         = MODEST_MAIL_OPERATION_STATUS_INVALID;
	priv->op_type        = MODEST_MAIL_OPERATION_TYPE_UNKNOWN;
	priv->error          = NULL;
	priv->done           = 0;
	priv->total          = 0;
	priv->source         = NULL;
	priv->error_checking = NULL;
	priv->error_checking_user_data = NULL;
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
	if (priv->source) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}
	if (priv->account) {
		g_object_unref (priv->account);
		priv->account = NULL;
	}


	G_OBJECT_CLASS(parent_class)->finalize (obj);
}

ModestMailOperation*
modest_mail_operation_new (ModestMailOperationTypeOperation op_type, 
			   GObject *source)
{
	ModestMailOperation *obj;
	ModestMailOperationPrivate *priv;
		
	obj = MODEST_MAIL_OPERATION(g_object_new(MODEST_TYPE_MAIL_OPERATION, NULL));
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(obj);

	priv->op_type = op_type;
	if (source != NULL)
		priv->source = g_object_ref(source);

	return obj;
}

ModestMailOperation*
modest_mail_operation_new_with_error_handling (ModestMailOperationTypeOperation op_type,
					       GObject *source,
					       ErrorCheckingUserCallback error_handler,
					       gpointer user_data)
{
	ModestMailOperation *obj;
	ModestMailOperationPrivate *priv;
		
	obj = modest_mail_operation_new (op_type, source);
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(obj);
	
	g_return_val_if_fail (error_handler != NULL, obj);
	priv->error_checking = error_handler;

	return obj;
}

void
modest_mail_operation_execute_error_handler (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
	g_return_if_fail(priv->status != MODEST_MAIL_OPERATION_STATUS_SUCCESS);	    

	if (priv->error_checking != NULL)
		priv->error_checking (self, priv->error_checking_user_data);
}


ModestMailOperationTypeOperation
modest_mail_operation_get_type_operation (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
	
	return priv->op_type;
}

gboolean 
modest_mail_operation_is_mine (ModestMailOperation *self, 
			       GObject *me)
{
	ModestMailOperationPrivate *priv;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
	if (priv->source == NULL) return FALSE;

	return priv->source == me;
}

GObject *
modest_mail_operation_get_source (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (self, NULL);
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
	if (!priv) {
		g_warning ("BUG: %s: priv == NULL", __FUNCTION__);
		return NULL;
	}
	
	return g_object_ref (priv->source);
}

ModestMailOperationStatus
modest_mail_operation_get_status (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (self, MODEST_MAIL_OPERATION_STATUS_INVALID);
	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self),
			      MODEST_MAIL_OPERATION_STATUS_INVALID);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	if (!priv) {
		g_warning ("BUG: %s: priv == NULL", __FUNCTION__);
		return MODEST_MAIL_OPERATION_STATUS_INVALID;
	}
	
	return priv->status;
}

const GError *
modest_mail_operation_get_error (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self), NULL);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	if (!priv) {
		g_warning ("BUG: %s: priv == NULL", __FUNCTION__);
		return NULL;
	}

	return priv->error;
}

gboolean 
modest_mail_operation_cancel (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	if (!MODEST_IS_MAIL_OPERATION (self)) {
		g_warning ("%s: invalid parametter", G_GNUC_FUNCTION);
		return FALSE;
	}

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	if (!priv) {
		g_warning ("BUG: %s: priv == NULL", __FUNCTION__);
		return FALSE;
	}

	/* Notify about operation end */
	modest_mail_operation_notify_end (self);

	did_a_cancel = TRUE;

	/* Set new status */
	priv->status = MODEST_MAIL_OPERATION_STATUS_CANCELED;
	
	modest_mail_operation_queue_cancel_all (modest_runtime_get_mail_operation_queue());

	
	return TRUE;
}

guint 
modest_mail_operation_get_task_done (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self), 0);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	return priv->done;
}

guint 
modest_mail_operation_get_task_total (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self), 0);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	return priv->total;
}

gboolean
modest_mail_operation_is_finished (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;
	gboolean retval = FALSE;

	if (!MODEST_IS_MAIL_OPERATION (self)) {
		g_warning ("%s: invalid parametter", G_GNUC_FUNCTION);
		return retval;
	}

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	if (priv->status == MODEST_MAIL_OPERATION_STATUS_SUCCESS   ||
	    priv->status == MODEST_MAIL_OPERATION_STATUS_FAILED    ||
	    priv->status == MODEST_MAIL_OPERATION_STATUS_CANCELED  ||
	    priv->status == MODEST_MAIL_OPERATION_STATUS_FINISHED_WITH_ERRORS) {
		retval = TRUE;
	} else {
		retval = FALSE;
	}

	return retval;
}

guint
modest_mail_operation_get_id (ModestMailOperation *self)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self), 0);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	return priv->done;
}

guint 
modest_mail_operation_set_id (ModestMailOperation *self,
			      guint id)
{
	ModestMailOperationPrivate *priv;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self), 0);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	return priv->done;
}

/*
 * Creates an image of the current state of a mail operation, the
 * caller must free it
 */
static ModestMailOperationState *
modest_mail_operation_clone_state (ModestMailOperation *self)
{
	ModestMailOperationState *state;
	ModestMailOperationPrivate *priv;

	/* FIXME: this should be fixed properly
	 * 
	 * in some cases, priv was NULL, so checking here to
	 * make sure.
	 */
	g_return_val_if_fail (self, NULL);
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	g_return_val_if_fail (priv, NULL);

	if (!priv)
		return NULL;

	state = g_slice_new (ModestMailOperationState);

	state->status = priv->status;
	state->op_type = priv->op_type;
	state->done = priv->done;
	state->total = priv->total;
	state->finished = modest_mail_operation_is_finished (self);
	state->bytes_done = 0;
	state->bytes_total = 0;

	return state;
}

/* ******************************************************************* */
/* ************************** SEND   ACTIONS ************************* */
/* ******************************************************************* */

void
modest_mail_operation_send_mail (ModestMailOperation *self,
				 TnyTransportAccount *transport_account,
				 TnyMsg* msg)
{
	TnySendQueue *send_queue = NULL;
	ModestMailOperationPrivate *priv;
	
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_TRANSPORT_ACCOUNT (transport_account));
	g_return_if_fail (TNY_IS_MSG (msg));
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	/* Get account and set it into mail_operation */
	priv->account = g_object_ref (transport_account);
	priv->done = 1;
	priv->total = 1;

	send_queue = TNY_SEND_QUEUE (modest_runtime_get_send_queue (transport_account));
	if (!TNY_IS_SEND_QUEUE(send_queue)) {
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     "modest: could not find send queue for account\n");
	} else {
		/* TODO: connect to the msg-sent in order to know when
		   the mail operation is finished */
		tny_send_queue_add (send_queue, msg, &(priv->error));
		/* TODO: we're setting always success, do the check in
		   the handler */
		priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;
	}

	/* TODO: do this in the handler of the "msg-sent"
	   signal.Notify about operation end */
	modest_mail_operation_notify_end (self);
}

void
modest_mail_operation_send_new_mail (ModestMailOperation *self,
				     TnyTransportAccount *transport_account,
				     TnyMsg *draft_msg,
				     const gchar *from,  const gchar *to,
				     const gchar *cc,  const gchar *bcc,
				     const gchar *subject, const gchar *plain_body,
				     const gchar *html_body,
				     const GList *attachments_list,
				     TnyHeaderFlags priority_flags)
{
	TnyMsg *new_msg = NULL;
	TnyFolder *folder = NULL;
	TnyHeader *header = NULL;
	ModestMailOperationPrivate *priv = NULL;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_TRANSPORT_ACCOUNT (transport_account));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	/* Check parametters */
	if (to == NULL) {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_BAD_PARAMETER,
			     _("Error trying to send a mail. You need to set at least one recipient"));
		return;
	}

	if (html_body == NULL) {
		new_msg = modest_tny_msg_new (to, from, cc, bcc, subject, plain_body, (GSList *) attachments_list); /* FIXME: attachments */
	} else {
		new_msg = modest_tny_msg_new_html_plain (to, from, cc, bcc, subject, html_body, plain_body, (GSList *) attachments_list);
	}
	if (!new_msg) {
		g_printerr ("modest: failed to create a new msg\n");
		return;
	}

	/* Set priority flags in message */
	header = tny_msg_get_header (new_msg);
	if (priority_flags != 0)
		tny_header_set_flags (header, priority_flags);

	/* Call mail operation */
	modest_mail_operation_send_mail (self, transport_account, new_msg);

	folder = modest_tny_account_get_special_folder (TNY_ACCOUNT (transport_account), TNY_FOLDER_TYPE_DRAFTS);
	if (folder) {
		if (draft_msg != NULL) {
			header = tny_msg_get_header (draft_msg);
			/* Note: This can fail (with a warning) if the message is not really already in a folder,
			 * because this function requires it to have a UID. */
			tny_folder_remove_msg (folder, header, NULL);
			g_object_unref (header);
		}
	}

	/* Free */
	g_object_unref (G_OBJECT (new_msg));
}

void
modest_mail_operation_save_to_drafts (ModestMailOperation *self,
				      TnyTransportAccount *transport_account,
				      TnyMsg *draft_msg,
				      const gchar *from,  const gchar *to,
				      const gchar *cc,  const gchar *bcc,
				      const gchar *subject, const gchar *plain_body,
				      const gchar *html_body,
				      const GList *attachments_list,
				      TnyHeaderFlags priority_flags)
{
	TnyMsg *msg = NULL;
	TnyFolder *folder = NULL;
	TnyHeader *header = NULL;
	ModestMailOperationPrivate *priv = NULL;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_TRANSPORT_ACCOUNT (transport_account));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	/* Get account and set it into mail_operation */
	priv->account = g_object_ref (transport_account);

	if (html_body == NULL) {
		msg = modest_tny_msg_new (to, from, cc, bcc, subject, plain_body, (GSList *) attachments_list); /* FIXME: attachments */
	} else {
		msg = modest_tny_msg_new_html_plain (to, from, cc, bcc, subject, html_body, plain_body, (GSList *) attachments_list);
	}
	if (!msg) {
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_INSTANCE_CREATION_FAILED,
			     "modest: failed to create a new msg\n");
		goto end;
	}

	/* add priority flags */
	header = tny_msg_get_header (msg);
	tny_header_set_flags (header, priority_flags);

	folder = modest_tny_account_get_special_folder (TNY_ACCOUNT (transport_account), TNY_FOLDER_TYPE_DRAFTS);
	if (!folder) {
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     "modest: failed to create a new msg\n");
		goto end;
	}

	if (draft_msg != NULL) {
		header = tny_msg_get_header (draft_msg);
		/* Remove the old draft expunging it */
		tny_folder_remove_msg (folder, header, NULL);
		tny_folder_sync (folder, TRUE, NULL);
		g_object_unref (header);
	}
	
	tny_folder_add_msg (folder, msg, &(priv->error));
	if (priv->error)
		goto end;

end:
	if (msg)
		g_object_unref (G_OBJECT(msg));
	if (folder)
		g_object_unref (G_OBJECT(folder));

 	modest_mail_operation_notify_end (self);
}

typedef struct 
{
	ModestMailOperation *mail_op;
	TnyStoreAccount *account;
	TnyTransportAccount *transport_account;
	gint max_size;
	gint retrieve_limit;
	gchar *retrieve_type;
	gchar *account_name;
} UpdateAccountInfo;

/***** I N T E R N A L    F O L D E R    O B S E R V E R *****/
/* We use this folder observer to track the headers that have been
 * added to a folder */
typedef struct {
	GObject parent;
	TnyList *new_headers;
} InternalFolderObserver;

typedef struct {
	GObjectClass parent;
} InternalFolderObserverClass;

static void tny_folder_observer_init (TnyFolderObserverIface *idace);

G_DEFINE_TYPE_WITH_CODE (InternalFolderObserver,
			 internal_folder_observer,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE(TNY_TYPE_FOLDER_OBSERVER, tny_folder_observer_init));


static void
foreach_add_item (gpointer header, gpointer user_data)
{
	/* printf("DEBUG: %s: header subject=%s\n", 
	 * __FUNCTION__, tny_header_get_subject(TNY_HEADER(header)));
	 */
	tny_list_prepend (TNY_LIST (user_data), 
			  g_object_ref (G_OBJECT (header)));
}

/* This is the method that looks for new messages in a folder */
static void
internal_folder_observer_update (TnyFolderObserver *self, TnyFolderChange *change)
{
	InternalFolderObserver *derived = (InternalFolderObserver *)self;
	
	TnyFolderChangeChanged changed;

	changed = tny_folder_change_get_changed (change);

	if (changed & TNY_FOLDER_CHANGE_CHANGED_ADDED_HEADERS) {
		TnyList *list;

		/* Get added headers */
		list = tny_simple_list_new ();
		tny_folder_change_get_added_headers (change, list);

		/* printf ("DEBUG: %s: Calling foreach with a list of size=%d\n", 
		 * 	__FUNCTION__, tny_list_get_length(list));
		 */
		 
		/* Add them to the folder observer */
		tny_list_foreach (list, foreach_add_item, 
				  derived->new_headers);

		g_object_unref (G_OBJECT (list));
	}
}

static void
internal_folder_observer_init (InternalFolderObserver *self) 
{
	self->new_headers = tny_simple_list_new ();
}
static void
internal_folder_observer_finalize (GObject *object) 
{
	InternalFolderObserver *self;

	self = (InternalFolderObserver *) object;
	g_object_unref (self->new_headers);

	G_OBJECT_CLASS (internal_folder_observer_parent_class)->finalize (object);
}
static void
tny_folder_observer_init (TnyFolderObserverIface *iface) 
{
	iface->update_func = internal_folder_observer_update;
}
static void
internal_folder_observer_class_init (InternalFolderObserverClass *klass) 
{
	GObjectClass *object_class;

	internal_folder_observer_parent_class = g_type_class_peek_parent (klass);
	object_class = (GObjectClass*) klass;
	object_class->finalize = internal_folder_observer_finalize;
}

/*****************/

static void
recurse_folders (TnyFolderStore *store, TnyFolderStoreQuery *query, TnyList *all_folders)
{
	TnyIterator *iter;
	TnyList *folders = tny_simple_list_new ();

	tny_folder_store_get_folders (store, folders, query, NULL);
	iter = tny_list_create_iterator (folders);

	while (!tny_iterator_is_done (iter)) {

		TnyFolderStore *folder = (TnyFolderStore*) tny_iterator_get_current (iter);

		tny_list_prepend (all_folders, G_OBJECT (folder));
		recurse_folders (folder, query, all_folders);    
 		g_object_unref (G_OBJECT (folder));

		tny_iterator_next (iter);
	}
	 g_object_unref (G_OBJECT (iter));
	 g_object_unref (G_OBJECT (folders));
}

/* 
 * Issues the "progress-changed" signal. The timer won't be removed,
 * so you must call g_source_remove to stop the signal emission
 */
static gboolean
idle_notify_progress (gpointer data)
{
	ModestMailOperation *mail_op = MODEST_MAIL_OPERATION (data);
	ModestMailOperationState *state;

	state = modest_mail_operation_clone_state (mail_op);
	g_signal_emit (G_OBJECT (mail_op), signals[PROGRESS_CHANGED_SIGNAL], 0, state, NULL);
	g_slice_free (ModestMailOperationState, state);
	
	return TRUE;
}

/* 
 * Issues the "progress-changed" signal and removes the timer. It uses
 * a lock to ensure that the progress information of the mail
 * operation is not modified while there are notifications pending
 */
static gboolean
idle_notify_progress_once (gpointer data)
{
	ModestPair *pair;

	pair = (ModestPair *) data;

	g_signal_emit (G_OBJECT (pair->first), signals[PROGRESS_CHANGED_SIGNAL], 0, pair->second, NULL);

	/* Free the state and the reference to the mail operation */
	g_slice_free (ModestMailOperationState, (ModestMailOperationState*)pair->second);
	g_object_unref (pair->first);

	return FALSE;
}

/* 
 * Used by update_account_thread to notify the queue from the main
 * loop. We call it inside an idle call to achieve that
 */
static gboolean
notify_update_account_queue (gpointer data)
{
	ModestMailOperation *mail_op = MODEST_MAIL_OPERATION (data);
	ModestMailOperationPrivate *priv = NULL;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(mail_op);
	
	modest_mail_operation_notify_end (mail_op);
	g_object_unref (mail_op);

	return FALSE;
}

static int
compare_headers_by_date (gconstpointer a, 
			 gconstpointer b)
{
	TnyHeader **header1, **header2;
	time_t sent1, sent2;

	header1 = (TnyHeader **) a;
	header2 = (TnyHeader **) b;

	sent1 = tny_header_get_date_sent (*header1);
	sent2 = tny_header_get_date_sent (*header2);

	/* We want the most recent ones (greater time_t) at the
	   beginning */
	if (sent1 < sent2)
		return 1;
	else
		return -1;
}

static gboolean 
set_last_updated_idle (gpointer data)
{
	/* It does not matter if the time is not exactly the same than
	   the time when this idle was called, it's just an
	   approximation and it won't be very different */
	modest_account_mgr_set_int (modest_runtime_get_account_mgr (), 
				    (gchar *) data, 
				    MODEST_ACCOUNT_LAST_UPDATED, 
				    time(NULL), 
				    TRUE);

	return FALSE;
}

static gpointer
update_account_thread (gpointer thr_user_data)
{
	static gboolean first_time = TRUE;
	UpdateAccountInfo *info;
	TnyList *all_folders = NULL;
	GPtrArray *new_headers;
	TnyIterator *iter = NULL;
	TnyFolderStoreQuery *query = NULL;
	ModestMailOperationPrivate *priv;
	ModestTnySendQueue *send_queue;

	info = (UpdateAccountInfo *) thr_user_data;
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(info->mail_op);

	/* Get account and set it into mail_operation */
	priv->account = g_object_ref (info->account);

	/*
	 * for POP3, we do a logout-login upon send/receive -- many POP-servers (like Gmail) do not
	 * show any updates unless we do that
	 */
	if (!first_time && TNY_IS_CAMEL_POP_STORE_ACCOUNT(priv->account)) 
		tny_camel_pop_store_account_reconnect (TNY_CAMEL_POP_STORE_ACCOUNT(priv->account));

	/* Get all the folders. We can do it synchronously because
	   we're already running in a different thread than the UI */
	all_folders = tny_simple_list_new ();
	query = tny_folder_store_query_new ();
	tny_folder_store_query_add_item (query, NULL, TNY_FOLDER_STORE_QUERY_OPTION_SUBSCRIBED);
	tny_folder_store_get_folders (TNY_FOLDER_STORE (info->account),
				      all_folders,
				      query,
				      &(priv->error));
	if (priv->error) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		goto out;
	}

	iter = tny_list_create_iterator (all_folders);
	while (!tny_iterator_is_done (iter)) {
		TnyFolderStore *folder = TNY_FOLDER_STORE (tny_iterator_get_current (iter));

		recurse_folders (folder, query, all_folders);
		tny_iterator_next (iter);
	}
	g_object_unref (G_OBJECT (iter));

	/* Update status and notify. We need to call the notification
	   with a source function in order to call it from the main
	   loop. We need that in order not to get into trouble with
	   Gtk+. We use a timeout in order to provide more status
	   information, because the sync tinymail call does not
	   provide it for the moment */
	gint timeout = g_timeout_add (250, idle_notify_progress, info->mail_op);

	/* Refresh folders */
	new_headers = g_ptr_array_new ();
	iter = tny_list_create_iterator (all_folders);

	while (!tny_iterator_is_done (iter) && !priv->error && !did_a_cancel) {

		InternalFolderObserver *observer;
		TnyFolderStore *folder = TNY_FOLDER_STORE (tny_iterator_get_current (iter));

		/* Refresh the folder */
		/* Our observer receives notification of new emails during folder refreshes,
		 * so we can use observer->new_headers.
		 * TODO: This does not seem to be providing accurate numbers.
		 * Possibly the observer is notified asynchronously.
		 */
		observer = g_object_new (internal_folder_observer_get_type (), NULL);
		tny_folder_add_observer (TNY_FOLDER (folder), TNY_FOLDER_OBSERVER (observer));
		
		/* This gets the status information (headers) from the server.
		 * We use the blocking version, because we are already in a separate 
		 * thread.
		 */

		if (!g_ascii_strcasecmp (info->retrieve_type, MODEST_ACCOUNT_RETRIEVE_VALUE_MESSAGES) || 
		    !g_ascii_strcasecmp (info->retrieve_type, MODEST_ACCOUNT_RETRIEVE_VALUE_MESSAGES_AND_ATTACHMENTS)) {
			TnyIterator *iter;

			/* If the retrieve type is full messages, refresh and get the messages */
			tny_folder_refresh (TNY_FOLDER (folder), &(priv->error));

			iter = tny_list_create_iterator (observer->new_headers);
			while (!tny_iterator_is_done (iter)) {
				TnyHeader *header = TNY_HEADER (tny_iterator_get_current (iter));
				 
				/* Apply per-message size limits */
				if (tny_header_get_message_size (header) < info->max_size)
					g_ptr_array_add (new_headers, g_object_ref (header));

				g_object_unref (header);
				tny_iterator_next (iter);
			}
			g_object_unref (iter);
		} else {
			/* We do not need to do it the first time
			   because it's automatically done by the tree
			   model */
			if (G_UNLIKELY (!first_time))
				tny_folder_poke_status (TNY_FOLDER (folder));
		}
		tny_folder_remove_observer (TNY_FOLDER (folder), TNY_FOLDER_OBSERVER (observer));
		g_object_unref (observer);
		observer = NULL;

		if (priv->error)
			priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;

		g_object_unref (G_OBJECT (folder));
		tny_iterator_next (iter);
	}

	did_a_cancel = FALSE;

	g_object_unref (G_OBJECT (iter));
	g_source_remove (timeout);

	if (new_headers->len > 0) {
		gint msg_num = 0;

		/* Order by date */
		g_ptr_array_sort (new_headers, (GCompareFunc) compare_headers_by_date);

		/* Apply message count limit */
		/* If the number of messages exceeds the maximum, ask the
		 * user to download them all,
		 * as per the UI spec "Retrieval Limits" section in 4.4: 
		 */
		printf ("************************** DEBUG: %s: account=%s, len=%d, retrieve_limit = %d\n", __FUNCTION__, 
			tny_account_get_id (priv->account), new_headers->len, info->retrieve_limit);
		if (new_headers->len > info->retrieve_limit) {
			/* TODO: Ask the user, instead of just failing, showing mail_nc_msg_count_limit_exceeded, 
			 * with 'Get all' and 'Newest only' buttons. */
			g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_RETRIEVAL_NUMBER_LIMIT,
			     "The number of messages to retrieve exceeds the chosen limit for account %s\n", 
			     tny_account_get_name (TNY_ACCOUNT (info->transport_account)));
			priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
			goto out;
		}
		
		priv->done = 0;
		priv->total = MIN (new_headers->len, info->retrieve_limit);
		while (msg_num < priv->total) {

			TnyHeader *header = TNY_HEADER (g_ptr_array_index (new_headers, msg_num));
			TnyFolder *folder = tny_header_get_folder (header);
			TnyMsg *msg       = tny_folder_get_msg (folder, header, NULL);
			ModestMailOperationState *state;
			ModestPair* pair;

			priv->done++;
			/* We can not just use the mail operation because the
			   values of done and total could change before the
			   idle is called */
			state = modest_mail_operation_clone_state (info->mail_op);
			pair = modest_pair_new (g_object_ref (info->mail_op), state, FALSE);
			g_idle_add_full (G_PRIORITY_HIGH_IDLE, idle_notify_progress_once,
					 pair, (GDestroyNotify) modest_pair_free);

			g_object_unref (msg);
			g_object_unref (folder);

			msg_num++;
		}
		g_ptr_array_foreach (new_headers, (GFunc) g_object_unref, NULL);
		g_ptr_array_free (new_headers, FALSE);
	}

	/* Perform send */
	priv->op_type = MODEST_MAIL_OPERATION_TYPE_SEND;
	priv->done = 0;
	priv->total = 0;
	if (priv->account != NULL) 
		g_object_unref (priv->account);
	priv->account = g_object_ref (info->transport_account);
	
	send_queue = modest_runtime_get_send_queue (info->transport_account);
	if (send_queue) {
		timeout = g_timeout_add (250, idle_notify_progress, info->mail_op);
		modest_tny_send_queue_try_to_send (send_queue);
		g_source_remove (timeout);
	} else {
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_INSTANCE_CREATION_FAILED,
			     "cannot create a send queue for %s\n", 
			     tny_account_get_name (TNY_ACCOUNT (info->transport_account)));
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
	}
	
	/* Check if the operation was a success */
	if (!priv->error) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;

		/* Update the last updated key */
		g_idle_add_full (G_PRIORITY_HIGH_IDLE, 
				 set_last_updated_idle, 
				 g_strdup (tny_account_get_id (TNY_ACCOUNT (info->account))),
				 (GDestroyNotify) g_free);
	}

 out:
	/* Notify about operation end. Note that the info could be
	   freed before this idle happens, but the mail operation will
	   be still alive */
	g_idle_add (notify_update_account_queue, g_object_ref (info->mail_op));
	
	/* Frees */
	g_object_unref (query);
	g_object_unref (all_folders);
	g_object_unref (info->account);
	g_object_unref (info->transport_account);
	g_free (info->retrieve_type);
	g_slice_free (UpdateAccountInfo, info);

	first_time = FALSE;

	return NULL;
}

gboolean
modest_mail_operation_update_account (ModestMailOperation *self,
				      const gchar *account_name)
{
	GThread *thread;
	UpdateAccountInfo *info;
	ModestMailOperationPrivate *priv;
	ModestAccountMgr *mgr;
	TnyStoreAccount *modest_account;
	TnyTransportAccount *transport_account;

	g_return_val_if_fail (MODEST_IS_MAIL_OPERATION (self), FALSE);
	g_return_val_if_fail (account_name, FALSE);

	/* Make sure that we have a connection, and request one 
	 * if necessary:
	 * TODO: Is there some way to trigger this for every attempt to 
	 * use the network? */
	if (!modest_platform_connect_and_wait(NULL))
		return FALSE;
	
	/* Init mail operation. Set total and done to 0, and do not
	   update them, this way the progress objects will know that
	   we have no clue about the number of the objects */
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
	priv->total = 0;
	priv->done  = 0;
	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;
	
	/* Get the Modest account */
	modest_account = (TnyStoreAccount *)
		modest_tny_account_store_get_server_account (modest_runtime_get_account_store (),
								     account_name,
								     TNY_ACCOUNT_TYPE_STORE);

	if (!modest_account) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     "cannot get tny store account for %s\n", account_name);
		modest_mail_operation_notify_end (self);

		return FALSE;
	}

	
	/* Get the transport account, we can not do it in the thread
	   due to some problems with dbus */
	transport_account = (TnyTransportAccount *)
		modest_tny_account_store_get_transport_account_for_open_connection (modest_runtime_get_account_store(),
										    account_name);
	if (!transport_account) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     "cannot get tny transport account for %s\n", account_name);
		modest_mail_operation_notify_end (self);

		return FALSE;
	}

	/* Create the helper object */
	info = g_slice_new (UpdateAccountInfo);
	info->mail_op = self;
	info->account = modest_account;
	info->transport_account = transport_account;

	/* Get the message size limit */
	info->max_size  = modest_conf_get_int (modest_runtime_get_conf (), 
					       MODEST_CONF_MSG_SIZE_LIMIT, NULL);
	if (info->max_size == 0)
		info->max_size = G_MAXINT;
	else
		info->max_size = info->max_size * KB;

	/* Get per-account retrieval type */
	mgr = modest_runtime_get_account_mgr ();
	info->retrieve_type = modest_account_mgr_get_string (mgr, account_name, 
							     MODEST_ACCOUNT_RETRIEVE, FALSE);

	/* Get per-account message amount retrieval limit */
	info->retrieve_limit = modest_account_mgr_get_int (mgr, account_name, 
							   MODEST_ACCOUNT_LIMIT_RETRIEVE, FALSE);
	if (info->retrieve_limit == 0)
		info->retrieve_limit = G_MAXINT;
		
	/* printf ("DEBUG: %s: info->retrieve_limit = %d\n", __FUNCTION__, info->retrieve_limit); */

	/* Set account busy */
	modest_account_mgr_set_account_busy(mgr, account_name, TRUE);
	priv->account_name = g_strdup(account_name);
	
	thread = g_thread_create (update_account_thread, info, FALSE, NULL);

	return TRUE;
}

/* ******************************************************************* */
/* ************************** STORE  ACTIONS ************************* */
/* ******************************************************************* */


TnyFolder *
modest_mail_operation_create_folder (ModestMailOperation *self,
				     TnyFolderStore *parent,
				     const gchar *name)
{
	ModestMailOperationPrivate *priv;
	TnyFolder *new_folder = NULL;

	g_return_val_if_fail (TNY_IS_FOLDER_STORE (parent), NULL);
	g_return_val_if_fail (name, NULL);
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	/* Check parent */
	if (TNY_IS_FOLDER (parent)) {
		/* Check folder rules */
		ModestTnyFolderRules rules = modest_tny_folder_get_rules (TNY_FOLDER (parent));
		if (rules & MODEST_FOLDER_RULES_FOLDER_NON_WRITEABLE) {
			/* Set status failed and set an error */
			priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
			g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
				     MODEST_MAIL_OPERATION_ERROR_FOLDER_RULES,
				     _("mail_in_ui_folder_create_error"));
		}
	}

	if (!priv->error) {
		/* Create the folder */
		new_folder = tny_folder_store_create_folder (parent, name, &(priv->error));
		CHECK_EXCEPTION (priv, MODEST_MAIL_OPERATION_STATUS_FAILED);
		if (!priv->error)
			priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;
	}

	/* Notify about operation end */
	modest_mail_operation_notify_end (self);

	return new_folder;
}

void
modest_mail_operation_remove_folder (ModestMailOperation *self,
				     TnyFolder           *folder,
				     gboolean             remove_to_trash)
{
	TnyAccount *account;
	ModestMailOperationPrivate *priv;
	ModestTnyFolderRules rules;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_FOLDER (folder));
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	
	/* Check folder rules */
	rules = modest_tny_folder_get_rules (TNY_FOLDER (folder));
	if (rules & MODEST_FOLDER_RULES_FOLDER_NON_DELETABLE) {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_FOLDER_RULES,
			     _("mail_in_ui_folder_delete_error"));
		goto end;
	}

	/* Get the account */
	account = modest_tny_folder_get_account (folder);
	priv->account = g_object_ref(account);

	/* Delete folder or move to trash */
	if (remove_to_trash) {
		TnyFolder *trash_folder = NULL;
		trash_folder = modest_tny_account_get_special_folder (account,
								      TNY_FOLDER_TYPE_TRASH);
		/* TODO: error_handling */
		 modest_mail_operation_xfer_folder (self, folder,
						    TNY_FOLDER_STORE (trash_folder), TRUE);
	} else {
		TnyFolderStore *parent = tny_folder_get_folder_store (folder);

		tny_folder_store_remove_folder (parent, folder, &(priv->error));
		CHECK_EXCEPTION (priv, MODEST_MAIL_OPERATION_STATUS_FAILED);

		if (parent)
			g_object_unref (G_OBJECT (parent));
	}
	g_object_unref (G_OBJECT (account));

 end:
	/* Notify about operation end */
	modest_mail_operation_notify_end (self);
}

static void
transfer_folder_status_cb (GObject *obj,
			   TnyStatus *status,
			   gpointer user_data)
{
	ModestMailOperation *self;
	ModestMailOperationPrivate *priv;
	ModestMailOperationState *state;

	g_return_if_fail (status != NULL);
	g_return_if_fail (status->code == TNY_FOLDER_STATUS_CODE_COPY_FOLDER);

	self = MODEST_MAIL_OPERATION (user_data);
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	if ((status->position == 1) && (status->of_total == 100))
		return;

	priv->done = status->position;
	priv->total = status->of_total;

	state = modest_mail_operation_clone_state (self);
	g_signal_emit (G_OBJECT (self), signals[PROGRESS_CHANGED_SIGNAL], 0, state, NULL);
	g_slice_free (ModestMailOperationState, state);
}


static void
transfer_folder_cb (TnyFolder *folder, 
		    TnyFolderStore *into, 
		    gboolean cancelled, 
		    TnyFolder *new_folder, GError **err, 
		    gpointer user_data)
{
	ModestMailOperation *self = NULL;
	ModestMailOperationPrivate *priv = NULL;

	self = MODEST_MAIL_OPERATION (user_data);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	if (*err) {
		priv->error = g_error_copy (*err);
		priv->done = 0;
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
	} else if (cancelled) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_CANCELED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_OPERATION_CANCELED,
			     _("Transference of %s was cancelled."),
			     tny_folder_get_name (folder));
	} else {
		priv->done = 1;
		priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;
	}
		
	/* Free */
	g_object_unref (folder);
	g_object_unref (into);
	if (new_folder != NULL)
		g_object_unref (new_folder);

	/* Notify about operation end */
	modest_mail_operation_notify_end (self);
}

void
modest_mail_operation_xfer_folder (ModestMailOperation *self,
				   TnyFolder *folder,
				   TnyFolderStore *parent,
				   gboolean delete_original)
{
	ModestMailOperationPrivate *priv = NULL;
	ModestTnyFolderRules parent_rules, rules;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_FOLDER (folder));
	g_return_if_fail (TNY_IS_FOLDER (parent));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	/* Get account and set it into mail_operation */
	priv->account = modest_tny_folder_get_account (TNY_FOLDER(folder));
	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;

	/* Get folder rules */
	rules = modest_tny_folder_get_rules (TNY_FOLDER (folder));
	parent_rules = modest_tny_folder_get_rules (TNY_FOLDER (parent));

	if (!TNY_IS_FOLDER_STORE (parent)) {
		
	}
	
	/* The moveable restriction is applied also to copy operation */
	if ((!TNY_IS_FOLDER_STORE (parent)) || (rules & MODEST_FOLDER_RULES_FOLDER_NON_MOVEABLE)) {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_FOLDER_RULES,
			     _("mail_in_ui_folder_move_target_error"));

		/* Notify the queue */
		modest_mail_operation_notify_end (self);
	} else if (parent_rules & MODEST_FOLDER_RULES_FOLDER_NON_WRITEABLE) {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_FOLDER_RULES,
			     _("FIXME: parent folder does not accept new folders"));

		/* Notify the queue */
		modest_mail_operation_notify_end (self);
	} else {
		/* Pick references for async calls */
		g_object_ref (folder);
		g_object_ref (parent);

		/* Move/Copy folder */		
		tny_folder_copy_async (folder,
				       parent,
				       tny_folder_get_name (folder),
				       delete_original,
				       transfer_folder_cb,
				       transfer_folder_status_cb,
				       self);
	}
}

void
modest_mail_operation_rename_folder (ModestMailOperation *self,
				     TnyFolder *folder,
				     const gchar *name)
{
	ModestMailOperationPrivate *priv;
	ModestTnyFolderRules rules;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_FOLDER_STORE (folder));
	g_return_if_fail (name);
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	/* Get account and set it into mail_operation */
	priv->account = modest_tny_folder_get_account (TNY_FOLDER(folder));

	/* Check folder rules */
	rules = modest_tny_folder_get_rules (TNY_FOLDER (folder));
	if (rules & MODEST_FOLDER_RULES_FOLDER_NON_RENAMEABLE) {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_FOLDER_RULES,
			     _("FIXME: unable to rename"));

		/* Notify about operation end */
		modest_mail_operation_notify_end (self);
	} else {
		/* Rename. Camel handles folder subscription/unsubscription */
		TnyFolderStore *into;

		into = tny_folder_get_folder_store (folder);
		tny_folder_copy_async (folder, into, name, TRUE,
				 transfer_folder_cb,
				 transfer_folder_status_cb,
				 self);
		if (into)
			g_object_unref (into);
		
	}
 }

/* ******************************************************************* */
/* **************************  MSG  ACTIONS  ************************* */
/* ******************************************************************* */

void modest_mail_operation_get_msg (ModestMailOperation *self,
				    TnyHeader *header,
				    GetMsgAsyncUserCallback user_callback,
				    gpointer user_data)
{
	GetMsgAsyncHelper *helper = NULL;
	TnyFolder *folder;
	ModestMailOperationPrivate *priv;
	
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_HEADER (header));
	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	folder = tny_header_get_folder (header);

	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;

	/* Get message from folder */
	if (folder) {
		/* Get account and set it into mail_operation */
		priv->account = modest_tny_folder_get_account (TNY_FOLDER(folder));

		helper = g_slice_new0 (GetMsgAsyncHelper);
		helper->mail_op = self;
		helper->user_callback = user_callback;
		helper->user_data = user_data;
		helper->header = g_object_ref (header);

		tny_folder_get_msg_async (folder, header, get_msg_cb, get_msg_status_cb, helper);

		g_object_unref (G_OBJECT (folder));
	} else {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     _("Error trying to get a message. No folder found for header"));

		/* Notify the queue */
		modest_mail_operation_notify_end (self);
	}
}

static void
get_msg_cb (TnyFolder *folder, 
	    gboolean cancelled, 
	    TnyMsg *msg, 
	    GError **error, 
	    gpointer user_data)
{
	GetMsgAsyncHelper *helper = NULL;
	ModestMailOperation *self = NULL;
	ModestMailOperationPrivate *priv = NULL;

	helper = (GetMsgAsyncHelper *) user_data;
	g_return_if_fail (helper != NULL);       
	self = helper->mail_op;
	g_return_if_fail (MODEST_IS_MAIL_OPERATION(self));
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	/* Check errors and cancel */
	if (*error) {
		priv->error = g_error_copy (*error);
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		goto out;
	}
	if (cancelled) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_CANCELED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     _("Error trying to refresh the contents of %s"),
			     tny_folder_get_name (folder));
		goto out;
	}

	priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;

	/* If user defined callback function was defined, call it */
	if (helper->user_callback) {
		helper->user_callback (self, helper->header, msg, helper->user_data);
	}

 out:
	/* Free */
	g_object_unref (helper->header);
	g_slice_free (GetMsgAsyncHelper, helper);
		
	/* Notify about operation end */
	modest_mail_operation_notify_end (self);	
}

static void     
get_msg_status_cb (GObject *obj,
		   TnyStatus *status,  
		   gpointer user_data)
{
	GetMsgAsyncHelper *helper = NULL;
	ModestMailOperation *self;
	ModestMailOperationPrivate *priv;
	ModestMailOperationState *state;

	g_return_if_fail (status != NULL);
	g_return_if_fail (status->code == TNY_FOLDER_STATUS_CODE_GET_MSG);

	helper = (GetMsgAsyncHelper *) user_data;
	g_return_if_fail (helper != NULL);       

	self = helper->mail_op;
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	if ((status->position == 1) && (status->of_total == 100))
		return;

	priv->done = 1;
	priv->total = 1;

	state = modest_mail_operation_clone_state (self);
	state->bytes_done = status->position;
	state->bytes_total = status->of_total;
	g_signal_emit (G_OBJECT (self), signals[PROGRESS_CHANGED_SIGNAL], 0, state, NULL);
	g_slice_free (ModestMailOperationState, state);
}

/****************************************************/
typedef struct {
	ModestMailOperation *mail_op;
	TnyList *headers;
	GetMsgAsyncUserCallback user_callback;
	gpointer user_data;
	GDestroyNotify notify;
} GetFullMsgsInfo;

typedef struct {
	GetMsgAsyncUserCallback user_callback;
	TnyHeader *header;
	TnyMsg *msg;
	gpointer user_data;
	ModestMailOperation *mail_op;
} NotifyGetMsgsInfo;


/* 
 * Used by get_msgs_full_thread to call the user_callback for each
 * message that has been read
 */
static gboolean
notify_get_msgs_full (gpointer data)
{
	NotifyGetMsgsInfo *info;

	info = (NotifyGetMsgsInfo *) data;	

	/* Call the user callback */
	info->user_callback (info->mail_op, info->header, info->msg, info->user_data);

	g_slice_free (NotifyGetMsgsInfo, info);

	return FALSE;
}

/* 
 * Used by get_msgs_full_thread to free al the thread resources and to
 * call the destroy function for the passed user_data
 */
static gboolean
get_msgs_full_destroyer (gpointer data)
{
	GetFullMsgsInfo *info;

	info = (GetFullMsgsInfo *) data;

	if (info->notify)
		info->notify (info->user_data);

	/* free */
	g_object_unref (info->headers);
	g_slice_free (GetFullMsgsInfo, info);

	return FALSE;
}

static gpointer
get_msgs_full_thread (gpointer thr_user_data)
{
	GetFullMsgsInfo *info;
	ModestMailOperationPrivate *priv = NULL;
	TnyIterator *iter = NULL;
	
	info = (GetFullMsgsInfo *) thr_user_data;	
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (info->mail_op);

	iter = tny_list_create_iterator (info->headers);
	while (!tny_iterator_is_done (iter)) { 
		TnyHeader *header;
		TnyFolder *folder;
		
		header = TNY_HEADER (tny_iterator_get_current (iter));
		folder = tny_header_get_folder (header);
				
		/* Get message from folder */
		if (folder) {
			TnyMsg *msg;
			/* The callback will call it per each header */
			msg = tny_folder_get_msg (folder, header, &(priv->error));

			if (msg) {
				ModestMailOperationState *state;
				ModestPair *pair;

				priv->done++;

				/* notify progress */
				state = modest_mail_operation_clone_state (info->mail_op);
				pair = modest_pair_new (g_object_ref (info->mail_op), state, FALSE);
				g_idle_add_full (G_PRIORITY_HIGH_IDLE, idle_notify_progress_once,
						 pair, (GDestroyNotify) modest_pair_free);

				/* The callback is the responsible for
				   freeing the message */
				if (info->user_callback) {
					NotifyGetMsgsInfo *info_notify;
					info_notify = g_slice_new0 (NotifyGetMsgsInfo);
					info_notify->user_callback = info->user_callback;
					info_notify->mail_op = info->mail_op;
					info_notify->header = g_object_ref (header);
					info_notify->msg = g_object_ref (msg);
					info_notify->user_data = info->user_data;
					g_idle_add_full (G_PRIORITY_HIGH_IDLE,
							 notify_get_msgs_full, 
							 info_notify, NULL);
				}
				g_object_unref (msg);
			}
		} else {
			/* Set status failed and set an error */
			priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
			g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
				     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
				     "Error trying to get a message. No folder found for header");
		}
		g_object_unref (header);		
		tny_iterator_next (iter);
	}

	/* Set operation status */
	if (priv->status == MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS)
		priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;

	/* Notify about operation end */
	g_idle_add (notify_update_account_queue, g_object_ref (info->mail_op));

	/* Free thread resources. Will be called after all previous idles */
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE + 1, get_msgs_full_destroyer, info, NULL);

	return NULL;
}

void 
modest_mail_operation_get_msgs_full (ModestMailOperation *self,
				     TnyList *header_list, 
				     GetMsgAsyncUserCallback user_callback,
				     gpointer user_data,
				     GDestroyNotify notify)
{
	TnyHeader *header = NULL;
	TnyFolder *folder = NULL;
	GThread *thread;
	ModestMailOperationPrivate *priv = NULL;
	GetFullMsgsInfo *info = NULL;
	gboolean size_ok = TRUE;
	gint max_size;
	TnyIterator *iter = NULL;
	
	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	
	/* Init mail operation */
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;
	priv->done = 0;
	priv->total = tny_list_get_length(header_list);

	/* Get account and set it into mail_operation */
	if (tny_list_get_length (header_list) >= 1) {
		iter = tny_list_create_iterator (header_list);
		header = TNY_HEADER (tny_iterator_get_current (iter));
		folder = tny_header_get_folder (header);		
		priv->account = modest_tny_folder_get_account (TNY_FOLDER(folder));
		g_object_unref (header);
		g_object_unref (folder);

		if (tny_list_get_length (header_list) == 1) {
			g_object_unref (iter);
			iter = NULL;
		}
	}

	/* Get msg size limit */
	max_size  = modest_conf_get_int (modest_runtime_get_conf (), 
					 MODEST_CONF_MSG_SIZE_LIMIT, 
					 &(priv->error));
	if (priv->error) {
		g_clear_error (&(priv->error));
		max_size = G_MAXINT;
	} else {
		max_size = max_size * KB;
	}

	/* Check message size limits. If there is only one message
	   always retrieve it */
	if (iter != NULL) {
		while (!tny_iterator_is_done (iter) && size_ok) {
			header = TNY_HEADER (tny_iterator_get_current (iter));
			if (tny_header_get_message_size (header) >= max_size)
				size_ok = FALSE;
			g_object_unref (header);
			tny_iterator_next (iter);
		}
		g_object_unref (iter);
	}

	if (size_ok) {
		/* Create the info */
		info = g_slice_new0 (GetFullMsgsInfo);
		info->mail_op = self;
		info->user_callback = user_callback;
		info->user_data = user_data;
		info->headers = g_object_ref (header_list);
		info->notify = notify;

		thread = g_thread_create (get_msgs_full_thread, info, FALSE, NULL);
	} else {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		/* FIXME: the error msg is different for pop */
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_MESSAGE_SIZE_LIMIT,
			     _("emev_ni_ui_imap_msg_size_exceed_error"));
		/* Remove from queue and free resources */
		modest_mail_operation_notify_end (self);
		if (notify)
			notify (user_data);
	}
}


void 
modest_mail_operation_remove_msg (ModestMailOperation *self,
				  TnyHeader *header,
				  gboolean remove_to_trash)
{
	TnyFolder *folder;
	ModestMailOperationPrivate *priv;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_HEADER (header));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);
	folder = tny_header_get_folder (header);

	/* Get account and set it into mail_operation */
	priv->account = modest_tny_folder_get_account (TNY_FOLDER(folder));

	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;

	/* Delete or move to trash */
	if (remove_to_trash) {
		TnyFolder *trash_folder;
		TnyStoreAccount *store_account;

		store_account = TNY_STORE_ACCOUNT (modest_tny_folder_get_account (folder));
		trash_folder = modest_tny_account_get_special_folder (TNY_ACCOUNT(store_account),
								      TNY_FOLDER_TYPE_TRASH);
		if (trash_folder) {
			TnyList *headers;

			/* Create list */
			headers = tny_simple_list_new ();
			tny_list_append (headers, G_OBJECT (header));
			g_object_unref (header);

			/* Move to trash */
			modest_mail_operation_xfer_msgs (self, headers, trash_folder, TRUE, NULL, NULL);
			g_object_unref (headers);
/* 			g_object_unref (trash_folder); */
		} else {
			ModestMailOperationPrivate *priv;

			/* Set status failed and set an error */
			priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
			priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
			g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
				     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
				     _("Error trying to delete a message. Trash folder not found"));
		}

		g_object_unref (G_OBJECT (store_account));
	} else {
		tny_folder_remove_msg (folder, header, &(priv->error));
		if (!priv->error)
			tny_folder_sync(folder, TRUE, &(priv->error));
	}

	/* Set status */
	if (!priv->error)
		priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;
	else
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;

	/* Free */
	g_object_unref (G_OBJECT (folder));

	/* Notify about operation end */
	modest_mail_operation_notify_end (self);
}

static void
transfer_msgs_status_cb (GObject *obj,
			 TnyStatus *status,  
			 gpointer user_data)
{
	XFerMsgAsyncHelper *helper = NULL;
	ModestMailOperation *self;
	ModestMailOperationPrivate *priv;
	ModestMailOperationState *state;


	g_return_if_fail (status != NULL);
	g_return_if_fail (status->code == TNY_FOLDER_STATUS_CODE_XFER_MSGS);

	helper = (XFerMsgAsyncHelper *) user_data;
	g_return_if_fail (helper != NULL);       

	self = helper->mail_op;
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	if ((status->position == 1) && (status->of_total == 100))
		return;

	priv->done = status->position;
	priv->total = status->of_total;

	state = modest_mail_operation_clone_state (self);
	g_signal_emit (G_OBJECT (self), signals[PROGRESS_CHANGED_SIGNAL], 0, state, NULL);
	g_slice_free (ModestMailOperationState, state);
}


static void
transfer_msgs_cb (TnyFolder *folder, gboolean cancelled, GError **err, gpointer user_data)
{
	XFerMsgAsyncHelper *helper;
	ModestMailOperation *self;
	ModestMailOperationPrivate *priv;

	helper = (XFerMsgAsyncHelper *) user_data;
	self = helper->mail_op;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE (self);

	if (*err) {
		priv->error = g_error_copy (*err);
		priv->done = 0;
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;	
	} else if (cancelled) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_CANCELED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     _("Error trying to refresh the contents of %s"),
			     tny_folder_get_name (folder));
	} else {
		priv->done = 1;
		priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;
	}

	/* If user defined callback function was defined, call it */
	if (helper->user_callback) {
		helper->user_callback (priv->source, helper->user_data);
	}

	/* Free */
	g_object_unref (helper->headers);
	g_object_unref (helper->dest_folder);
	g_object_unref (helper->mail_op);
	g_slice_free   (XFerMsgAsyncHelper, helper);
	g_object_unref (folder);

	/* Notify about operation end */
	modest_mail_operation_notify_end (self);
}

void
modest_mail_operation_xfer_msgs (ModestMailOperation *self,
				 TnyList *headers, 
				 TnyFolder *folder, 
				 gboolean delete_original,
				 XferMsgsAsynUserCallback user_callback,
				 gpointer user_data)
{
	ModestMailOperationPrivate *priv;
	TnyIterator *iter;
	TnyFolder *src_folder;
	XFerMsgAsyncHelper *helper;
	TnyHeader *header;
	ModestTnyFolderRules rules;

	g_return_if_fail (MODEST_IS_MAIL_OPERATION (self));
	g_return_if_fail (TNY_IS_LIST (headers));
	g_return_if_fail (TNY_IS_FOLDER (folder));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);
	priv->total = 1;
	priv->done = 0;
	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;

	/* Apply folder rules */
	rules = modest_tny_folder_get_rules (TNY_FOLDER (folder));

	if (rules & MODEST_FOLDER_RULES_FOLDER_NON_WRITEABLE) {
 		/* Set status failed and set an error */
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_FOLDER_RULES,
			     _("FIXME: folder does not accept msgs"));
		/* Notify the queue */
		modest_mail_operation_notify_end (self);
		return;
	}

	/* Create the helper */
	helper = g_slice_new0 (XFerMsgAsyncHelper);
	helper->mail_op = g_object_ref(self);
	helper->dest_folder = g_object_ref(folder);
	helper->headers = g_object_ref(headers);
	helper->user_callback = user_callback;
	helper->user_data = user_data;

	/* Get source folder */
	iter = tny_list_create_iterator (headers);
	header = TNY_HEADER (tny_iterator_get_current (iter));
	src_folder = tny_header_get_folder (header);
	g_object_unref (header);
	g_object_unref (iter);

	/* Get account and set it into mail_operation */
	priv->account = modest_tny_folder_get_account (src_folder);

	/* Transfer messages */
	tny_folder_transfer_msgs_async (src_folder, 
					headers, 
					folder, 
					delete_original, 
					transfer_msgs_cb, 
					transfer_msgs_status_cb,
					helper);
}


static void
on_refresh_folder (TnyFolder   *folder, 
		   gboolean     cancelled, 
		   GError     **error,
		   gpointer     user_data)
{
	RefreshAsyncHelper *helper = NULL;
	ModestMailOperation *self = NULL;
	ModestMailOperationPrivate *priv = NULL;

	helper = (RefreshAsyncHelper *) user_data;
	self = helper->mail_op;
	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	if (*error) {
		priv->error = g_error_copy (*error);
		priv->status = MODEST_MAIL_OPERATION_STATUS_FAILED;
		goto out;
	}

	if (cancelled) {
		priv->status = MODEST_MAIL_OPERATION_STATUS_CANCELED;
		g_set_error (&(priv->error), MODEST_MAIL_OPERATION_ERROR,
			     MODEST_MAIL_OPERATION_ERROR_ITEM_NOT_FOUND,
			     _("Error trying to refresh the contents of %s"),
			     tny_folder_get_name (folder));
		goto out;
	}

	priv->status = MODEST_MAIL_OPERATION_STATUS_SUCCESS;

 out:
	/* Call user defined callback, if it exists */
	if (helper->user_callback)
		helper->user_callback (priv->source, folder, helper->user_data);

	/* Free */
	g_object_unref (helper->mail_op);
	g_slice_free   (RefreshAsyncHelper, helper);
	g_object_unref (folder);

	/* Notify about operation end */
	modest_mail_operation_notify_end (self);
}

static void
on_refresh_folder_status_update (GObject *obj,
				 TnyStatus *status,
				 gpointer user_data)
{
	RefreshAsyncHelper *helper = NULL;
	ModestMailOperation *self = NULL;
	ModestMailOperationPrivate *priv = NULL;
	ModestMailOperationState *state;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (status != NULL);
	g_return_if_fail (status->code == TNY_FOLDER_STATUS_CODE_REFRESH);

	helper = (RefreshAsyncHelper *) user_data;
	self = helper->mail_op;
	g_return_if_fail (MODEST_IS_MAIL_OPERATION(self));

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	priv->done = status->position;
	priv->total = status->of_total;

	state = modest_mail_operation_clone_state (self);
	g_signal_emit (G_OBJECT (self), signals[PROGRESS_CHANGED_SIGNAL], 0, state, NULL);
	g_slice_free (ModestMailOperationState, state);
}

void 
modest_mail_operation_refresh_folder  (ModestMailOperation *self,
				       TnyFolder *folder,
				       RefreshAsyncUserCallback user_callback,
				       gpointer user_data)
{
	ModestMailOperationPrivate *priv = NULL;
	RefreshAsyncHelper *helper = NULL;

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	/* Pick a reference */
	g_object_ref (folder);

	priv->status = MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS;

	/* Get account and set it into mail_operation */
	priv->account = modest_tny_folder_get_account  (folder);

	/* Create the helper */
	helper = g_slice_new0 (RefreshAsyncHelper);
	helper->mail_op = g_object_ref(self);
	helper->user_callback = user_callback;
	helper->user_data = user_data;

	/* Refresh the folder. TODO: tinymail could issue a status
	   updates before the callback call then this could happen. We
	   must review the design */
	tny_folder_refresh_async (folder,
				  on_refresh_folder,
				  on_refresh_folder_status_update,
				  helper);
}

/**
 *
 * It's used by the mail operation queue to notify the observers
 * attached to that signal that the operation finished. We need to use
 * that because tinymail does not give us the progress of a given
 * operation when it finishes (it directly calls the operation
 * callback).
 */
static void
modest_mail_operation_notify_end (ModestMailOperation *self)
{
	ModestMailOperationState *state;
	ModestMailOperationPrivate *priv = NULL;

	g_return_if_fail (self);

	priv = MODEST_MAIL_OPERATION_GET_PRIVATE(self);

	if (!priv) {
		g_warning ("BUG: %s: priv == NULL", __FUNCTION__);
		return;
	}
	
	/* Set the account back to not busy */
	if (priv->account_name) {
		modest_account_mgr_set_account_busy(modest_runtime_get_account_mgr(), priv->account_name,
																				FALSE);
		g_free(priv->account_name);
		priv->account_name = NULL;
	}
	
	/* Notify the observers about the mail opertation end */
	state = modest_mail_operation_clone_state (self);
	g_signal_emit (G_OBJECT (self), signals[PROGRESS_CHANGED_SIGNAL], 0, state, NULL);
	g_slice_free (ModestMailOperationState, state);
}
