// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FUK2NodeToolsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
