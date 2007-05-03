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

#ifndef __MODEST_MAIL_OPERATION_H__
#define __MODEST_MAIL_OPERATION_H__

#include <tny-transport-account.h>
#include <tny-folder-store.h>

G_BEGIN_DECLS

/* convenience macros */
#define MODEST_TYPE_MAIL_OPERATION             (modest_mail_operation_get_type())
#define MODEST_MAIL_OPERATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),MODEST_TYPE_MAIL_OPERATION,ModestMailOperation))
#define MODEST_MAIL_OPERATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),MODEST_TYPE_MAIL_OPERATION,GObject))
#define MODEST_IS_MAIL_OPERATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),MODEST_TYPE_MAIL_OPERATION))
#define MODEST_IS_MAIL_OPERATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),MODEST_TYPE_MAIL_OPERATION))
#define MODEST_MAIL_OPERATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),MODEST_TYPE_MAIL_OPERATION,ModestMailOperationClass))

typedef struct _ModestMailOperation      ModestMailOperation;
typedef struct _ModestMailOperationClass ModestMailOperationClass;

/**
 * ModestMailOperationStatus:
 *
 * The state of a mail operation
 */
typedef enum _ModestMailOperationStatus {
	MODEST_MAIL_OPERATION_STATUS_INVALID,
	MODEST_MAIL_OPERATION_STATUS_SUCCESS,
	MODEST_MAIL_OPERATION_STATUS_FINISHED_WITH_ERRORS,
	MODEST_MAIL_OPERATION_STATUS_FAILED,
	MODEST_MAIL_OPERATION_STATUS_IN_PROGRESS,
	MODEST_MAIL_OPERATION_STATUS_CANCELED
} ModestMailOperationStatus;

/**
 * ModestMailOperationId:
 *
 * The id for identifying the type of mail operation
 */
typedef enum _ModestMailOperationId {
	MODEST_MAIL_OPERATION_ID_SEND,
	MODEST_MAIL_OPERATION_ID_RECEIVE,
	MODEST_MAIL_OPERATION_ID_OPEN,
	MODEST_MAIL_OPERATION_ID_DELETE,
	MODEST_MAIL_OPERATION_ID_INFO,
	MODEST_MAIL_OPERATION_ID_UNKNOWN,
} ModestMailOperationId;


struct _ModestMailOperation {
	 GObject parent;
	/* insert public members, if any */
};

struct _ModestMailOperationClass {
	GObjectClass parent_class;

	/* Signals */
	void (*progress_changed) (ModestMailOperation *self, gpointer user_data);
};

/* member functions */
GType        modest_mail_operation_get_type    (void) G_GNUC_CONST;

/* typical parameter-less _new function */
ModestMailOperation*    modest_mail_operation_new     (ModestMailOperationId id);

/**
 * modest_mail_operation_get_id
 * @self: a #ModestMailOperation
 * 
 * Gets the private id field of mail operation. This id identifies
 * the class/type of mail operation.  
  **/
ModestMailOperationId
modest_mail_operation_get_id (ModestMailOperation *self);

/* fill in other public functions, eg.: */

/**
 * modest_mail_operation_send_mail:
 * @self: a #ModestMailOperation
 * @transport_account: a non-NULL #TnyTransportAccount
 * @msg: a non-NULL #TnyMsg
 * 
 * Sends and already existing message using the provided
 * #TnyTransportAccount. This operation is synchronous, so the
 * #ModestMailOperation should not be added to any
 * #ModestMailOperationQueue
  **/
void    modest_mail_operation_send_mail       (ModestMailOperation *self,
					       TnyTransportAccount *transport_account,
					       TnyMsg* msg);

/**
 * modest_mail_operation_send_new_mail:
 * @self: a #ModestMailOperation
 * @transport_account: a non-NULL #TnyTransportAccount
 * @from: the email address of the mail sender
 * @to: a non-NULL email address of the mail receiver
 * @cc: a comma-separated list of email addresses where to send a carbon copy
 * @bcc: a comma-separated list of email addresses where to send a blind carbon copy
 * @subject: the subject of the new mail
 * @plain_body: the plain text body of the new mail.
 * @html_body: the html version of the body of the new mail. If NULL, the mail will
 *             be sent with the plain body only.
 * @attachments_list: a #GList of attachments, each attachment must be a #TnyMimePart
 * 
 * Sends a new mail message using the provided
 * #TnyTransportAccount. This operation is synchronous, so the
 * #ModestMailOperation should not be added to any
 * #ModestMailOperationQueue
  **/
void    modest_mail_operation_send_new_mail   (ModestMailOperation *self,
					       TnyTransportAccount *transport_account,
					       const gchar *from,
					       const gchar *to,
					       const gchar *cc,
					       const gchar *bcc,
					       const gchar *subject,
					       const gchar *plain_body,
					       const gchar *html_body,
					       const GList *attachments_list,
					       TnyHeaderFlags priority_flags);


/**
 * modest_mail_operation_save_to_drafts:
 * @self: a #ModestMailOperation
 * @transport_account: a non-NULL #TnyTransportAccount
 * @from: the email address of the mail sender
 * @to: a non-NULL email address of the mail receiver
 * @cc: a comma-separated list of email addresses where to send a carbon copy
 * @bcc: a comma-separated list of email addresses where to send a blind carbon copy
 * @subject: the subject of the new mail
 * @plain_body: the plain text body of the new mail.
 * @html_body: the html version of the body of the new mail. If NULL, the mail will
 *             be sent with the plain body only.
 * @attachments_list: a #GList of attachments, each attachment must be a #TnyMimePart
 * 
 * Save a mail message to drafts using the provided
 * #TnyTransportAccount. This operation is synchronous, so the
 * #ModestMailOperation should not be added to any
 * #ModestMailOperationQueue
  **/
void    modest_mail_operation_save_to_drafts   (ModestMailOperation *self,
						TnyTransportAccount *transport_account,
						const gchar *from,
						const gchar *to,
						const gchar *cc,
						const gchar *bcc,
						const gchar *subject,
						const gchar *plain_body,
						const gchar *html_body,
						const GList *attachments_list,
						TnyHeaderFlags priority_flags);
/**
 * modest_mail_operation_update_account:
 * @self: a #ModestMailOperation
 * @store_account: a #TnyStoreAccount
 * 
 * Asynchronously refreshes the root folders of the given store
 * account. The caller should add the #ModestMailOperation to a
 * #ModestMailOperationQueue and then free it. The caller will be
 * notified by the "progress_changed" signal each time the progress of
 * the operation changes.
 *
 * Note that the store account passed as parametter will be freed by
 * the mail operation so you must pass a new reference
 *
 * Example
 * <informalexample><programlisting>
 * queue = modest_tny_platform_factory_get_modest_mail_operation_queue_instance (fact)
 * mail_op = modest_mail_operation_new ();
 * g_signal_connect (G_OBJECT (mail_op), "progress_changed", G_CALLBACK(on_progress_changed), NULL);
 * modest_mail_operation_queue_add (queue, mail_op);
 * modest_mail_operation_update_account (mail_op, account)
 * g_object_unref (G_OBJECT (mail_op));
 * </programlisting></informalexample>
 * 
 * Returns: TRUE if the mail operation could be started, or FALSE otherwise
 **/
gboolean      modest_mail_operation_update_account (ModestMailOperation *self,
						    TnyStoreAccount *store_account);

/* Functions that perform store operations */

/**
 * modest_mail_operation_create_folder:
 * @self: a #ModestMailOperation
 * @parent: the #TnyFolderStore which is the parent of the new folder
 * @name: the non-NULL new name for the new folder
 * 
 * Creates a new folder as a children of a existing one. If the store
 * account supports subscriptions this method also sets the new folder
 * as subscribed. This operation is synchronous, so the
 * #ModestMailOperation should not be added to any
 * #ModestMailOperationQueue
 * 
 * Returns: a newly created #TnyFolder or NULL in case of error.
 **/
TnyFolder*    modest_mail_operation_create_folder  (ModestMailOperation *self,
						    TnyFolderStore *parent,
						    const gchar *name);

/**
 * modest_mail_operation_remove_folder:
 * @self: a #ModestMailOperation
 * @folder: a #TnyFolder
 * @remove_to_trash: TRUE to move it to trash or FALSE to delete
 * permanently
 * 
 * Removes a folder. This operation is synchronous, so the
 * #ModestMailOperation should not be added to any
 * #ModestMailOperationQueue
 **/
void          modest_mail_operation_remove_folder  (ModestMailOperation *self,
						    TnyFolder *folder,
						    gboolean remove_to_trash);

/**
 * modest_mail_operation_rename_folder:
 * @self: a #ModestMailOperation
 * @folder: a #TnyFolder
 * @name: a non-NULL name without "/"
 * 
 * Renames a given folder with the provided new name. This operation
 * is synchronous, so the #ModestMailOperation should not be added to
 * any #ModestMailOperationQueue
 **/
void          modest_mail_operation_rename_folder  (ModestMailOperation *self,
						    TnyFolder *folder, 
						    const gchar *name);

/**
 * modest_mail_operation_xfer_folder:
 * @self: a #ModestMailOperation
 * @folder: a #TnyFolder
 * @parent: the new parent of the folder as #TnyFolderStore
 * @delete_original: wheter or not delete the original folder
 * 
 * Sets the given @folder as child of a provided #TnyFolderStore. This
 * operation also transfers all the messages contained in the folder
 * and all of his children folders with their messages as well. This
 * operation is synchronous, so the #ModestMailOperation should not be
 * added to any #ModestMailOperationQueue.
 *
 * If @delete_original is TRUE this function moves the original
 * folder, if it is FALSE the it just copies it
 *
 * Returns: the newly transfered folder
 **/
TnyFolder*    modest_mail_operation_xfer_folder    (ModestMailOperation *self,
						    TnyFolder *folder, 
						    TnyFolderStore *parent,
						    gboolean delete_original);


/* Functions that performs msg operations */

/**
 * modest_mail_operation_xfer_msgs:
 * @self: a #ModestMailOperation
 * @header_list: a #TnyList of #TnyHeader to transfer
 * @folder: the #TnyFolder where the messages will be transferred
 * @delete_original: whether or not delete the source messages
 * 
 * Asynchronously transfers messages from their current folder to
 * another one. The caller should add the #ModestMailOperation to a
 * #ModestMailOperationQueue and then free it. The caller will be
 * notified by the "progress_changed" when the operation is completed.
 *
 * If the @delete_original paramter is TRUE then this function moves
 * the messages between folders, otherwise it copies them.
 * 
 * Example
 * <informalexample><programlisting>
 * queue = modest_tny_platform_factory_get_modest_mail_operation_queue_instance (fact);
 * mail_op = modest_mail_operation_new ();
 * modest_mail_operation_queue_add (queue, mail_op);
 * g_signal_connect (G_OBJECT (mail_op), "progress_changed", G_CALLBACK(on_progress_changed), queue);
 *
 * modest_mail_operation_xfer_msg (mail_op, headers, folder, TRUE);
 * 
 * g_object_unref (G_OBJECT (mail_op));
 * </programlisting></informalexample>
 *
 **/
void          modest_mail_operation_xfer_msgs      (ModestMailOperation *self,
						    TnyList *header_list, 
						    TnyFolder *folder,
						    gboolean delete_original);

/**
 * modest_mail_operation_remove_msg:
 * @self: a #ModestMailOperation
 * @header: the #TnyHeader of the message to move
 * @remove_to_trash: TRUE to move it to trash or FALSE to delete it
 * permanently
 * 
 * Deletes a message. This operation is synchronous, so the
 * #ModestMailOperation should not be added to any
 * #ModestMailOperationQueue
 **/
void          modest_mail_operation_remove_msg     (ModestMailOperation *self,
						    TnyHeader *header,
						    gboolean remove_to_trash);

/**
 * modest_mail_operation_process_msg:
 * @self: a #ModestMailOperation
 * @header: the #TnyHeader of the message to get
 * permanently
 * 
 * Gets a message and process it using @callback function
 * pased as argument. This operation is assynchronous, so the
 * #ModestMailOperation should be added to
 * #ModestMailOperationQueue
 **/
void          modest_mail_operation_process_msg     (ModestMailOperation *self,
						     TnyHeader *header,
						     TnyGetMsgCallback user_callback,
						     gpointer user_data);

/* Functions to control mail operations */
/**
 * modest_mail_operation_get_task_done:
 * @self: a #ModestMailOperation
 * 
 * Gets the amount of work done for a given mail operation. This
 * amount of work is an absolute value.
 * 
 * Returns: the amount of work completed
 **/
guint     modest_mail_operation_get_task_done      (ModestMailOperation *self);

/**
 * modest_mail_operation_get_task_total:
 * @self: a #ModestMailOperation
 * 
 * Gets the total amount of work that must be done to complete a given
 * mail operation. This amount of work is an absolute value.
 * 
 * Returns: the total required amount of work
 **/
guint     modest_mail_operation_get_task_total     (ModestMailOperation *self);


/**
 * modest_mail_operation_is_finished:
 * @self: a #ModestMailOperation
 * 
 * Checks if the operation is finished. A #ModestMailOperation is
 * finished if it's in any of the following states:
 * MODEST_MAIL_OPERATION_STATUS_SUCCESS,
 * MODEST_MAIL_OPERATION_STATUS_FAILED,
 * MODEST_MAIL_OPERATION_STATUS_CANCELED or
 * MODEST_MAIL_OPERATION_STATUS_FINISHED_WITH_ERRORS
 * 
 * Returns: TRUE if the operation is finished, FALSE otherwise
 **/
gboolean  modest_mail_operation_is_finished        (ModestMailOperation *self);

/**
 * modest_mail_operation_is_finished:
 * @self: a #ModestMailOperation
 * 
 * Gets the current status of the given mail operation
 *
 * Returns: the current status or MODEST_MAIL_OPERATION_STATUS_INVALID in case of error
 **/
ModestMailOperationStatus modest_mail_operation_get_status  (ModestMailOperation *self);

/**
 * modest_mail_operation_get_error:
 * @self: a #ModestMailOperation
 * 
 * Gets the error associated to the mail operation if exists
 * 
 * Returns: the #GError associated to the #ModestMailOperation if it
 * exists or NULL otherwise
 **/
const GError*             modest_mail_operation_get_error   (ModestMailOperation *self);

/**
 * modest_mail_operation_cancel:
 * @self: a #ModestMailOperation
 *
 * Cancels an active mail operation
 * 
 * Returns: TRUE if the operation was succesfully canceled, FALSE otherwise
 **/
gboolean  modest_mail_operation_cancel          (ModestMailOperation *self);

/**
 * modest_mail_operation_refresh_folder
 * @self: a #ModestMailOperation
 * @folder: the #TnyFolder to refresh
 * 
 * Refreshes the contents of a folder
 */
void      modest_mail_operation_refresh_folder  (ModestMailOperation *self,
						 TnyFolder *folder);

/**
 *
 * This function is a workarround. It emits the progress-changed
 * signal. It's used by the mail operation queue to notify the
 * observers attached to that signal that the operation finished. We
 * need to use that for the moment because tinymail does not give us
 * the progress of a given operation very well. So we must delete it
 * when tinymail has that functionality and remove the call to it in
 * the queue as well.
 */
void _modest_mail_operation_notify_end (ModestMailOperation *self);

G_END_DECLS

#endif /* __MODEST_MAIL_OPERATION_H__ */

