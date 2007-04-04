/* Copyright (c) 2007, Nokia Corporation
 * All rights reserved.
 *
 */

#define _GNU_SOURCE /* So we can use the getline() function, which is a convenient GNU extension. */
#include <stdio.h>

#include "modest-easysetup-country-combo-box.h"
#include <gtk/gtkliststore.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>

#include <stdlib.h>
#include <string.h> /* For memcpy() */

#include <libintl.h> /* For dgettext(). */

/* Include config.h so that _() works: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

G_DEFINE_TYPE (EasysetupCountryComboBox, easysetup_country_combo_box, GTK_TYPE_COMBO_BOX);

#define COUNTRY_COMBO_BOX_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), EASYSETUP_TYPE_COUNTRY_COMBO_BOX, EasysetupCountryComboBoxPrivate))

typedef struct _EasysetupCountryComboBoxPrivate EasysetupCountryComboBoxPrivate;

struct _EasysetupCountryComboBoxPrivate
{
	GtkTreeModel *model;
};

static void
easysetup_country_combo_box_get_property (GObject *object, guint property_id,
															GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
easysetup_country_combo_box_set_property (GObject *object, guint property_id,
															const GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
easysetup_country_combo_box_dispose (GObject *object)
{
	if (G_OBJECT_CLASS (easysetup_country_combo_box_parent_class)->dispose)
		G_OBJECT_CLASS (easysetup_country_combo_box_parent_class)->dispose (object);
}

static void
easysetup_country_combo_box_finalize (GObject *object)
{
	EasysetupCountryComboBoxPrivate *priv = COUNTRY_COMBO_BOX_GET_PRIVATE (object);

	g_object_unref (G_OBJECT (priv->model));

	G_OBJECT_CLASS (easysetup_country_combo_box_parent_class)->finalize (object);
}

static void
easysetup_country_combo_box_class_init (EasysetupCountryComboBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EasysetupCountryComboBoxPrivate));

	object_class->get_property = easysetup_country_combo_box_get_property;
	object_class->set_property = easysetup_country_combo_box_set_property;
	object_class->dispose = easysetup_country_combo_box_dispose;
	object_class->finalize = easysetup_country_combo_box_finalize;
}

enum MODEL_COLS {
	MODEL_COL_NAME = 0,
	MODEL_COL_ID = 1
};

/** id and country must be freed.
 */
static void parse_mcc_mapping_line (const char* line, char** id, char** country)
{
	/* Initialize output parameters: */
	*id = NULL;
	*country = NULL;
	
	g_assert(line);
	
	const gboolean is_valid_utf8 = g_utf8_validate (line, -1, NULL);
	if(!is_valid_utf8) {
		g_warning("UTF8 validation failed.");
		return;
	}
	
	/* Look at each character, to find the whitespace between the ID and name: */
	char* result_id = NULL;
	char* result_country = NULL;
	
	const char* p = line;
	const char* p_start_of_country = NULL;
	while (p && *p)
	{
		p = g_utf8_next_char(p);
		gunichar ch = g_utf8_get_char(p);
		if (g_unichar_isspace(ch)) { /* Note: This checks for any whitespace, not just space. */
			if(!result_id) {
				/* The text before this must be the ID: */
				const int length = p - line;
				result_id = g_malloc (length + 1); /* 1 for null-termination. */
				memcpy(result_id, line, length);
				result_id[length] = 0; /* Null-termination. */
			}
			else if(p_start_of_country)
			{
				/* This whitespace is probably the newline after the country. */
				
				/* The text after the whitespace, after the ID, must be the country: */
				int length = p - p_start_of_country;
				result_country = g_malloc(length + 1);
				memcpy(result_country, p_start_of_country, length);
				result_country[length] = 0; /* Null-termination. */
				break;
			}
		}
		else if(result_id && !p_start_of_country) {
			p_start_of_country = p;
		}
	}
	
	*id = result_id;
	*country = result_country;
}

/** Note that the mcc_mapping file is installed 
 * by the operator-wizard-settings package.
 */
static void load_from_file (EasysetupCountryComboBox *self)
{
	EasysetupCountryComboBoxPrivate *priv = COUNTRY_COMBO_BOX_GET_PRIVATE (self);
	
	/* Load the file one line at a time: */
	const gchar* filepath = "/usr/share/operator-wizard/mcc_mapping";
	FILE *file = fopen(filepath, "r");
	if (!file)
	{
		const gchar* filepath_hack = "./src/maemo/easysetup/mcc_mapping";
		g_warning ("Could not locate the official mcc_mapping countries list file from %s, "
			"so attempting to load it instead from %s", filepath, filepath_hack);
		file = fopen(filepath_hack, "r");
	}
	
	if (!file) {
		g_warning("Could not open mcc_mapping file");
		return;
	}

	GtkListStore *liststore = GTK_LIST_STORE (priv->model);
			
	/* We use the getline() GNU extension,
	 * because it reads per line, which simplifies our code,
	 * and it doesn't require us to hard-code a buffer length.
	 */
	int len = 0;
	char *line = NULL;
	while (getline (&line, &len, file) > 0) /* getline will realloc line if necessary. */
	{
		/* printf ("DBEUG: len=%d, line: %s\n", len, line); */
		
		char *id_str = NULL;
		char *country = NULL;
		parse_mcc_mapping_line (line, &id_str, &country);
		/* printf("DEBUG: parsed: id=%s, country=%s\n", id_str, country); */
		
		if(id_str && country) {
			guint id = (guint)g_ascii_strtod(id_str, NULL); /* Note that this parses locale-independent text. */
			
			/* Get the translation for the country name:
			 * Note that the osso_countries_1.0 translation domain files are installed 
			 * by the operator-wizard-settings package. */
			const gchar *name_translated = dgettext ("osso_countries_1.0", country);
			if(!name_translated)
			  name_translated = country;
			
			/* Add the row to the model: */
			GtkTreeIter iter;
			gtk_list_store_append (liststore, &iter);
			gtk_list_store_set(liststore, &iter, MODEL_COL_ID, id, MODEL_COL_NAME, name_translated, -1);
		}
		
		g_free (id_str);
		g_free (country);
	}

	if (line)
		free (line);
		
	fclose (file);
}

static void
easysetup_country_combo_box_init (EasysetupCountryComboBox *self)
{
	EasysetupCountryComboBoxPrivate *priv = COUNTRY_COMBO_BOX_GET_PRIVATE (self);

	/* Create a tree model for the combo box,
	 * with a string for the name, and an int for the MCC ID.
	 * This must match our MODEL_COLS enum constants.
	 */
	priv->model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_UINT));

	/* Setup the combo box: */
	GtkComboBox *combobox = GTK_COMBO_BOX (self);
	gtk_combo_box_set_model (combobox, priv->model);

	/* Country column:
	 * The ID model column in not shown in the view. */
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer, 
	"text", MODEL_COL_NAME, NULL);
	
	/* Fill the model with rows: */
	load_from_file (self);
}

EasysetupCountryComboBox*
easysetup_country_combo_box_new (void)
{
	return g_object_new (EASYSETUP_TYPE_COUNTRY_COMBO_BOX, NULL);
}

/**
 * Returns the MCC number of the selected country, or 0 if no country was selected. 
 */
guint
easysetup_country_combo_box_get_active_country_id (EasysetupCountryComboBox *self)
{
	GtkTreeIter active;
	const gboolean found = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &active);
	if (found) {
		EasysetupCountryComboBoxPrivate *priv = COUNTRY_COMBO_BOX_GET_PRIVATE (self);

		guint id = 0;
		gtk_tree_model_get (priv->model, &active, MODEL_COL_ID, &id, -1); 
		return id;	
	}

	return 0; /* Failed. */
}


/* This allows us to pass more than one piece of data to the signal handler,
 * and get a result: */
typedef struct 
{
		EasysetupCountryComboBox* self;
		guint mcc_id;
		gboolean found;
} ForEachData;

static gboolean
on_model_foreach_select_id(GtkTreeModel *model, 
	GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	ForEachData *state = (ForEachData*)(user_data);
	
	/* Select the item if it has the matching ID: */
	guint id = 0;
	gtk_tree_model_get (model, iter, MODEL_COL_ID, &id, -1); 
	if(id == state->mcc_id) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (state->self), iter);
		
		state->found = TRUE;
		return TRUE; /* Stop walking the tree. */
	}
	
	return FALSE; /* Keep walking the tree. */
}

/**
 * Selects the MCC number of the selected country.
 * Specify 0 to select no country. 
 */
gboolean
easysetup_country_combo_box_set_active_country_id (EasysetupCountryComboBox *self, guint mcc_id)
{
	EasysetupCountryComboBoxPrivate *priv = COUNTRY_COMBO_BOX_GET_PRIVATE (self);
	
	/* Create a state instance so we can send two items of data to the signal handler: */
	ForEachData *state = g_new0 (ForEachData, 1);
	state->self = self;
	state->mcc_id = mcc_id;
	state->found = FALSE;
	
	/* Look at each item, and select the one with the correct ID: */
	gtk_tree_model_foreach (priv->model, &on_model_foreach_select_id, state);

	const gboolean result = state->found;
	
	/* Free the state instance: */
	g_free(state);
	
	return result;
}

