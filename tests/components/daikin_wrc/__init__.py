from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness walks the component manifest, not the climate platform.
    manifest.auto_load = ["climate_ir", "climate"]
    manifest.dependencies = manifest.dependencies + ["binary_sensor"]
