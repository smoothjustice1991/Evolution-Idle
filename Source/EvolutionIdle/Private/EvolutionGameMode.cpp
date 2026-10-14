#include "EvolutionGameMode.h"
#include "EvolutionGameSubsystem.h"
#include "UI/SEvoHUD.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

AEvolutionGameMode::AEvolutionGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AEvolutionPlayerController::StaticClass();
	HUDClass = AEvolutionHUD::StaticClass();
}

void AEvolutionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AEvolutionHUD::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	UGameInstance* GameInstance = GetGameInstance();
	if (!Viewport || !GameInstance)
	{
		return;
	}

	UEvolutionGameSubsystem* Subsystem = GameInstance->GetSubsystem<UEvolutionGameSubsystem>();

	SAssignNew(HUDWidget, SEvoHUD).Subsystem(Subsystem);
	Viewport->AddViewportWidgetContent(HUDWidget.ToSharedRef(), 10);
}

void AEvolutionHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget.IsValid())
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(HUDWidget.ToSharedRef());
		}
		HUDWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}
