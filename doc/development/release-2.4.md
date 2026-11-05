# 2.4 release review

Reviewed 3 November 2026. The Library currently publishes 2.3.11; the release manifest is 2.4.0.

## Checked locally

- Manifest slugs are unique; all module manual paths exist.
- Build, tests, static analysis, macOS ARM64 packaging, and packaged-asset validation pass.
- The previous branch commit's [CI passed](https://github.com/freddyz/computerscare-vcv-modules/actions/runs/41768433935).
- The dev toolbar remains disabled unless `VCVMON_CONTROL` is set.

## Before publishing

- Run CI for the final commit on Windows, Linux, and both Mac architectures.
- Listen through the two new modules at normal and extreme settings. Include Phlooper record/erase/Hold, Blunch staging/sync.
- Merge the release documentation to `master`, which the manifest's manual URLs use.
- Tag the approved commit `v2.4.0`. Check the resulting packages before submitting that exact version and commit in the plugin's existing Library thread.

[Library update instructions](https://github.com/VCVRack/library#pushing-an-update)
