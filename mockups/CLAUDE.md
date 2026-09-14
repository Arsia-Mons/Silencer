# mockups/ - artist reference scenes

Non-runtime art studies. Keep each level in its own named subfolder; never
modify the source `.SIL` or runtime assets when generating a mockup.

The interdimensional-base study uses Python for Blender automation and
Pillow for its source-reference image, not the game's build pipeline.
See `interdimensional-base/README.md` for the regeneration commands.

Preserve source collision coordinates and actor anchors. Keep invented
depth, materials, props and presentation in separate Blender collections,
and document them as interpretation rather than recovered source data.

Commit the editable `.blend`, portable `.glb`, renders and generating scripts
together. Do not commit Blender backups or temporary renders.
