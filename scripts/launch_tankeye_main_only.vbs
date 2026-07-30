Option Explicit

Dim fso, shell, appDir, launchScript, command

Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")

appDir = fso.GetParentFolderName(WScript.ScriptFullName)
launchScript = fso.BuildPath(appDir, "launch_tankeye.ps1")

command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File " & _
          """" & launchScript & """ -BuildDir build -Configuration Release -WindowMode Maximized"

shell.CurrentDirectory = appDir
shell.Run command, 0, False
