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

#ifndef __MODEST_SINGLETONS_H__
#define __MODEST_SINGLETONS_H__

#include <config.h>

#include <glib-object.h>
#include <modest-conf.h>
#include <modest-account-mgr.h>
#include <modest-tny-account-store.h>
#include <modest-mail-operation-queue.h>
#include <modest-cache-mgr.h>
#include <modest-widget-factory.h>

G_BEGIN_DECLS

/* convenience macros */
#define MODEST_TYPE_SINGLETONS             (modest_singletons_get_type())
#define MODEST_SINGLETONS(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),MODEST_TYPE_SINGLETONS,ModestSingletons))
#define MODEST_SINGLETONS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),MODEST_TYPE_SINGLETONS,GObject))
#define MODEST_IS_SINGLETONS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),MODEST_TYPE_SINGLETONS))
#define MODEST_IS_SINGLETONS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),MODEST_TYPE_SINGLETONS))
#define MODEST_SINGLETONS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),MODEST_TYPE_SINGLETONS,ModestSingletonsClass))

typedef struct _ModestSingletons      ModestSingletons;
typedef struct _ModestSingletonsClass ModestSingletonsClass;

struct _ModestSingletons {
	 GObject parent;
};

struct _ModestSingletonsClass {
	GObjectClass parent_class;
};


/**
 * modest_singletons_get_type:
 * 
 * get the GType for ModestSingletons
 *  
 * Returns: the GType
 */
GType        modest_singletons_get_type    (void) G_GNUC_CONST;


/**
 * modest_singletons_new:
 * 
 * create a new ModestSingletons instance;
 * don't use this function directly, use the modest-runtime
 * functions instead.
 * 
 * Returns: a new ModestSingletons instance, or NULL in case
 * of any error
 */
ModestSingletons*    modest_singletons_new         (void);


/**
 * modest_singletons_get_conf:
 * 
 * get the ModestConf singleton instance
 * don't use this function directly, use the modest-runtime
 * functions instead.
 *
 * Returns: the ModestConf singleton
 **/
ModestConf*               modest_singletons_get_conf          (ModestSingletons *self);


/**
 * modest_singletons_get_account_mgr:
 * 
 * get the ModestAccountMgr singleton instance
 * don't use this function directly, use the modest-runtime
 * functions instead.
 * 
 * Returns: the ModestAccountMgr singleton
 **/
ModestAccountMgr*         modest_singletons_get_account_mgr   (ModestSingletons *self);


/**
 * modest_singletons_get_account_store:
 * 
 * get the ModestTnyAccountStore singleton instance
 * don't use this function directly, use the modest-runtime
 * functions instead.
 *
 * Returns: the ModestTnyAccountStore singleton
 **/
ModestTnyAccountStore*    modest_singletons_get_account_store (ModestSingletons *self);


/**
 * modest_singletons_get_cache_mgr:
 * 
 * get the ModestCacheMgr singleton instance
 * don't use this function directly, use the modest-runtime
 * functions instead.
 *
 * Returns: the ModestCacheMgr singleton
 **/
ModestCacheMgr*           modest_singletons_get_cache_mgr     (ModestSingletons *self);


/**
 * modest_singletons_get_mail_operation_queue:
 * 
 * get the ModestMailOperationQueue singleton instance
 * don't use this function directly, use the modest-runtime
 * functions instead.
 *
 * Returns: the ModestMailOperationQueue singleton
 **/
ModestMailOperationQueue* modest_singletons_get_mail_operation_queue (ModestSingletons *self);




/**
 * modest_singletons_get_widget_factory:
 * 
 * get the ModestWidgetFactory singleton instance
 * don't use this function directly, use the modest-runtime
 * functions instead.
 *
 * Returns: the ModestWidgetFactory singleton
 **/
ModestWidgetFactory* modest_singletons_get_widget_factory (ModestSingletons *self);


G_END_DECLS

#endif /* __MODEST_SINGLETONS_H__ */

