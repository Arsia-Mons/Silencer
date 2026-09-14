# mockups/ - artist reference scenes

Non-runtime art studies. Keep each level in its own named subfolder; never
modify the source `.SIL` or runtime assets when generating a mockup.

The interdimensional-base study uses Python for Blender automation and
Pillow for its source-reference image, not the game's build pipeline.
See `interdimensional-base/README.md` for the regeneration commands.
`sil_reference.py` shares the source decoder and tile compositor between the
base and level references. Run `python3 mockups/generate_level_references.py`
from the repo root to regenerate `levels/`; its non-recursive `*.SIL` glob
intentionally excludes `shared/assets/level/community/`.
Source images omit every tile with a nonzero per-cell luminance byte:
these are light masks, not opaque artwork. Keep non-light tiles on all layers;
do not hide entire layers or banks to turn lighting off.

Preserve source collision coordinates and actor anchors. Keep invented
depth, materials, props and presentation in separate Blender collections,
and document them as interpretation rather than recovered source data.

Commit the editable `.blend`, portable `.glb`, renders and generating scripts
together. Do not commit Blender backups or temporary renders.
Source-only level references have PNG pairs and a source manifest, not 3D scenes.
