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


#ifndef __MODEST_MAEMO_UTILS_H__
#define __MODEST_MAEMO_UTILS_H__

#include <gtk/gtk.h>

/**
 * modest_maemo_utils_menubar_to_menu:
 * @ui_manager: a ui manager, with the menubar at "/MenuBar" 
 * 
 * convert a menubar description (in a GtkUIManager) in to a menu
 * 
 * Returns: a new menu, or NULL in case of error
 */
GtkWidget*    modest_maemo_utils_menubar_to_menu (GtkUIManager *ui_manager);


/**
 * modest_maemo_utils_get_device_name
 *
 * get the name for this device. Note: this queries the bluetooth
 * name over DBUS, and may block. The result will be available in
 * MODEST_CONF_DEVICE_NAME in ModestConf; it will be updated when it
 * changes
 * 
 */
void modest_maemo_utils_get_device_name (void);

/**
 * modest_maemo_utils_folder_writable:
 * @filename: a string
 *
 * Checks if @filename is in a writable folder
 *
 * Returns: %TRUE if @filename is writable, %FALSE otherwise
 */
gboolean modest_maemo_utils_folder_writable (const gchar *filename);

/**
 * modest_maemo_utils_file_exists:
 * @filename: a string
 *
 * Checks if @filename exists
 *
 * Returns: %TRUE if @filename currently exists, %FALSE otherwise
 */
gboolean modest_maemo_utils_file_exists (const gchar *filename);

/**
 * modest_maemo_utils_create_temp_stream:
 * @path: a string with the created file path
 *
 * Creates a temporary fs stream 
 *
 * Returns: a #TnyFsStream, or %NULL if operation failed.
 */
TnyFsStream *modest_maemo_utils_create_temp_stream (gchar **path);

#endif /*__MODEST_MAEMO_UTILS_H__*/
