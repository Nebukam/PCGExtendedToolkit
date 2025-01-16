# Contributing
PCG Extended Toolkit (PCGEx) is a multiplatform plugin that is expected to work on `Linux`, `macOS` and `windows` at the very least. It is regularily packaged and pushed to FAB, and must pass certain validations to be published.  
At the time of writing, Unreal `5.3`, `5.4` and `5.5` are officially supported; hence, while making a PR please ensure that:
- The plugin compiles for the editor in those 3 versions
- The plugin can be packaged without error or warnings in a project for those 3 versions
- The plugin can be packaged *as a plugin* (this is different from packaging a project) for those 3 versions

Additionally, any contribution should not include or rely on any other third parties libraries to be integrated into the codebase. It's complex enough as is.
