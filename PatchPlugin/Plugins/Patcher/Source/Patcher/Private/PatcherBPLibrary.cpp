// Copyright Epic Games, Inc. All Rights Reserved.

#include "PatcherBPLibrary.h"
#include "Patcher.h"

UPatcherBPLibrary::UPatcherBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

float UPatcherBPLibrary::PatcherSampleFunction(float Param)
{
	return -1;
}

