/*
 * Copyright (C) 2008 Nokia Corporation, all rights reserved.
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

#ifndef                                         __MODEST_NUMBER_EDITOR_H__
#define                                         __MODEST_NUMBER_EDITOR_H__

#include                                        <gtk/gtk.h>
#include                                        <hildon/hildon-entry.h>

G_BEGIN_DECLS

#define                                         MODEST_TYPE_NUMBER_EDITOR \
                                                (modest_number_editor_get_type())

#define                                         MODEST_NUMBER_EDITOR(obj) \
                                                (G_TYPE_CHECK_INSTANCE_CAST (obj, MODEST_TYPE_NUMBER_EDITOR, ModestNumberEditor))

#define                                         MODEST_NUMBER_EDITOR_CLASS(klass) \
                                                (G_TYPE_CHECK_CLASS_CAST ((klass), MODEST_TYPE_NUMBER_EDITOR, \
                                                ModestNumberEditorClass))

#define                                         MODEST_IS_NUMBER_EDITOR(obj) \
                                                (G_TYPE_CHECK_INSTANCE_TYPE (obj, MODEST_TYPE_NUMBER_EDITOR))

#define                                         MODEST_IS_NUMBER_EDITOR_CLASS(klass) \
                                                (G_TYPE_CHECK_CLASS_TYPE ((klass), MODEST_TYPE_NUMBER_EDITOR))

#define                                         MODEST_NUMBER_EDITOR_GET_CLASS(obj) \
						(G_TYPE_INSTANCE_GET_CLASS ((obj), MODEST_TYPE_NUMBER_EDITOR, \
						ModestNumberEditorClass))

typedef struct                                  _ModestNumberEditor ModestNumberEditor;

typedef struct                                  _ModestNumberEditorClass ModestNumberEditorClass;

struct                                          _ModestNumberEditor 
{
    HildonEntry parent;
};

typedef enum
{
    MODEST_NUMBER_EDITOR_ERROR_MAXIMUM_VALUE_EXCEED,
    MODEST_NUMBER_EDITOR_ERROR_MINIMUM_VALUE_EXCEED,
    MODEST_NUMBER_EDITOR_ERROR_ERRONEOUS_VALUE
}                                               ModestNumberEditorErrorType;

struct                                          _ModestNumberEditorClass 
{
	HildonEntryClass parent_class;
	
	gboolean  (*range_error)  (ModestNumberEditor *editor, ModestNumberEditorErrorType type); 
	void      (*valid_changed) (ModestNumberEditor *editor, gboolean valid);
};

GType G_GNUC_CONST
modest_number_editor_get_type                   (void);

GtkWidget*  
modest_number_editor_new                        (gint min, gint max);

void
modest_number_editor_set_range                  (ModestNumberEditor *editor, 
                                                 gint min,
                                                 gint max);

gint
modest_number_editor_get_value                  (ModestNumberEditor *editor);

void
modest_number_editor_set_value                  (ModestNumberEditor *editor, 
                                                 gint value);

gboolean
modest_number_editor_is_valid                   (ModestNumberEditor *editor);


GType modest_number_editor_error_type_get_type (void);
#define MODEST_TYPE_NUMBER_EDITOR_ERROR_TYPE (modest_number_editor_error_type_get_type())

G_END_DECLS

#endif                                          /* __MODEST_NUMBER_EDITOR_H__ */
