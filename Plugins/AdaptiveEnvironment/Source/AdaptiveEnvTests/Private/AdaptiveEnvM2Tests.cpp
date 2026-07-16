#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AEParameterPackageService.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AdaptiveEnvM2Tests
{
	/* Owns transient evidence and parameter assets used by one deterministic test arrangement. */
	struct FFixture
	{
		/* Stores source observations and transformations. */
		UAELiteratureEvidenceAsset* EvidenceAsset = nullptr;

		/* Stores synthesis records and effective runtime parameters. */
		UAEParameterSynthesisAsset* ParameterPackage = nullptr;
	};

	/* Creates one stable GUID from a small test-local numeric identifier. */
	FGuid MakeGuid(const uint32 Value)
	{
		return FGuid(0xAE000002, 0, 0, Value);
	}

	/* Builds a complete two-source weighted-mean parameter chain and refreshes its derived data. */
	FFixture BuildValidFixture()
	{
		FFixture Fixture;
		Fixture.EvidenceAsset = NewObject<UAELiteratureEvidenceAsset>();
		Fixture.ParameterPackage = NewObject<UAEParameterSynthesisAsset>();

		// Arrange two independent literature sources with stable source identities.
		FAELiteratureSource& SourceA = Fixture.EvidenceAsset->Sources.AddDefaulted_GetRef();
		SourceA.SourceId = MakeGuid(1);
		SourceA.CitationKey = TEXT("StudyA2024");
		SourceA.Title = TEXT("Illustrative movement exposure study A");
		SourceA.Authors = TEXT("Researcher A");
		SourceA.PublicationYear = 2024;
		SourceA.DOI = TEXT("10.0000/example.a");
		SourceA.StudyLocation = TEXT("Temperate grassland");
		SourceA.StudiedSpecies = TEXT("Grassland vegetation");
		SourceA.StudyDesign = TEXT("Controlled repeated movement treatment");
		SourceA.SourceFileRelativePath = TEXT("Research/M2/StudyA.pdf");

		FAELiteratureSource& SourceB = Fixture.EvidenceAsset->Sources.AddDefaulted_GetRef();
		SourceB.SourceId = MakeGuid(2);
		SourceB.CitationKey = TEXT("StudyB2025");
		SourceB.Title = TEXT("Illustrative movement exposure study B");
		SourceB.Authors = TEXT("Researcher B");
		SourceB.PublicationYear = 2025;
		SourceB.DOI = TEXT("10.0000/example.b");
		SourceB.StudyLocation = TEXT("Montane grassland");
		SourceB.StudiedSpecies = TEXT("Grassland vegetation");
		SourceB.StudyDesign = TEXT("Controlled repeated movement treatment");
		SourceB.SourceFileRelativePath = TEXT("Research/M2/StudyB.pdf");

		// Arrange two raw percentage observations with precise source locators.
		FAEEvidenceRecord& EvidenceA = Fixture.EvidenceAsset->EvidenceRecords.AddDefaulted_GetRef();
		EvidenceA.EvidenceId = MakeGuid(11);
		EvidenceA.SourceId = SourceA.SourceId;
		EvidenceA.ParameterConcept = TEXT("MovementExposurePerMeter");
		EvidenceA.EvidenceType = EAEEvidenceType::PointEstimate;
		EvidenceA.RawValue = 2.5;
		EvidenceA.RawUnit = TEXT("%");
		EvidenceA.Species = TEXT("Grassland vegetation");
		EvidenceA.StudyContext = TEXT("Repeated movement treatment over one growing season");
		EvidenceA.SampleSize = 48;
		EvidenceA.PageOrTable = TEXT("Table 3, row Movement");
		EvidenceA.ExtractionNotes = TEXT("Illustrative test evidence only.");

		FAEEvidenceRecord& EvidenceB = Fixture.EvidenceAsset->EvidenceRecords.AddDefaulted_GetRef();
		EvidenceB.EvidenceId = MakeGuid(12);
		EvidenceB.SourceId = SourceB.SourceId;
		EvidenceB.ParameterConcept = TEXT("MovementExposurePerMeter");
		EvidenceB.EvidenceType = EAEEvidenceType::PointEstimate;
		EvidenceB.RawValue = 3.5;
		EvidenceB.RawUnit = TEXT("%");
		EvidenceB.Species = TEXT("Grassland vegetation");
		EvidenceB.StudyContext = TEXT("Repeated movement treatment over one growing season");
		EvidenceB.SampleSize = 60;
		EvidenceB.PageOrTable = TEXT("Table 2, row Movement");
		EvidenceB.ExtractionNotes = TEXT("Illustrative test evidence only.");

		// Normalize both percentages into the canonical exposure-per-metre scale.
		FAEEvidenceTransformation& TransformationA = Fixture.EvidenceAsset->Transformations.AddDefaulted_GetRef();
		TransformationA.TransformationId = MakeGuid(21);
		TransformationA.EvidenceId = EvidenceA.EvidenceId;
		TransformationA.Method = EAETransformationMethod::PercentToRatio;
		TransformationA.MethodVersion = TEXT("1.0.0");
		TransformationA.InputUnit = TEXT("%");
		TransformationA.OutputUnit = TEXT("exposure/m");
		TransformationA.Formula = TEXT("RawValue / 100");
		TransformationA.NormalizedValue = 0.025;
		TransformationA.NormalizedMinimum = 0.025;
		TransformationA.NormalizedMaximum = 0.025;
		TransformationA.Assumptions = TEXT("Illustrative percentage maps linearly to normalized exposure.");

		FAEEvidenceTransformation& TransformationB = Fixture.EvidenceAsset->Transformations.AddDefaulted_GetRef();
		TransformationB.TransformationId = MakeGuid(22);
		TransformationB.EvidenceId = EvidenceB.EvidenceId;
		TransformationB.Method = EAETransformationMethod::PercentToRatio;
		TransformationB.MethodVersion = TEXT("1.0.0");
		TransformationB.InputUnit = TEXT("%");
		TransformationB.OutputUnit = TEXT("exposure/m");
		TransformationB.Formula = TEXT("RawValue / 100");
		TransformationB.NormalizedValue = 0.035;
		TransformationB.NormalizedMinimum = 0.035;
		TransformationB.NormalizedMaximum = 0.035;
		TransformationB.Assumptions = TEXT("Illustrative percentage maps linearly to normalized exposure.");

		// Combine the two normalized observations with compact auditable quality scores.
		FAEParameterSynthesis& Synthesis = Fixture.ParameterPackage->Syntheses.AddDefaulted_GetRef();
		Synthesis.SynthesisId = MakeGuid(31);
		Synthesis.SynthesisVersion = TEXT("1.0.0");
		Synthesis.TargetParameter = TEXT("MovementExposurePerMeter");
		Synthesis.OutputUnit = TEXT("exposure/m");
		Synthesis.Method = EAESynthesisMethod::WeightedMean;
		Synthesis.Formula = TEXT("Sum(Value * Weight) / Sum(Weight)");
		Synthesis.WeightingRationale = TEXT("Context relevance times study quality times researcher confidence.");
		Synthesis.ReviewStatus = TEXT("Approved");

		FAESynthesisContribution& ContributionA = Synthesis.Contributions.AddDefaulted_GetRef();
		ContributionA.EvidenceId = EvidenceA.EvidenceId;
		ContributionA.TransformationId = TransformationA.TransformationId;
		ContributionA.NormalizedValue = TransformationA.NormalizedValue;
		ContributionA.NormalizedUnit = TransformationA.OutputUnit;
		ContributionA.ContextRelevance = 0.8f;
		ContributionA.StudyQuality = 0.75f;
		ContributionA.ResearcherConfidence = 1.0f;
		ContributionA.bIncluded = true;
		ContributionA.InclusionReason = TEXT("Direct compatible movement evidence.");
		ContributionA.ScoringNotes = TEXT("Controlled design with compatible vegetation context.");

		FAESynthesisContribution& ContributionB = Synthesis.Contributions.AddDefaulted_GetRef();
		ContributionB.EvidenceId = EvidenceB.EvidenceId;
		ContributionB.TransformationId = TransformationB.TransformationId;
		ContributionB.NormalizedValue = TransformationB.NormalizedValue;
		ContributionB.NormalizedUnit = TransformationB.OutputUnit;
		ContributionB.ContextRelevance = 0.8f;
		ContributionB.StudyQuality = 0.5f;
		ContributionB.ResearcherConfidence = 1.0f;
		ContributionB.bIncluded = true;
		ContributionB.InclusionReason = TEXT("Direct compatible movement evidence.");
		ContributionB.ScoringNotes = TEXT("Compatible context with lower study quality.");

		// Define one effective parameter without a manual adjustment.
		FAEDerivedParameter& Parameter = Fixture.ParameterPackage->DerivedParameters.AddDefaulted_GetRef();
		Parameter.ParameterId = MakeGuid(41);
		Parameter.ParameterName = TEXT("MovementExposurePerMeter");
		Parameter.Description = TEXT("Normalized movement exposure added per metre travelled.");
		Parameter.Unit = TEXT("exposure/m");
		Parameter.SynthesisId = Synthesis.SynthesisId;
		Parameter.ParameterVersion = TEXT("1.0.0");
		Parameter.PlausibleMinimum = 0.0;
		Parameter.PlausibleMaximum = 0.1;

		// Identify and seal this immutable package version.
		Fixture.ParameterPackage->Metadata.PackageId = MakeGuid(51);
		Fixture.ParameterPackage->Metadata.SemanticVersion = TEXT("1.0.0");
		Fixture.ParameterPackage->Metadata.CreatedAt = FDateTime(2026, 7, 16, 12, 0, 0);
		Fixture.ParameterPackage->Metadata.CreatedBy = TEXT("AdaptiveEnvM2Tests");
		Fixture.ParameterPackage->Metadata.ReviewStatus = TEXT("Approved");
		Fixture.ParameterPackage->Metadata.ChangeSummary = TEXT("Deterministic M2 fixture.");

		FString RefreshError;
		FAEParameterPackageService::RefreshPackage(*Fixture.ParameterPackage, RefreshError);
		return Fixture;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2ValidChainTest,
	"AdaptiveEnv.M2.Provenance.ValidChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify a two-source evidence chain validates and produces the expected weighted mean. */
bool FAEM2ValidChainTest::RunTest(const FString& Parameters)
{
	// Arrange one complete two-source parameter package.
	const AdaptiveEnvM2Tests::FFixture Fixture = AdaptiveEnvM2Tests::BuildValidFixture();

	// Validate provenance and assert the deterministic weighted result.
	const FAEValidationResult Result = FAEParameterPackageService::ValidatePackage(*Fixture.EvidenceAsset, *Fixture.ParameterPackage);
	TestTrue(TEXT("Complete M2 chain is valid"), Result.IsValid());
	TestEqual(TEXT("No validation errors"), Result.Count(EAEValidationSeverity::Error), 0);
	TestTrue(TEXT("Weighted mean is 0.029"), FMath::IsNearlyEqual(Fixture.ParameterPackage->Syntheses[0].ComputedValue, 0.029, 1.0e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2TransformationTest,
	"AdaptiveEnv.M2.Transformation.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify standard transformations recompute values and reject mixed units. */
bool FAEM2TransformationTest::RunTest(const FString& Parameters)
{
	// Arrange one valid percentage transformation from the shared fixture.
	const AdaptiveEnvM2Tests::FFixture Fixture = AdaptiveEnvM2Tests::BuildValidFixture();
	FAEEvidenceTransformation Transformation = Fixture.EvidenceAsset->Transformations[0];
	double Value = 0.0;
	FString Error;

	// Assert deterministic conversion and explicit mixed-unit rejection.
	TestTrue(TEXT("Percentage conversion succeeds"), FAEParameterPackageService::ComputeTransformation(Fixture.EvidenceAsset->EvidenceRecords[0], Transformation, Value, Error));
	TestTrue(TEXT("Percentage converts to ratio"), FMath::IsNearlyEqual(Value, 0.025, 1.0e-9));
	Transformation.InputUnit = TEXT("cm");
	TestFalse(TEXT("Mixed input unit fails"), FAEParameterPackageService::ComputeTransformation(Fixture.EvidenceAsset->EvidenceRecords[0], Transformation, Value, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2SynthesisOrderTest,
	"AdaptiveEnv.M2.Synthesis.StableOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify contribution array order does not change numeric results or content hashes. */
bool FAEM2SynthesisOrderTest::RunTest(const FString& Parameters)
{
	// Arrange two equivalent packages with reversed contribution storage order.
	const AdaptiveEnvM2Tests::FFixture First = AdaptiveEnvM2Tests::BuildValidFixture();
	const FString FirstHash = First.ParameterPackage->Metadata.ContentHash;
	First.ParameterPackage->Syntheses[0].Contributions.Swap(0, 1);
	FString Error;
	TestTrue(TEXT("Reversed package refresh succeeds"), FAEParameterPackageService::RefreshPackage(*First.ParameterPackage, Error));

	// Assert stable sorting protects both result and canonical hash.
	TestTrue(TEXT("Reversed weighted mean remains stable"), FMath::IsNearlyEqual(First.ParameterPackage->Syntheses[0].ComputedValue, 0.029, 1.0e-9));
	TestEqual(TEXT("Reversed storage order keeps hash"), First.ParameterPackage->Metadata.ContentHash, FirstHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2InvalidContractsTest,
	"AdaptiveEnv.M2.Validation.InvalidContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify broken provenance and invalid compact scores produce blocking findings. */
bool FAEM2InvalidContractsTest::RunTest(const FString& Parameters)
{
	// Arrange one broken source link and one out-of-range quality component.
	const AdaptiveEnvM2Tests::FFixture Fixture = AdaptiveEnvM2Tests::BuildValidFixture();
	Fixture.EvidenceAsset->EvidenceRecords[0].SourceId = AdaptiveEnvM2Tests::MakeGuid(999);
	Fixture.ParameterPackage->Syntheses[0].Contributions[0].ContextRelevance = -0.1f;

	// Assert validation reports errors instead of silently accepting the package.
	const FAEValidationResult Result = FAEParameterPackageService::ValidatePackage(*Fixture.EvidenceAsset, *Fixture.ParameterPackage);
	TestFalse(TEXT("Broken package is invalid"), Result.IsValid());
	TestTrue(TEXT("Broken package reports multiple errors"), Result.Count(EAEValidationSeverity::Error) >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2ManualAdjustmentTest,
	"AdaptiveEnv.M2.Parameter.ManualAdjustmentPreservesEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify package refresh preserves a documented runtime calibration and its evidence value. */
bool FAEM2ManualAdjustmentTest::RunTest(const FString& Parameters)
{
	// Arrange one explicit runtime calibration after evidence synthesis.
	const AdaptiveEnvM2Tests::FFixture Fixture = AdaptiveEnvM2Tests::BuildValidFixture();
	FAEDerivedParameter& Parameter = Fixture.ParameterPackage->DerivedParameters[0];
	Parameter.bHasManualAdjustment = true;
	Parameter.RuntimeValue = 0.02;
	Parameter.AdjustmentMethod = TEXT("ManualScaleAdjustment");
	Parameter.AdjustmentReason = TEXT("Compensates for compressed ecological time.");
	FString Error;

	// Refresh upstream values and assert calibration cannot overwrite the evidence result.
	TestTrue(TEXT("Adjusted package refresh succeeds"), FAEParameterPackageService::RefreshPackage(*Fixture.ParameterPackage, Error));
	TestTrue(TEXT("Evidence value remains synthesized"), FMath::IsNearlyEqual(Parameter.EvidenceBasedValue, 0.029, 1.0e-9));
	TestTrue(TEXT("Runtime adjustment remains effective"), FMath::IsNearlyEqual(Parameter.RuntimeValue, 0.02, 1.0e-9));
	TestTrue(TEXT("Adjusted package validates"), FAEParameterPackageService::ValidatePackage(*Fixture.EvidenceAsset, *Fixture.ParameterPackage).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2HashAndJsonTest,
	"AdaptiveEnv.M2.Package.HashAndJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify effective changes alter SHA-256 and canonical JSON exports contain the package seal. */
bool FAEM2HashAndJsonTest::RunTest(const FString& Parameters)
{
	// Arrange one valid package and capture its canonical digest.
	const AdaptiveEnvM2Tests::FFixture Fixture = AdaptiveEnvM2Tests::BuildValidFixture();
	const FString OriginalHash = Fixture.ParameterPackage->Metadata.ContentHash;
	TestEqual(TEXT("SHA-256 uses 64 hex characters"), OriginalHash.Len(), 64);

	// Change one effective value through a documented adjustment and refresh the seal.
	FAEDerivedParameter& Parameter = Fixture.ParameterPackage->DerivedParameters[0];
	Parameter.bHasManualAdjustment = true;
	Parameter.RuntimeValue = 0.02;
	Parameter.AdjustmentMethod = TEXT("ManualScaleAdjustment");
	Parameter.AdjustmentReason = TEXT("Hash sensitivity test.");
	FString Error;
	TestTrue(TEXT("Changed package refresh succeeds"), FAEParameterPackageService::RefreshPackage(*Fixture.ParameterPackage, Error));
	TestNotEqual(TEXT("Effective value changes content hash"), Fixture.ParameterPackage->Metadata.ContentHash, OriginalHash);

	// Export audit JSON, verify its seal, and remove the temporary test artifact.
	const FString ExportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/AdaptiveEnv/M2/parameter_package.json"));
	TestTrue(TEXT("JSON export succeeds"), FAEParameterPackageService::ExportPackageJson(*Fixture.ParameterPackage, ExportPath, Error));
	FString Json;
	TestTrue(TEXT("Exported JSON is readable"), FFileHelper::LoadFileToString(Json, *ExportPath));
	TestTrue(TEXT("Export contains content hash"), Json.Contains(Fixture.ParameterPackage->Metadata.ContentHash));
	IFileManager::Get().Delete(*ExportPath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAEM2RuntimeLookupTest,
	"AdaptiveEnv.M2.Runtime.EffectiveParameterLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/* Verify M3 and M4 can resolve one effective value, unit, and version by stable name. */
bool FAEM2RuntimeLookupTest::RunTest(const FString& Parameters)
{
	// Arrange one valid package and query its stable runtime contract.
	const AdaptiveEnvM2Tests::FFixture Fixture = AdaptiveEnvM2Tests::BuildValidFixture();
	double Value = 0.0;
	FString Unit;
	FString Version;

	// Assert successful lookup and explicit failure for an unknown parameter.
	TestTrue(TEXT("Known parameter resolves"), FAEParameterPackageService::TryGetEffectiveParameter(*Fixture.ParameterPackage, TEXT("MovementExposurePerMeter"), Value, Unit, Version));
	TestTrue(TEXT("Runtime value matches synthesis"), FMath::IsNearlyEqual(Value, 0.029, 1.0e-9));
	TestEqual(TEXT("Runtime unit is canonical"), Unit, FString(TEXT("exposure/m")));
	TestEqual(TEXT("Runtime parameter version is stable"), Version, FString(TEXT("1.0.0")));
	TestFalse(TEXT("Unknown parameter fails"), FAEParameterPackageService::TryGetEffectiveParameter(*Fixture.ParameterPackage, TEXT("MissingParameter"), Value, Unit, Version));
	return true;
}

#endif
