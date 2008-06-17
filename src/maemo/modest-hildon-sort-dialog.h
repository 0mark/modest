/* Copyright (c) 2006, Nokia Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 * * Neither the name of the Nokia Corporation nor the names of its
 *	 contributors may be used to endorse or promote products derived from
 *	 this software without specific prior written permission.
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

#ifndef __MODEST_HILDON_SORT_DIALOG
#define __MODEST_HILDON_SORT_DIALOG

#include <glib.h>
#include <hildon/hildon-sort-dialog.h>
#include <widgets/modest-sort-criterium-view.h>

G_BEGIN_DECLS

#define MODEST_TYPE_HILDON_SORT_DIALOG modest_hildon_sort_dialog_get_type()

#define MODEST_HILDON_SORT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), \
	MODEST_TYPE_HILDON_SORT_DIALOG, ModestHildonSortDialog))

#define MODEST_HILDON_SORT_DIALOG_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), \
	MODEST_TYPE_HILDON_SORT_DIALOG, ModestHildonSortDialogClass))

#define MODEST_IS_HILDON_SORT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
	MODEST_TYPE_HILDON_SORT_DIALOG))

#define MODEST_IS_HILDON_SORT_DIALOG_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), \
	MODEST_TYPE_HILDON_SORT_DIALOG))

#define MODEST_HILDON_SORT_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), \
	MODEST_TYPE_HILDON_SORT_DIALOG, ModestHildonSortDialogClass))

typedef struct {
	HildonSortDialog parent;
	
} ModestHildonSortDialog;

typedef struct {
	HildonSortDialogClass parent_class;

} ModestHildonSortDialogClass;

GType modest_hildon_sort_dialog_get_type (void);

GtkWidget* modest_hildon_sort_dialog_new (GtkWindow *parent);

G_END_DECLS

#endif /* __MODEST_HILDON_SORT_DIALOG */
