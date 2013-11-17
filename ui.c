/**
 * Gameshark / Action Replay Plugin for PSEmu compatible emulators
 * Copyright (C) 2008-2013 Tim Hentenaar <tim@hentenaar.com>
 * Released under the GNU General Public License (v2)
 */

#include "ui.h"
#include "gs.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <gtk/gtk.h>

static GtkWidget    *dlg       = NULL; /**< The config dialog */
static GtkWidget    *combo;            /**< The combo box */
static GtkWidget    *sk_1;             /**< Shortcut key 1 */
static GtkWidget    *sk_2;             /**< Shortcut key 2 */
static GtkWidget    *cheat_dlg = NULL; /**< The cheat dialog */
static GtkWidget    *code_tree;        /**< Treeview */
static GtkWidget    *code_text;        /**< Code text entry */
static GtkWidget    *desc_text;        /**< Description text entry */
static GtkListStore *list_store;       /**< Codes list store */
static GtkWidget    *about;            /**< About dialog */
static uint16_t      keycode[2];       /**< Keycodes */

enum { /* List columns */
	COL_STATUS = 0,
	COL_CODE,
	COL_DESC,
	N_COLUMNS
};

static void gpu_do_config(GtkButton *button, gs_state_t *state) {
	uint32_t ret; void *gpu;

	if ((gpu = dlopen(state->config.gpu_paths[state->config.real_gpu],RTLD_LAZY))) {
		state->gpu.configure = (int32_t (*)())dlsym(gpu,"GPUconfigure");
		if (state->gpu.configure) ret = state->gpu.configure();
		dlclose(gpu);
	}
}

static void gpu_do_about(GtkButton *button, gs_state_t *state) {
	void *gpu;

	if ((gpu = dlopen(state->config.gpu_paths[state->config.real_gpu],RTLD_LAZY))) {
		state->gpu.about = (void (*)())dlsym(gpu,"GPUabout");
		if (state->gpu.about) state->gpu.about();
		dlclose(gpu);
	}
}

static void ui_do_about(gs_state_t *state) {
	char *version; int result;
	
	if (!about) {
		version = g_strdup_printf("%d.%d",VERSION_MAJOR,VERSION_MINOR);
		about = gtk_about_dialog_new();
		gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about),"PSX-GS");
		gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about),(const char *)version);
		gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about),
			"Copyright (C) 2008-2010 Tim Hentenaar <tim@hentenaar.com>.\nLicensed under the GNU General Public License (v2).\n\n"
			"Built on: " __DATE__ " at " __TIME__);
		gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about),"http://www.hentenaar.com");
		g_free(version);
	}
	
	gtk_widget_show_all(about);
	result = gtk_dialog_run(GTK_DIALOG(about));
	gtk_widget_hide(about);
}

static gboolean set_key(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	keycode[(widget == sk_1) ? 0 : 1]  = event->keyval;
	gtk_entry_set_text(GTK_ENTRY(widget),gdk_keyval_name(event->keyval));
	return TRUE;
}

static void ui_do_config(gs_state_t *state) {
	GtkWidget *fixed,*notebook,*page,*label,*label2,*button;
	char *ctmp; int dlg_result = 0;

	if (!dlg) {
		/* Create the dialog */
		dlg = gtk_dialog_new_with_buttons("Configuration",NULL,0,
						  GTK_STOCK_OK,GTK_RESPONSE_OK,
						  GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
						  NULL);
		fixed    = gtk_fixed_new();
		notebook = gtk_notebook_new();

		/* Create notebook page 1 (GPU Selection) */
		page     = gtk_fixed_new();
		label    = gtk_label_new("GPU Selection");

		/* Label */
		label2   = gtk_label_new("GPU:");
		gtk_widget_set_size_request(label2,246,63);
		gtk_fixed_put(GTK_FIXED(page),label2,-99,19);

		/* Combo Box (GPU list) */
		combo    = gtk_combo_box_new_text();

		for (dlg_result=0;state->config.gpu_list && state->config.gpu_list[dlg_result];dlg_result++) 
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo),state->config.gpu_list[dlg_result]);
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo),state->config.real_gpu); 

		gtk_widget_set_size_request(combo,221,26);
		gtk_fixed_put(GTK_FIXED(page),combo,89,36);

		/* Configure button */
		button = gtk_button_new_with_label("Configure");
		gtk_widget_set_size_request(button,81,28);
		gtk_fixed_put(GTK_FIXED(page),button,92,83);
		g_signal_connect(G_OBJECT(button),"clicked",G_CALLBACK(gpu_do_config),(gpointer)state);

		/* About button */
		button = gtk_button_new_with_label("About");
		gtk_widget_set_size_request(button,81,28);
		gtk_fixed_put(GTK_FIXED(page),button,287,83);
		g_signal_connect(G_OBJECT(button),"clicked",G_CALLBACK(gpu_do_about),(gpointer)state);

		gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page,label);

		/* Create notebook page 2 (Shortcut Keys) */
		page   = gtk_fixed_new();
		label  = gtk_label_new("Shortcut Keys");

		/* Display Cheat Dialog */
		label2 = gtk_label_new("Display Cheat Dialog");
		sk_1   = gtk_entry_new(); keycode[0] = state->config.dialog_key;

		if (state->config.dialog_key) 
			gtk_entry_set_text(GTK_ENTRY(sk_1),gdk_keyval_name(state->config.dialog_key));

		g_signal_connect(G_OBJECT(sk_1),"key-press-event",G_CALLBACK(set_key),NULL);

		gtk_widget_set_size_request(label2,138,20);
		gtk_widget_set_size_request(sk_1,97,22);

		gtk_fixed_put(GTK_FIXED(page),sk_1,6,18);
		gtk_fixed_put(GTK_FIXED(page),label2,114,18);

		/* Toggle Cheats On/Off */
		label2 = gtk_label_new("Toggle Cheats On/Off");
		sk_2   = gtk_entry_new(); keycode[1] = state->config.cheat_toggle_key;
		
		if (state->config.cheat_toggle_key) 
			gtk_entry_set_text(GTK_ENTRY(sk_2),gdk_keyval_name(state->config.cheat_toggle_key));

		g_signal_connect(G_OBJECT(sk_2),"key-press-event",G_CALLBACK(set_key),NULL);

		gtk_widget_set_size_request(label2,150,20);
		gtk_widget_set_size_request(sk_2,97,22);

		gtk_fixed_put(GTK_FIXED(page),sk_2,6,45);
		gtk_fixed_put(GTK_FIXED(page),label2,114,45);

		gtk_notebook_append_page(GTK_NOTEBOOK(notebook),page,label);

		/* Size up the notebook and add it to the layout */
		gtk_widget_set_size_request(notebook,417,162);
		gtk_fixed_put(GTK_FIXED(fixed),notebook,0,4);

		/* Add the layout to the dialog's vbox */
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),fixed,TRUE,TRUE,0);
	} 

	/* Display the dialog */
	gtk_widget_show_all(dlg);
	dlg_result = gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_hide(dlg);

	if (dlg_result == GTK_RESPONSE_OK) {
		/* Save the config */
		state->config.dialog_key       = keycode[0];
		state->config.cheat_toggle_key = keycode[1];
		state->config.real_gpu         = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	}
}


/* Checks if a code is valid */
static int is_valid_code(char *code) {
	uint8_t type; uint64_t cd;

	for (type=0;type<strlen(code);type++) if (isspace(*(code+type))) {
		memmove(code+type,code+type+1,strlen(code+type+1));
		*(code+type+(strlen(code+type)-1)) = 0;
	}

	if (strlen(code) != 12) return 0; 
	cd = strtoull(code,NULL,16); type = (cd >> 40) & 0xff;

	if (type == 0) return 1;
	if (type < GS_WORD_INC_ONCE) return 0;
	if (type > GS_BYTE_GT) return 0;

	if ((type & 0xf0) == 0x10 && (type & 0x0f) > 1)   return 0;
	if ((type & 0xf0) == 0x20 && (type & 0x0f) > 1)   return 0;
	if ((type & 0xf0) == 0x30 && (type & 0x0f) > 0)   return 0;
	if ((type & 0xf0) > 0x30 && (type & 0xf0) < 0x50) return 0;
	if ((type & 0xf0) == 0x50 && (type & 0x0f) > 0)   return 0;
	if ((type & 0xf0) > 0x50 && (type & 0xf0) < 0x80) return 0;
	if ((type & 0xf0) > 0x80 && (type & 0xff) < 0xC2) return 0;
	if ((type & 0xff) > 0xC2 && (type & 0xf0) < 0xD0) return 0;
	if ((type & 0xf0) == 0xD0 && (type & 0x0f) > 6)   return 0;
	if ((type & 0xff) > 0xD6 && (type & 0xff) < 0xE0) return 0;
	if ((type & 0xf0) == 0xE0 && (type & 0xf0) > 3)   return 0;

	return 1;
}

/**
 * This is called when the toggle renderer in the list is clicked 
 */
static void tree_enable_callback(GtkCellRendererToggle *cell_r, gchar *path, gpointer data) {
	GtkTreeIter iter;

	/* Get an iterator for the path */
	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list_store),&iter,path);

	/* Update the list store column */
	gtk_list_store_set(list_store,&iter,COL_STATUS,gtk_cell_renderer_toggle_get_active(cell_r) ? 0 : 1,-1);
}

/**
 * This is called when the code cell is edited 
 */
static void tree_code_edited(GtkCellRendererText *cell_r, gchar *path, gchar *new_text, gpointer data) {
	GtkTreeIter iter; uint64_t code; uint8_t type; 

	/* Verify the code format */
	new_text = g_strstrip(new_text);
	if (strlen(new_text) != 12) return;

	if (is_valid_code(new_text)) {
		/* Get an iterator for the path */
		gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list_store),&iter,path);

		/* Update the list store column */
		gtk_list_store_set(list_store,&iter,COL_CODE,new_text,-1);
	}
}

/**
 * Add a new code 
 */
static void tree_code_add(GtkButton *button, gpointer data) {
	GtkTreeIter iter; const char *code,*desc; 
	code = g_strstrip((char *)gtk_entry_get_text(GTK_ENTRY(code_text)));
	desc = gtk_entry_get_text(GTK_ENTRY(desc_text));

	if (!code || !desc) return;

	/* Verify the code type, etc. */
	if (is_valid_code((char *)code)) {
		/* Update the list store */
		gtk_list_store_append(list_store,&iter);
		gtk_list_store_set(list_store,&iter,
				   COL_STATUS,TRUE,
				   COL_CODE,code,
				   COL_DESC,desc,
				   -1);
	}
}

static void ui_do_cheat_dialog(gs_state_t *state) {
	GtkTreeIter iter; GtkTreeViewColumn *col; GtkCellRenderer *cell_r; GtkWidget *fixed,*button,*label,*label2;
	GValue g_status = {0,}, g_code = {0,}, g_desc = {0,}; gs_code_t *tmp,*tmp2; int dlg_result = 0,i=-1; uint64_t code = 0;

	if (!cheat_dlg) {
		/* Create the dialog */
		cheat_dlg = gtk_dialog_new_with_buttons("Game Shark / Action Replay Codes",NULL,0,
						  	GTK_STOCK_APPLY,2,
						  	GTK_STOCK_SAVE,1,
						  	NULL);
		fixed = gtk_fixed_new();

		/* Create the list store */
		list_store = gtk_list_store_new(N_COLUMNS,
						G_TYPE_BOOLEAN,	/* Activated ? */
						G_TYPE_STRING,	/* Code        */
						G_TYPE_STRING   /* Description */);

		/* Now, add in the current codes (if any) */
		for (tmp=state->codes;tmp;tmp=tmp->next) {
			gtk_list_store_append(list_store,&iter);
			gtk_list_store_set(list_store,&iter,
					   COL_STATUS,tmp->active ? TRUE : FALSE,
					   COL_CODE,tmp->code,
					   COL_DESC,tmp->desc,
					   -1);
		}

		/* Create the TreeView */
		code_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(code_tree),TRUE);
				
		/* Now add the columns */
		cell_r = gtk_cell_renderer_toggle_new();
		col    = gtk_tree_view_column_new_with_attributes("Enabled",cell_r,"active",COL_STATUS,NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(code_tree),col);
		g_signal_connect(G_OBJECT(cell_r),"toggled",G_CALLBACK(tree_enable_callback),state);
		
		cell_r = gtk_cell_renderer_text_new();
		col    = gtk_tree_view_column_new_with_attributes("Code",cell_r,"text",COL_CODE,NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(code_tree),col);
		g_object_set(G_OBJECT(cell_r),"editable",TRUE,NULL);
		g_signal_connect(G_OBJECT(cell_r),"edited",G_CALLBACK(tree_code_edited),state);

		cell_r = gtk_cell_renderer_text_new();
		col    = gtk_tree_view_column_new_with_attributes("Description",cell_r,"text",COL_DESC,NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(code_tree),col);

		gtk_widget_set_size_request(code_tree,609,210);
		gtk_fixed_put(GTK_FIXED(fixed),code_tree,2,2);

		/* "Instructions" label */
		label = gtk_label_new("Click on a code to modify it, and toggle the checkbox\nto activate/deactivate that"
				      " particular code.");
		gtk_widget_set_size_request(label,361,80);
		gtk_fixed_put(GTK_FIXED(fixed),label,146,200);

		/* Create a couple of text fields for entering new codes, and a button for adding them. */
		code_text = gtk_entry_new(); label  = gtk_label_new("Code:");
		desc_text = gtk_entry_new(); label2 = gtk_label_new("Description:");
		button    = gtk_button_new_with_label("Add");

		gtk_widget_set_size_request(code_text,222,25); gtk_widget_set_size_request(label,100,80);
		gtk_widget_set_size_request(desc_text,222,25); gtk_widget_set_size_request(label2,100,80);
		gtk_widget_set_size_request(button,70,25);

		gtk_fixed_put(GTK_FIXED(fixed),code_text,230,292); gtk_fixed_put(GTK_FIXED(fixed),label,139,260);
		gtk_fixed_put(GTK_FIXED(fixed),desc_text,230,316); gtk_fixed_put(GTK_FIXED(fixed),label2,139,285);
		gtk_fixed_put(GTK_FIXED(fixed),button,230,341);

		g_signal_connect(G_OBJECT(button),"clicked",G_CALLBACK(tree_code_add),state);

		/* Add the layout to the dialog's vbox */
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cheat_dlg)->vbox),fixed,TRUE,TRUE,0);
	} else { 
		/* Update the list store based on state */
		for (tmp=state->codes;tmp;tmp=tmp->next) {
			gtk_list_store_append(list_store,&iter);
			gtk_list_store_set(list_store,&iter,
					   COL_STATUS,tmp->active ? TRUE : FALSE,
					   COL_CODE,tmp->code,
					   COL_DESC,tmp->desc,
					   -1);
		}
	}

	/* Display the dialog */
	gtk_widget_show_all(cheat_dlg);
	dlg_result = gtk_dialog_run(GTK_DIALOG(cheat_dlg));
	gtk_widget_hide(cheat_dlg);

	if (dlg_result >= 1) {
		/* Update state based on the list store */
		for (tmp=state->codes;tmp;tmp=tmp2) {
			if (tmp->desc) {
				memset(tmp->desc,0,strlen(tmp->desc));
				free(tmp->desc);
			}

			if (tmp->code) {
				memset(tmp->code,0,strlen(tmp->code));
				free(tmp->code);
			}

			tmp2 = tmp->next;
			if (tmp->prev) tmp->prev->next = tmp->next;
			if (tmp->next) tmp->next->prev = tmp->prev;
			memset(tmp,0,sizeof(gs_code_t));
			free(tmp);
		} state->codes = NULL;

		i = 0;
		while (i != -1 && (i || gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store),&iter))) {
			if ((tmp = calloc(sizeof(gs_code_t),1))) {
				gtk_tree_model_get_value(GTK_TREE_MODEL(list_store),&iter,COL_STATUS,&g_status);
				gtk_tree_model_get_value(GTK_TREE_MODEL(list_store),&iter,COL_CODE,&g_code);
				gtk_tree_model_get_value(GTK_TREE_MODEL(list_store),&iter,COL_DESC,&g_desc);
				
				code = strtoull(g_value_get_string(&g_code),NULL,16);
				tmp->value   = code & 0xffff; code >>= 16;
				tmp->address = code & 0xffffff; code >>= 24;
				tmp->type    = code & 0xff;

				tmp->code    = strdup(g_value_get_string(&g_code));
				tmp->desc    = strdup(g_value_get_string(&g_desc));
				tmp->active  = (g_value_get_boolean(&g_status) ? 1 : 0);
				g_value_unset(&g_code); g_value_unset(&g_desc); g_value_unset(&g_status);

				if (!state->codes) state->codes = tmp;
				else {
					for (tmp2=state->codes;tmp2;tmp2=tmp2) {
						if (!tmp2->next) {
							tmp2->next = tmp;
							tmp->prev = tmp2;
							tmp2 = NULL;
						} else tmp2 = tmp2->next;
					}
				}
			}

			i = (gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store),&iter) ? 1 : -1);
		}

		if (dlg_result == 1) {
			/* TODO: Save codes in a per-game file. */
		}
	}
}

int main(int argc, char **argv) {
	gs_state_t *state; gs_code_t *tmp,*tmp2; char *ctmp; int32_t ret = 0;

	gtk_init(&argc,&argv);
	if (!(state = calloc(sizeof(gs_state_t),1))) exit(EXIT_FAILURE); 
	unserialize_state(state,0);

	/* Display a dialog */
	if (getenv("DIALOG_ID")) {
		switch(atoi(getenv("DIALOG_ID"))) {
			case DIALOG_CONFIG:
				ui_do_config(state);
			break;

			case DIALOG_ABOUT:
				ui_do_about(state);
			break;

			case DIALOG_CHEAT:
				ui_do_cheat_dialog(state);
			break;
		}

		serialize_state(state);
	}
	

	/* Destroy the list of codes */
	for (tmp=state->codes;tmp;tmp=tmp2) {
		if (tmp->desc) {
			memset(tmp->desc,0,strlen(tmp->desc));
			free(tmp->desc);
		}

		if (tmp->code) {
			memset(tmp->code,0,strlen(tmp->code));
			free(tmp->code);
		}

		tmp2 = tmp->next;
		if (tmp->prev) tmp->prev->next = tmp->next;
		if (tmp->next) tmp->next->prev = tmp->prev;
		memset(tmp,0,sizeof(gs_code_t));
		free(tmp);
	}

	/* Destroy config variables */
	if (state->config.gpu_paths) {
		ret = -1;
		while (state->config.gpu_paths[++ret]) {
			memset(state->config.gpu_paths[ret],0,strlen(state->config.gpu_paths[ret]));
			free(state->config.gpu_paths[ret]);
		} 

		memset(state->config.gpu_paths,0,sizeof(char *)*(ret+1));
		free(state->config.gpu_paths);
	}

	if (state->config.gpu_list) {
		ret = -1;
		while (state->config.gpu_list[++ret]) {
			memset(state->config.gpu_list[ret],0,strlen(state->config.gpu_list[ret]));
			free(state->config.gpu_list[ret]);
		} 

		memset(state->config.gpu_list,0,sizeof(char *)*(ret+1));
		free(state->config.gpu_list);
	}

	memset(state,0,sizeof(gs_state_t));
	free(state); 
	return 0;
}

/* vi:set ts=4: */
