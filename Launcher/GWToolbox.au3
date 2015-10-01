#RequireAdmin
#include <File.au3>
#include <ComboConstants.au3>
#include <MsgBoxConstants.au3>
#include <GUIConstantsEx.au3>
#include <GuiComboBox.au3>
#include <WinAPIProc.au3>

; dx runtime: http://www.microsoft.com/en-us/download/details.aspx?id=8109

Global Const $overwrite = True
Global Const $debug = True And Not @Compiled
Global Const $host = "http://fbgmguild.com/GWToolboxpp/"
Global Const $folder = @LocalAppDataDir & "\GWToolboxpp\"
Global Const $dllpath = $folder & "GWToolbox.dll"
Global Const $imgFolder = $folder & "img\"
Global Const $locationLogsFolder = $folder & "location logs\"

Global $mKernelHandle, $mGWProcHandle, $mCharname

If Not FileExists($folder) Then DirCreate($folder)
If Not FileExists($folder) Then Error("failed to create installation folder")
If Not FileExists($imgFolder) Then DirCreate($imgFolder)
if Not FileExists($locationLogsFolder) Then DirCreate($locationLogsFolder)

#Region fileinstalls
; various
If $debug Then
	FileCopy("..\Debug\GWToolbox.dll", $dllpath, $overwrite)
Else
	FileInstall("..\Release\GWToolbox.dll", $dllpath, $overwrite)
EndIf
If Not FileExists($dllpath) Then Error("Failed to install GWToolbox.dll")
FileInstall("..\resources\DefaultTheme.txt", $folder & "Theme.txt")
FileInstall("..\resources\Friz_Quadrata_Regular.ttf", $folder & "Font.ttf")
FileInstall("..\resources\DefaultSettings.ini", $folder & "GWToolbox.ini")
FileInstall("..\resources\Tick_v2.png", $imgFolder & "Tick.png")
; Skill icons (bonds)
FileInstall("..\resources\balthspirit.jpg", $imgFolder & "balthspirit.jpg")
FileInstall("..\resources\protbond.jpg", $imgFolder & "protbond.jpg")
FileInstall("..\resources\lifebond.jpg", $imgFolder & "lifebond.jpg")
; tab icons
FileInstall("..\resources\info.png", $imgFolder & "info.png")
FileInstall("..\resources\comment.png", $imgFolder & "comment.png")
FileInstall("..\resources\cupcake.png", $imgFolder & "cupcake.png")
FileInstall("..\resources\feather.png", $imgFolder & "feather.png")
FileInstall("..\resources\keyboard.png", $imgFolder & "keyboard.png")
FileInstall("..\resources\list.png", $imgFolder & "list.png")
FileInstall("..\resources\plane.png", $imgFolder & "plane.png")
FileInstall("..\resources\settings.png", $imgFolder & "settings.png")
; pcons
FileInstall("..\resources\Essence_of_Celerity.png", $imgFolder & "Essence_of_Celerity.png")
FileInstall("..\resources\Grail_of_Might.png", $imgFolder & "Grail_of_Might.png")
FileInstall("..\resources\Armor_of_Salvation.png", $imgFolder & "Armor_of_Salvation.png")
FileInstall("..\resources\Red_Rock_Candy.png", $imgFolder & "Red_Rock_Candy.png")
FileInstall("..\resources\Blue_Rock_Candy.png", $imgFolder & "Blue_Rock_Candy.png")
FileInstall("..\resources\Green_Rock_Candy.png", $imgFolder & "Green_Rock_Candy.png")
FileInstall("..\resources\Birthday_Cupcake.png", $imgFolder & "Birthday_Cupcake.png")
FileInstall("..\resources\Candy_Apple.png", $imgFolder & "Candy_Apple.png")
FileInstall("..\resources\Candy_Corn.png", $imgFolder & "Candy_Corn.png")
FileInstall("..\resources\Golden_Egg.png", $imgFolder & "Golden_Egg.png")
FileInstall("..\resources\Slice_of_Pumpkin_Pie.png", $imgFolder & "Slice_of_Pumpkin_Pie.png")
FileInstall("..\resources\Sugary_Blue_Drink.png", $imgFolder & "Sugary_Blue_Drink.png")
FileInstall("..\resources\Dwarven_Ale.png", $imgFolder & "Dwarven_Ale.png")
FileInstall("..\resources\Lunar_Fortune.png", $imgFolder & "Lunar_Fortune.png")
FileInstall("..\resources\War_Supplies.png", $imgFolder & "War_Supplies.png")
FileInstall("..\resources\Drake_Kabob.png", $imgFolder & "Drake_Kabob.png")
FileInstall("..\resources\Bowl_of_Skalefin_Soup.png", $imgFolder & "Bowl_of_Skalefin_Soup.png")
FileInstall("..\resources\Pahnai_Salad.png", $imgFolder & "Pahnai_Salad.png")
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
		Local $gui = GUICreate("GWToolbox++", 150, 60)
		Local $comboCharname = GUICtrlCreateCombo("", 6, 6, 138, 25, $CBS_DROPDOWNLIST)
		GUICtrlSetData(-1, $lComboStr, $lFirstChar)
		Local $button = GUICtrlCreateButton("Launch", 6, 31, 138, 24)
		GUISetState(@SW_SHOW, $gui)
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
If Not $found Then Error("GWToolbox.dll was not loaded, unknown error")

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
