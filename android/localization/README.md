# Android UI languages

The launcher, PKG installer and native in-game menu share `strings.json`.
English is the fallback for unknown technical diagnostics. Automatic selects the
system language when supported, otherwise English. Locale selection is stored in
launcher preferences; launcher recreation does not restart the native game.

Supported languages match the inFAMOUS port: English, Portuguese, Spanish, French,
German, Italian, Russian, Japanese, Korean, Simplified Chinese, Turkish and Polish.
This controls port UI text; it does not add game audio or subtitle packs.

Common action translations were adapted from the GPL inFAMOUS Daedalus Android
`AppLanguage`, `LanguageCatalog` and `LanguageCatalogTrPl` catalogs, with additional
Uncharted UI translations. Generate the native header after changing the catalog:

    python3 android/localization/generate.py

Android's installed CJK fallback fonts are merged into the dynamic native font
atlas. Missing translations retain the original English diagnostic.
