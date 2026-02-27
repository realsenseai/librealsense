# CI Status and Historical Incidents

This document tracks notable CI failures and infrastructure incidents for audit and historical reference purposes.

---

## 2026-04-18: Nightly Build 13972 - Infrastructure Failure

**Build Number:** 13972  
**Date:** 2026-04-18  
**Build Type:** CI Nightly  
**Status:** Failed (Infrastructure)

### Summary
CI Nightly build 13972 failed due to infrastructure outage. **No code or test failures occurred.** The failure was entirely due to infrastructure unavailability.

### Root Cause
Jetson build node `vtg-librs-jetson01.iil.intel.com` was offline and unreachable during the build window.

### Affected Platforms
- Jetson platform builds (all Jetson-related build targets)

### Impact
- Build jobs targeting Jetson platform could not execute
- No code quality, compilation, or test issues identified
- Other platform builds unaffected

### Resolution
Infrastructure issue - Jetson node outage. No code changes required.

### Notes
This incident is documented for audit trail purposes. The build failure was purely infrastructure-related and does not reflect any issues with the codebase, build scripts, or test suites at this point in time.

---

