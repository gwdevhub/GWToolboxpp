#RequireAdmin

#include <InetConstants.au3>
#include <ButtonConstants.au3>
#include <ComboConstants.au3>
#include <GUIConstantsEx.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>
#include <FileConstants.au3>
#include <Array.au3>
#include <File.au3>

Opt("MustDeclareVars", True)
Opt("GUIOnEventMode", True)

Global $DevelopMode = True
If @Compiled Then $DevelopMode = False
Global $installDLL

; TODO: process command-line arguments
Global Const $no_gui = False

Global $injected = False
Global $lWinList = WinList("[CLASS:ArenaNet_Dx_Window_Class; REGEXPTITLE:^\D+$]")
Global $lCharArray[$lWinList[0][0]][2]
Global $selectedClient = -1

Global $mKernelHandle, $mGWProcHandle, $mCharname

Global Const $host = "http://fbgmguild.com/GWToolboxpp/"
Global Const $folder = @LocalAppDataDir & "\GWToolboxpp\"
Global Const $imgFolder = $folder & "img\"
Global Const $ini = $folder & "GWToolbox.ini"
Global Const $launcher_section = "Launcher"
Global Const $theme = $folder & "Theme.txt"
Global Const $dll = $folder & "GWToolboxpp.dll"

Global $close_after = (IniRead($ini, $launcher_section, "close_after", False) == "True")
Global $autolaunch = (IniRead($ini, $launcher_section, "autolaunch", False) == "True")
Global $launch_exe = (IniRead($ini, $launcher_section, "launch_exe", False) == "True")
Global $gw_exe = IniRead($ini, $launcher_section, "gw_exe", "")

If $no_gui Then
	$autolaunch = True
	$close_after = True
EndIf

If Not $no_gui Then
	Global Const $main_gui = GUICreate("GWToolbox++ Launcher", 480, 280)
	GUISetOnEvent($GUI_EVENT_CLOSE, "exitProgram")

	GUICtrlCreateLabel("GWToolbox++ Launcher", 8, 8, 464, 50, $SS_CENTER)
		GUICtrlSetFont(-1, 30, 400, 0, "MS Sans Serif")
	Global Const $status = GUICtrlCreateLabel("Loading...", 108, 72, 264, 33, $SS_CENTER)
		GUICtrlSetFont(-1, 18, 400, 0, "MS Sans Serif")
	If $DevelopMode Then
		$installDLL = GUICtrlCreateButton("Install DLL", 380, 72, 60, 30)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
	EndIf

	GUICtrlCreateGroup("", 8, 112, 464, 97, -1, $WS_EX_TRANSPARENT)
	Global Const $Radio1 = GUICtrlCreateRadio("", 16, 136, 17, 17)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
		If Not $launch_exe Then GUICtrlSetState(-1, $GUI_CHECKED)
	Global Const $Radio2 = GUICtrlCreateRadio("", 16, 168, 17, 17)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
		If $launch_exe Then GUICtrlSetState(-1, $GUI_CHECKED)
	Global Const $clientselect = GUICtrlCreateCombo("Loading client list...", 40, 136, 121, 25, $CBS_DROPDOWNLIST)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
	Global Const $exeselect = GUICtrlCreateButton($gw_exe, 40, 168, 123, 25)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")

	Global Const $LaunchButton = GUICtrlCreateButton("Launch", 176, 128, 139, 65)
		GUICtrlSetFont(-1, 24, 400, 0, "MS Sans Serif")
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
		GUICtrlSetState(-1, $GUI_DISABLE)

	Global Const $close_after_checkbox = GUICtrlCreateCheckbox("Close after launch", 336, 168, 121, 17)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
		If $close_after Then GUICtrlSetState(-1, $GUI_CHECKED)
	Global Const $autolaunch_checkbox = GUICtrlCreateCheckbox("Automatically launch", 336, 144, 121, 17)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
		If $autolaunch Then GUICtrlSetState(-1, $GUI_CHECKED)

	Global Const $install_folder_button = GUICtrlCreateButton("Open installation folder", 104, 224, 131, 25)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")
	Global Const $website_button = GUICtrlCreateButton("Go to website", 256, 224, 131, 25)
		GUICtrlSetOnEvent(-1, "GuIEventHandler")

	GUISetState(@SW_SHOW)
EndIf

; 1. create directory
If (Not FileExists($folder)) Then
	Out("Creating Settings Directory")
	DirCreate($folder)
EndIf
If Not FileExists($imgFolder) Then
	Out("Creating Img Directory")
	DirCreate($imgFolder)
EndIf

; 2. install required files if missing
InstallResources()

; 3. find gw client
ScanForGWClients()

; 4. check for updates
If $DevelopMode Then
	FileInstall("..\Debug\API.dll", $dll, True) ; True = overwrite
Else
	If FileExists($dll) Then FileDelete($dll)
	Global $download = InetGet($host & "GWToolboxpp.dll", $ini, Default, $INET_DOWNLOADBACKGROUND)
	If $download == 0 Then
		MsgBox(0, "GWToolbox++ Launcher", "Download error: " & @error)
	EndIf
	Out("Updating... 0 %")

	While Not InetGetInfo($download, $INET_DOWNLOADCOMPLETE)
		Sleep(250)
		Local $completed = InetGetInfo($download)
		Local $size = InetGetInfo($download)
		Local $progress
		If $size > 0 Then
			$progress = $completed / $size * 100 & " %"
		Else
			$progress = $completed
		EndIf
		Out("Updating... " & $progress)
	WEnd
EndIf



; 5. proceed if $autolaunch or wait for user input
Out("Ready")
GUICtrlSetState($LaunchButton, $GUI_ENABLE)
If $autolaunch Then
	InjectGW()
EndIf

; 6. exit if $close_after or wait for user input
While 1
	If $injected And $close_after Then exitProgram()

	Sleep(100)
WEnd

Func ScanForGWClients()
	$lWinList = WinList("[CLASS:ArenaNet_Dx_Window_Class; REGEXPTITLE:^\D+$]")
	If $lWinList[0][0] == 0 Then
		If Not $no_gui Then
			GUICtrlSetData($clientselect, "")
			GUICtrlSetData($clientselect, "No client found")
		EndIf
		$selectedClient = -1
	Else
		Local $lComboStr
		Local $lFirstChar

		For $i = 1 To $lWinList[0][0]
			$lCharArray[$i - 1][1] = WinGetProcess($lWinList[$i][1])
			MemoryOpen($lCharArray[$i - 1][1])
			If $i = 1 Then $lFirstChar = ScanForCharname()
			$lCharArray[$i - 1][0] = MemoryRead($mCharname, 'wchar[30]')
			MemoryClose()

			$lComboStr &= $lCharArray[$i - 1][0]
			If $i <> $lWinList[0][0] Then $lComboStr &= '|'
		Next

		If Not $no_gui Then
			GUICtrlSetData($clientselect, "")
			GUICtrlSetData($clientselect, $lComboStr, $lFirstChar)
		EndIf
		$selectedClient = 0

	EndIf
EndFunc

Func InjectGW()
	If $DevelopMode Then FileCopy("..\Debug\API.dll", $dll, 1)

	If Not $no_gui Then GUICtrlSetState($LaunchButton, $GUI_DISABLE)
	Out("Launching...")
	If $launch_exe Then
		; TODO

	Else
		If $lWinList[0][0] = 0 Then
			MsgBox(0, "GWToolbox++ Launcher", "Cannot find Guild Wars Client")
			exitProgram()
		EndIf

		If Not $no_gui Then
			Local $lTmp = GUICtrlRead($clientselect)
			For $i = 0 To UBound($lCharArray) - 1
				If $lCharArray[$i][0] = $lTmp Then
					$selectedClient = $i
					ExitLoop
				EndIf
			Next
		EndIf

		If $selectedClient < 0 Then
			MsgBox(0, "GWToolbox++ Launcher", "Cannot find Guild Wars Client")
			exitProgram()
		EndIf

		ConsoleWrite("injecting" & @CRLF)
		_InjectDll($lCharArray[$selectedClient][1], $dll)

	EndIf

	Out("Done")
	$injected = True
	If Not $no_gui Then GUICtrlSetState($LaunchButton, $GUI_ENABLE)
EndFunc


Func Out($msg)
	GUICtrlSetData($status, $msg)
EndFunc


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


Func GuIEventHandler()
	Switch @GUI_CtrlId
		Case $install_folder_button
			ShellExecute($folder)
		Case $website_button
			ShellExecute($Host)

		Case $close_after_checkbox
			$close_after = (GUICtrlRead($close_after_checkbox) == $GUI_CHECKED)
			IniWrite($ini, $launcher_section, "close_after", $close_after)
		Case $autolaunch_checkbox
			$autolaunch = (GUICtrlRead($autolaunch_checkbox) == $GUI_CHECKED)
			IniWrite($ini, $launcher_section, "autolaunch", $autolaunch)

		Case $Radio1
			$launch_exe = False
			IniWrite($ini, $launcher_section, "launch_exe", $launch_exe)
		Case $Radio2
			$launch_exe = True
			IniWrite($ini, $launcher_section, "launch_exe", $launch_exe)

		Case $LaunchButton
			InjectGW()
			$injected = True

		Case $exeselect
			Opt("GUIOnEventMode", False)
			$gw_exe = FileOpenDialog("Select gw.exe", @ProgramFilesDir, "(*.exe)", BitOR($FD_FILEMUSTEXIST, $FD_PATHMUSTEXIST), "Gw.exe", $main_gui)
			Opt("GUIOnEventMode", True)
			If @error <> 1 Then
				IniWrite($ini, $launcher_section, "gw_exe", $gw_exe)
				GUICtrlSetData($exeselect, $gw_exe)
			EndIf

		Case $installDLL
			FileInstall("..\Debug\API.dll", $dll, True) ; True = overwrite

		Case Else
			MsgBox(0, "Error", "Not implemented")
	EndSwitch

EndFunc

Func InstallResources()
	If Not FileExists($folder & "Theme.txt") Then
		FileInstall("..\resources\DefaultTheme.txt", $folder & "Theme.txt")
	EndIf

	If Not FileExists($folder & "Font.ttf") Then
		FileInstall("..\resources\Friz_Quadrata_Regular.ttf", $folder & "Font.ttf")
	EndIf

	If Not FileExists($folder & "GWToolbox.ini") Then
		FileInstall("..\resources\DefaultSettings.ini", $folder & "GWToolbox.ini")
	EndIf

	If Not FileExists($imgFolder & "Tick.png") Then
		FileInstall("..\resources\Tick.png", $imgFolder & "Tick.png")
	EndIf

	; Skill icons (bonds)
	If Not FileExists($imgFolder & "balthspirit.jpg") Then
		FileInstall("..\resources\balthspirit.jpg", $imgFolder & "balthspirit.jpg")
	EndIf
	If Not FileExists($imgFolder & "protbond.jpg") Then
		FileInstall("..\resources\protbond.jpg", $imgFolder & "protbond.jpg")
	EndIf
	If Not FileExists($imgFolder & "lifebond.jpg") Then
		FileInstall("..\resources\lifebond.jpg", $imgFolder & "lifebond.jpg")
	EndIf

	; tab icons
	If Not FileExists($imgFolder & "info.png") Then
		FileInstall("..\resources\info.png", $imgFolder & "info.png")
	EndIf

	If Not FileExists($imgFolder & "comment.png") Then
		FileInstall("..\resources\comment.png", $imgFolder & "comment.png")
	endIf
	If Not FileExists($imgFolder & "cupcake.png") Then
		FileInstall("..\resources\cupcake.png", $imgFolder & "cupcake.png")
	endIf
	If Not FileExists($imgFolder & "feather.png") Then
		FileInstall("..\resources\feather.png", $imgFolder & "feather.png")
	endIf
	If Not FileExists($imgFolder & "keyboard.png") Then
		FileInstall("..\resources\keyboard.png", $imgFolder & "keyboard.png")
	endIf
	If Not FileExists($imgFolder & "list.png") Then
		FileInstall("..\resources\list.png", $imgFolder & "list.png")
	endIf
	If Not FileExists($imgFolder & "plane.png") Then
		FileInstall("..\resources\plane.png", $imgFolder & "plane.png")
	endIf
	If Not FileExists($imgFolder & "settings.png") Then
		FileInstall("..\resources\settings.png", $imgFolder & "settings.png")
	endIf

	; pcons
	If Not FileExists($imgFolder & "Essence_of_Celerity.png") Then
		FileInstall("..\resources\Essence_of_Celerity.png", $imgFolder & "Essence_of_Celerity.png")
	EndIf

	If Not FileExists($imgFolder & "Grail_of_Might.png") Then
		FileInstall("..\resources\Grail_of_Might.png", $imgFolder & "Grail_of_Might.png")
	EndIf

	If Not FileExists($imgFolder & "Armor_of_Salvation.png") Then
		FileInstall("..\resources\Armor_of_Salvation.png", $imgFolder & "Armor_of_Salvation.png")
	EndIf

	If Not FileExists($imgFolder & "Red_Rock_Candy.png") Then
		FileInstall("..\resources\Red_Rock_Candy.png", $imgFolder & "Red_Rock_Candy.png")
	EndIf

	If Not FileExists($imgFolder & "Blue_Rock_Candy.png") Then
		FileInstall("..\resources\Blue_Rock_Candy.png", $imgFolder & "Blue_Rock_Candy.png")
	EndIf

	If Not FileExists($imgFolder & "Green_Rock_Candy.png") Then
		FileInstall("..\resources\Green_Rock_Candy.png", $imgFolder & "Green_Rock_Candy.png")
	EndIf

	If Not FileExists($imgFolder & "Birthday_Cupcake.png") Then
		FileInstall("..\resources\Birthday_Cupcake.png", $imgFolder & "Birthday_Cupcake.png")
	EndIf

	If Not FileExists($imgFolder & "Candy_Apple.png") Then
		FileInstall("..\resources\Candy_Apple.png", $imgFolder & "Candy_Apple.png")
	EndIf

	If Not FileExists($imgFolder & "Candy_Corn.png") Then
		FileInstall("..\resources\Candy_Corn.png", $imgFolder & "Candy_Corn.png")
	EndIf

	If Not FileExists($imgFolder & "Golden_Egg.png") Then
		FileInstall("..\resources\Golden_Egg.png", $imgFolder & "Golden_Egg.png")
	EndIf

	If Not FileExists($imgFolder & "Slice_of_Pumpkin_Pie.png") Then
		FileInstall("..\resources\Slice_of_Pumpkin_Pie.png", $imgFolder & "Slice_of_Pumpkin_Pie.png")
	EndIf

	If Not FileExists($imgFolder & "Sugary_Blue_Drink.png") Then
		FileInstall("..\resources\Sugary_Blue_Drink.png", $imgFolder & "Sugary_Blue_Drink.png")
	EndIf

	If Not FileExists($imgFolder & "Dwarven_Ale.png") Then
		FileInstall("..\resources\Dwarven_Ale.png", $imgFolder & "Dwarven_Ale.png")
	EndIf

	If Not FileExists($imgFolder & "Lunar_Fortune.png") Then
		FileInstall("..\resources\Lunar_Fortune.png", $imgFolder & "Lunar_Fortune.png")
	EndIf

	If Not FileExists($imgFolder & "War_Supplies.png") Then
		FileInstall("..\resources\War_Supplies.png", $imgFolder & "War_Supplies.png")
	EndIf

	If Not FileExists($imgFolder & "Drake_Kabob.png") Then
		FileInstall("..\resources\Drake_Kabob.png", $imgFolder & "Drake_Kabob.png")
	EndIf

	If Not FileExists($imgFolder & "Bowl_of_Skalefin_Soup.png") Then
		FileInstall("..\resources\Bowl_of_Skalefin_Soup.png", $imgFolder & "Bowl_of_Skalefin_Soup.png")
	EndIf

	If Not FileExists($imgFolder & "Pahnai_Salad.png") Then
		FileInstall("..\resources\Pahnai_Salad.png", $imgFolder & "Pahnai_Salad.png")
	EndIf
EndFunc

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

;=================================================================================================
; Function:            _InjectDll($ProcessId, $DllPath)
; Description:        Injects a .dll into a running program.
; Return Value(s):    On Success - Returns true
;                    On Failure - Returns false
;                    @Error - 0 = No error.
;                             1 = Invalid ProcessId.
;                             2 = File does not exist.
;                             3 = File is not a .dll (invalid file).
;                             4 = Failed to open 'Advapi32.dll'.
;                             5 = Failed to get the full path.
;                             6 = Failed to open the process.
;                             7 = Failed to call 'GetModuleHandle'.
;                             8 = Failed to call 'GetProcAddress'.
;                             9 = Failed to call 'VirtualAllocEx'.
;                             10 = Failed to write the memory.
;                             11 = Failed to create the 'RemoteThread'.
; Author(s):        KillerDeluxe
;=================================================================================================
Func _InjectDll($ProcessId, $DllPath)
	If $ProcessId == 0 Then Return SetError(1, "", False)
	If Not (FileExists($DllPath)) Then Return SetError(2, "", False)
	If Not (StringRight($DllPath, 4) == ".dll") Then Return SetError(3, "", False)

	Local $Kernel32 = DllOpen("kernel32.dll")
	If @error Then Return SetError(4, "", False)

	Local $DLL_Path = DllStructCreate("char[255]")
	DllCall($Kernel32, "DWORD", "GetFullPathNameA", "str", $DllPath, "DWORD", 255, "ptr", DllStructGetPtr($DLL_Path), "int", 0)
	If @error Then Return SetError(5, "", False)

	Local $hProcess = DllCall($Kernel32, "DWORD", "OpenProcess", "DWORD", 0x1F0FFF, "int", 0, "DWORD", $ProcessId)
	If @error Then Return SetError(6, "", False)

	Local $hModule = DllCall($Kernel32, "DWORD", "GetModuleHandleA", "str", "kernel32.dll")
	If @error Then Return SetError(7, "", False)

	Local $lpStartAddress = DllCall($Kernel32, "DWORD", "GetProcAddress", "DWORD", $hModule[0], "str", "LoadLibraryA")
	If @error Then Return SetError(8, "", False)

	Local $lpParameter = DllCall($Kernel32, "DWORD", "VirtualAllocEx", "int", $hProcess[0], "int", 0, "ULONG_PTR", DllStructGetSize($DLL_Path), "DWORD", 0x3000, "int", 4)
	If @error Then Return SetError(9, "", False)

	DllCall($Kernel32, "BOOL", "WriteProcessMemory", "int", $hProcess[0], "DWORD", $lpParameter[0], "str", DllStructGetData($DLL_Path, 1), "ULONG_PTR", DllStructGetSize($DLL_Path), "int", 0)
	If @error Then Return SetError(10, "", False)

	Local $hThread = DllCall($Kernel32, "int", "CreateRemoteThread", "DWORD", $hProcess[0], "int", 0, "int", 0, "DWORD", $lpStartAddress[0], "DWORD", $lpParameter[0], "int", 0, "int", 0)
	If @error Then Return SetError(11, "", False)

	DllCall($Kernel32, "BOOL", "CloseHandle", "DWORD", $hProcess[0])
	DllClose($Kernel32)

	Return SetError(0, "", True)
EndFunc   ;==>_InjectDll

Func exitProgram()
	Exit
EndFunc
