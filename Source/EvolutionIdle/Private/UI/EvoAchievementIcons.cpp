#include "UI/EvoAchievementIcons.h"
#include "EvolutionIdle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

const FSlateBrush* FEvoAchievementIcons::Get(const FString& AchievementId, bool bUnlocked)
{
	if (AchievementId.IsEmpty())
	{
		return nullptr;
	}
	const FString FileName = bUnlocked ? AchievementId : AchievementId + TEXT("_locked");
	if (const TSharedPtr<FSlateDynamicImageBrush>* Found = Cache.Find(FileName))
	{
		return Found->Get();
	}
	TSharedPtr<FSlateDynamicImageBrush> Brush = Load(FileName);
	Cache.Add(FileName, Brush);
	return Brush.Get();
}

TSharedPtr<FSlateDynamicImageBrush> FEvoAchievementIcons::Load(const FString& FileName)
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Achievements"), FileName + TEXT(".png"));
	TArray<uint8> File;
	if (!FFileHelper::LoadFileToArray(File, *Path, FILEREAD_Silent))
	{
		UE_LOG(LogEvolution, Verbose, TEXT("No achievement icon at %s"), *Path);
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(File.GetData(), File.Num()))
	{
		UE_LOG(LogEvolution, Warning, TEXT("Achievement icon %s is not a valid PNG"), *Path);
		return nullptr;
	}

	TArray64<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		UE_LOG(LogEvolution, Warning, TEXT("Could not decode achievement icon %s"), *Path);
		return nullptr;
	}

	const int64 Width = Wrapper->GetWidth();
	const int64 Height = Wrapper->GetHeight();
	TArray<uint8> Pixels;
	Pixels.Append(Raw.GetData(), static_cast<int32>(Raw.Num()));
	return FSlateDynamicImageBrush::CreateWithImageData(
		FName(*FString::Printf(TEXT("EvoAchievementIcon_%s"), *FileName)),
		FVector2D(static_cast<double>(Width), static_cast<double>(Height)),
		Pixels);
}
