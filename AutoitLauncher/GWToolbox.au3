#RequireAdmin

#AutoIt3Wrapper_Icon=..\resources\gwtoolbox.ico

#include <File.au3>
#include <ComboConstants.au3>
#include <FileConstants.au3>
#include <MsgBoxConstants.au3>
#include <GUIConstantsEx.au3>
#include <GuiComboBox.au3>
#include <WinAPIProc.au3>
#include <WinAPISys.au3>
#include <InetConstants.au3>
#include <GUIConstantsEx.au3>
#include <ProgressConstants.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>

; dx runtime: http://www.microsoft.com/en-us/download/details.aspx?id=8109

Opt("MustDeclareVars", True)
Opt("GuiResizeMode", BitOR($GUI_DOCKSIZE, $GUI_DOCKTOP, $GUI_DOCKLEFT))

; ==== Globals ====
Global Const $folder = @LocalAppDataDir & "\GWToolboxpp\"
Global $dllpath = $folder & "GWToolbox.dll"
Global Const $inipath = $folder & "GWToolbox.ini"
Global Const $fontpath = $folder & "Font.ttf"

Global $mKernelHandle, $mGWProcHandle, $mCharname
Global $gui = 0, $label = 0, $progress = 0, $changelabel = 0, $height = 0

; ==== Disclaimer ====
If Not (FileExists($dllpath) Or FileExists($inipath) Or FileExists($fontpath)) Then
	If MsgBox($MB_OKCANCEL, "GWToolbox++", 'DISCLAIMER:'&@CRLF& _
	'THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF ' & _
	'FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY ' & _
	'DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.' & @CRLF&@CRLF & _
	'By clicking the OK button you agree with all the above.') <> $IDOK Then Exit
EndIf

; ==== Install Visual Studio C++ 2015 Runtime ====
Func CheckVCRedist()
	Local $Key
	If @CPUArch == "X64" Then
		$Key = RegRead("HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86", "Installed")
	Else
		$Key = RegRead("HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86", "Installed")
	EndIf
	If $Key == 1 Then Return False; Already installed

	If MsgBox($MB_YESNO, "GWToolbox++ Launcher", "You do not have The Visual C++ Runtime (vc_redist.x86) required for Toolbox to run. Would you like me to open where to download it?") == $IDYES Then
		ShellExecute('https://www.microsoft.com/en-us/download/details.aspx?id=48145')
	EndIf

	Return True
EndFunc

; ==== Install DirectX 9.0c Runtime ====
Func CheckDirectX()
	Local $Key
	If @CPUArch == "X64" Then
		$Key = RegRead("HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\DirectX", "Version")
	Else
		$Key = RegRead("HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\DirectX", "Version")
	EndIf
	If $Key == '4.09.00.0904' Then Return False; Already installed

	If MsgBox($MB_YESNO, "GWToolbox++ Launcher", "You do not have The DirectX 9.0c Runtime required for Toolbox to run. Would you like me to open where to download it?") == $IDYES Then
		ShellExecute('https://www.microsoft.com/en-us/download/details.aspx?id=35')
	EndIf

	Return True
EndFunc

If CheckVCRedist() Or CheckDirectX() Then
	MsgBox(0, "GWToolbox++", "Please install the runtimes you were prompted to install, then run this launcher again.")
	Exit
EndIf


; ==== Create directories ====
Out("Creating directories and reading .ini... ", false)
If Not FileExists($folder) Then DirCreate($folder)
If Not FileExists($folder) Then Exit MsgBox($MB_ICONERROR, "GWToolbox++ Launcher Error", "GWToolbox++ Launcher was unable to create the folder '" & $folder & "'." & @CRLF & _
	"Make sure you have admin rights and your antivirus is not blocking GWToolbox++. Quitting.")

; ==== Install default files if needed ====
If Not FileExists($inipath) Then FileInstall("..\resources\GWToolbox.ini", $inipath, $FC_NOOVERWRITE)
If Not FileExists($fontpath) Then FileInstall("..\resources\Font.ttf", $fontpath, $FC_NOOVERWRITE)

; ==== Update information from ini ====
Global Const $updatemode = IniRead($inipath, "Updater", "update_mode", "2")
$dllpath = IniRead($inipath, "Updater", "dllpath", $dllpath)
Out("done.")

; ==== Update mode ====
Global $check_remote_version = False
Global $notify_update = False
Global $do_update = False
Switch $updatemode
	Case 0 ; do nothing
		ConsoleWrite("Do Nothing")
		$check_remote_version = False
		$notify_update = False
		$do_update = False
	Case 1 ; check and warn
		$check_remote_version = True
		$notify_update = True
		$do_update = False
	Case 2 ; check and ask
		$check_remote_version = True
		$notify_update = True
		$do_update = False ; unless user specifies otherwise
	Case 3 ; check and do
		$check_remote_version = True
		$notify_update = False
		$do_update = True
EndSwitch

Out("update mode: "&$updatemode)
Out("check remote: "&$check_remote_version)
Out("notify update: "&$notify_update)
Out("dll path: "&$dllpath)

; ==== Version information ====
Global Const $localversion = IniRead($inipath, "Updater", "dllversion", "0")
Out("dll version: "&$localversion)
Global $remoteversion = "0"

Out("Checking remote version... ", False)
CheckRemoteVersion()
Func CheckRemoteVersion()
	If Not $check_remote_version Then Return
	Local $data = InetRead("https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/toolboxversion.txt", $INET_FORCERELOAD)
	If @error Then
		MsgBox($MB_ICONERROR, "GWToolbox++ Error", _
			"I am unable to read the toolbox version from the remote server (cannot download)." & @CRLF & _
			"Either the server is offline or some software is blocking the download." & @CRLF & _
			"I will try to launch the local version, but it might be not up-to-date.")
		$do_update = False
		$notify_update = False
		Return
	EndIf

	$remoteversion = BinaryToString($data)
	If @error Then
		MsgBox($MB_ICONERROR, "GWToolbox++ Error", _
			"I am unable to read the toolbox version from the remote server (corrupt file)." & @CRLF & _
			"Either the server is offline or some software is blocking the download." & @CRLF & _
			"I will try to launch the local version, but it might be not up-to-date.")
		$do_update = False
		$notify_update = False
		Return
	EndIf
EndFunc
Out("done: "&$remoteversion)

NotifyUpdate()
Func NotifyUpdate()
	If Not $check_remote_version Then Return
	If Not $notify_update Then Return
	If $remoteversion == $localversion Then Return
	Out("Displaying changelog.")

	; todo: download and display changelog.

	; if $updatemode==1, that's it
	; if $updatemode==2, ask, and if uses says yes, then set $do_update = True
	; if $updatemode==3, start the download

	If $updatemode == 3 Then $do_update = True
EndFunc
Out("do_update: "&$do_update)

Download()
Func Download()
	If Not $do_update Then Return
	If $remoteversion == $localversion Then Return

	Local Const $sUrl = "https://github.com/HasKha/GWToolboxpp/releases/download/" & $remoteversion & "_Release/GWToolbox.dll"
	Out("Downloading '"&$sUrl&"'")

	Local Const $hDownload = InetGet($sUrl, $dllpath, BitOR($INET_FORCERELOAD, $INET_FORCEBYPASS), $INET_DOWNLOADBACKGROUND)
	Do
		Sleep(100)
	Until InetGetInfo($hDownload, $INET_DOWNLOADCOMPLETE)
	Sleep(100)
	Local $BytesSize = InetGetInfo($hDownload, $INET_DOWNLOADREAD)
	Local $FileSize = FileGetSize($dllpath)
	InetClose($hDownload)

	Out("Download completed. Bytes downloaded: "&$BytesSize)
	Out("File size on disk: "&$FileSize)

	If Not FileExists($dllpath) Then
		Error("Error downloading GWToolbox.dll")
	ElseIf $FileSize == 0 Then
		Error("Downloaded file is empty on disk. This usually indicates the download failed, was corrupted, or blocked."&@CRLF&"Please manually download and replace the GWToolbox.dll in " & $dllpath)
	ElseIf $BytesSize <> $FileSize Then
		Warning("Download size ("&$BytesSize / 1024&" KB) and file size ("&$FileSize / 1024&" KB) mismatch."&@CRLF&"This usually indicates the download failed or was corrupted."&@CRLF&"If you have issues, please manually download and replace the GWToolbox.dll in " & $dllpath)
	EndIf
EndFunc

; === Last checks before using the dll ===
; this is in case the user moved the gwtoolbox.dll to another location, for example, while playing around with gwlauncher plugins, or simply renaming gwtoolbox.dll to whydopeopledothis.dll
If Not FileExists($dllpath) Then
	Local Const $msgbutton = MsgBox(BitOR($MB_ICONWARNING, $MB_OKCANCEL), "GWToolbox++", "Error: Cannot find GWToolbox.dll in "&$dllpath&"."&@CRLF&"Please help me find GWToolbox.dll")
	If ($msgbutton <> $IDOK) Then Exit
	$dllpath = FileOpenDialog("GWToolbox++: Please locate GWToolbox.dll", $folder, "Dll (*.dll)", $FD_FILEMUSTEXIST + $FD_PATHMUSTEXIST)
	If @error Then Exit
EndIf

; again?!?
If Not FileExists($dllpath) Then Error("Cannot find GWToolbox dll in "&$dllpath)

If FileGetSize($dllpath) == 0 Then Error("GWToolbox dll in "&$dllpath&" is an empty file!")

; === Client selection ===
Global $WinList = WinList("[CLASS:ArenaNet_Dx_Window_Class]")
Global $gwPID
Switch $WinList[0][0]
	Case 0
		Error("Guild Wars not running")
	Case 1
		$gwPID = WinGetProcess($WinList[1][1])
	Case Else
		Local $char_names[$WinList[0][0]]
		Local $lFirstChar
		Local $lComboStr
		For $i = 1 To $WinList[0][0]
			Local $lPID = WinGetProcess($WinList[$i][1])
			MemoryOpen($lPID)
			If $i = 1 Then $lFirstChar = ScanForCharname()
			$char_names[$i - 1] = MemoryRead($mCharname, 'wchar[30]')
			MemoryClose()

			$lComboStr &= $char_names[$i - 1]
			If $i <> $WinList[0][0] Then $lComboStr &= '|'
		Next
		Local Const $selection_gui = GUICreate("GWToolbox++", 150, 60)
		Local Const $comboCharname = GUICtrlCreateCombo("", 6, 6, 138, 25, $CBS_DROPDOWNLIST)
		GUICtrlSetData(-1, $lComboStr, $lFirstChar)
		Local Const $button = GUICtrlCreateButton("Launch", 6, 31, 138, 24)
		GUISetState(@SW_SHOW, $selection_gui)
		While True
			Switch GUIGetMsg()
				Case $button
					ExitLoop
				Case $GUI_EVENT_CLOSE
					Exit
			EndSwitch
			Sleep(10)
		WEnd

		Local Const $index = _GUICtrlComboBox_GetCurSel($comboCharname)
		$gwPID = WinGetProcess($WinList[$index + 1][1])
EndSwitch

If Not ProcessExists($gwPID) Then Error("Cannot find Guild Wars process")

; === Check for DEP (Data Execution prevention) ===
Local $dep_status = _WinAPI_GetSystemDEPPolicy()
if ($dep_status == 1) Then Error("You have DEP (Data Execution Prevention) enabled for all applications, GWToolbox will not work unless you disable it or whitelist gw.exe")

; === Injection ===
Local $Kernel32 = DllOpen("kernel32.dll")
If $Kernel32 == -1 Then Error("Failed to open kernel32.dll")

Local $DLL_Path = DllStructCreate("char[255]")
Local $getfullpath_ret = DllCall($Kernel32, "DWORD", "GetFullPathNameA", "str", $dllpath, "DWORD", 255, "ptr", DllStructGetPtr($DLL_Path), "int", 0)
If @error Then Error1("Failed to DllCall GetFullPathNameA", @error)
If $getfullpath_ret == 0 Then Error2("Failed to call 'GetFullPathNameA'", _WinAPI_GetLastError(), $getfullpath_ret)
Local $sDLLFullPath = DllStructGetData($DLL_Path,1)

Local $modules = _WinAPI_EnumProcessModules($gwPID)
If @error Then Error2("Failed to call 'EnumProcessModules' (1)", @error, $modules)
For $i = 1 To $modules[0][0]
	If $modules[$i][1] == $dllpath Then Error("GWToolbox++ already in specified process")
Next

Local $hProcess = DllCall($Kernel32, "DWORD", "OpenProcess", "DWORD", 0x1F0FFF, "int", 0, "DWORD", $gwPID)[0]
If @error Then Error1("Failed to DllCall OpenProcess", @error)
If $hProcess == 0 Then Error2("OpenProcess failed", _WinAPI_GetLastError(), $hProcess)

Local $hModule = DllCall($Kernel32, "DWORD", "GetModuleHandleA", "str", "kernel32.dll")[0]
If @error Then Error1("Failed to DllCall GetModuleHandleA", @error)
If $hModule == 0 Then Error2("GetModuleHandle failed", _WinAPI_GetLastError(), $hModule)

Local $lpStartAddress = DllCall($Kernel32, "DWORD", "GetProcAddress", "DWORD", $hModule, "str", "LoadLibraryA")[0]
If @error Then Error1("Failed to DllCall GetProcAddress", @error)
If $lpStartAddress == 0 Then Error2("GetProcAddress failed", _WinAPI_GetLastError(), $lpStartAddress)

Local $lpParameter = DllCall($Kernel32, "DWORD", "VirtualAllocEx", "int", $hProcess, "int", 0, "ULONG_PTR", DllStructGetSize($DLL_Path), "DWORD", 0x3000, "int", 4)[0]
If @error Then Error1("Failed to DllCall VirtualAllocEx", @error)
If $lpParameter == 0 Then Error2("VirtualAllocEx failed", _WinAPI_GetLastError(), $lpParameter)

Local $writeProcMem_ret = DllCall($Kernel32, "BOOL", "WriteProcessMemory", "int", $hProcess, "DWORD", $lpParameter, "str", $sDLLFullPath, "ULONG_PTR", DllStructGetSize($DLL_Path), "int", 0)[0]
If @error Then Error1("Failed to DllCall WriteProcessMemory", @error)
If $writeProcMem_ret == 0 Then Error2("WriteProcessMemory failed", _WinAPI_GetLastError(), $writeProcMem_ret)

Local $hThread = DllCall($Kernel32, "int", "CreateRemoteThread", "DWORD", $hProcess, "int", 0, "int", 0, "DWORD", $lpStartAddress, "DWORD", $lpParameter, "int", 0, "int", 0)[0]
If @error Then Error1("Failed to DllCall CreateRemoteThread", @error)
If $hThread == 0 Then Error2("CreateRemoteThread failed", _WinAPI_GetLastError(), $hThread)

Local $nWaitEvent = DllCall($Kernel32, "DWORD", "WaitForSingleObject", "HANDLE", $hThread, "DWORD", 1000)[0]
If @error Then Error1("Failed to DllCall WaitForSingleObject", @error)
If $nWaitEvent <> 0 Then Error2("WaitForSingleObject timed out", _WinAPI_GetLastError(), Hex($nWaitEvent))

Local $nExitCode = 0
Local $hTBModule = DllCall($Kernel32, "HANDLE", "GetExitCodeThread", "HANDLE", $hThread, "DWORD*", $nExitCode)[0]
If @error Then Error1("Failed to DllCall GetExitCodeThread", @error)
If $hTBModule == 0 Then Error2("GetExitCodeThread failed", _WinAPI_GetLastError(), $nExitCode)

Local $closehandle_ret = DllCall($Kernel32, "BOOL", "CloseHandle", "DWORD", $hProcess)[0]
If @error Then Error1("Failed to DllCall CloseHandle", @error)
If $closehandle_ret == 0 Then Error2("CloseHandle failed", _WinAPI_GetLastError(), $closehandle_ret)

Local $found = False
Local $deadlock = TimerInit()
While Not $found And TimerDiff($deadlock) < 4000

	$modules = _WinAPI_EnumProcessModules($gwPID)
	If @error Then Error2("Failed to call 'EnumProcessModules' (2)", @error, $modules)

	For $i = 1 To $modules[0][0]
		If $modules[$i][1] == $dllpath Then
			$found = True
			ExitLoop
		EndIf
	Next
	Sleep(200)
WEnd

If Not $found Then
	If ($dep_status == 3) Then
		Error("GWToolbox.dll was not loaded, unknown error" & @CRLF & "Note: you have DEP (Data Execution Prevention) enabled, make sure to whitelist gw.exe")
	Else
		Error("GWToolbox.dll was not loaded, unknown error")
	EndIf
EndIf

If $gui > 0 Then
	GUICtrlCreateLabel("You can close me, but please read me first!", 8, $height - 25, 284, 17, $SS_CENTER, $GUI_WS_EX_PARENTDRAG)

	While GUIGetMsg() <> $GUI_EVENT_CLOSE
		Sleep(0)
	WEnd
EndIf

#Region error message boxes
Func Out($msg, $enter_newline = True)
	ConsoleWrite($msg)
	If $enter_newline Then ConsoleWrite(@CRLF)
EndFunc
Func Warning($msg)
	MsgBox($MB_ICONWARNING, "GWToolbox++", "Warning: " & $msg)
EndFunc
Func Error($msg)
	MsgBox($MB_ICONERROR, "GWToolbox++", "Error: " & $msg)
	Exit
EndFunc
Func Error1($msg, $err)
	MsgBox($MB_ICONERROR, "GWToolbox++", "Error: " & $msg & @CRLF & "Error code: " & $err)
	Exit
EndFunc
Func Error2($msg, $err, $ret)
	MsgBox($MB_ICONERROR, "GWToolbox++","Error: " & $msg & @CRLF & "Error code: " & $err & @CRLF & "Return value: " & $ret)
	Exit
EndFunc
#EndRegion

#Region memory managment and char name read
Func MemoryOpen($aPID)
	$mKernelHandle = DllOpen('kernel32.dll')
	Local $lOpenProcess = DllCall($mKernelHandle, 'int', 'OpenProcess', 'int', 0x1F0FFF, 'int', 1, 'int', $aPID)
	$mGWProcHandle = $lOpenProcess[0]
EndFunc   ;==>MemoryOpen

;~ Description: Internal use only.
Func MemoryClose()
	DllCall($mKernelHandle, 'int', 'CloseHandle', 'int', $mGWProcHandle)
EndFunc   ;==>MemoryClose

;~ Description: Internal use only.
Func MemoryRead($aAddress, $aType = 'dword')
	Local $lBuffer = DllStructCreate($aType)
	DllCall($mKernelHandle, 'int', 'ReadProcessMemory', 'int', $mGWProcHandle, 'int', $aAddress, 'ptr', DllStructGetPtr($lBuffer), 'int', DllStructGetSize($lBuffer), 'int', '')
	Return DllStructGetData($lBuffer, 1)
EndFunc   ;==>MemoryRead

Func ScanForCharname()
	Local $lCharNameCode = BinaryToString('0x90909066C705')
	Local $lCurrentSearchAddress = 0x00401000
	Local $lMBI[7], $lMBIBuffer = DllStructCreate('dword;dword;dword;dword;dword;dword;dword')
	Local $lSearch, $lTmpMemData, $lTmpAddress, $lTmpBuffer = DllStructCreate('ptr'), $i

	While $lCurrentSearchAddress < 0x00900000
		Local $lMBI[7]
		DllCall($mKernelHandle, 'int', 'VirtualQueryEx', 'int', $mGWProcHandle, 'int', $lCurrentSearchAddress, 'ptr', DllStructGetPtr($lMBIBuffer), 'int', DllStructGetSize($lMBIBuffer))
		For $i = 0 To 6
			$lMBI[$i] = StringStripWS(DllStructGetData($lMBIBuffer, ($i + 1)), 3)
		Next

		If $lMBI[4] = 4096 Then
			Local $lBuffer = DllStructCreate('byte[' & $lMBI[3] & ']')
			DllCall($mKernelHandle, 'int', 'ReadProcessMemory', 'int', $mGWProcHandle, 'int', $lCurrentSearchAddress, 'ptr', DllStructGetPtr($lBuffer), 'int', DllStructGetSize($lBuffer), 'int', '')

			$lTmpMemData = DllStructGetData($lBuffer, 1)
			$lTmpMemData = BinaryToString($lTmpMemData)

			$lSearch = StringInStr($lTmpMemData, $lCharNameCode, 2)
			If $lSearch > 0 Then
				$lTmpAddress = $lCurrentSearchAddress + $lSearch - 1
				DllCall($mKernelHandle, 'int', 'ReadProcessMemory', 'int', $mGWProcHandle, 'int', $lTmpAddress + 0x6, 'ptr', DllStructGetPtr($lTmpBuffer), 'int', DllStructGetSize($lTmpBuffer), 'int', '')
				$mCharname = DllStructGetData($lTmpBuffer, 1)
				Return MemoryRead($mCharname, 'wchar[30]')
			EndIf

			$lCurrentSearchAddress += $lMBI[3]
		EndIf
	WEnd

	Return False
EndFunc   ;==>ScanForCharname
#EndRegion
