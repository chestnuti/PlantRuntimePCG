#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "PlayerReplayManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerReplayPersistenceSaveLoadTest,
	"PlantRuntimePCG.Replay.Persistence.SaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerReplayPersistenceSaveLoadTest::RunTest(const FString& Parameters)
{
	const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ReplayPersistenceTestWorld"));
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage(), true);
	TestNotNull(TEXT("Temporary World"), TestWorld);
	if (TestWorld == nullptr)
	{
		return false;
	}

	ACharacter* Character = TestWorld->SpawnActor<ACharacter>();
	APlayerReplayManager* Manager = TestWorld->SpawnActor<APlayerReplayManager>();
	TestNotNull(TEXT("Character"), Character);
	TestNotNull(TEXT("Replay Manager"), Manager);
	if (Character != nullptr && Manager != nullptr)
	{
		Manager->SaveSlotName = FString::Printf(TEXT("ReplayAutomation_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Manager->TargetCharacter = Character;
		TestTrue(TEXT("Recording starts"), Manager->StartRecording(true));
		Character->SetActorLocation(FVector(100.0, 200.0, 300.0));
		Manager->Tick(0.1f);
		Manager->RecordAttackEvent(TEXT("Attack.Test"), Character, Character->GetActorLocation());
		Manager->StopRecording();
		const int32 ExpectedFrameCount = Manager->RecordedFrames.Num();
		const int32 ExpectedEventCount = Manager->RecordedEvents.Num();

		TestTrue(TEXT("Recording saves"), Manager->SaveRecording());
		Manager->ClearRecording();
		TestTrue(TEXT("Saved recording exists"), Manager->HasSavedRecording());
		TestTrue(TEXT("Recording loads"), Manager->LoadRecording());
		TestEqual(TEXT("Frame count is restored"), Manager->RecordedFrames.Num(), ExpectedFrameCount);
		TestEqual(TEXT("Event count is restored"), Manager->RecordedEvents.Num(), ExpectedEventCount);
		if (!Manager->RecordedEvents.IsEmpty())
		{
			TestEqual(TEXT("Event name is restored"), Manager->RecordedEvents[0].EventName, FName(TEXT("Attack.Test")));
		}
		TestTrue(TEXT("Test slot deletes"), Manager->DeleteSavedRecording());
		TestFalse(TEXT("Deleted slot no longer exists"), Manager->HasSavedRecording());
	}

	TestWorld->DestroyWorld(false);
	TestWorld->RemoveFromRoot();
	return true;
}

#endif
