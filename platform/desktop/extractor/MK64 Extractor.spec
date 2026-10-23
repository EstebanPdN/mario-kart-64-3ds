from pathlib import Path
import sys
base = Path(SPECPATH)
engine = base / 'runtime/bin' / ('mk64-extract.exe' if sys.platform == 'win32' else 'mk64-extract')
a = Analysis([str(base / 'app.py')], pathex=[str(base)],
    binaries=[(str(engine), 'runtime/bin')],
    datas=[(str(base / 'runtime/metadata'), 'runtime/metadata'),
           (str(base / 'assets'), 'assets'), (str(base / 'licenses'), 'licenses'),
           (str(base / 'CREDITS.txt'), '.')],
    hiddenimports=[], hookspath=[], hooksconfig={}, runtime_hooks=[],
    excludes=['PySide6.QtWebEngineCore', 'PySide6.QtWebEngineWidgets',
              'PySide6.QtQml', 'PySide6.QtQuick', 'tkinter'])
pyz = PYZ(a.pure)
icon = base / 'assets' / ('icon.icns' if sys.platform == 'darwin' else 'icon.ico')
exe = EXE(pyz, a.scripts, [], exclude_binaries=True, name='MK64 Extractor',
    debug=False, bootloader_ignore_signals=False, strip=False, upx=False,
    console=False, icon=str(icon) if icon.exists() else None)
coll = COLLECT(exe, a.binaries, a.datas, strip=False, upx=False, name='MK64 Extractor')
if sys.platform == 'darwin':
    app = BUNDLE(coll, name='MK64 Extractor.app',
        icon=str(icon) if icon.exists() else None,
        bundle_identifier='io.estebanpdn.mk64-extractor',
        info_plist={'NSHighResolutionCapable': True,
                    'CFBundleShortVersionString': '1.0.0',
                    'LSMinimumSystemVersion': '14.0'})
