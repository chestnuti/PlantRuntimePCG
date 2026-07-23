import { createHash } from "node:crypto";
import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const outputDirectory = dirname(fileURLToPath(import.meta.url));
const parameterNames = [
  [
    "AdaptiveEnv.M3.Exposure.v3",
    "ae000003-0000-0000-0000-000000000001",
    [
      ["ExposureCollectEventReferenceCount", "count", 2, 0.1, 100],
      ["ExposureCollectEventWeight", "ratio", 0.15, 0, 1],
      ["ExposureCombatEventReferenceCount", "count", 2, 0.1, 100],
      ["ExposureCombatEventWeight", "ratio", 0.15, 0, 1],
      ["ExposureDwellReferenceSeconds", "s", 5, 0.1, 3600],
      ["ExposureDwellWeight", "ratio", 0.15, 0, 1],
      ["ExposureHalfLifeSimulationHours", "h", 2, 0.01, 1000],
      ["ExposureMaximum", "ratio", 1, 0.01, 100],
      ["ExposurePassReferenceCount", "count", 2, 0.1, 100],
      ["ExposurePassWeight", "ratio", 0.2, 0, 1],
      ["ExposureSprintDistanceReferenceMeters", "m", 2, 0.01, 10000],
      ["ExposureSprintWeight", "ratio", 0.15, 0, 1],
      ["ExposureTravelDistanceReferenceMeters", "m", 5, 0.01, 10000],
      ["ExposureTravelDistanceWeight", "ratio", 0.2, 0, 1],
    ],
  ],
  [
    "AdaptiveEnv.M4.EnvironmentConstraint.v2",
    "ae000004-0000-0000-0000-000000000001",
    [
      ["ActiveThreshold", "ratio", 0.25, 0, 1],
      ["HysteresisWidth", "ratio", 0.1, 0, 0.49],
      ["MoistureOptimalMaximumRatio", "ratio", 0.7, 0, 1],
      ["MoistureOptimalMinimumRatio", "ratio", 0.3, 0, 1],
      ["MoistureToleranceWidthRatio", "ratio", 0.2, 0.001, 1],
      ["OverusedThreshold", "ratio", 0.75, 0, 1],
      ["SlopeFullySuitableDegrees", "degree", 10, 0, 89],
      ["SlopeUnsuitableDegrees", "degree", 45, 0.001, 90],
      ["TransitionDebounceSimulationHours", "h", 0.5, 0, 1000],
    ],
  ],
  [
    "AdaptiveEnv.M5.EcologicalResponse.v1",
    "ae000005-0000-0000-0000-000000000001",
    [
      ["ConstraintSensitivity", "ratio", 0.5, 0, 10],
      ["DamageActivationImpact", "ratio", 0.25, 0, 1],
      ["DamageMaximumRatePerSimulationHour", "ratio/h", 0.2, 0, 1],
      ["DamageSaturationImpact", "ratio", 0.75, 0, 1],
      ["RecoveryActivationExposure", "ratio", 0.2, 0, 1],
      ["RecoveryBaseRatePerSimulationHour", "ratio/h", 0.1, 0, 1],
      ["RecoveryDelaySimulationHours", "h", 1, 0, 1000],
    ],
  ],
];

function sha256(text) {
  return createHash("sha256").update(text, "utf8").digest("hex");
}

function ueDouble(value) {
  const precise = value.toPrecision(17);
  if (precise.includes("e")) {
    const [mantissa, exponent] = precise.split("e");
    const normalizedMantissa = mantissa.replace(/(\.\d*?[1-9])0+$|\.0+$/, "$1");
    const sign = exponent.startsWith("-") ? "-" : "+";
    const digits = exponent.replace(/^[+-]/, "").padStart(2, "0");
    return `${normalizedMantissa}e${sign}${digits}`;
  }
  return precise.replace(/(\.\d*?[1-9])0+$|\.0+$/, "$1");
}

function quote(value) {
  return JSON.stringify(value);
}

function canonicalParameter(parameter) {
  return `{"ParameterId":${quote(parameter.ParameterId)},"Name":${quote(parameter.Name)},"Unit":${quote(parameter.Unit)},` +
    `"EvidenceBasedValue":${ueDouble(parameter.EvidenceBasedValue)},"EffectiveValue":${ueDouble(parameter.EffectiveValue)},` +
    `"PlausibleMinimum":${ueDouble(parameter.PlausibleMinimum)},"PlausibleMaximum":${ueDouble(parameter.PlausibleMaximum)},` +
    `"ParameterVersion":${quote(parameter.ParameterVersion)},"OriginLayer":${quote(parameter.OriginLayer)}}`;
}

function canonicalBlock(block, includeHash) {
  const parameters = block.Parameters.map(canonicalParameter).join(",");
  const hashField = includeHash ? `,"BlockHash":${quote(block.BlockHash)}` : "";
  return `{"BlockId":${quote(block.BlockId)},"ModelContract":${quote(block.ModelContract)},` +
    `"BlockVersion":${quote(block.BlockVersion)},"Parameters":[${parameters}]${hashField}}`;
}

function canonicalBundle(bundle, includeHash) {
  const blocks = bundle.Blocks.map((block) => canonicalBlock(block, true)).join(",");
  const hashField = includeHash ? `,"ContentHash":${quote(bundle.ContentHash)}` : "";
  return `{"Format":${quote(bundle.Format)},"SchemaVersion":${bundle.SchemaVersion},"BundleId":${quote(bundle.BundleId)},` +
    `"SemanticVersion":${quote(bundle.SemanticVersion)},"ParentContentHash":${quote(bundle.ParentContentHash)},` +
    `"SourceAuditHash":${quote(bundle.SourceAuditHash)},"GeneratorVersion":${quote(bundle.GeneratorVersion)},` +
    `"Blocks":[${blocks}]${hashField}}`;
}

function parameterGuid(blockIndex, parameterIndex) {
  const suffix = String(parameterIndex + 1).padStart(12, "0");
  return `ae00000${blockIndex + 3}-0000-0000-0000-${suffix}`;
}

function buildParameter(blockIndex, parameterIndex, definition) {
  const [name, unit, value, minimum, maximum] = definition;
  return {
    ParameterId: parameterGuid(blockIndex, parameterIndex),
    Name: name,
    Unit: unit,
    EvidenceBasedValue: value,
    EffectiveValue: value,
    PlausibleMinimum: minimum,
    PlausibleMaximum: maximum,
    ParameterVersion: "1.0.0",
    OriginLayer: "ExperimentOverride",
  };
}

function buildBlock(definition, blockIndex) {
  const [modelContract, blockId, parameters] = definition;
  const blockWithoutHash = {
    BlockId: blockId,
    ModelContract: modelContract,
    BlockVersion: "1.0.0",
    Parameters: parameters.map((parameter, index) => buildParameter(blockIndex, index, parameter)),
  };
  return { ...blockWithoutHash, BlockHash: sha256(canonicalBlock(blockWithoutHash, false)) };
}

const audit = {
  Format: "AdaptiveEnv.TestParameterAudit",
  SchemaVersion: 1,
  AuditId: "ae000002-0000-0000-0000-000000000001",
  Purpose: "Deterministic M3-M5 runtime integration and visualization testing only.",
  ProvenanceClassification: "ExperimentOverride",
  LiteratureClaim: false,
  ParameterSelectionMethod: "Values adapted from current AdaptiveEnv automation-test fixtures and valid contract interiors.",
  ModelContracts: parameterNames.map(([contract]) => contract),
  KnownLimitations: [
    "Values are not literature-derived and must not be used as thesis evidence.",
    "Plausible bounds are test guards rather than ecological confidence intervals.",
    "Calibration against gameplay telemetry and ecological observations remains required.",
  ],
};
const auditJson = JSON.stringify(audit);
const sourceAuditHash = sha256(auditJson);

const bundleWithoutContentHash = {
  Format: "AdaptiveEnv.ParameterBundle",
  SchemaVersion: 2,
  BundleId: "ae000002-0000-0000-0000-000000000001",
  SemanticVersion: "1.0.0",
  ParentContentHash: "",
  SourceAuditHash: sourceAuditHash,
  GeneratorVersion: "adaptive-env-test-bundle-generator/1.0.0",
  Blocks: parameterNames.map(buildBlock),
};
const bundle = {
  ...bundleWithoutContentHash,
  ContentHash: sha256(canonicalBundle(bundleWithoutContentHash, false)),
};

writeFileSync(join(outputDirectory, "AdaptiveEnv_M3M4M5_Test_v1.audit.json"), auditJson, "utf8");
writeFileSync(join(outputDirectory, "AdaptiveEnv_M3M4M5_Test_v1.aeparams.json"), canonicalBundle(bundle, true), "utf8");

console.log(JSON.stringify({
  bundle: "AdaptiveEnv_M3M4M5_Test_v1.aeparams.json",
  audit: "AdaptiveEnv_M3M4M5_Test_v1.audit.json",
  sourceAuditHash,
  contentHash: bundle.ContentHash,
  blockHashes: bundle.Blocks.map(({ ModelContract, BlockHash }) => ({ ModelContract, BlockHash })),
}, null, 2));
