/*

  Copyright (c) 2005 uim Project http://uim.freedesktop.org/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#include <pref-gtk-custom-widgets.h>
#include <gdk/gdkkeysyms.h>

#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include "uim/config.h"
#include "uim/uim.h"
#include "uim/uim-custom.h"
#include "uim/gettext.h"

#define USE_SUB_GROUP 1

static GtkWidget *pref_window = NULL;
static GtkWidget *pref_tree_view = NULL;
static GtkWidget *pref_hbox = NULL;
static GtkWidget *current_group_widget = NULL;

gboolean uim_pref_gtk_value_changed = FALSE;

enum
{
  GROUP_COLUMN=0,
  GROUP_WIDGET=1,
  NUM_COLUMNS
};

static gboolean	pref_tree_selection_changed(GtkTreeSelection *selection,
					     gpointer data);
static GtkWidget *create_pref_treeview(void);
static GtkWidget *create_group_widget(const char *group_name);
#if USE_SUB_GROUP
static void create_sub_group_widgets(GtkWidget *parent_widget,
				     const char *parent_group);
#endif

static void
save_confirm_dialog_response_cb(GtkDialog *dialog, gint arg, gpointer user_data)
{
  switch (arg)
  {
  case GTK_RESPONSE_YES:
    uim_custom_save();
    uim_custom_broadcast();
    uim_pref_gtk_value_changed = FALSE;
    break;
  case GTK_RESPONSE_NO:
    uim_pref_gtk_value_changed = FALSE;
    break;
  default:
    break;
  }
}

static gboolean
pref_tree_selection_changed(GtkTreeSelection *selection,
			     gpointer data)
{
  GtkTreeStore *store;
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *group_name;
  GtkWidget *group_widget;

  /* Preference save check should be here. */
  if (uim_pref_gtk_value_changed) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL,
				    GTK_DIALOG_MODAL,
				    GTK_MESSAGE_QUESTION,
				    GTK_BUTTONS_YES_NO,
				    _("Some value(s) have been changed.\n"
				      "Save?"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(pref_window));
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    g_signal_connect(G_OBJECT(dialog), "response",
		     G_CALLBACK(save_confirm_dialog_response_cb), NULL);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }

  if(gtk_tree_selection_get_selected(selection, &model, &iter) == FALSE)
    return TRUE;

  store = GTK_TREE_STORE(model);
  gtk_tree_model_get(model, &iter,
		     GROUP_COLUMN, &group_name,
		     GROUP_WIDGET, &group_widget,
		     -1);

  if(group_name == NULL)
    return TRUE;

  /* hide current selected group's widget */
  if(current_group_widget)
    gtk_widget_hide(current_group_widget);

  /* whether group_widget is already packed or not */
  if(!gtk_widget_get_parent(group_widget))
    gtk_box_pack_start (GTK_BOX (pref_hbox), group_widget, TRUE, TRUE, 0);

  /* show selected group's widget */
  gtk_widget_show_all(group_widget);

  current_group_widget = group_widget;
  
  free(group_name);
  return TRUE;
}


static void
quit_confirm_dialog_response_cb(GtkDialog *dialog, gint arg, gpointer user_data)
{
  gboolean *quit = user_data;

  switch (arg)
  {
  case GTK_RESPONSE_OK:
    *quit = TRUE;
    break;
  case GTK_RESPONSE_CANCEL:
    *quit = FALSE;
    break;
  default:
    break;
  }
}

static void
quit_confirm(void)
{
  if (uim_pref_gtk_value_changed) {
    GtkWidget *dialog;
    gboolean quit = FALSE;

    dialog = gtk_message_dialog_new(NULL,
				    GTK_DIALOG_MODAL,
				    GTK_MESSAGE_QUESTION,
				    GTK_BUTTONS_OK_CANCEL,
				    _("Some value(s) have been changed.\n"
				      "Do you realy quit this program?"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(pref_window));
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    g_signal_connect(G_OBJECT(dialog), "response",
		     G_CALLBACK(quit_confirm_dialog_response_cb), &quit);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (quit)
      gtk_main_quit();
  } else {
    gtk_main_quit();
  }
}

static void
delete_event_cb(GtkWidget *widget, gpointer data)
{
  quit_confirm();
}

static GtkWidget *
create_pref_treeview(void)
{
  GtkTreeStore *tree_store;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  char **primary_groups, **grp;
  GtkTreeSelection *selection;
  GtkTreePath *first_path;
  tree_store = gtk_tree_store_new (NUM_COLUMNS,
				   G_TYPE_STRING,
				   GTK_TYPE_WIDGET);
  
  pref_tree_view = gtk_tree_view_new();

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("Group"),
						    renderer,
						    "text", GROUP_COLUMN,
						    NULL);
  gtk_tree_view_column_set_sort_column_id(column, 0);
  gtk_tree_view_append_column(GTK_TREE_VIEW(pref_tree_view), column);
  
  primary_groups = uim_custom_primary_groups();
  for (grp = primary_groups; *grp; grp++) {
    struct uim_custom_group *group = uim_custom_group_get(*grp);
    gtk_tree_store_append (tree_store, &iter, NULL/* parent iter */);
    gtk_tree_store_set (tree_store, &iter,
			GROUP_COLUMN, group->label,
			GROUP_WIDGET, create_group_widget(*grp),
			-1);
    uim_custom_group_free(group);
  }
  uim_custom_symbol_list_free( primary_groups );

  gtk_tree_view_set_model (GTK_TREE_VIEW(pref_tree_view),
			   GTK_TREE_MODEL(tree_store));
  g_object_unref (tree_store);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW(pref_tree_view), TRUE);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pref_tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT(selection), "changed",
		    G_CALLBACK(pref_tree_selection_changed), NULL);

  first_path = gtk_tree_path_new_from_indices (0, -1);

  gtk_tree_view_set_cursor(GTK_TREE_VIEW(pref_tree_view),
			   first_path, NULL, FALSE);  

  return pref_tree_view;
}

static void
ok_button_clicked(GtkButton *button, gpointer user_data)
{
  /*const char *group_name = user_data;*/

  if (uim_pref_gtk_value_changed) {
    uim_custom_save();
    uim_custom_broadcast();
    uim_pref_gtk_value_changed = FALSE;
  }

  gtk_main_quit();
}

static void
apply_button_clicked(GtkButton *button, gpointer user_data)
{
  /*const char *group_name = user_data;*/

  if (uim_pref_gtk_value_changed) {
    uim_custom_save();
    uim_custom_broadcast();
    uim_pref_gtk_value_changed = FALSE;
  }
}

static void
set_to_default_cb(GtkWidget *widget, gpointer data)
{
  uim_pref_gtk_set_default_value(widget);

  if (GTK_IS_CONTAINER(widget))
    gtk_container_foreach(GTK_CONTAINER(widget),
			  (GtkCallback) (set_to_default_cb), NULL);
}

static void
defaults_button_clicked(GtkButton *button, gpointer user_data)
{
  gtk_container_foreach(GTK_CONTAINER(current_group_widget),
			(GtkCallback) (set_to_default_cb), NULL);
}

static GtkWidget *
create_setting_button_box(const char *group_name)
{
  GtkWidget *setting_button_box;
  GtkWidget *button;

  setting_button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(setting_button_box), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(setting_button_box), 8);

  /* Defaults button */
  button = gtk_button_new_with_mnemonic(_("_Defaults"));
  g_signal_connect(G_OBJECT(button), "clicked",
		   G_CALLBACK(defaults_button_clicked), (gpointer) group_name);
  gtk_box_pack_start(GTK_BOX(setting_button_box), button, TRUE, TRUE, 8);

  /* Apply button */
  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
  g_signal_connect(G_OBJECT(button), "clicked",
		   G_CALLBACK(apply_button_clicked), (gpointer) group_name);
  gtk_box_pack_start(GTK_BOX(setting_button_box), button, TRUE, TRUE, 8);

  /* Cancel button */
  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  g_signal_connect(G_OBJECT(button), "clicked",
		   G_CALLBACK(quit_confirm), NULL);
  gtk_box_pack_start(GTK_BOX(setting_button_box), button, TRUE, TRUE, 8);

  /* OK button */
  button = gtk_button_new_from_stock(GTK_STOCK_OK);
  g_signal_connect(G_OBJECT(button), "clicked",
		   G_CALLBACK(ok_button_clicked), (gpointer) group_name);
  gtk_box_pack_start(GTK_BOX(setting_button_box), button, TRUE, TRUE, 8);
  return setting_button_box;
}

static GtkWidget *
create_group_widget(const char *group_name)
{
  GtkWidget *vbox;
  GtkWidget *group_label;
  GtkWidget *setting_button_box;
  struct uim_custom_group *group;
  char *label_text;
  vbox = gtk_vbox_new(FALSE, 8);

  gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

  group = uim_custom_group_get(group_name);

  if(group == NULL)
    return NULL;

  group_label = gtk_label_new("");
  label_text  = g_markup_printf_escaped("<span size=\"xx-large\">%s</span>",
					group->label);
  gtk_label_set_markup(GTK_LABEL(group_label), label_text);
  g_free(label_text);

  gtk_box_pack_start (GTK_BOX(vbox), group_label, FALSE, TRUE, 8);

#if USE_SUB_GROUP
  create_sub_group_widgets(vbox, group_name);
#else
  {
    char **custom_syms, **custom_sym;
    custom_syms = uim_custom_collect_by_group(group_name);
    if (custom_syms) {
      for (custom_sym = custom_syms; *custom_sym; custom_sym++) {
	uim_pref_gtk_add_custom(vbox, *custom_sym);
      }
      uim_custom_symbol_list_free(custom_syms);
    }
  }
#endif

  uim_custom_group_free(group);

  setting_button_box = create_setting_button_box(group_name);
  gtk_box_pack_end(GTK_BOX(vbox), setting_button_box, FALSE, FALSE, 8);

  return vbox;
}

#if USE_SUB_GROUP
static void create_sub_group_widgets(GtkWidget *parent_widget, const char *parent_group)
{
    char **sgrp_syms = uim_custom_group_subgroups(parent_group);
    char **sgrp_sym;

    for(sgrp_sym = sgrp_syms; *sgrp_sym; sgrp_sym++)
    {
        struct uim_custom_group *sgrp =  uim_custom_group_get(*sgrp_sym);
	char **custom_syms, **custom_sym;
	GString *sgrp_str;
	GtkWidget *frame;
	GtkWidget *vbox;

	if(!sgrp)
	  continue;

	/* XXX quick hack to use AND expression of groups */
	sgrp_str = g_string_new("");
	g_string_printf(sgrp_str, "%s '%s", parent_group, *sgrp_sym);
	custom_syms = uim_custom_collect_by_group(sgrp_str->str);
	g_string_free(sgrp_str, TRUE);

	if (!custom_syms)
	  continue;
	if (!*custom_syms) {
	  uim_custom_symbol_list_free(custom_syms);
	  continue;
	}

	vbox = gtk_vbox_new(FALSE, 8);
	if (strcmp(*sgrp_sym, "main")) {
	  frame = gtk_frame_new(sgrp->label);
	  gtk_frame_set_label_align(GTK_FRAME(frame), 0.02, 0.5);
	  gtk_box_pack_start(GTK_BOX(parent_widget), frame, FALSE, FALSE, 0);

	  gtk_container_add(GTK_CONTAINER(frame), vbox);
	} else {

	  /*
	   * Removing frame for 'main' subgroup. If you feel it
	   * strange, Replace it as you favor.  -- YamaKen 2005-02-06
	   */
	  gtk_box_pack_start(GTK_BOX(parent_widget), vbox, FALSE, FALSE, 0);
	}

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	for (custom_sym = custom_syms; *custom_sym; custom_sym++) {
	  uim_pref_gtk_add_custom(vbox, *custom_sym);
	}
	uim_custom_symbol_list_free(custom_syms);

	uim_custom_group_free(sgrp);
    }

    uim_custom_symbol_list_free(sgrp_syms);
}
#endif

static GtkWidget *
create_pref_window(void)
{
  GtkWidget *window;
  GtkWidget *scrolled_win; /* treeview container */

  pref_window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  
  g_signal_connect(G_OBJECT (window), "delete_event",
		   G_CALLBACK (delete_event_cb), NULL);

  pref_hbox = gtk_hbox_new(FALSE, 8);

  scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(pref_hbox), scrolled_win, FALSE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(scrolled_win), create_pref_treeview());
  gtk_container_add(GTK_CONTAINER(window), pref_hbox);

  {
  GdkScreen *scr = gtk_window_get_screen(GTK_WINDOW(window));
  gtk_window_set_default_size(GTK_WINDOW(window),
			      gdk_screen_get_width(scr)  / 2,
			      gdk_screen_get_height(scr) / 2);
  gtk_window_set_position(GTK_WINDOW(window),
			  GTK_WIN_POS_CENTER_ALWAYS);
  }
  
  return window;
}

static gboolean
check_dot_uim_file(void)
{
  GString *dot_uim = g_string_new(g_get_home_dir());
  GtkWidget *dialog;
  const gchar *message =
    N_("The user customize file \"~/.uim\" is found.\n"
       "This file will override all conflicted settings set by\n"
       "this tool (stored in ~/.uim.d/customs/*.scm).\n"
       "Please check the file if you find your settings aren't applied.");

  g_string_append(dot_uim, "/.uim");

  if (!g_file_test(dot_uim->str, G_FILE_TEST_EXISTS)) {
    g_string_free(dot_uim, TRUE);
    return FALSE;
  }
  g_string_free(dot_uim, TRUE);

  dialog = gtk_message_dialog_new(NULL,
				  GTK_DIALOG_MODAL,
				  GTK_MESSAGE_WARNING,
				  GTK_BUTTONS_OK,
				  _(message));

  /*
   *  2005-02-07 Takuro Ashie <ashie@homa.ne.jp>
   *    FIXME! We shoud add a check box like
   *    "Show this message every time on start up.".
   */

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(GTK_WIDGET(dialog));

  return FALSE;
}

int 
main (int argc, char *argv[])
{
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
  bind_textdomain_codeset(PACKAGE, "UTF-8");

  gtk_set_locale();
  
  gtk_init(&argc, &argv);

  if (uim_init() < 0) {
    fprintf(stderr, "uim_init() failed.\n");
    return -1;
  }

  if (uim_custom_enable()) {
    GtkWidget *pref;

    gtk_idle_add((GtkFunction) check_dot_uim_file, NULL);
  
    pref = create_pref_window();

    gtk_widget_show_all(pref);

    gtk_main();
  } else {
    fprintf(stderr, "uim_custom_enable() failed.\n");
    uim_quit();
    return -1;
  }

  uim_quit();
  return 0;
}
