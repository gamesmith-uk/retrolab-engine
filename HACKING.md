# Development

### Making a change

1. Create a new branch (or fork the repository, if you don't have access).
2. Implement the change in the new branch/fork.
3. Submit a [Pull Request](https://github.com/gamesmith-uk/retrolab-engine/compare).
   1. If PR is closing an issue (ex. 84), put *"Closes #84"* in the description.
4. Wait for checks to complete (will run automated tests).
5. Merge with master.

### Publishing a new version

1. Integrate all changes to master.
2. Change version number in CMakeLists.txt.
2. Create a new tag from master, with name `'vVERSION'` (ex. `v1.0.1`).
3. Wait for [automated deploy](https://github.com/gamesmith-uk/retrolab-engine/actions?query=workflow%3A%22Build+artifacts%22) to finish.
4. Update version on web (src/compilerVersions.json)