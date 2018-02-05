#RequireAdmin

#AutoIt3Wrapper_Icon=..\resources\gwtoolbox.ico

#include <File.au3>
#include <ComboConstants.au3>
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
Global Const $debug = True And Not @Compiled
Global Const $no_update = True
Global Const $host = "http://fbgmguild.com/GWToolboxpp/"
Global Const $folder = @LocalAppDataDir & "\GWToolboxpp\"
Global Const $dllpath = $folder & "GWToolbox.dll"
Global Const $imgFolder = $folder & "img\"
Global Const $locationLogsFolder = $folder & "location logs\"

Global $mKernelHandle, $mGWProcHandle, $mCharname
Global $gui = 0, $label = 0, $progress = 0, $changelabel = 0, $height = 0

Func EnsureFolderExists($folder)
	If Not FileExists($folder) Then DirCreate($folder)
EndFunc

; ==== Create directories ====
EnsureFolderExists($folder)
If Not FileExists($folder) Then Error("failed to create installation folder")
EnsureFolderExists($folder & "img\")
EnsureFolderExists($folder & "img\bonds\")
EnsureFolderExists($folder & "img\materials\")
EnsureFolderExists($folder & "img\icons\")
EnsureFolderExists($folder & "img\pcons\")
EnsureFolderExists($folder & "location logs\")

; ==== Disclaimer ====
If Not FileExists($dllpath) Then
	If MsgBox($MB_OKCANCEL, "GWToolbox++", 'DISCLAIMER:'&@CRLF& _
	'THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF ' & _
	'FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY ' & _
	'DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.' & @CRLF&@CRLF & _
	'By clicking the OK button you agree with all the above.') <> $IDOK Then Exit
EndIf

; ==== Updates ====
Update()
Func Update()
	If $debug And $no_update Then Return

	Local $width = 300
	$height = 60
	$gui = GUICreate("GWToolbox++", $width, $height)
	$label = GUICtrlCreateLabel("Checking for Updates...", 8, 8, 284, 17, Default, $GUI_WS_EX_PARENTDRAG)
	$progress = GUICtrlCreateProgress(8, 27, 284, 25)
	GUISetState(@SW_SHOW, $gui)

	If FileExists($dllpath) Then ; otherwise don't even check version, we have to download anyway

		Local $version_tmp = $folder & "tmp.txt"
		Local $version_download = InetGet($host & "version.txt", $version_tmp, BitOR($INET_FORCERELOAD, $INET_FORCEBYPASS), $INET_DOWNLOADBACKGROUND)
		Local $version_data
		While True
			$version_data = InetGetInfo($version_download)

			GUICtrlSetData($Label, "Checking for Updates... " & $version_data[$INET_DOWNLOADREAD] & " / " & $version_data[$INET_DOWNLOADSIZE] & " bytes")
			If $version_data[$INET_DOWNLOADSIZE] > 0 Then GUICtrlSetData($Progress, $version_data[$INET_DOWNLOADREAD] / $version_data[$INET_DOWNLOADSIZE] * 100)
			If $version_data[$INET_DOWNLOADCOMPLETE] Then
				GUICtrlSetData($Progress, 100)
				GUICtrlSetData($Label, "Checking for Updates... Done (" & Round($version_data[$INET_DOWNLOADREAD]) & " bytes")
				ExitLoop
			EndIf

			If GUIGetMsg() == $GUI_EVENT_CLOSE Then
				InetClose($version_download)
				FileDelete($version_tmp)
				Exit
			EndIf
		WEnd

		If $version_data[$INET_DOWNLOADERROR] Then Return MsgBox($MB_ICONERROR, "GWToolbox++", "Error checking for updates (1)")

		Local $remote_version = FileRead($version_tmp)
		InetClose($version_download)
		FileDelete($version_tmp)
		Local $local_version = IniRead($folder & "GWToolbox.ini", "launcher", "dllversion", "0")
		If $remote_version == "" Or $local_version == "" Then Return MsgBox($MB_ICONERROR, "GWToolbox++", "Error checking for updates (2)")

		If $remote_version == $local_version Then
			GUIDelete($gui)
			$gui = 0
			Return ; updated, we're done
		EndIf
	EndIf

	GUICtrlSetData($Label, "Updating...")
	GUICtrlSetData($Progress, 0)

	Local $dll_tmp = $folder & "tmp.dll"
	Local $dll_download = InetGet($host & "GWToolbox.dll", $dll_tmp, BitOR($INET_FORCERELOAD, $INET_FORCEBYPASS), $INET_DOWNLOADBACKGROUND)
	Local $dll_data
	Local $changelog_tmp = $folder & "tmp.txt"
	Local $changelog_download = InetGet($host & "changelog.txt", $changelog_tmp, BitOR($INET_FORCERELOAD, $INET_FORCEBYPASS), $INET_DOWNLOADBACKGROUND)
	Local $changelog_data
	While True
		$dll_data = InetGetInfo($dll_download)
		$changelog_data = InetGetInfo($changelog_download)

		If $changelabel == 0 And $changelog_data[$INET_DOWNLOADCOMPLETE] And $changelog_data[$INET_DOWNLOADSUCCESS] Then
			Local $changelog = FileRead($changelog_tmp)
			Local $changelog_height = Int(StringLeft($changelog, 4))
			Local $changelog_message = StringTrimLeft($changelog, 4)
			Local $pos = WinGetPos($gui)
			WinMove($gui, "", $pos[0], $pos[1], $pos[2], $pos[3] + $changelog_height)
			$changelabel = GUICtrlCreateLabel("Change log:" & @CRLF & $changelog_message, 8, $height, $width - 2 * 8, $changelog_height, Default, $GUI_WS_EX_PARENTDRAG)
			$height += $changelog_height
		EndIf

		GUICtrlSetData($Label, "Updating... " & Round($dll_data[$INET_DOWNLOADREAD] / 1024) & " / " & Round($dll_data[$INET_DOWNLOADSIZE] / 1024) & " Kbytes")
		If $dll_data[$INET_DOWNLOADSIZE] > 0 Then GUICtrlSetData($Progress, $dll_data[$INET_DOWNLOADREAD] / $dll_data[$INET_DOWNLOADSIZE] * 100)
		If $dll_data[$INET_DOWNLOADCOMPLETE] And $changelog_data[$INET_DOWNLOADCOMPLETE] Then
			GUICtrlSetData($Progress, 100)
			GUICtrlSetData($Label, "Updating... Done (" & Round($dll_data[$INET_DOWNLOADREAD] / 1024) & " Kbytes)")
			ExitLoop
		EndIf

		If GUIGetMsg() == $GUI_EVENT_CLOSE Then
			InetClose($dll_download)
			InetClose($changelog_download)
			FileDelete($dll_tmp)
			FileDelete($changelog_tmp)
			Exit
		EndIf
	WEnd

	If $dll_data[$INET_DOWNLOADERROR] Then Return MsgBox($MB_ICONERROR, "GWToolbox++", "Error checking for updates(3)")

	Sleep(50)

	FileDelete($dllpath)
	FileMove($dll_tmp, $dllpath)

	Sleep(50)

	InetClose($dll_download)
	InetClose($changelog_download)
	FileDelete($dll_tmp)
	FileDelete($changelog_tmp)
EndFunc

#Region fileinstalls
; ==== Install resources ====
; various
FileInstall("..\Release\GWToolbox.dll", $dllpath)
If Not FileExists($dllpath) Then Error("Failed to install GWToolbox.dll")
FileInstall("..\resources\Font.ttf", $folder & "Font.ttf")
FileInstall("..\resources\GWToolbox.ini", $folder & "GWToolbox.ini")
FileInstall("..\resources\Markers.ini", $folder & "Markers.ini")

FileInstall("..\resources\bonds\Balthazar's_Spirit.jpg", $folder & "img\bonds\Balthazar's_Spirit.jpg")
FileInstall("..\resources\bonds\Essence_Bond.jpg", $folder & "img\bonds\Essence_Bond.jpg")
FileInstall("..\resources\bonds\Holy_Veil.jpg", $folder & "img\bonds\Holy_Veil.jpg")
FileInstall("..\resources\bonds\Life_Attunement.jpg", $folder & "img\bonds\Life_Attunement.jpg")
FileInstall("..\resources\bonds\Life_Barrier.jpg", $folder & "img\bonds\Life_Barrier.jpg")
FileInstall("..\resources\bonds\Life_Bond.jpg", $folder & "img\bonds\Life_Bond.jpg")
FileInstall("..\resources\bonds\Live_Vicariously.jpg", $folder & "img\bonds\Live_Vicariously.jpg")
FileInstall("..\resources\bonds\Mending.jpg", $folder & "img\bonds\Mending.jpg")
FileInstall("..\resources\bonds\Protective_Bond.jpg", $folder & "img\bonds\Protective_Bond.jpg")
FileInstall("..\resources\bonds\Purifying_Veil.jpg", $folder & "img\bonds\Purifying_Veil.jpg")
FileInstall("..\resources\bonds\Retribution.jpg", $folder & "img\bonds\Retribution.jpg")
FileInstall("..\resources\bonds\Strength_of_Honor.jpg", $folder & "img\bonds\Strength_of_Honor.jpg")
FileInstall("..\resources\bonds\Succor.jpg", $folder & "img\bonds\Succor.jpg")
FileInstall("..\resources\bonds\Vital_Blessing.jpg", $folder & "img\bonds\Vital_Blessing.jpg")
FileInstall("..\resources\bonds\Watchful_Spirit.jpg", $folder & "img\bonds\Watchful_Spirit.jpg")

FileInstall("..\resources\icons\airplane.png", $folder & "img\icons\airplane.png")
FileInstall("..\resources\icons\cupcake.png", $folder & "img\icons\cupcake.png")
FileInstall("..\resources\icons\dialogue.png", $folder & "img\icons\dialogue.png")
FileInstall("..\resources\icons\feather.png", $folder & "img\icons\feather.png")
FileInstall("..\resources\icons\info.png", $folder & "img\icons\info.png")
FileInstall("..\resources\icons\keyboard.png", $folder & "img\icons\keyboard.png")
FileInstall("..\resources\icons\list.png", $folder & "img\icons\list.png")
FileInstall("..\resources\icons\settings.png", $folder & "img\icons\settings.png")

FileInstall("..\resources\materials\Essence_of_Celerity.png", $folder & "img\materials\Essence_of_Celerity.png")
FileInstall("..\resources\materials\Grail_of_Might.png", $folder & "img\materials\Grail_of_Might.png")
FileInstall("..\resources\materials\Armor_of_Salvation.png", $folder & "img\materials\Armor_of_Salvation.png")
FileInstall("..\resources\materials\Powerstone_of_Courage.png", $folder & "img\materials\Powerstone_of_Courage.png")
FileInstall("..\resources\materials\Scroll_of_Resurrection.png", $folder & "img\materials\Scroll_of_Resurrection.png")

FileInstall("..\resources\pcons\Essence_of_Celerity.png", $folder & "img\pcons\Essence_of_Celerity.png")
FileInstall("..\resources\pcons\Grail_of_Might.png", $folder & "img\pcons\Grail_of_Might.png")
FileInstall("..\resources\pcons\Armor_of_Salvation.png", $folder & "img\pcons\Armor_of_Salvation.png")
FileInstall("..\resources\pcons\Red_Rock_Candy.png", $folder & "img\pcons\Red_Rock_Candy.png")
FileInstall("..\resources\pcons\Blue_Rock_Candy.png", $folder & "img\pcons\Blue_Rock_Candy.png")
FileInstall("..\resources\pcons\Green_Rock_Candy.png", $folder & "img\pcons\Green_Rock_Candy.png")
FileInstall("..\resources\pcons\Golden_Egg.png", $folder & "img\pcons\Golden_Egg.png")
FileInstall("..\resources\pcons\Candy_Apple.png", $folder & "img\pcons\Candy_Apple.png")
FileInstall("..\resources\pcons\Candy_Corn.png", $folder & "img\pcons\Candy_Corn.png")
FileInstall("..\resources\pcons\Birthday_Cupcake.png", $folder & "img\pcons\Birthday_Cupcake.png")
FileInstall("..\resources\pcons\Slice_of_Pumpkin_Pie.png", $folder & "img\pcons\Slice_of_Pumpkin_Pie.png")
FileInstall("..\resources\pcons\War_Supplies.png", $folder & "img\pcons\War_Supplies.png")
FileInstall("..\resources\pcons\Dwarven_Ale.png", $folder & "img\pcons\Dwarven_Ale.png")
FileInstall("..\resources\pcons\Lunar_Fortune.png", $folder & "img\pcons\Lunar_Fortune.png")
FileInstall("..\resources\pcons\Sugary_Blue_Drink.png", $folder & "img\pcons\Sugary_Blue_Drink.png")
FileInstall("..\resources\pcons\Drake_Kabob.png", $folder & "img\pcons\Drake_Kabob.png")
FileInstall("..\resources\pcons\Bowl_of_Skalefin_Soup.png", $folder & "img\pcons\Bowl_of_Skalefin_Soup.png")
FileInstall("..\resources\pcons\Pahnai_Salad.png", $folder & "img\pcons\Pahnai_Salad.png")
#EndRegion

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
		Local $selection_gui = GUICreate("GWToolbox++", 150, 60)
		Local $comboCharname = GUICtrlCreateCombo("", 6, 6, 138, 25, $CBS_DROPDOWNLIST)
		GUICtrlSetData(-1, $lComboStr, $lFirstChar)
		Local $button = GUICtrlCreateButton("Launch", 6, 31, 138, 24)
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

		Local $index = _GUICtrlComboBox_GetCurSel($comboCharname)
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

Local $hProcess = DllCall($Kernel32, "DWORD", "OpenProcess", "DWORD", 0x1F0FFF, "int", 0, "DWORD", $gwPID)
If @error Then Error1("Failed to DllCall OpenProcess", @error)
If $hProcess == 0 Then Error2("OpenProcess failed", _WinAPI_GetLastError(), $hProcess)

Local $hModule = DllCall($Kernel32, "DWORD", "GetModuleHandleA", "str", "kernel32.dll")
If @error Then Error1("Failed to DllCall GetModuleHandleA", @error)
If $hModule == 0 Then Error2("GetModuleHandle failed", _WinAPI_GetLastError(), $hModule)

Local $lpStartAddress = DllCall($Kernel32, "DWORD", "GetProcAddress", "DWORD", $hModule[0], "str", "LoadLibraryA")
If @error Then Error1("Failed to DllCall GetProcAddress", @error)
If $lpStartAddress == 0 Then Error2("GetProcAddress failed", _WinAPI_GetLastError(), $lpStartAddress)

Local $lpParameter = DllCall($Kernel32, "DWORD", "VirtualAllocEx", "int", $hProcess[0], "int", 0, "ULONG_PTR", DllStructGetSize($DLL_Path), "DWORD", 0x3000, "int", 4)
If @error Then Error1("Failed to DllCall VirtualAllocEx", @error)
If $lpParameter == 0 Then Error2("VirtualAllocEx failed", _WinAPI_GetLastError(), $lpParameter)

Local $writeProcMem_ret = DllCall($Kernel32, "BOOL", "WriteProcessMemory", "int", $hProcess[0], "DWORD", $lpParameter[0], "str", $sDLLFullPath, "ULONG_PTR", DllStructGetSize($DLL_Path), "int", 0)
If @error Then Error1("Failed to DllCall WriteProcessMemory", @error)
If $writeProcMem_ret == 0 Then Error2("WriteProcessMemory failed", _WinAPI_GetLastError(), $writeProcMem_ret)

Local $hThread = DllCall($Kernel32, "int", "CreateRemoteThread", "DWORD", $hProcess[0], "int", 0, "int", 0, "DWORD", $lpStartAddress[0], "DWORD", $lpParameter[0], "int", 0, "int", 0)
If @error Then Error1("Failed to DllCall CreateRemoteThread", @error)
If $hThread == 0 Then Error2("CreateRemoteThread failed", _WinAPI_GetLastError(), $hThread)

Local $closehandle_ret = DllCall($Kernel32, "BOOL", "CloseHandle", "DWORD", $hProcess[0])
If @error Then Error1("Failed to DllCall CloseHandle", @error)
If $closehandle_ret == 0 Then Error2("CloseHandle failed", _WinAPI_GetLastError(), $closehandle_ret)

DllClose($Kernel32)

Local $found = False
Local $deadlock = TimerInit()
While Not $found And TimerDiff($deadlock) < 3000

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
	Local $pos = WinGetPos($gui)
	GUICtrlCreateLabel("You can close me, but please read me first!", 8, $height - 25, 284, 17, $SS_CENTER, $GUI_WS_EX_PARENTDRAG)

	While GUIGetMsg() <> $GUI_EVENT_CLOSE
		Sleep(0)
	WEnd
EndIf

#Region error message boxes
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
