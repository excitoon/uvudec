/*
UVNet Universal Decompiler (uvudec)
Copyright 2010 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#ifndef UVDASM_PLUGIN_H
#define UVDASM_PLUGIN_H

#include "plugin/plugin.h"
#include "uvd_types.h"

class UVDDisassemblerPlugin : public UVDPlugin
{
public:
	UVDDisassemblerPlugin();
	~UVDDisassemblerPlugin();

	virtual uv_err_t getName(std::string &out);
	virtual uv_err_t getDescription(std::string &out);	
	virtual uv_err_t getVersion(UVDVersion &out);
	virtual uv_err_t getArchitecture(UVDData *data, const std::string &architecture, UVDArchitecture **out);
};

#endif
