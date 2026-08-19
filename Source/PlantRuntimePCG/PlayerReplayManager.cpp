#include "PlayerReplayManager.h"

#include "AIController.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "BrainComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerReplaySaveGame.h"

APlayerReplayManager::APlayerReplayManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void APlayerReplayManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double SafeDeltaSeconds = FMath::Max(static_cast<double>(DeltaSeconds), 0.0);
	if (bIsRecording)
	{
		RecordingTimeSeconds += SafeDeltaSeconds;
		SampleAccumulatorSeconds += SafeDeltaSeconds;
		const double SafeInterval = FMath::Max(static_cast<double>(SampleIntervalSeconds), 1.0 / 120.0);
		if (SampleAccumulatorSeconds >= SafeInterval)
		{
			CaptureFrame();
			SampleAccumulatorSeconds = FMath::Fmod(SampleAccumulatorSeconds, SafeInterval);
		}
	}

	if (!bIsReplaying || bObservationPaused)
	{
		return;
	}

	ReplayTimeSeconds += SafeDeltaSeconds * FMath::Max(static_cast<double>(PlaybackRate), 0.01);
	UpdateReplayTransform();
	DispatchPendingEvents();
	FinishOrLoopReplay();
}

void APlayerReplayManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bObservationPaused)
	{
		ExitObservationPause();
	}
	Super::EndPlay(EndPlayReason);
}

void APlayerReplayManager::SetTargetCharacter(ACharacter* NewTargetCharacter)
{
	TargetCharacter = NewTargetCharacter;
}

bool APlayerReplayManager::StartRecording(const bool bClearExistingData)
{
	if (!IsValid(TargetCharacter))
	{
		return false;
	}
	if (bObservationPaused)
	{
		ExitObservationPause();
	}
	if (bClearExistingData)
	{
		ClearRecording();
	}
	bIsReplaying = false;
	bIsRecording = true;
	RecordingTimeSeconds = RecordedFrames.IsEmpty() ? 0.0 : RecordedFrames.Last().TimeSeconds;
	SampleAccumulatorSeconds = 0.0;
	CaptureFrame();
	return true;
}

void APlayerReplayManager::StopRecording()
{
	if (bIsRecording)
	{
		CaptureFrame();
	}
	bIsRecording = false;
}

void APlayerReplayManager::ClearRecording()
{
	if (bObservationPaused)
	{
		ExitObservationPause();
	}
	bIsRecording = false;
	bIsReplaying = false;
	RecordedFrames.Reset();
	RecordedEvents.Reset();
	RecordingTimeSeconds = 0.0;
	ReplayTimeSeconds = 0.0;
	SampleAccumulatorSeconds = 0.0;
	CurrentFrameIndex = 0;
	CurrentEventIndex = 0;
}

bool APlayerReplayManager::SaveRecording()
{
	LastPersistenceError.Reset();
	if (SaveSlotName.IsEmpty())
	{
		LastPersistenceError = TEXT("Save slot name is empty.");
		return false;
	}
	if (RecordedFrames.IsEmpty())
	{
		LastPersistenceError = TEXT("No recorded frames are available.");
		return false;
	}

	UPlayerReplaySaveGame* SaveData = Cast<UPlayerReplaySaveGame>(
		UGameplayStatics::CreateSaveGameObject(UPlayerReplaySaveGame::StaticClass()));
	if (SaveData == nullptr)
	{
		LastPersistenceError = TEXT("Could not create replay SaveGame object.");
		return false;
	}

	SaveData->FormatVersion = UPlayerReplaySaveGame::CurrentFormatVersion;
	SaveData->MapName = GetCurrentMapName();
	SaveData->SampleIntervalSeconds = SampleIntervalSeconds;
	SaveData->Frames = RecordedFrames;
	SaveData->Events = RecordedEvents;
	if (!UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex))
	{
		LastPersistenceError = FString::Printf(TEXT("Failed to write replay slot '%s'."), *SaveSlotName);
		return false;
	}
	return true;
}

bool APlayerReplayManager::LoadRecording()
{
	LastPersistenceError.Reset();
	if (SaveSlotName.IsEmpty())
	{
		LastPersistenceError = TEXT("Save slot name is empty.");
		return false;
	}
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		LastPersistenceError = FString::Printf(TEXT("Replay slot '%s' does not exist."), *SaveSlotName);
		return false;
	}

	const UPlayerReplaySaveGame* SaveData = Cast<UPlayerReplaySaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (SaveData == nullptr)
	{
		LastPersistenceError = FString::Printf(TEXT("Replay slot '%s' has an unexpected SaveGame class."), *SaveSlotName);
		return false;
	}
	if (SaveData->FormatVersion != UPlayerReplaySaveGame::CurrentFormatVersion)
	{
		LastPersistenceError = FString::Printf(
			TEXT("Replay format %d is unsupported; expected %d."),
			SaveData->FormatVersion,
			UPlayerReplaySaveGame::CurrentFormatVersion);
		return false;
	}
	if (!FMath::IsFinite(SaveData->SampleIntervalSeconds) || SaveData->SampleIntervalSeconds < 1.0f / 120.0f)
	{
		LastPersistenceError = TEXT("Replay sample interval is invalid.");
		return false;
	}
	if (bRequireMatchingMapOnLoad && SaveData->MapName != GetCurrentMapName())
	{
		LastPersistenceError = FString::Printf(
			TEXT("Replay map '%s' does not match current map '%s'."),
			*SaveData->MapName,
			*GetCurrentMapName());
		return false;
	}
	FString ValidationError;
	if (!ValidateLoadedRecording(SaveData->Frames, SaveData->Events, ValidationError))
	{
		LastPersistenceError = MoveTemp(ValidationError);
		return false;
	}

	ClearRecording();
	RecordedFrames = SaveData->Frames;
	RecordedEvents = SaveData->Events;
	SampleIntervalSeconds = SaveData->SampleIntervalSeconds;
	for (FPlayerReplayEvent& Event : RecordedEvents)
	{
		Event.TargetActor = nullptr;
	}
	return true;
}

bool APlayerReplayManager::DeleteSavedRecording()
{
	LastPersistenceError.Reset();
	if (SaveSlotName.IsEmpty())
	{
		LastPersistenceError = TEXT("Save slot name is empty.");
		return false;
	}
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		return true;
	}
	if (!UGameplayStatics::DeleteGameInSlot(SaveSlotName, SaveUserIndex))
	{
		LastPersistenceError = FString::Printf(TEXT("Failed to delete replay slot '%s'."), *SaveSlotName);
		return false;
	}
	return true;
}

bool APlayerReplayManager::HasSavedRecording() const
{
	return !SaveSlotName.IsEmpty() && UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex);
}

void APlayerReplayManager::RecordAttackEvent(const FName EventName, AActor* TargetActor, const FVector WorldLocation)
{
	AddRecordedEvent(EPlayerReplayEventType::Attack, EventName, TargetActor, WorldLocation);
}

void APlayerReplayManager::RecordCollectEvent(const FName EventName, AActor* TargetActor, const FVector WorldLocation)
{
	AddRecordedEvent(EPlayerReplayEventType::Collect, EventName, TargetActor, WorldLocation);
}

bool APlayerReplayManager::StartReplay()
{
	if (!IsValid(TargetCharacter) || RecordedFrames.IsEmpty())
	{
		return false;
	}
	if (bObservationPaused)
	{
		ExitObservationPause();
	}
	bIsRecording = false;
	bIsReplaying = true;
	ReplayTimeSeconds = 0.0;
	CurrentFrameIndex = 0;
	CurrentEventIndex = 0;
	TargetCharacter->SetActorLocationAndRotation(
		RecordedFrames[0].Location,
		RecordedFrames[0].Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	DispatchPendingEvents();
	return true;
}

void APlayerReplayManager::StopReplay()
{
	if (bObservationPaused)
	{
		ExitObservationPause();
	}
	bIsReplaying = false;
	ReplayTimeSeconds = 0.0;
	CurrentFrameIndex = 0;
	CurrentEventIndex = 0;
}

bool APlayerReplayManager::PauseForObservation()
{
	if (!bIsReplaying || bObservationPaused)
	{
		return false;
	}
	EnterObservationPause();
	return true;
}

bool APlayerReplayManager::ResumeFromObservation()
{
	if (!bObservationPaused)
	{
		return false;
	}
	ExitObservationPause();
	return true;
}

bool APlayerReplayManager::ToggleObservationPause()
{
	return bObservationPaused ? ResumeFromObservation() : PauseForObservation();
}

void APlayerReplayManager::CaptureFrame()
{
	if (!IsValid(TargetCharacter))
	{
		bIsRecording = false;
		return;
	}

	FPlayerReplayFrame& Frame = RecordedFrames.AddDefaulted_GetRef();
	Frame.TimeSeconds = RecordingTimeSeconds;
	Frame.Location = TargetCharacter->GetActorLocation();
	Frame.Rotation = TargetCharacter->GetActorRotation();
	Frame.Velocity = TargetCharacter->GetVelocity();
}

void APlayerReplayManager::UpdateReplayTransform()
{
	if (!IsValid(TargetCharacter) || RecordedFrames.IsEmpty())
	{
		StopReplay();
		return;
	}

	while (CurrentFrameIndex + 1 < RecordedFrames.Num()
		&& RecordedFrames[CurrentFrameIndex + 1].TimeSeconds <= ReplayTimeSeconds)
	{
		++CurrentFrameIndex;
	}

	const FPlayerReplayFrame& FrameA = RecordedFrames[CurrentFrameIndex];
	const int32 NextIndex = FMath::Min(CurrentFrameIndex + 1, RecordedFrames.Num() - 1);
	const FPlayerReplayFrame& FrameB = RecordedFrames[NextIndex];
	const double FrameDuration = FrameB.TimeSeconds - FrameA.TimeSeconds;
	const double Alpha = FrameDuration > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp((ReplayTimeSeconds - FrameA.TimeSeconds) / FrameDuration, 0.0, 1.0)
		: 0.0;
	const FVector ReplayLocation = FMath::Lerp(FrameA.Location, FrameB.Location, Alpha);
	const FQuat ReplayRotation = FQuat::Slerp(FrameA.Rotation.Quaternion(), FrameB.Rotation.Quaternion(), Alpha).GetNormalized();
	TargetCharacter->SetActorLocationAndRotation(
		ReplayLocation,
		ReplayRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void APlayerReplayManager::DispatchPendingEvents()
{
	while (CurrentEventIndex < RecordedEvents.Num()
		&& RecordedEvents[CurrentEventIndex].TimeSeconds <= ReplayTimeSeconds)
	{
		FPlayerReplayEvent& Event = RecordedEvents[CurrentEventIndex++];
		AActor* ResolvedTarget = ResolveEventTarget(Event);
		if (Event.Type == EPlayerReplayEventType::Attack)
		{
			OnReplayAttack.Broadcast(Event.EventName, ResolvedTarget, Event.WorldLocation);
		}
		else
		{
			OnReplayCollect.Broadcast(Event.EventName, ResolvedTarget, Event.WorldLocation);
		}
	}
}

void APlayerReplayManager::AddRecordedEvent(
	const EPlayerReplayEventType Type,
	const FName EventName,
	AActor* TargetActor,
	const FVector& WorldLocation)
{
	if (!bIsRecording || WorldLocation.ContainsNaN())
	{
		return;
	}
	FPlayerReplayEvent& Event = RecordedEvents.AddDefaulted_GetRef();
	Event.TimeSeconds = RecordingTimeSeconds;
	Event.Type = Type;
	Event.EventName = EventName;
	Event.TargetActor = TargetActor;
	Event.TargetActorName = IsValid(TargetActor) ? TargetActor->GetFName() : NAME_None;
	Event.TargetActorClassPath = IsValid(TargetActor) ? FSoftClassPath(TargetActor->GetClass()) : FSoftClassPath();
	Event.WorldLocation = WorldLocation;
}

AActor* APlayerReplayManager::ResolveEventTarget(FPlayerReplayEvent& Event) const
{
	if (IsValid(Event.TargetActor))
	{
		return Event.TargetActor;
	}
	if (Event.TargetActorName.IsNone() && Event.TargetActorClassPath.IsNull())
	{
		return nullptr;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	AActor* NearestMatchingActor = nullptr;
	const double MaximumDistanceSquared = FMath::Square(FMath::Max(static_cast<double>(TargetResolveRadiusCm), 0.0));
	double NearestDistanceSquared = MaximumDistanceSquared;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		const bool bClassMatches = Event.TargetActorClassPath.IsNull()
			|| FSoftClassPath(Candidate->GetClass()) == Event.TargetActorClassPath;
		if (!bClassMatches)
		{
			continue;
		}
		if (!Event.TargetActorName.IsNone() && Candidate->GetFName() == Event.TargetActorName)
		{
			Event.TargetActor = Candidate;
			return Candidate;
		}
		const double DistanceSquared = FVector::DistSquared(Candidate->GetActorLocation(), Event.WorldLocation);
		if (DistanceSquared <= NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestMatchingActor = Candidate;
		}
	}
	Event.TargetActor = NearestMatchingActor;
	return NearestMatchingActor;
}

bool APlayerReplayManager::ValidateLoadedRecording(
	const TArray<FPlayerReplayFrame>& Frames,
	const TArray<FPlayerReplayEvent>& Events,
	FString& OutError) const
{
	if (Frames.IsEmpty())
	{
		OutError = TEXT("Saved replay contains no frames.");
		return false;
	}
	double PreviousTime = -1.0;
	for (const FPlayerReplayFrame& Frame : Frames)
	{
		if (!FMath::IsFinite(Frame.TimeSeconds) || Frame.TimeSeconds < PreviousTime
			|| Frame.Location.ContainsNaN() || Frame.Rotation.ContainsNaN() || Frame.Velocity.ContainsNaN())
		{
			OutError = TEXT("Saved replay contains an invalid or unordered frame.");
			return false;
		}
		PreviousTime = Frame.TimeSeconds;
	}
	PreviousTime = -1.0;
	for (const FPlayerReplayEvent& Event : Events)
	{
		if (!FMath::IsFinite(Event.TimeSeconds) || Event.TimeSeconds < PreviousTime
			|| Event.TimeSeconds > Frames.Last().TimeSeconds || Event.WorldLocation.ContainsNaN())
		{
			OutError = TEXT("Saved replay contains an invalid or unordered event.");
			return false;
		}
		PreviousTime = Event.TimeSeconds;
	}
	return true;
}

FString APlayerReplayManager::GetCurrentMapName() const
{
	return GetWorld() != nullptr ? UGameplayStatics::GetCurrentLevelName(this, true) : FString();
}

void APlayerReplayManager::EnterObservationPause()
{
	bObservationPaused = true;
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Environment = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Environment->SetObservationPaused(true);
		}
	}
	PauseAI();
	OnObservationPauseChanged.Broadcast(true);
}

void APlayerReplayManager::ExitObservationPause()
{
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Environment = World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Environment->SetObservationPaused(false);
		}
	}
	ResumeAI();
	bObservationPaused = false;
	OnObservationPauseChanged.Broadcast(false);
}

void APlayerReplayManager::PauseAI()
{
	PausedAIStates.Reset();
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AAIController> It(World); It; ++It)
	{
		AAIController* Controller = *It;
		FPausedAIState& State = PausedAIStates.AddDefaulted_GetRef();
		State.Controller = Controller;
		if (UBrainComponent* Brain = Controller->GetBrainComponent())
		{
			State.bBrainWasRunning = Brain->IsRunning();
			if (State.bBrainWasRunning)
			{
				Brain->StopLogic(TEXT("Replay observation pause"));
			}
		}
		if (UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent())
		{
			State.bMoveWasActive = PathFollowing->GetStatus() == EPathFollowingStatus::Moving;
			if (State.bMoveWasActive)
			{
				PathFollowing->PauseMove(FAIRequestID::CurrentRequest, EPathFollowingVelocityMode::Reset);
			}
		}
	}
}

void APlayerReplayManager::ResumeAI()
{
	for (const FPausedAIState& State : PausedAIStates)
	{
		AAIController* Controller = State.Controller.Get();
		if (Controller == nullptr)
		{
			continue;
		}
		if (State.bMoveWasActive)
		{
			if (UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent())
			{
				PathFollowing->ResumeMove(FAIRequestID::CurrentRequest);
			}
		}
		if (State.bBrainWasRunning)
		{
			if (UBrainComponent* Brain = Controller->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
		}
	}
	PausedAIStates.Reset();
}

void APlayerReplayManager::FinishOrLoopReplay()
{
	if (RecordedFrames.IsEmpty() || ReplayTimeSeconds < RecordedFrames.Last().TimeSeconds)
	{
		return;
	}
	if (bLoopPlayback)
	{
		ReplayTimeSeconds = 0.0;
		CurrentFrameIndex = 0;
		CurrentEventIndex = 0;
		TargetCharacter->SetActorLocationAndRotation(
			RecordedFrames[0].Location,
			RecordedFrames[0].Rotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	else
	{
		bIsReplaying = false;
	}
}
