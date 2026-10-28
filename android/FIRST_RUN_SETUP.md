# First-run setup flow — 0.2.5

Follow inFAMOUS's Bionic launcher and GoW3's storage compatibility model:

1. An unconfigured launch requests READ/WRITE external storage using the system
   Allow / Don't allow dialog inside the app, instead of opening All files settings.
2. Once granted, offer Set up Uncharted with Install PKG, Select folder and Not now.
3. Install PKG continues the direct installer; Select folder continues the picker.
   Existing valid game folders are preserved and skip setup.
4. A denial offers an explicit retry through Android settings; Not now cancels the
   continuation. Returning from settings consumes a pending action at most once.
   Configuration changes recreate a visible setup/settings offer in the app theme.

The APK's compatibility target changes to 28, matching inFAMOUS/GoW3, with legacy
read/write permissions declared. The native API minimum stays 33. This does not
claim new support for older Android versions or change the CMake/NDK target.
All storage gates use one access policy: granted all-files access, or granted
legacy read and write permissions for a target below 30. Readback, shader,
performance, signed package identity and app version 0.2.5 remain unchanged.

Tests cover grant, refusal, explicit settings retry, continuation of each action,
cancel, state restoration and repeated callbacks. Android CI compiles the actual
launcher and verifies the APK's target SDK and WRITE permission as well as the
existing package/version/signature checks. Device UI verification remains required.
