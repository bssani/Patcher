// Fill out your copyright notice in the Description page of Project Settings.


#include "PatchingGameInstance.h"
#include "ChunkDownloader.h"
#include "Misc/CoreDelegates.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"

// 게임 인스턴스가 초기화될 때 호출
void UPatchingGameInstance::Init()
{
    Super::Init();

    // 설정에서 PatchVersionURL을 구성
    TArray<FString> TempCdnBaseUrls;
    GConfig->GetArray(TEXT("/Script/Plugins.ChunkDownloader"), TEXT("CdnBaseUrls"), TempCdnBaseUrls, GGameIni);

    if (TempCdnBaseUrls.Num() > 0) {
        BaseUrl = TempCdnBaseUrls[0];

        PatchVersionURL = BaseUrl + "/ContentBuildId.txt";
    }

    // 패치 버전 정보를 가져오기 위한 HTTP 요청을 생성
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UPatchingGameInstance::OnPatchVersionResponse);

    // HTTP 요청을 구성하고 전송
    Request->SetURL(PatchVersionURL);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
    Request->SetHeader("Content-Type", TEXT("application-json"));
    if (!Request->ProcessRequest()) {
        UE_LOG(LogTemp, Display, TEXT("Failed HTTP request to ContentBuildId.txt. File Unreachable."));
    }
}

// 패치 버전 요청에 대한 HTTP 응답의 콜백
void UPatchingGameInstance::OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr response, bool bWasSucessful)
{
    if (bWasSucessful) {
        if (!response.IsValid()) { 
            // 응답이 유효하지 않은 경우 처리
            UE_LOG(LogTemp, Display, TEXT("Failed HTTP request response from ContentBuildId.txt file. File Unreachable."));
        }
        else if (EHttpResponseCodes::IsOk(response->GetResponseCode())) {
            // 응답에서 패치 콘텐츠 ID를 처리
            FString ContentBuildId = response->GetContentAsString(); 
            UE_LOG(LogTemp, Display, TEXT("Patch Content ID Response: %s"), *ContentBuildId);
            // 현재 플랫폼으로 청크 다운로더를 초기화
            TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
            Downloader->Initialize(UGameplayStatics::GetPlatformName(), 8);

            // 캐시된 빌드 ID를 로드하고 빌드 매니페스트를 업데이트
            Downloader->LoadCachedBuild(DeploymentName);
                       
            TFunction<void(bool bSuccess)> UpdateCompleteCallback = [&](bool bSuccess) {bIsDownloadManifestUpToDate = bSuccess; };
            Downloader->UpdateBuild(DeploymentName, ContentBuildId, UpdateCompleteCallback);
        }
        else {
            UE_LOG(LogTemp, Display, TEXT("Failed HTTP request for ContentBuildId.txt."));
        }
    }
}

// 게임 인스턴스가 종료될 때 호출
void UPatchingGameInstance::Shutdown()
{
    Super::Shutdown();
    // 리소스를 해제하기 위해 청크 다운로더를 종료
    FChunkDownloader::Shutdown();
}

// 다운로드 매니페스트 완료 상태를 업데이트
void UPatchingGameInstance::OnManifestUpdateComplete(bool bSuccess)
{
    bIsDownloadManifestUpToDate = bSuccess;
}

// 로딩 프로세스의 진행 상황을 가져온다
void UPatchingGameInstance::GetLoadingProgress(int32& BytesDownloaded, int32& TotalBytesToDownload, float& DownloadPercent, int32& ChunksMounted, int32& TotalChunksToMount, float& MountPercent) const
{
    // 청크 다운로더 인스턴스를 가져온다
    TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

    FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

    // 다운로드와 마운트 진행률을 계산
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

// 게임 패치 프로세스를 시작
bool UPatchingGameInstance::PatchGame()
{
    // 청크 다운로더 인스턴스를 가져오기
    if (bIsDownloadManifestUpToDate)
    {
        
        TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

        // 다운로드할 모든 청크 ID를 가져오기
        Downloader->GetAllChunkIds(ChunkDownloadList);

        // 청크 상태를 로그에 기록
        for (int32 ChunkID : ChunkDownloadList)
        {
            int32 ChunkStatus = static_cast<int32>(Downloader->GetChunkStatus(ChunkID));
            UE_LOG(LogTemp, Display, TEXT("Chunk %i status: %i"), ChunkID, ChunkStatus);

        }

        // 청크를 다운로드하고 로딩 모드를 시작
            TFunction<void(bool bSuccess)> DownloadCompleteCallback = [&](bool bSuccess) {OnDownloadComplete(bSuccess); };
            Downloader->DownloadChunks(ChunkDownloadList, DownloadCompleteCallback, 1);

           
            TFunction<void(bool bSuccess)> LoadingModeCompleteCallback = [&](bool bSuccess) {OnLoadingModeComplete(bSuccess); };
            Downloader->BeginLoadingMode(LoadingModeCompleteCallback);
        

        return true;
    }

    // 매니페스트가 최신 상태가 아니면 패치할 수 없다
    UE_LOG(LogTemp, Display, TEXT("Manifest Update Failed. Can't patch the game"));

    return false;
}

// 로딩 모드가 완료되었을 때 호출
void UPatchingGameInstance::OnLoadingModeComplete(bool bSuccess)
{
    OnDownloadComplete(bSuccess);
}

// 모든 청크가 마운트되었을 때 호출
void UPatchingGameInstance::OnMountComplete(bool bSuccess)
{
    OnPatchComplete.Broadcast(bSuccess);
}

// 청크 다운로드 프로세스가 완료되었을 때 호출
void UPatchingGameInstance::OnDownloadComplete(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Display, TEXT("Download complete"));

        // 다운로드된 청크를 마운트
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