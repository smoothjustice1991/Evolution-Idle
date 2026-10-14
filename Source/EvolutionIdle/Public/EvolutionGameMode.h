#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "EvolutionGameMode.generated.h"

class SEvoHUD;

/** Game mode for the UI-only clicker: no pawn, mouse cursor visible, Slate HUD on top of the viewport. */
UCLASS()
class EVOLUTIONIDLE_API AEvolutionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEvolutionGameMode();
};

UCLASS()
class EVOLUTIONIDLE_API AEvolutionPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};

/** Creates the Slate interface and adds it to the game viewport. */
UCLASS()
class EVOLUTIONIDLE_API AEvolutionHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SEvoHUD> HUDWidget;
};
