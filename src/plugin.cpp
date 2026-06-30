#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("hltn-mapping", "en-US")

extern struct obs_source_info hltn_mapping_filter;

bool obs_module_load(void)
{
	obs_register_source(&hltn_mapping_filter);
	return true;
}

void obs_module_unload(void)
{
}
