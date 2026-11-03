#include "chzzk-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("OBSChzzkLiveEditor contributors")

namespace {
constexpr const char *kDockId = "obs-live-editor-dock";
QPointer<ChzzkDock> g_dock;
} // namespace

const char *obs_module_description(void)
{
	return "Edit CHZZK live title, category, and tags from an OBS dock.";
}

bool obs_module_load(void)
{
	// OBS 33+ loads third-party plugins from ProgramData. Keep the bundled Qt TLS
	// backend relative to the module instead of modifying the OBS installation.
	const char *binaryPath = obs_get_module_binary_path(obs_current_module());
	if (binaryPath && *binaryPath)
		QCoreApplication::addLibraryPath(QFileInfo(QString::fromUtf8(binaryPath)).absolutePath());

	g_dock = new ChzzkDock();
	if (!obs_frontend_add_dock_by_id(kDockId, obs_module_text("Dock.Title"), g_dock)) {
		delete g_dock;
		g_dock = nullptr;
		obs_log(LOG_ERROR, "could not register dock");
		return false;
	}

	obs_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	// OBS owns the wrapper dock and its child widget after registration.
	// Removing the dock destroys ChzzkDock; deleting it again corrupts the heap.
	if (g_dock)
		obs_frontend_remove_dock(kDockId);
	g_dock.clear();
	obs_log(LOG_INFO, "plugin unloaded");
}
