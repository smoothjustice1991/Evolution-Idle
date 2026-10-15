#pragma once

#include "CoreMinimal.h"

struct FSlateBrush;
class FSlateDynamicImageBrush;

/**
 * Loads the achievement icons (Content/Achievements/<id>.png and <id>_locked.png) as Slate brushes.
 * Icons are loaded on first use and kept for the lifetime of this object.
 * Returns nullptr when an icon file is missing, so callers can fall back to a text badge.
 */
class FEvoAchievementIcons
{
public:
	const FSlateBrush* Get(const FString& AchievementId, bool bUnlocked);

private:
	TSharedPtr<FSlateDynamicImageBrush> Load(const FString& FileName);

	/** File name -> brush (nullptr entries remember files that failed to load). */
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> Cache;
};
