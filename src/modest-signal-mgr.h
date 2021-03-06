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

#ifndef __MODEST_SIGNAL_MGR_H__
#define __MODEST_SIGNAL_MGR_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * modest_signal_mgr_connect:
 *
 * do a g_signal_connect, and add the handler data to the list
 * @lst: a GSList
 *  
 * TRUE if this succeeded, FALSE otherwise.
 */
GSList *  modest_signal_mgr_connect               (GSList *lst, 
						   GObject *instance, 
						   const gchar *signal_name,
						   GCallback handler, 
						   gpointer data);

/**
 * modest_signal_mgr_disconnect:
 * @list: 
 * @instance: 
 * 
 * disconnect the handler for a particular object for a particular signal
 * 
 * Returns: 
 **/
GSList *  modest_signal_mgr_disconnect            (GSList *list, 
						   GObject *instance,
						   const gchar *signal_name);


/**
 * modest_signal_mgr_disconnect_all_and_destroy:
 * @lst: the list of signal handlers
 *
 * disconnect all signals in the list, and destroy the list
 */
void      modest_signal_mgr_disconnect_all_and_destroy (GSList *lst);

/**
 * modest_signal_mgr_disconnect:
 * @list: 
 * @instance: 
 * 
 * disconnect the handler for a particular object for a particular signal
 * 
 * Returns: 
 **/
gboolean  modest_signal_mgr_is_connected               (GSList *list, 
							GObject *instance,
							const gchar *signal_name);


G_END_DECLS
#endif /*__MODEST_SIGNAL_MGR__*/
