#include "FileDialog.h"

#include <dlfcn.h>

#include <gtk/gtk.h>

#include <optional>
#include <string>
#include <vector>

#include "EmbeddedIcon.h"

namespace {

struct GtkFileDialogs {
    void* library = nullptr;
    gboolean (*initCheck)(int*, char***) = nullptr;
    GtkWidget* (*newFileChooser)(const gchar*, GtkWindow*, GtkFileChooserAction, const gchar*, ...) = nullptr;
    GtkFileFilter* (*newFilter)() = nullptr;
    gboolean (*setCurrentFolder)(GtkFileChooser*, const gchar*) = nullptr;
    void (*filterSetName)(GtkFileFilter*, const gchar*) = nullptr;
    void (*filterAddPattern)(GtkFileFilter*, const gchar*) = nullptr;
    void (*chooserAddFilter)(GtkFileChooser*, GtkFileFilter*) = nullptr;
    gint (*run)(GtkDialog*) = nullptr;
    void (*destroy)(GtkWidget*) = nullptr;
    gchar* (*fileName)(GtkFileChooser*) = nullptr;
    void (*freeString)(gpointer) = nullptr;
    void (*setIcon)(GtkWindow*, GdkPixbuf*) = nullptr;
    GdkPixbufLoader* (*loaderNew)() = nullptr;
    gboolean (*loaderWrite)(GdkPixbufLoader*, const guchar*, gsize, GError**) = nullptr;
    gboolean (*loaderClose)(GdkPixbufLoader*, GError**) = nullptr;
    GdkPixbuf* (*loaderPixbuf)(GdkPixbufLoader*) = nullptr;
    void (*unref)(gpointer) = nullptr;
};

template <typename F>
F symbolOf(void* library, const char* name)
{
    return reinterpret_cast<F>(dlsym(library, name));
}

const GtkFileDialogs* gtkFileDialogs()
{
    static const GtkFileDialogs dialogs = [] {
        GtkFileDialogs loaded;
        loaded.library = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (!loaded.library) {
            return loaded;
        }
        void* library = loaded.library;
        loaded.initCheck = symbolOf<decltype(loaded.initCheck)>(library, "gtk_init_check");
        loaded.newFileChooser = symbolOf<decltype(loaded.newFileChooser)>(library, "gtk_file_chooser_dialog_new");
        loaded.newFilter = symbolOf<decltype(loaded.newFilter)>(library, "gtk_file_filter_new");
        loaded.setCurrentFolder = symbolOf<decltype(loaded.setCurrentFolder)>(library, "gtk_file_chooser_set_current_folder");
        loaded.filterSetName = symbolOf<decltype(loaded.filterSetName)>(library, "gtk_file_filter_set_name");
        loaded.filterAddPattern = symbolOf<decltype(loaded.filterAddPattern)>(library, "gtk_file_filter_add_pattern");
        loaded.chooserAddFilter = symbolOf<decltype(loaded.chooserAddFilter)>(library, "gtk_file_chooser_add_filter");
        loaded.run = symbolOf<decltype(loaded.run)>(library, "gtk_dialog_run");
        loaded.destroy = symbolOf<decltype(loaded.destroy)>(library, "gtk_widget_destroy");
        loaded.fileName = symbolOf<decltype(loaded.fileName)>(library, "gtk_file_chooser_get_filename");
        loaded.freeString = symbolOf<decltype(loaded.freeString)>(library, "g_free");
        loaded.setIcon = symbolOf<decltype(loaded.setIcon)>(library, "gtk_window_set_icon");
        loaded.loaderNew = symbolOf<decltype(loaded.loaderNew)>(library, "gdk_pixbuf_loader_new");
        loaded.loaderWrite = symbolOf<decltype(loaded.loaderWrite)>(library, "gdk_pixbuf_loader_write");
        loaded.loaderClose = symbolOf<decltype(loaded.loaderClose)>(library, "gdk_pixbuf_loader_close");
        loaded.loaderPixbuf = symbolOf<decltype(loaded.loaderPixbuf)>(library, "gdk_pixbuf_loader_get_pixbuf");
        loaded.unref = symbolOf<decltype(loaded.unref)>(library, "g_object_unref");
        if (!loaded.initCheck || !loaded.newFileChooser || !loaded.run || !loaded.destroy || !loaded.fileName || !loaded.freeString ||
            !loaded.initCheck(nullptr, nullptr)) {
            loaded.library = nullptr;
        }
        return loaded;
    }();
    return dialogs.library ? &dialogs : nullptr;
}

void applyAppIcon(const GtkFileDialogs& gtk, GtkWidget* dialog)
{
    if (!gtk.setIcon || !gtk.loaderNew || !gtk.loaderWrite || !gtk.loaderClose || !gtk.loaderPixbuf || !gtk.unref) {
        return;
    }
    const std::string png = embeddedAppIcon();
    GdkPixbufLoader* loader = gtk.loaderNew();
    if (gtk.loaderWrite(loader, reinterpret_cast<const guchar*>(png.data()), png.size(), nullptr) && gtk.loaderClose(loader, nullptr)) {
        if (GdkPixbuf* icon = gtk.loaderPixbuf(loader)) {
            gtk.setIcon(reinterpret_cast<GtkWindow*>(dialog), icon);
        }
    }
    gtk.unref(loader);
}

std::optional<std::string> runChooser(const std::string& title, GtkFileChooserAction action, const char* accept, const std::string& start,
                                      const char* filterName, const std::vector<const char*>& patterns)
{
    const GtkFileDialogs* gtk = gtkFileDialogs();
    if (!gtk) {
        return std::nullopt;
    }
    GtkWidget* dialog = gtk->newFileChooser(title.c_str(), nullptr, action, "_Cancel", GTK_RESPONSE_CANCEL, accept, GTK_RESPONSE_ACCEPT, nullptr);
    applyAppIcon(*gtk, dialog);
    if (!start.empty() && gtk->setCurrentFolder) {
        gtk->setCurrentFolder(reinterpret_cast<GtkFileChooser*>(dialog), start.c_str());
    }
    if (!patterns.empty() && gtk->newFilter && gtk->filterSetName && gtk->filterAddPattern && gtk->chooserAddFilter) {
        GtkFileFilter* filter = gtk->newFilter();
        gtk->filterSetName(filter, filterName);
        for (const char* pattern : patterns) {
            gtk->filterAddPattern(filter, pattern);
        }
        gtk->chooserAddFilter(reinterpret_cast<GtkFileChooser*>(dialog), filter);
    }
    const gint response = gtk->run(reinterpret_cast<GtkDialog*>(dialog));
    std::optional<std::string> picked;
    if (response == GTK_RESPONSE_ACCEPT) {
        if (gchar* path = gtk->fileName(reinterpret_cast<GtkFileChooser*>(dialog))) {
            picked = path;
            gtk->freeString(path);
        }
    }
    gtk->destroy(dialog);
    return picked;
}

}

std::optional<std::string> ChooseDllFile(const std::string& start)
{
    return runChooser("Choose GWToolboxdll.dll", GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", start, "GWToolbox DLL",
                      {"GWToolboxdll.dll", "*.dll", "*.DLL"});
}

std::optional<std::string> ChooseFolder(const std::string& start)
{
    return runChooser("Choose where to install GWToolbox", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Select", start, "", {});
}
