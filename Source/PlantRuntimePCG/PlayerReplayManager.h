#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerReplayManager.generated.h"

class ACharacter;

UENUM(BlueprintType)
enum class EPlayerReplayEventType : uint8
{
	Attack,
	Collect
};

USTRUCT(BlueprintType)
struct FPlayerReplayFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	double TimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FVector Velocity = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FPlayerReplayEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	double TimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	EPlayerReplayEventType Type = EPlayerReplayEventType::Attack;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FName EventName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FName TargetActorName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FSoftClassPath TargetActorClassPath;

	/** Runtime cache; persistent loading resolves it from name, class, and location. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Replay")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Replay")
	FVector WorldLocation = FVector::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerReplayGameplayEventSignature, FName, EventName, AActor*, TargetActor, FVector, WorldLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerReplayPauseChangedSignature, bool, bPaused);

/** Records and replays one Character while leaving camera input under live player control. */
UCLASS(BlueprintType, Blueprintable)
class PLANTRUNTIMEPCG_API APlayerReplayManager final : public AActor
{
	GENERATED_BODY()

public:
	APlayerReplayManager();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Character whose world transform is recorded and replayed. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Replay|Target")
	TObjectPtr<ACharacter> TargetCharacter;

	/** Recording cadence in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Recording", meta = (ClampMin = "0.008333", UIMin = "0.008333"))
	float SampleIntervalSeconds = 0.033333f;

	/** Scales replay-clock advancement without affecting the World. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Playback", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PlaybackRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Playback")
	bool bLoopPlayback = false;

	/** SaveGame slot used by the no-argument persistence functions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Persistence")
	FString SaveSlotName = TEXT("PlayerReplay");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Persistence", meta = (ClampMin = "0"))
	int32 SaveUserIndex = 0;

	/** Rejects a replay recorded in a different map when enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Persistence")
	bool bRequireMatchingMapOnLoad = true;

	/** Radius used to recover a missing event target by recorded class and location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Persistence", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetResolveRadiusCm = 300.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Replay|Persistence")
	FString LastPersistenceError;

	/** Broadcast when a recorded attack reaches the replay cursor. */
	UPROPERTY(BlueprintAssignable, Category = "Replay|Events")
	FPlayerReplayGameplayEventSignature OnReplayAttack;

	/** Broadcast when a recorded collection reaches the replay cursor. */
	UPROPERTY(BlueprintAssignable, Category = "Replay|Events")
	FPlayerReplayGameplayEventSignature OnReplayCollect;

	/** Lets Blueprint-owned environment systems join observation pause. */
	UPROPERTY(BlueprintAssignable, Category = "Replay|Pause")
	FPlayerReplayPauseChangedSignature OnObservationPauseChanged;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Replay|Data")
	TArray<FPlayerReplayFrame> RecordedFrames;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Replay|Data")
	TArray<FPlayerReplayEvent> RecordedEvents;

	UFUNCTION(BlueprintCallable, Category = "Replay|Target")
	void SetTargetCharacter(ACharacter* NewTargetCharacter);

	UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
	bool StartRecording(bool bClearExistingData = true);

	UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
	void StopRecording();

	UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
	void ClearRecording();

	/** Saves current frames and events to SaveSlotName. */
	UFUNCTION(BlueprintCallable, Category = "Replay|Persistence")
	bool SaveRecording();

	/** Atomically loads frames and events from SaveSlotName. */
	UFUNCTION(BlueprintCallable, Category = "Replay|Persistence")
	bool LoadRecording();

	UFUNCTION(BlueprintCallable, Category = "Replay|Persistence")
	bool DeleteSavedRecording();

	UFUNCTION(BlueprintPure, Category = "Replay|Persistence")
	bool HasSavedRecording() const;

	/** Called from the Character's existing attack dispatcher while recording. */
	UFUNCTION(BlueprintCallable, Category = "Replay|Events")
	void RecordAttackEvent(FName EventName, AActor* TargetActor, FVector WorldLocation);

	/** Called from the Character's existing collection dispatcher while recording. */
	UFUNCTION(BlueprintCallable, Category = "Replay|Events")
	void RecordCollectEvent(FName EventName, AActor* TargetActor, FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
	bool StartReplay();

	UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
	void StopReplay();

	/** Freezes replay, Adaptive Environment, and active AI while camera input keeps ticking. */
	UFUNCTION(BlueprintCallable, Category = "Replay|Pause")
	bool PauseForObservation();

	UFUNCTION(BlueprintCallable, Category = "Replay|Pause")
	bool ResumeFromObservation();

	UFUNCTION(BlueprintCallable, Category = "Replay|Pause")
	bool ToggleObservationPause();

	UFUNCTION(BlueprintPure, Category = "Replay|State")
	bool IsRecording() const { return bIsRecording; }

	UFUNCTION(BlueprintPure, Category = "Replay|State")
	bool IsReplaying() const { return bIsReplaying; }

	UFUNCTION(BlueprintPure, Category = "Replay|State")
	bool IsObservationPaused() const { return bObservationPaused; }

	UFUNCTION(BlueprintPure, Category = "Replay|State")
	double GetReplayTimeSeconds() const { return ReplayTimeSeconds; }

private:
	struct FPausedAIState
	{
		TWeakObjectPtr<class AAIController> Controller;
		bool bBrainWasRunning = false;
		bool bMoveWasActive = false;
	};

	void CaptureFrame();
	void UpdateReplayTransform();
	void DispatchPendingEvents();
	void AddRecordedEvent(EPlayerReplayEventType Type, FName EventName, AActor* TargetActor, const FVector& WorldLocation);
	void EnterObservationPause();
	void ExitObservationPause();
	void PauseAI();
	void ResumeAI();
	void FinishOrLoopReplay();
	AActor* ResolveEventTarget(FPlayerReplayEvent& Event) const;
	bool ValidateLoadedRecording(const TArray<FPlayerReplayFrame>& Frames, const TArray<FPlayerReplayEvent>& Events, FString& OutError) const;
	FString GetCurrentMapName() const;

	bool bIsRecording = false;
	bool bIsReplaying = false;
	bool bObservationPaused = false;
	double RecordingTimeSeconds = 0.0;
	double ReplayTimeSeconds = 0.0;
	double SampleAccumulatorSeconds = 0.0;
	int32 CurrentFrameIndex = 0;
	int32 CurrentEventIndex = 0;
	TArray<FPausedAIState> PausedAIStates;
};
