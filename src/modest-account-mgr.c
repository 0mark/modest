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
#include <modest-marshal.h>
#include <modest-runtime.h>
#include <modest-account-mgr.h>
#include <modest-account-mgr-priv.h>
#include <modest-account-mgr-helpers.h>
#include <modest-platform.h>

/* 'private'/'protected' functions */
static void modest_account_mgr_class_init (ModestAccountMgrClass * klass);
static void modest_account_mgr_init       (ModestAccountMgr * obj);
static void modest_account_mgr_finalize   (GObject * obj);
static void modest_account_mgr_base_init  (gpointer g_class);

static const gchar *_modest_account_mgr_get_account_keyname_cached (ModestAccountMgrPrivate *priv, 
								    const gchar* account_name,
								    const gchar *name, 
								    gboolean is_server);

static gboolean modest_account_mgr_unset_default_account (ModestAccountMgr *self);

/* list my signals */
enum {
	ACCOUNT_INSERTED_SIGNAL,
	ACCOUNT_CHANGED_SIGNAL,
	ACCOUNT_REMOVED_SIGNAL,
	ACCOUNT_BUSY_SIGNAL,
	DEFAULT_ACCOUNT_CHANGED_SIGNAL,
	DISPLAY_NAME_CHANGED_SIGNAL,
	LAST_SIGNAL
};

/* globals */
static GObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = {0};

GType
modest_account_mgr_get_type (void)
{
	static GType my_type = 0;

	if (!my_type) {
		static const GTypeInfo my_info = {
			sizeof (ModestAccountMgrClass),
			modest_account_mgr_base_init,	/* base init */
			NULL,	/* base finalize */
			(GClassInitFunc) modest_account_mgr_class_init,
			NULL,	/* class finalize */
			NULL,	/* class data */
			sizeof (ModestAccountMgr),
			1,	/* n_preallocs */
			(GInstanceInitFunc) modest_account_mgr_init,
			NULL
		};

		my_type = g_type_register_static (G_TYPE_OBJECT,
						  "ModestAccountMgr",
						  &my_info, 0);
	}
	return my_type;
}

static void 
modest_account_mgr_base_init (gpointer g_class)
{
	static gboolean modest_account_mgr_initialized = FALSE;

	if (!modest_account_mgr_initialized) {
		/* signal definitions */
		signals[ACCOUNT_INSERTED_SIGNAL] =
			g_signal_new ("account_inserted",
				      MODEST_TYPE_ACCOUNT_MGR,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET(ModestAccountMgrClass, account_inserted),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__STRING,
				      G_TYPE_NONE, 1, G_TYPE_STRING);

		signals[ACCOUNT_REMOVED_SIGNAL] =
			g_signal_new ("account_removed",
				      MODEST_TYPE_ACCOUNT_MGR,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET(ModestAccountMgrClass, account_removed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__STRING,
				      G_TYPE_NONE, 1, G_TYPE_STRING);

		signals[ACCOUNT_CHANGED_SIGNAL] =
			g_signal_new ("account_changed",
				      MODEST_TYPE_ACCOUNT_MGR,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET(ModestAccountMgrClass, account_changed),
				      NULL, NULL,
				      modest_marshal_VOID__STRING_INT,
				      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_INT);

		signals[ACCOUNT_BUSY_SIGNAL] =
			g_signal_new ("account_busy_changed",
				      MODEST_TYPE_ACCOUNT_MGR,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET(ModestAccountMgrClass, account_busy_changed),
				      NULL, NULL,
				      modest_marshal_VOID__STRING_BOOLEAN,
				      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

		signals[DEFAULT_ACCOUNT_CHANGED_SIGNAL] =
			g_signal_new ("default_account_changed",
				      MODEST_TYPE_ACCOUNT_MGR,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET(ModestAccountMgrClass, default_account_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

		signals[DISPLAY_NAME_CHANGED_SIGNAL] =
			g_signal_new ("display_name_changed",
				      MODEST_TYPE_ACCOUNT_MGR,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET(ModestAccountMgrClass, display_name_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__STRING,
				      G_TYPE_NONE, 1, G_TYPE_STRING);

		modest_account_mgr_initialized = TRUE;
	}
}

static void
modest_account_mgr_class_init (ModestAccountMgrClass * klass)
{
	GObjectClass *gobject_class;
	gobject_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);
	gobject_class->finalize = modest_account_mgr_finalize;

	g_type_class_add_private (gobject_class,
				  sizeof (ModestAccountMgrPrivate));
}


static void
modest_account_mgr_init (ModestAccountMgr * obj)
{
	ModestAccountMgrPrivate *priv =
		MODEST_ACCOUNT_MGR_GET_PRIVATE (obj);

	priv->modest_conf   = NULL;
	priv->busy_accounts = NULL;
	priv->timeout       = 0;
	
	priv->notification_id_accounts = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, g_free);

	/* we maintain hashes for the modest-conf keys we build from account name
	 * + key. many seem to be used often, and generating them showed up high
	 * in oprofile */
	/* both hashes are hashes to hashes;
	 * account-key => keyname ==> account-key-name
	 */	
	priv->server_account_key_hash = g_hash_table_new_full (g_str_hash,
							       g_str_equal,
							       g_free,
							       (GDestroyNotify)g_hash_table_destroy);
	priv->account_key_hash        = g_hash_table_new_full (g_str_hash,
							       g_str_equal,
							       g_free,
							       (GDestroyNotify)g_hash_table_destroy);
}

static void
modest_account_mgr_finalize (GObject * obj)
{
	ModestAccountMgrPrivate *priv = 
		MODEST_ACCOUNT_MGR_GET_PRIVATE (obj);

	if (priv->notification_id_accounts) {
		/* TODO: forget dirs */

		g_hash_table_destroy (priv->notification_id_accounts);
	}

	if (priv->modest_conf) {
		g_object_unref (G_OBJECT(priv->modest_conf));
		priv->modest_conf = NULL;
	}

	if (priv->timeout)
		g_source_remove (priv->timeout);
	priv->timeout = 0;

	if (priv->server_account_key_hash) {
		g_hash_table_destroy (priv->server_account_key_hash);
		priv->server_account_key_hash = NULL;
	}
	
	if (priv->account_key_hash) {
		g_hash_table_destroy (priv->account_key_hash);
		priv->account_key_hash = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize (obj);
}


ModestAccountMgr *
modest_account_mgr_new (ModestConf *conf)
{
	GObject *obj;
	ModestAccountMgrPrivate *priv;

	g_return_val_if_fail (conf, NULL);

	obj = G_OBJECT (g_object_new (MODEST_TYPE_ACCOUNT_MGR, NULL));
	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (obj);

	g_object_ref (G_OBJECT(conf));
	priv->modest_conf = conf;

	return MODEST_ACCOUNT_MGR (obj);
}


static const gchar *
null_means_empty (const gchar * str)
{
	return str ? str : "";
}


gboolean
modest_account_mgr_add_account (ModestAccountMgr *self,
				const gchar *name,
				const gchar *display_name,
				const gchar *user_fullname,
				const gchar *user_email,
				const gchar *retrieve_type,
				const gchar *store_account,
				const gchar *transport_account,
				gboolean enabled)
{
	ModestAccountMgrPrivate *priv;
	const gchar *key;
	gboolean ok;
	gchar *default_account;
	GError *err = NULL;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (strchr(name, '/') == NULL, FALSE);
	
	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	/*
	 * we create the account by adding an account 'dir', with the name <name>,
	 * and in that the 'display_name' string key
	 */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_DISPLAY_NAME,
							      FALSE);
	if (modest_account_mgr_account_exists (self, key, FALSE)) {
		g_printerr ("modest: account already exists\n");
		return FALSE;
	}
	
	ok = modest_conf_set_string (priv->modest_conf, key, name, &err);
	if (!ok) {
		g_printerr ("modest: cannot set display name\n");
		if (err) {
			g_printerr ("modest: Error adding account conf: %s\n", err->message);
			g_error_free (err);
		}
		return FALSE;
	}
	
	if (store_account) {
		key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_STORE_ACCOUNT,
								      FALSE);
		ok = modest_conf_set_string (priv->modest_conf, key, store_account, &err);
		if (!ok) {
			g_printerr ("modest: failed to set store account '%s'\n",
				    store_account);
			if (err) {
				g_printerr ("modest: Error adding store account conf: %s\n", err->message);
				g_error_free (err);
			}
			return FALSE;
		}
	}
	
	if (transport_account) {
		key = _modest_account_mgr_get_account_keyname_cached (priv, name,
								      MODEST_ACCOUNT_TRANSPORT_ACCOUNT,
								      FALSE);
		ok = modest_conf_set_string (priv->modest_conf, key, transport_account, &err);
		if (!ok) {
			g_printerr ("modest: failed to set transport account '%s'\n",
				    transport_account);
			if (err) {
				g_printerr ("modest: Error adding transport account conf: %s\n", err->message);
				g_error_free (err);
			}	
			return FALSE;
		}
	}

	/* Make sure that leave-messages-on-server is enabled by default, 
	 * as per the UI spec, though it is only meaningful for accounts using POP.
	 * (possibly this gconf key should be under the server account): */
	modest_account_mgr_set_bool (self, name, MODEST_ACCOUNT_LEAVE_ON_SERVER, TRUE, FALSE);
	modest_account_mgr_set_bool (self, name, MODEST_ACCOUNT_ENABLED, enabled,FALSE);

	/* Fill other data */
	modest_account_mgr_set_string (self, name,
				       MODEST_ACCOUNT_DISPLAY_NAME, 
				       display_name, FALSE);
	modest_account_mgr_set_string (self, name,
				       MODEST_ACCOUNT_FULLNAME, 
				       user_fullname, FALSE);
	modest_account_mgr_set_string (self, name,
				       MODEST_ACCOUNT_EMAIL, 
				       user_email, FALSE);
	modest_account_mgr_set_string (self, name,
				       MODEST_ACCOUNT_RETRIEVE, 
				       retrieve_type, FALSE);

	/* Notify the observers */
	g_signal_emit (self, signals[ACCOUNT_INSERTED_SIGNAL], 0, name);

	/* if no default account has been defined yet, do so now */
	default_account = modest_account_mgr_get_default_account (self);
	if (!default_account)
		modest_account_mgr_set_default_account (self, name);
	g_free (default_account);
	
	/* (re)set the automatic account update */
	modest_platform_set_update_interval
		(modest_conf_get_int (priv->modest_conf, MODEST_CONF_UPDATE_INTERVAL, NULL));
	
	return TRUE;
}


gboolean
modest_account_mgr_add_server_account (ModestAccountMgr * self,
				       const gchar *name, 
				       const gchar *hostname,
				       guint portnumber,
				       const gchar *username, 
				       const gchar *password,
				       ModestTransportStoreProtocol proto,
				       ModestConnectionProtocol security,
				       ModestAuthProtocol auth)
{
	ModestAccountMgrPrivate *priv;
	const gchar *key;
	gboolean ok = TRUE;
	GError *err = NULL;

	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (strchr(name, '/') == NULL, FALSE);
			      
	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	/* hostname */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_HOSTNAME, TRUE);
	if (modest_conf_key_exists (priv->modest_conf, key, &err)) {
		g_printerr ("modest: server account '%s' already exists\n", name);
		ok =  FALSE;
	}
	if (!ok)
		goto cleanup;
	
	modest_conf_set_string (priv->modest_conf, key, null_means_empty(hostname), &err);
	if (err) {
		g_printerr ("modest: failed to set %s: %s\n", key, err->message);
		g_error_free (err);
		ok = FALSE;
	}
	if (!ok)
		goto cleanup;
	
	/* username */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_USERNAME, TRUE);
	ok = modest_conf_set_string (priv->modest_conf, key, null_means_empty (username), &err);
	if (err) {
		g_printerr ("modest: failed to set %s: %s\n", key, err->message);
		g_error_free (err);
		ok = FALSE;
	}
	if (!ok)
		goto cleanup;
	
	
	/* password */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_PASSWORD, TRUE);
	ok = modest_conf_set_string (priv->modest_conf, key, null_means_empty (password), &err);
	if (err) {
		g_printerr ("modest: failed to set %s: %s\n", key, err->message);
		g_error_free (err);
		ok = FALSE;
	}
	if (!ok)
		goto cleanup;

	/* proto */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_PROTO, TRUE);
	ok = modest_conf_set_string (priv->modest_conf, key,
				     modest_protocol_info_get_transport_store_protocol_name(proto),
				     &err);
	if (err) {
		g_printerr ("modest: failed to set %s: %s\n", key, err->message);
		g_error_free (err);
		ok = FALSE;
	}
	if (!ok)
		goto cleanup;


	/* portnumber */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_PORT, TRUE);
	ok = modest_conf_set_int (priv->modest_conf, key, portnumber, &err);
	if (err) {
		g_printerr ("modest: failed to set %s: %s\n", key, err->message);
		g_error_free (err);
		ok = FALSE;
	}
	if (!ok)
		goto cleanup;

	
	/* auth mechanism */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_AUTH_MECH, TRUE);
	ok = modest_conf_set_string (priv->modest_conf, key,
				     modest_protocol_info_get_auth_protocol_name (auth),
				     &err);
	if (err) {
		g_printerr ("modest: failed to set %s: %s\n", key, err->message);
		g_error_free (err);
		ok = FALSE;
	}
	if (!ok)
		goto cleanup;
	
	/* Add the security settings: */
	modest_account_mgr_set_server_account_security (self, name, security);

cleanup:
	if (!ok) {
		g_printerr ("modest: failed to add server account\n");
		return FALSE;
	}

	return TRUE;
}

/** modest_account_mgr_add_server_account_uri:
 * Only used for mbox and maildir accounts.
 */
gboolean
modest_account_mgr_add_server_account_uri (ModestAccountMgr * self,
					   const gchar *name, 
					   ModestTransportStoreProtocol proto,
					   const gchar *uri)
{
	ModestAccountMgrPrivate *priv;
	const gchar *key;
	gboolean ok;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (strchr(name, '/') == NULL, FALSE);
	g_return_val_if_fail (uri, FALSE);
	
	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	
	/* proto */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_PROTO, TRUE);
	ok = modest_conf_set_string (priv->modest_conf, key,
				     modest_protocol_info_get_transport_store_protocol_name(proto),
				     NULL);

	if (!ok) {
		g_printerr ("modest: failed to set proto\n");
		return FALSE;
	}
	
	/* uri */
	key = _modest_account_mgr_get_account_keyname_cached (priv, name, MODEST_ACCOUNT_URI, TRUE);
	ok = modest_conf_set_string (priv->modest_conf, key, uri, NULL);

	if (!ok) {
		g_printerr ("modest: failed to set uri\n");
		return FALSE;
	}
	return TRUE;
}

/* 
 * Utility function used by modest_account_mgr_remove_account
 */
static void
real_remove_account (ModestConf *conf,
		     const gchar *acc_name,
		     gboolean server_account)
{
	GError *err = NULL;
	gchar *key;
	
	key = _modest_account_mgr_get_account_keyname (acc_name, NULL, server_account);
	modest_conf_remove_key (conf, key, &err);

	if (err) {
		g_printerr ("modest: error removing key: %s\n", err->message);
		g_error_free (err);
	}
	g_free (key);
}

gboolean
modest_account_mgr_remove_account (ModestAccountMgr * self,
				   const gchar* name)
{
	ModestAccountMgrPrivate *priv;
	gchar *default_account_name, *store_acc_name, *transport_acc_name;
	gboolean default_account_deleted;

	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);

	if (!modest_account_mgr_account_exists (self, name, FALSE)) {
		g_printerr ("modest: %s: account '%s' does not exist\n", __FUNCTION__, name);
		return FALSE;
	}

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	default_account_deleted = FALSE;

	/* If this was the default, then remove that setting: */
	default_account_name = modest_account_mgr_get_default_account (self);
	if (default_account_name && (strcmp (default_account_name, name) == 0)) {
		modest_account_mgr_unset_default_account (self);
		default_account_deleted = TRUE;
	}
	g_free (default_account_name);

	/* Delete transport and store accounts */
	store_acc_name = modest_account_mgr_get_string (self, name, 
							MODEST_ACCOUNT_STORE_ACCOUNT, FALSE);
	if (store_acc_name)
		real_remove_account (priv->modest_conf, store_acc_name, TRUE);

	transport_acc_name = modest_account_mgr_get_string (self, name, 
							    MODEST_ACCOUNT_TRANSPORT_ACCOUNT, FALSE);
	if (transport_acc_name)
		real_remove_account (priv->modest_conf, transport_acc_name, TRUE);
			
	/* Remove the modest account */
	real_remove_account (priv->modest_conf, name, FALSE);

	if (default_account_deleted) {	
		/* pick another one as the new default account. We do
		   this *after* deleting the keys, because otherwise a
		   call to account_names will retrieve also the
		   deleted account */
		modest_account_mgr_set_first_account_as_default (self);
	}
	
	/* Notify the observers. We do this *after* deleting
	   the keys, because otherwise a call to account_names
	   will retrieve also the deleted account */
	g_signal_emit (G_OBJECT(self), signals[ACCOUNT_REMOVED_SIGNAL], 0, name);

	/* if this was the last account, stop any auto-updating */
	/* (re)set the automatic account update */
	GSList *acc_names = modest_account_mgr_account_names (self, TRUE);
	if (!acc_names) 
		modest_platform_set_update_interval (0);
	else
		modest_account_mgr_free_account_names (acc_names);
	
	return TRUE;
}



/* strip the first /n/ character from each element
 * caller must make sure all elements are strings with
 * length >= n, and also that data can be freed.
 * change is in-place
 */
static void
strip_prefix_from_elements (GSList * lst, guint n)
{
	while (lst) {
		memmove (lst->data, lst->data + n,
			 strlen(lst->data) - n + 1);
		lst = lst->next;
	}
}


GSList*
modest_account_mgr_account_names (ModestAccountMgr * self, gboolean only_enabled)
{
	GSList *accounts;
	ModestAccountMgrPrivate *priv;
	GError *err = NULL;
	
	const size_t prefix_len = strlen (MODEST_ACCOUNT_NAMESPACE "/");

	g_return_val_if_fail (self, NULL);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	accounts = modest_conf_list_subkeys (priv->modest_conf,
                                             MODEST_ACCOUNT_NAMESPACE, &err);

	if (err) {
		g_printerr ("modest: failed to get subkeys (%s): %s\n",
			    MODEST_ACCOUNT_NAMESPACE, err->message);
		g_error_free (err);
		return NULL; /* assume accounts did not get value when err is set...*/
	}
	
	strip_prefix_from_elements (accounts, prefix_len);
		
	GSList *result = NULL;
	
	/* Unescape the keys to get the account names: */
	GSList *iter = accounts;
	while (iter) {
		if (!(iter->data))
			continue;
			
		const gchar* account_name_key = (const gchar*)iter->data;
		gchar* unescaped_name = account_name_key ? 
			modest_conf_key_unescape (account_name_key) 
			: NULL;
		
		gboolean add = TRUE;
		if (only_enabled) {
			if (unescaped_name && 
			    !modest_account_mgr_get_bool (self, unescaped_name, 
							  MODEST_ACCOUNT_ENABLED, FALSE))
				add = FALSE;
		}
		
		/* Ignore modest accounts whose server accounts don't exist: 
		 * (We could be getting this list while the account is being deleted, 
		 * while the child server accounts have already been deleted, but the 
		 * parent modest account already exists.
		 */
		if (add) {
			gchar* server_account_name = modest_account_mgr_get_string
				(self, account_name_key, MODEST_ACCOUNT_STORE_ACCOUNT,
				 FALSE);
			if (server_account_name) {
				if (!modest_account_mgr_account_exists (self, server_account_name, TRUE))
					add = FALSE;
				g_free (server_account_name);
			}
		}
		
		if (add) {
			gchar* server_account_name = modest_account_mgr_get_string
				(self, account_name_key, MODEST_ACCOUNT_TRANSPORT_ACCOUNT,
				 FALSE);
			if (server_account_name) {
				if (!modest_account_mgr_account_exists (self, server_account_name, TRUE))
					add = FALSE;
				g_free (server_account_name);
			}
		}
		
		if (add)
			result = g_slist_append (result, unescaped_name);
		else 
			g_free (unescaped_name);

		g_free (iter->data);
		iter->data = NULL;
		
		iter = g_slist_next (iter);	
	}
	

	/* we already freed the strings in the loop */
	g_slist_free (accounts);
	
	accounts = NULL;
	return result;
}


void
modest_account_mgr_free_account_names (GSList *account_names)
{
	g_slist_foreach (account_names, (GFunc)g_free, NULL);
	g_slist_free (account_names);
}



gchar *
modest_account_mgr_get_string (ModestAccountMgr *self, const gchar *name,
			       const gchar *key, gboolean server_account) {

	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gchar *retval;
	GError *err = NULL;

	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (name, NULL);
	g_return_val_if_fail (key, NULL);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	
	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);
	
	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	retval = modest_conf_get_string (priv->modest_conf, keyname, &err);	
	if (err) {
		g_printerr ("modest: error getting string '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = NULL;
	}

	return retval;
}


gint
modest_account_mgr_get_int (ModestAccountMgr *self, const gchar *name, const gchar *key,
			    gboolean server_account)
{
	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gint retval;
	GError *err = NULL;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), -1);
	g_return_val_if_fail (name, -1);
	g_return_val_if_fail (key, -1);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);
	
	retval = modest_conf_get_int (priv->modest_conf, keyname, &err);
	if (err) {
		g_printerr ("modest: error getting int '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = -1;
	}

	return retval;
}



gboolean
modest_account_mgr_get_bool (ModestAccountMgr * self, const gchar *account,
			     const gchar * key, gboolean server_account)
{
	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gboolean retval;
	GError *err = NULL;

	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (account, FALSE);
	g_return_val_if_fail (key, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	///keyname = _modest_account_mgr_get_account_keyname (account, key, server_account);

	keyname = _modest_account_mgr_get_account_keyname_cached (priv, account, key, server_account);
		
	retval = modest_conf_get_bool (priv->modest_conf, keyname, &err);		
	if (err) {
		g_printerr ("modest: error getting bool '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = FALSE;
	}

	return retval;
}



GSList * 
modest_account_mgr_get_list (ModestAccountMgr *self, const gchar *name,
			     const gchar *key, ModestConfValueType list_type,
			     gboolean server_account)
{
	ModestAccountMgrPrivate *priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	const gchar *keyname;
	GSList *retval;
	GError *err = NULL;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), NULL);
	g_return_val_if_fail (name, NULL);
	g_return_val_if_fail (key, NULL);
	
	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key,
								  server_account);
	
	retval = modest_conf_get_list (priv->modest_conf, keyname, list_type, &err);
	if (err) {
		g_printerr ("modest: error getting list '%s': %s\n", keyname,
			    err->message);
		g_error_free (err);
		retval = FALSE;
	}
	return retval;
}


gboolean
modest_account_mgr_set_string (ModestAccountMgr * self, 
			       const gchar * name,
			       const gchar * key, 
			       const gchar * val, 
			       gboolean server_account)
{
	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gboolean retval;
	GError *err = NULL;

	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (key, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);
	
	retval = modest_conf_set_string (priv->modest_conf, keyname, val, &err);
	if (err) {
		g_printerr ("modest: error setting string '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = FALSE;
	}
	return retval;
}

gboolean
modest_account_mgr_set_int (ModestAccountMgr * self, const gchar * name,
			    const gchar * key, int val, gboolean server_account)
{
	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gboolean retval;
	GError *err = NULL;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (key, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);
	
	retval = modest_conf_set_int (priv->modest_conf, keyname, val, &err);
	if (err) {
		g_printerr ("modest: error setting int '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = FALSE;
	}
	return retval;
}



gboolean
modest_account_mgr_set_bool (ModestAccountMgr * self, const gchar * name,
			     const gchar * key, gboolean val, gboolean server_account)
{
	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gboolean retval;
	GError *err = NULL;

	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (key, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);
	
	retval = modest_conf_set_bool (priv->modest_conf, keyname, val, &err);
	if (err) {
		g_printerr ("modest: error setting bool '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = FALSE;
	}

	return retval;
}


gboolean
modest_account_mgr_set_list (ModestAccountMgr *self,
			     const gchar *name,
			     const gchar *key,
			     GSList *val,
			     ModestConfValueType list_type,
			     gboolean server_account)
{
	ModestAccountMgrPrivate *priv;
	const gchar *keyname;
	GError *err = NULL;
	gboolean retval;
	
	g_return_val_if_fail (self, FALSE);
	g_return_val_if_fail (name, FALSE);
	g_return_val_if_fail (key,  FALSE);
	g_return_val_if_fail (val,  FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);

	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);
	
	retval = modest_conf_set_list (priv->modest_conf, keyname, val, list_type, &err);
	if (err) {
		g_printerr ("modest: error setting list '%s': %s\n", keyname, err->message);
		g_error_free (err);
		retval = FALSE;
	}

	return retval;
}

gboolean
modest_account_mgr_account_exists (ModestAccountMgr * self, const gchar* name,
				   gboolean server_account)
{
	ModestAccountMgrPrivate *priv;

	const gchar *keyname;
	gboolean retval;
	GError *err = NULL;

	g_return_val_if_fail (self, FALSE);
        g_return_val_if_fail (name, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, NULL, server_account);
	retval = modest_conf_key_exists (priv->modest_conf, keyname, &err);
	if (err) {
		g_printerr ("modest: error determining existance of '%s': %s\n", keyname,
			    err->message);
		g_error_free (err);
		retval = FALSE;
	}
	return retval;
}

gboolean
modest_account_mgr_account_with_display_name_exists  (ModestAccountMgr *self, 
						      const gchar *display_name)
{
	GSList *account_names = NULL;
	GSList *cursor = NULL;
	
	cursor = account_names = modest_account_mgr_account_names (self, 
								   TRUE /* enabled accounts, because disabled accounts are not user visible. */);

	gboolean found = FALSE;
	
	/* Look at each non-server account to check their display names; */
	while (cursor) {
		const gchar * account_name = (gchar*)cursor->data;
		
		ModestAccountData *account_data = modest_account_mgr_get_account_data (self, account_name);
		if (!account_data) {
			g_printerr ("modest: failed to get account data for %s\n", account_name);
			continue;
		}

		if(account_data->display_name && (strcmp (account_data->display_name, display_name) == 0)) {
			found = TRUE;
			break;
		}

		modest_account_mgr_free_account_data (self, account_data);
		cursor = cursor->next;
	}
	modest_account_mgr_free_account_names (account_names);
	account_names = NULL;
	
	return found;
}




gboolean 
modest_account_mgr_unset (ModestAccountMgr *self, const gchar *name,
			  const gchar *key, gboolean server_account)
{
	ModestAccountMgrPrivate *priv;
	
	const gchar *keyname;
	gboolean retval;
	GError *err = NULL;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
        g_return_val_if_fail (name, FALSE);
        g_return_val_if_fail (key, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	keyname = _modest_account_mgr_get_account_keyname_cached (priv, name, key, server_account);

	retval = modest_conf_remove_key (priv->modest_conf, keyname, &err);
	if (err) {
		g_printerr ("modest: error unsetting'%s': %s\n", keyname,
			    err->message);
		g_error_free (err);
		retval = FALSE;
	}
	return retval;
}

gchar*
_modest_account_mgr_account_from_key (const gchar *key, gboolean *is_account_key, gboolean *is_server_account)
{
	/* Initialize input parameters: */
	if (is_account_key)
		*is_account_key = FALSE;

	if (is_server_account)
		*is_server_account = FALSE;

	const gchar* account_ns        = MODEST_ACCOUNT_NAMESPACE "/";
	const gchar* server_account_ns = MODEST_SERVER_ACCOUNT_NAMESPACE "/";
	gchar *cursor;
	gchar *account = NULL;

	/* determine whether it's an account or a server account,
	 * based on the prefix */
	if (g_str_has_prefix (key, account_ns)) {

		if (is_server_account)
			*is_server_account = FALSE;
		
		account = g_strdup (key + strlen (account_ns));

	} else if (g_str_has_prefix (key, server_account_ns)) {

		if (is_server_account)
			*is_server_account = TRUE;
		
		account = g_strdup (key + strlen (server_account_ns));	
	} else
		return NULL;

	/* if there are any slashes left in the key, it's not
	 * the toplevel entry for an account
	 */
	cursor = strstr(account, "/");
	
	if (is_account_key && cursor)
		*is_account_key = TRUE;

	/* put a NULL where the first slash was */
	if (cursor)
		*cursor = '\0';

	if (account) {
		/* The key is an escaped string, so unescape it to get the actual account name: */
		gchar *unescaped_name = modest_conf_key_unescape (account);
		g_free (account);
		return unescaped_name;
	} else
		return NULL;
}





/* optimization: only with non-alphanum chars, escaping is needed */
inline static gboolean
is_alphanum (const gchar* str)
{
	const gchar *cursor;
	for (cursor = str; cursor && *cursor; ++cursor) {
		const char c = *cursor;
		/* we cannot trust isalnum(3), because it might consider locales */
		/*       numbers            ALPHA            alpha       */
		if (!((c>=48 && c<=57)||(c>=65 && c<=90)||(c>=97 && c<=122)))
			return FALSE;
	}
	return TRUE;
}
		



/* must be freed by caller */
gchar *
_modest_account_mgr_get_account_keyname (const gchar *account_name, const gchar* name,
					 gboolean server_account)
{
	gchar *retval = NULL;	
	gchar *namespace = server_account ? MODEST_SERVER_ACCOUNT_NAMESPACE : MODEST_ACCOUNT_NAMESPACE;
	gchar *escaped_account_name, *escaped_name;
	
	if (!account_name)
		return g_strdup (namespace);

	/* optimization: only escape names when need to be escaped */
	if (is_alphanum (account_name))
		escaped_account_name = (gchar*)account_name;
	else
		escaped_account_name = modest_conf_key_escape (account_name);
	
	if (is_alphanum (name))
		escaped_name = (gchar*)name;
	else
		escaped_name = modest_conf_key_escape (name);
	//////////////////////////////////////////////////////////////

	if (escaped_account_name && escaped_name)
		retval = g_strconcat (namespace, "/", escaped_account_name, "/", escaped_name, NULL);
	else if (escaped_account_name)
		retval = g_strconcat (namespace, "/", escaped_account_name, NULL);

	/* Sanity check: */
	if (!retval || !modest_conf_key_is_valid (retval)) {
		g_warning ("%s: Generated conf key was invalid: %s", __FUNCTION__,
			   retval ? retval: "<empty>");
		g_free (retval);
		retval = NULL;
	}
	
	/* g_free is only needed if we actually allocated anything */
	if (name != escaped_name)
		g_free (escaped_name);
	if (account_name != escaped_account_name)
		g_free (escaped_account_name);

	return retval;
}

static const gchar *
_modest_account_mgr_get_account_keyname_cached (ModestAccountMgrPrivate *priv, 
						const gchar* account_name,
						const gchar *name, 
						gboolean is_server)
{
	GHashTable *hash = is_server ? priv->server_account_key_hash : priv->account_key_hash;
	GHashTable *account_hash;
	gchar *key = NULL;
	const gchar *search_name;

	if (!account_name)
		return is_server ? MODEST_SERVER_ACCOUNT_NAMESPACE : MODEST_ACCOUNT_NAMESPACE;

	search_name = name ? name : "<dummy>";
	
	account_hash = g_hash_table_lookup (hash, account_name);	
	if (!account_hash) { /* no hash for this account yet? create it */
		account_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);		
		key = _modest_account_mgr_get_account_keyname (account_name, name, is_server);
		g_hash_table_insert (account_hash, g_strdup(search_name), key);
		g_hash_table_insert (hash, g_strdup(account_name), account_hash);
		return key;
	}
	
	/* we have a hash for this account, but do we have the key? */	
	key = g_hash_table_lookup (account_hash, search_name);
	if (!key) {
		key = _modest_account_mgr_get_account_keyname (account_name, name, is_server);
		g_hash_table_insert (account_hash, g_strdup(search_name), key);
	}
	
	return key;
}


gboolean
modest_account_mgr_has_accounts (ModestAccountMgr* self, gboolean enabled)
{
	/* Check that at least one account exists: */
	GSList *account_names = modest_account_mgr_account_names (self,
								  enabled);
	gboolean accounts_exist = account_names != NULL;
	
	modest_account_mgr_free_account_names (account_names);
	account_names = NULL;
	
	return accounts_exist;
}

static int
compare_account_name(gconstpointer a, gconstpointer b)
{
	const gchar* account_name = (const gchar*) a;
	const gchar* account_name2 = (const gchar*) b;
	return strcmp(account_name, account_name2);
}

void 
modest_account_mgr_set_account_busy(ModestAccountMgr* self, 
				    const gchar* account_name, 
				    gboolean busy)
{
	ModestAccountMgrPrivate* priv;

	g_return_if_fail (MODEST_IS_ACCOUNT_MGR(self));
	g_return_if_fail (account_name);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
	if (busy) {
		GSList *account_names = modest_account_mgr_account_names (self, TRUE);
		GSList* account = g_slist_find_custom(account_names, account_name, 
						      (GCompareFunc) compare_account_name);

		if (account && !modest_account_mgr_account_is_busy(self, account_name))	{
			priv->busy_accounts = g_slist_append(priv->busy_accounts, g_strdup(account_name));
			g_signal_emit (G_OBJECT(self), signals[ACCOUNT_BUSY_SIGNAL], 
				       0, account_name, TRUE);
		}
		modest_account_mgr_free_account_names (account_names);
		account_names = NULL;
	} else {
		GSList* account = 
			g_slist_find_custom(priv->busy_accounts, account_name, (GCompareFunc) compare_account_name);

		if (account) {
			g_free(account->data);
			priv->busy_accounts = g_slist_delete_link(priv->busy_accounts, account);
			g_signal_emit (G_OBJECT(self), signals[ACCOUNT_BUSY_SIGNAL], 
				       0, account_name, FALSE);
		}
	}
}

gboolean
modest_account_mgr_account_is_busy (ModestAccountMgr* self, const gchar* account_name)
{
	ModestAccountMgrPrivate* priv;
	
	g_return_val_if_fail (MODEST_IS_ACCOUNT_MGR(self), FALSE);
	g_return_val_if_fail (account_name, FALSE);

	priv = MODEST_ACCOUNT_MGR_GET_PRIVATE (self);
		
	return (g_slist_find_custom(priv->busy_accounts, account_name, (GCompareFunc) compare_account_name)
		!= NULL);
}

void
modest_account_mgr_notify_account_update (ModestAccountMgr* self, 
					  const gchar *server_account_name)
{
	ModestTransportStoreProtocol proto;
	gchar *proto_name = NULL;

	/* Get protocol */
	proto_name = modest_account_mgr_get_string (self, server_account_name, 
						    MODEST_ACCOUNT_PROTO, TRUE);
	if (!proto_name) {
		g_free (proto_name);
		g_return_if_reached ();
	}
	proto = modest_protocol_info_get_transport_store_protocol (proto_name);
	g_free (proto_name);

	/* Emit "update-account" */
	g_signal_emit (G_OBJECT(self), 
		       signals[ACCOUNT_CHANGED_SIGNAL], 0, 
		       server_account_name, 
		       (modest_protocol_info_protocol_is_store (proto)) ? 
		       TNY_ACCOUNT_TYPE_STORE : 
		       TNY_ACCOUNT_TYPE_TRANSPORT);
}


gboolean
modest_account_mgr_set_default_account  (ModestAccountMgr *self, const gchar* account)
{
	ModestConf *conf;
	gboolean retval;
	
	g_return_val_if_fail (self,    FALSE);
	g_return_val_if_fail (account, FALSE);
	g_return_val_if_fail (modest_account_mgr_account_exists (self, account, FALSE),
			      FALSE);
	
	conf = MODEST_ACCOUNT_MGR_GET_PRIVATE (self)->modest_conf;

	/* Change the default account and notify */
	retval = modest_conf_set_string (conf, MODEST_CONF_DEFAULT_ACCOUNT, account, NULL);
	if (retval)
		g_signal_emit (G_OBJECT(self), signals[DEFAULT_ACCOUNT_CHANGED_SIGNAL], 0);

	return retval;
}


gchar*
modest_account_mgr_get_default_account  (ModestAccountMgr *self)
{
	gchar *account;	
	ModestConf *conf;
	GError *err = NULL;
	
	g_return_val_if_fail (self, NULL);

	conf = MODEST_ACCOUNT_MGR_GET_PRIVATE (self)->modest_conf;
	account = modest_conf_get_string (conf, MODEST_CONF_DEFAULT_ACCOUNT, &err);
	
	if (err) {
		g_printerr ("modest: failed to get '%s': %s\n",
			    MODEST_CONF_DEFAULT_ACCOUNT, err->message);
		g_error_free (err);
		g_free (account);
		return  NULL;
	}
	
	/* sanity check */
	if (account && !modest_account_mgr_account_exists (self, account, FALSE)) {
		g_printerr ("modest: default account does not exist\n");
		g_free (account);
		return NULL;
	}

	return account;
}

static gboolean
modest_account_mgr_unset_default_account (ModestAccountMgr *self)
{
	ModestConf *conf;
	gboolean retval;
	
	g_return_val_if_fail (self,    FALSE);

	conf = MODEST_ACCOUNT_MGR_GET_PRIVATE (self)->modest_conf;
		
	retval = modest_conf_remove_key (conf, MODEST_CONF_DEFAULT_ACCOUNT, NULL /* err */);

	if (retval)
		g_signal_emit (G_OBJECT(self), signals[DEFAULT_ACCOUNT_CHANGED_SIGNAL], 0);

	return retval;
}


gchar* 
modest_account_mgr_get_display_name (ModestAccountMgr *self, 
				     const gchar* name)
{
	return modest_account_mgr_get_string (self, name, MODEST_ACCOUNT_DISPLAY_NAME, FALSE);
}

void 
modest_account_mgr_set_display_name (ModestAccountMgr *self, 
				     const gchar *account_name,
				     const gchar *display_name)
{
	modest_account_mgr_set_string (self, 
				       account_name,
				       MODEST_ACCOUNT_DISPLAY_NAME, 
				       display_name, 
				       FALSE /* not server account */);

	/* Notify about the change in the display name */
	g_signal_emit (self, signals[DISPLAY_NAME_CHANGED_SIGNAL], 0, account_name);
}
