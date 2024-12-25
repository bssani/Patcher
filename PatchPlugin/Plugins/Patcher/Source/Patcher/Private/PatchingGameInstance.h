// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "PatchingGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPatchCompleteDelegate, bool, Succeeded);

UCLASS()
class UPatchingGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;
    const FString DeploymentName = "PatchingCDN"; // DefaultGame.ini  /Script/Plugins.ChunkDownloader 에 있는 이름

public:
    UFUNCTION(BlueprintPure, Category = "Patching|Stats")
    void GetLoadingProgress(int32& BytesDownloaded, int32& TotalBytesToDownload, float& DownloadPercent, int32& ChunksMounted, int32& TotalChunksToMount, float& MountPercent) const;

public:
    UPROPERTY(BlueprintAssignable, Category = "Patching");
    FPatchCompleteDelegate OnPatchComplete;

public:
    UFUNCTION(BlueprintCallable, Category = "Patching")
    bool PatchGame();

    void OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr response, bool bWasSucessful);

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Patching");
    bool bIsDownloadManifestUpToDate;

protected:
    void OnManifestUpdateComplete(bool bSuccess);

protected:
    TArray<int32> ChunkDownloadList;
    FString BaseUrl;
    FString PatchVersionURL;

protected:
    void OnDownloadComplete(bool bSuccess);

    void OnLoadingModeComplete(bool bSuccess);

    void OnMountComplete(bool bSuccess);

};
