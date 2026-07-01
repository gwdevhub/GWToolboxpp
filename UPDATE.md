# How to release

Releases are published automatically: `.github/workflows/cmake.yml` builds and creates a
GitHub release whenever a push lands on `master`, reading the version from `CMakeLists.txt`.

1. Bump `GWTOOLBOXDLL_VERSION` in the root `CMakeLists.txt`. Set `GWTOOLBOXDLL_VERSION_BETA`
   to `""` for a full release, or a label (e.g. `beta1`) to publish a pre-release tag instead.
2. Update `toolboxVersion` in `site/site.config.json` (the docs download-button fallback).
3. Add patch notes under a new `## Version x.x` heading at the top of
   `site/src/content/docs/history.md`.
4. Commit to `dev`, then merge `dev` into `master` and push.

On the `master` push the workflow builds, code-signs the binaries (Certum, if the `CERTUM_*`
secrets are set), tags `x.x_Release` (or `x.x_<beta>`), and creates the release with
`GWToolbox.exe`, `GWToolboxdll.dll` and `GWToolboxdll.pdb.gz` attached. No manual tagging or
release creation is needed; if the tag already exists the release step is skipped.
