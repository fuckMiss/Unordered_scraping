import os
import subprocess
import sys
from pathlib import Path


def runtime_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2]


def desktop_dir() -> Path:
    one_drive = os.environ.get("OneDrive")
    candidates = []
    if one_drive:
        candidates.extend(
            [
                Path(one_drive) / "Desktop",
                Path(one_drive) / "桌面",
            ]
        )
    user_profile = os.environ.get("USERPROFILE")
    if user_profile:
        candidates.extend(
            [
                Path(user_profile) / "Desktop",
                Path(user_profile) / "桌面",
            ]
        )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    if user_profile:
        path = Path(user_profile) / "Desktop"
        path.mkdir(parents=True, exist_ok=True)
        return path
    raise RuntimeError("Could not locate Desktop folder.")


def remove_old_shortcuts(name: str) -> None:
    desktop = desktop_dir()
    for suffix in (".lnk", ".url"):
        path = desktop / f"{name}{suffix}"
        if path.exists():
            path.unlink()


def write_lnk_shortcut(name: str, target: Path, icon: Path, work_dir: Path) -> None:
    if not target.exists():
        raise FileNotFoundError(str(target))
    shortcut_path = desktop_dir() / f"{name}.lnk"
    wscript = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "wscript.exe"
    icon_file = icon if icon.exists() else target
    ps_script = "\n".join(
        [
            "$ErrorActionPreference = 'Stop'",
            "$shell = New-Object -ComObject WScript.Shell",
            "$shortcut = $shell.CreateShortcut($env:TANKEYE_SHORTCUT_PATH)",
            "$shortcut.TargetPath = $env:TANKEYE_WSCRIPT_EXE",
            "$shortcut.Arguments = '\"' + $env:TANKEYE_LAUNCHER + '\"'",
            "$shortcut.WorkingDirectory = $env:TANKEYE_WORK_DIR",
            "$shortcut.IconLocation = $env:TANKEYE_ICON",
            "$shortcut.Description = 'Start TankEye-Iris'",
            "$shortcut.Save()",
        ]
    )
    env = os.environ.copy()
    env["TANKEYE_SHORTCUT_PATH"] = str(shortcut_path.resolve())
    env["TANKEYE_WSCRIPT_EXE"] = str(wscript.resolve())
    env["TANKEYE_LAUNCHER"] = str(target.resolve())
    env["TANKEYE_WORK_DIR"] = str(work_dir.resolve())
    env["TANKEYE_ICON"] = str(icon_file.resolve())
    subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            ps_script,
        ],
        check=True,
        env=env,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )


def main() -> int:
    root = runtime_dir()
    icon = root / "app" / "qt" / "assets" / "app_icon.ico"
    shortcut_name = "TankEye-Iris"
    target = root / "launch_tankeye_main_only.vbs"

    remove_old_shortcuts(shortcut_name)
    write_lnk_shortcut(shortcut_name, target, icon, root)
    message = f"已创建桌面图标：\n- {shortcut_name}"
    try:
        import tkinter as tk
        from tkinter import messagebox

        tk_root = tk.Tk()
        tk_root.withdraw()
        messagebox.showinfo("TankEye-Iris", message)
        tk_root.destroy()
    except Exception:
        print(message)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
