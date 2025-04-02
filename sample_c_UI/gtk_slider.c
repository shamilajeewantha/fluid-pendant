#include <gtk/gtk.h>

static GtkWidget *label;  // Label to display the trackbar value

// Callback function to update the label based on the slider value
static void on_slider_value_changed(GtkRange *range, gpointer user_data) {
    int value = (int)gtk_range_get_value(range);  // Get the current slider value
    char buffer[50];
    sprintf(buffer, "Trackbar Value: %d", value);  // Format the value as a string
    gtk_label_set_text(GTK_LABEL(label), buffer);  // Set the label text to display the value
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);  // Initialize GTK

    // Create a new window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Trackbar Example");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);

    // Set up the signal to close the window when the "X" button is clicked
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create a vertical box (vbox) to organize widgets in a vertical layout
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create a label above the slider
    label = gtk_label_new("Trackbar Value: 0");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);

    // Create the slider (scale) with a range of -90 to 90
    GtkWidget *slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -90, 90, 1);
    gtk_scale_set_value_pos(GTK_SCALE(slider), GTK_POS_TOP);  // Show the value on top of the slider
    gtk_box_pack_start(GTK_BOX(vbox), slider, FALSE, FALSE, 5);

    // Connect the signal to update the label whenever the slider value changes
    g_signal_connect(slider, "value-changed", G_CALLBACK(on_slider_value_changed), NULL);

    // Show all widgets in the window
    gtk_widget_show_all(window);

    // Enter the GTK main loop
    gtk_main();

    return 0;
}
