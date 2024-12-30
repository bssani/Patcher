// Fill out your copyright notice in the Description page of Project Settings.


#include "PatchingGameInstance.h"
#include "ChunkDownloader.h"
#include "Misc/CoreDelegates.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"

// ���� �ν��Ͻ��� �ʱ�ȭ�� �� ȣ��
void UPatchingGameInstance::Init()
{
    Super::Init();

    // �������� PatchVersionURL�� ����
    TArray<FString> TempCdnBaseUrls;
    GConfig->GetArray(TEXT("/Script/Plugins.ChunkDownloader"), TEXT("CdnBaseUrls"), TempCdnBaseUrls, GGameIni);

    if (TempCdnBaseUrls.Num() > 0) {
        BaseUrl = TempCdnBaseUrls[0];

        PatchVersionURL = BaseUrl + "/ContentBuildId.txt";
    }

    // ��ġ ���� ������ �������� ���� HTTP ��û�� ����
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UPatchingGameInstance::OnPatchVersionResponse);

    // HTTP ��û�� �����ϰ� ����
    Request->SetURL(PatchVersionURL);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
    Request->SetHeader("Content-Type", TEXT("application-json"));
    if (!Request->ProcessRequest()) {
        UE_LOG(LogTemp, Display, TEXT("Failed HTTP request to ContentBuildId.txt. File Unreachable."));
    }
}

// ��ġ ���� ��û�� ���� HTTP ������ �ݹ�
void UPatchingGameInstance::OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr response, bool bWasSucessful)
{
    if (bWasSucessful) {
        if (!response.IsValid()) { 
            // ������ ��ȿ���� ���� ��� ó��
            UE_LOG(LogTemp, Display, TEXT("Failed HTTP request response from ContentBuildId.txt file. File Unreachable."));
        }
        else if (EHttpResponseCodes::IsOk(response->GetResponseCode())) {
            // ���信�� ��ġ ������ ID�� ó��
            FString ContentBuildId = response->GetContentAsString(); 
            UE_LOG(LogTemp, Display, TEXT("Patch Content ID Response: %s"), *ContentBuildId);
            // ���� �÷������� ûũ �ٿ�δ��� �ʱ�ȭ
            TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
            Downloader->Initialize(UGameplayStatics::GetPlatformName(), 8);

            // ĳ�õ� ���� ID�� �ε��ϰ� ���� �Ŵ��佺Ʈ�� ������Ʈ
            Downloader->LoadCachedBuild(DeploymentName);
                       
            TFunction<void(bool bSuccess)> UpdateCompleteCallback = [&](bool bSuccess) {bIsDownloadManifestUpToDate = bSuccess; };
            Downloader->UpdateBuild(DeploymentName, ContentBuildId, UpdateCompleteCallback);
        }
        else {
            UE_LOG(LogTemp, Display, TEXT("Failed HTTP request for ContentBuildId.txt."));
        }
    }
}

// ���� �ν��Ͻ��� ����� �� ȣ��
void UPatchingGameInstance::Shutdown()
{
    Super::Shutdown();
    // ���ҽ��� �����ϱ� ���� ûũ �ٿ�δ��� ����
    FChunkDownloader::Shutdown();
}

// �ٿ�ε� �Ŵ��佺Ʈ �Ϸ� ���¸� ������Ʈ
void UPatchingGameInstance::OnManifestUpdateComplete(bool bSuccess)
{
    bIsDownloadManifestUpToDate = bSuccess;
}

// �ε� ���μ����� ���� ��Ȳ�� �����´�
void UPatchingGameInstance::GetLoadingProgress(int32& BytesDownloaded, int32& TotalBytesToDownload, float& DownloadPercent, int32& ChunksMounted, int32& TotalChunksToMount, float& MountPercent) const
{
    // ûũ �ٿ�δ� �ν��Ͻ��� �����´�
    TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

    FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

    // �ٿ�ε�� ����Ʈ ������� ���
    BytesDownloaded = LoadingStats.BytesDownloaded;
    TotalBytesToDownload = LoadingStats.TotalBytesToDownload;

    ChunksMounted = LoadingStats.ChunksMounted;
    TotalChunksToMount = LoadingStats.TotalChunksToMount;

    if (BytesDownloaded != 0 || TotalBytesToDownload != 0)
    {
        DownloadPercent = ((float)BytesDownloaded / (float)TotalBytesToDownload) * 100.0f;
    }
    else
    {
        DownloadPercent = 100.0f;
    }

    if (MountPercent != 0 || TotalChunksToMount != 0)
    {
        MountPercent = ((float)ChunksMounted / (float)TotalChunksToMount) * 100.0f;
    }
    else
    {
        MountPercent = 100.0f;
    }
}

// ���� ��ġ ���μ����� ����
bool UPatchingGameInstance::PatchGame()
{
    // ûũ �ٿ�δ� �ν��Ͻ��� ��������
    if (bIsDownloadManifestUpToDate)
    {
        
        TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

        // �ٿ�ε��� ��� ûũ ID�� ��������
        Downloader->GetAllChunkIds(ChunkDownloadList);

        // ûũ ���¸� �α׿� ���
        for (int32 ChunkID : ChunkDownloadList)
        {
            int32 ChunkStatus = static_cast<int32>(Downloader->GetChunkStatus(ChunkID));
            UE_LOG(LogTemp, Display, TEXT("Chunk %i status: %i"), ChunkID, ChunkStatus);

        }

        // ûũ�� �ٿ�ε��ϰ� �ε� ��带 ����
            TFunction<void(bool bSuccess)> DownloadCompleteCallback = [&](bool bSuccess) {OnDownloadComplete(bSuccess); };
            Downloader->DownloadChunks(ChunkDownloadList, DownloadCompleteCallback, 1);

           
            TFunction<void(bool bSuccess)> LoadingModeCompleteCallback = [&](bool bSuccess) {OnLoadingModeComplete(bSuccess); };
            Downloader->BeginLoadingMode(LoadingModeCompleteCallback);
        

        return true;
    }

    // �Ŵ��佺Ʈ�� �ֽ� ���°� �ƴϸ� ��ġ�� �� ����
    UE_LOG(LogTemp, Display, TEXT("Manifest Update Failed. Can't patch the game"));

    return false;
}

// �ε� ��尡 �Ϸ�Ǿ��� �� ȣ��
void UPatchingGameInstance::OnLoadingModeComplete(bool bSuccess)
{
    OnDownloadComplete(bSuccess);
}

// ��� ûũ�� ����Ʈ�Ǿ��� �� ȣ��
void UPatchingGameInstance::OnMountComplete(bool bSuccess)
{
    OnPatchComplete.Broadcast(bSuccess);
}

// ûũ �ٿ�ε� ���μ����� �Ϸ�Ǿ��� �� ȣ��
void UPatchingGameInstance::OnDownloadComplete(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Display, TEXT("Download complete"));

        // �ٿ�ε�� ûũ�� ����Ʈ
        TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();
        FJsonSerializableArrayInt DownloadedChunks;

        for (int32 ChunkID : ChunkDownloadList)
        {
            DownloadedChunks.Add(ChunkID);
        }

        TFunction<void(bool bSuccess)> MountCompleteCallback = [&](bool bSuccess) {OnMountComplete(bSuccess); };
        Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);

        OnPatchComplete.Broadcast(true);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Load process failed"));

        OnPatchComplete.Broadcast(false);
    }
}