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


#ifndef __MODEST_GNOME_INFO_BAR_H__
#define __MODEST_GNOME_INFO_BAR_H__

#include <gtk/gtkhbox.h>
#include "modest-progress-object.h"
/* other include files */

G_BEGIN_DECLS

/* convenience macros */
#define MODEST_TYPE_GNOME_INFO_BAR             (modest_gnome_info_bar_get_type())
#define MODEST_GNOME_INFO_BAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),MODEST_TYPE_GNOME_INFO_BAR,ModestGnomeInfoBar))
#define MODEST_GNOME_INFO_BAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),MODEST_TYPE_GNOME_INFO_BAR,ModestProgressObject))
#define MODEST_IS_GNOME_INFO_BAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),MODEST_TYPE_GNOME_INFO_BAR))
#define MODEST_IS_GNOME_INFO_BAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),MODEST_TYPE_GNOME_INFO_BAR))
#define MODEST_GNOME_INFO_BAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj),MODEST_TYPE_GNOME_INFO_BAR,ModestGnomeInfoBarClass))

typedef struct _ModestGnomeInfoBar      ModestGnomeInfoBar;
typedef struct _ModestGnomeInfoBarClass ModestGnomeInfoBarClass;

struct _ModestGnomeInfoBar {
	 GtkHBox parent;
};

struct _ModestGnomeInfoBarClass {
	GtkHBoxClass parent_class;
};

/* member functions */
GType         modest_gnome_info_bar_get_type       (void) G_GNUC_CONST;

/* typical parameter-less _new function */
GtkWidget*    modest_gnome_info_bar_new            (void);

/**
 * modest_gnome_info_bar_new:
 * @void: 
 * 
 * Sets a text in the status bar of the widget
 * 
 * Return value: 
 **/
void          modest_gnome_info_bar_set_message    (ModestGnomeInfoBar *self,
						    const gchar *message);


/**
 * modest_gnome_info_bar_set_progress:
 * @self: 
 * @message: 
 * @done: 
 * @total: 
 * 
 * Causes the progress bar of the widget to fill in the amount of work
 * done of a given total. If message is supplied then it'll be
 * superimposed on the progress bar
 **/
void          modest_gnome_info_bar_set_progress   (ModestGnomeInfoBar *self,
						    const gchar *message,
						    gint done,
						    gint total);

G_END_DECLS

#endif /* __MODEST_GNOME_INFO_BAR_H__ */

