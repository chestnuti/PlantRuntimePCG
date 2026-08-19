#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerReplayManager.h"
#include "PlayerReplaySaveGame.generated.h"

/** Serialized payload owned by APlayerReplayManager. */
UCLASS()
class PLANTRUNTIMEPCG_API UPlayerReplaySaveGame final : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentFormatVersion = 1;

	UPROPERTY()
	int32 FormatVersion = CurrentFormatVersion;

	UPROPERTY()
	FString MapName;

	UPROPERTY()
	float SampleIntervalSeconds = 0.033333f;

	UPROPERTY()
	TArray<FPlayerReplayFrame> Frames;

	UPROPERTY()
	TArray<FPlayerReplayEvent> Events;
};
