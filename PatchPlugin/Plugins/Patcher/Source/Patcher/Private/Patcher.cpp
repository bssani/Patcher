// Copyright Epic Games, Inc. All Rights Reserved.

#include "Patcher.h"

#define LOCTEXT_NAMESPACE "FPatcherModule"

void FPatcherModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
}

void FPatcherModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPatcherModule, Patcher)