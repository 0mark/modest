/* Copyright (c) 2007, Nokia Corporation
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

#ifndef MODEST_RECPT_VIEW_H
#define MODEST_RECPT_VIEW_H
#include <glib-object.h>
#include <widgets/modest-scroll-text.h>

G_BEGIN_DECLS

#define MODEST_TYPE_RECPT_VIEW             (modest_recpt_view_get_type ())
#define MODEST_RECPT_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MODEST_TYPE_RECPT_VIEW, ModestRecptView))
#define MODEST_RECPT_VIEW_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), MODEST_TYPE_RECPT_VIEW, ModestRecptViewClass))
#define MODEST_IS_RECPT_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MODEST_TYPE_RECPT_VIEW))
#define MODEST_IS_RECPT_VIEW_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), MODEST_TYPE_RECPT_VIEW))
#define MODEST_RECPT_VIEW_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), MODEST_TYPE_RECPT_VIEW, ModestRecptViewClass))

typedef struct _ModestRecptView ModestRecptView;
typedef struct _ModestRecptViewClass ModestRecptViewClass;

struct _ModestRecptView
{
	ModestScrollText parent;

};

struct _ModestRecptViewClass
{
	ModestScrollTextClass parent_class;

	void (*activate)           (ModestRecptView *recpt_view, const gchar *address);
};

GType modest_recpt_view_get_type (void);

GtkWidget* modest_recpt_view_new (void);

void modest_recpt_view_set_recipients (ModestRecptView *recpt_view, const gchar *recipients);

G_END_DECLS

#endif
