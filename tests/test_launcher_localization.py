#!/usr/bin/env python3
"""Launcher translations: verify PT coverage and all active core languages.

This protects against untranslated labels, hardcoded Portuguese, lost HUD
details, and formatted file/status messages that bypass the translation catalog.
"""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rows = json.loads((root/"android/localization/strings.json").read_text())
launcher = (root/"android/scripts/g2/game_app/Launcher.java").read_text()
language = (root/"android/scripts/g2/game_app/AppLanguage.java").read_text()
activity = (root/"android/scripts/g2/game_app/GameActivity.java").read_text()
codes = ("pt","es","fr","de","it","ru","ja","ko","zh","tr","pl")

# Portuguese is never missing from the shared native/Android UI catalog.
assert all(v.get("pt") for v in rows.values()), [
    k for k,v in rows.items() if not v.get("pt")]

core = (
    "Small GPU result readbacks (U2/U3)",
    "Optional: small GPU readbacks may reduce FPS. Enable only if necessary.",
    "Uncharted · action icons",
    "Classic · previous PlayStation buttons",
    "Control size",
    "Edit button position and size",
    "Open the in-game menu by swiping from left to right.",
    "Internal resolution (upscaler off):",
    "Manual resolution is available only with temporal upscaling disabled. FSR 3.1 and Snapdragon GSR 2 set their own input resolution; changes apply on the next game start.",
    "Required for temporal upscale",
    "Native AA",
    "Uncharted game files",
    "Licenses",
    "Download a driver package",
    "Select an installed package to remove it.",
    "Remove the driver package",
    "Import your own Lossless.dll first.",
    "Importing Lossless.dll ...",
    "Lossless.dll imported. Select LSFG to enable frame generation.",
    "Installing the driver package ...",
    "Looking up the driver releases ...",
    "Backing up the current saves failed: nothing was changed.",
    "The game's files are ready.",
    "Backing up failed.",
)
formats = (
    "Saves copied to %s",
    "Preparing %s ...",
    "Preparing failed: %s",
    "DLL import failed: %s",
    "Driver package installed and selected: %s",
    "Installing the package failed: %s",
    "No packages found. %s",
    "Downloading %s ...",
    "Downloading failed: %s",
    "Remove %s?",
    "Removed %s",
    "No backups in %s",
    "Replace the app's saves with %s? The current ones are backed up first.",
    "Saves restored from %s",
    "Restoring failed: %s",
)
for key in (*core,*formats):
    assert key in rows, key
    for code in codes:
        assert rows[key].get(code), (key, code)
        assert rows[key][code] != key, ("untranslated", key, code)
    if key in formats:
        assert key.count("%s") == 1
        assert all(rows[key][c].count("%s")==1 for c in codes), key
        assert 'AppLanguage.format("'+key+'"' in launcher, key

for k in ("Minimal · FPS","Compact · FPS / GPU / CPU / RAM"):
    assert k in rows
    assert all("FPS" in rows[k][code] for code in codes), k

# Selected locale applies consistently to buttons, dialogs, and slider labels.
assert 'option.setText(AppLanguage.text(choice[1]))' in launcher
assert 'AppLanguage.text("Control size")+": "' in launcher
assert 'AppLanguage.text("Opacity")+": "' in launcher
assert 'AppLanguage.text("Required for temporal upscale")' in launcher
assert 'AppLanguage.text("UPSCALE")' in launcher
assert 'new ExpeditionDialog(this).setTitle("Uncharted game files")' in launcher
for item in ("setTitle(CharSequence title)",
             "setMessage(CharSequence message)",
             "setNeutralButton(CharSequence label"):
    assert item in launcher, item
for invalid in ("Tamanho dos controles: ", "Opacidade: ",
                "Classico · botoes", "icones de acoes", "Editar posicao"):
    assert invalid not in launcher, invalid
assert 'static String format(String pattern, Object... values)' in language
assert 'String.format(Locale.ROOT, text(pattern), values)' in language
assert 'source.startsWith("Snapdragon GSR 2 ")' in language

# The translation task must not silently re-expose the two diagnostic switches.
assert 'u3_raycaster_readback' not in launcher
assert 'gpu_shader_loop_safety' not in launcher
assert 'Os.setenv("BB_COPY64_EOP_READBACK","u3".equals(game)?"1":"0",true)' in activity
assert 'Os.setenv("BB_EFFECT_LOOP_CLAMP","1",true)' in activity
assert '"gpu_small_readbacks",false)' in launcher
print("PASS: 12-language launcher messages, 15 formatted statuses, HUD labels and fixed U3/shader defaults")
