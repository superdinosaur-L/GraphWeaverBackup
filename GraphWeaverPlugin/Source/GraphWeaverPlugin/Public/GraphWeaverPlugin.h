// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FGraphWeaverPluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
