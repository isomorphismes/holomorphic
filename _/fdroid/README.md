# F-Droid release path

The Android project can produce the unsigned release APK F-Droid expects without Play signing secrets or the public Lasso development signing input.

## Upstream release contract

1. Keep the default Android `versionCode` and `versionName` in `android/app/build.gradle.kts` equal to the tagged public release. Store-specific environment overrides are not used by the F-Droid build.
2. Run the `F-Droid release build` workflow. It removes the repository's Lasso development signing input before building, builds `assembleRelease`, checks the package/version and three native ABIs, and retains the unsigned APK as evidence.
3. Keep the repository license and F-Droid metadata aligned on `GPL-3.0-or-later`. `THIRD_PARTY.md` records dependencies and referenced material that are not relicensed by that grant.
4. Tag the exact release commit `v<versionName>`.
5. Replace `FULL_COMMIT_HASH` in `org.isomorphisms.analyticcontinuation.yml.template` with the full hash of that tagged commit.
6. Copy the template to `fdroiddata/metadata/org.isomorphisms.analyticcontinuation.yml`, run `fdroid lint org.isomorphisms.analyticcontinuation`, then submit the fdroiddata merge request.

F-Droid rebuilds the app from source and applies its own signing key. The upstream unsigned release APK is a build gate, not the artifact submitted to F-Droid.

After first inclusion, `UpdateCheckMode: Tags` and `AutoUpdateMode: Version` allow F-Droid to discover later release tags automatically.
