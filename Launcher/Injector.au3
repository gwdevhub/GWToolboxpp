#Region ;**** Directives created by AutoIt3Wrapper_GUI ****
#AutoIt3Wrapper_Icon=icon.ico
#AutoIt3Wrapper_Outfile=release\GWHackMenu.exe
#AutoIt3Wrapper_Res_Comment=Hack Menu for Guild Wars
#AutoIt3Wrapper_Res_Description=by d8rken and 4D 1
#AutoIt3Wrapper_Res_Fileversion=1.1.0.1
#AutoIt3Wrapper_Res_requestedExecutionLevel=requireAdministrator
#AutoIt3Wrapper_Res_File_Add=logo.jpg, rt_rcdata, LOGO_JPG
#AutoIt3Wrapper_Res_File_Add=refresh-icon.bmp, rt_bitmap, REFRESH_BMP
#AutoIt3Wrapper_Run_Au3Stripper=y
#EndRegion ;**** Directives created by AutoIt3Wrapper_GUI ****
#include "ResourcesEx.au3"
;#NoTrayIcon
Global $mGWList = WinList("[CLASS:ArenaNet_Dx_Window_Class; REGEXPTITLE:^\D+$]")
Global Const $hKernel32 = DllOpen("kernel32.dll")
Global $NameList[$mGWList[0][0]][3]
Global $NameStr = ''
Global $SelectedClient

Global Const $GUI_WS_EX_PARENTDRAG = 0x00100000
;Global Const $WS_POPUP = 0x80000000
;Global Const $WS_EX_TOPMOST = 0x00000008
;Global Const $SS_CENTER = 0x1
;Global Const $SS_CENTERIMAGE = 0x0200
Global Const $ES_CENTER = 0x0001
;Global Const $BS_ICON = 0x0040
;Global Const $BS_BITMAP = 128
Global Const $CBS_DROPDOWNLIST = 0x0003
Global Const $GUI_DISABLE = 128

Global Const $HACK_MENU_PATH = "../Release/"

Switch $mGWList[0][0]
	Case 0
		MsgBoxOK("No GW Clients Found.")
	Case 1
		If @Compiled Then
		FileInstall("../Release/GWHackMenu.dll", "GWHackMenu.dll")
		FileInstall("../Release/GWHackMenu.xml", "GWHackMenu.xml")
		_InjectDll(WinGetProcess($mGWList[1][1]), "GWHackMenu.dll")
		Else
		_InjectDll(WinGetProcess($mGWList[1][1]),$HACK_MENU_PATH & "GWHackMenu.dll")
		EndIf
	Case Else
		LoadNameList()
		GetSelectedClientInfo(SelectionGUI())
		If @Compiled Then
		FileInstall("../Release/GWHackMenu.dll", "GWHackMenu.dll")
		FileInstall("../Release/GWHackMenu.xml", "GWHackMenu.xml")
		_InjectDll($NameList[$SelectedClient][1], "GWHackMenu.dll")
		Else
		_InjectDll($NameList[$SelectedClient][1],$HACK_MENU_PATH & "GWHackMenu.dll")
		EndIf

EndSwitch

Func LoadNameList()
	Local $iProc, $hProc, $sCharname, $pCharnamePtr
	For $i = 1 To $mGWList[0][0]
		$iProc = WinGetProcess($mGWList[$i][1])
		$hProc = OpenProcess($iProc)
		If $i = 1 Then
			$pCharnamePtr = ScanForPtr($hProc, '90909066C705', 6)
		EndIf
		$sCharname = ReadProcessMemory($hProc, $pCharnamePtr, 'wchar[30]')
		$NameList[$i - 1][0] = $sCharname
		$NameList[$i - 1][1] = $iProc
		$NameList[$i - 1][2] = $mGWList[$i][1]
		$NameStr &= $sCharname
		If $i <> $mGWList[0][0] Then $NameStr &= '|'
		CloseHandle($hProc)
	Next
EndFunc   ;==>LoadNameList

Func SelectionGUI()


	Local $selectionGUI = GUICreate("GW Hack Menu - Select Your Character!", 370, 211, -1, -1, $WS_POPUP, $WS_EX_TOPMOST)
	GUISetBkColor(0x000000, $selectionGUI)
	Local $groupSelection = GUICtrlCreateGroup("Choose A Character!", 8, 128, 353, 73)
	GUICtrlSetColor($groupSelection, 0xFFFFFF)
	Local $labelGroupSelection = GUICtrlCreateLabel("Choose A Character!", 20, 128)
	GUICtrlSetColor($labelGroupSelection, 0xFFFFFF)
	Local $comboCharnames = GUICtrlCreateCombo("", 20, 162, 201, 25, BitOR($ES_CENTER, $CBS_DROPDOWNLIST))
	GUICtrlSetData($comboCharnames, $NameStr, $NameList[0][0])
	Local $buttonChooseCharacter = GUICtrlCreateButton("Go!", 260, 160, 89, 25)
	GUICtrlSetColor($buttonChooseCharacter, 0x000000)
	Local $buttonRefresh = GUICtrlCreateButton("", 228, 160, 25, 25, $BS_BITMAP)
	GUICtrlSetColor($buttonRefresh, 0x000000)
	GUICtrlSetState($buttonRefresh, $GUI_DISABLE)
	Local $imageRefresh = GUICtrlCreatePic("", 233, 165, 16, 16)
	Local $GUITitlePic = GUICtrlCreatePic("", 8, 8, 356, 116)
	GUICtrlSetState($GUITitlePic, $GUI_DISABLE)
	Local $GUIDrag = GUICtrlCreatePic("", 8, 8, 329, 116, -1, $GUI_WS_EX_PARENTDRAG)
	Local $exitLabel = GUICtrlCreateLabel("X", 352, 3)
	GUICtrlSetColor($exitLabel, 0xFFFFFF)
	GUICtrlSetFont($exitLabel, 14, 700)

	If @Compiled Then
		_Resource_SetToCtrlID($imageRefresh, 'REFRESH_BMP',$RT_BITMAP)
		_Resource_SetToCtrlID($GUITitlePic, 'LOGO_JPG')
	Else
		GUICtrlSetImage($imageRefresh, "refresh-icon.bmp")
		GUICtrlSetImage($GUITitlePic, "logo.jpg")
	EndIf

	GUISetState(@SW_SHOW, $selectionGUI)


	While 1
		Switch GUIGetMsg()
			Case $imageRefresh
				$mGWList = WinList("[CLASS:ArenaNet_Dx_Window_Class; REGEXPTITLE:^\D+$]")
				ReDim $NameList[$mGWList[0][0]][3]
				$NameStr = ''
				LoadNameList()
				GUICtrlSetData($comboCharnames,"")
				GUICtrlSetData($comboCharnames,$NameStr, $NameList[0][0])
			Case $buttonChooseCharacter
				Local $tmp = GUICtrlRead($comboCharnames)
				GUIDelete($selectionGUI)
				Return $tmp
			Case $exitLabel
				Exit
		EndSwitch
		Sleep(25)
	WEnd
EndFunc   ;==>SelectionGUI

Func GetSelectedClientInfo(Const $aCharname)
	For $i = 0 To $mGWList[0][0] - 1
		If $NameList[$i][0] == $aCharname Then
			$SelectedClient = $i
			Return
		EndIf
	Next
EndFunc   ;==>GetSelectedClientInfo

Func CloseHandle($iHandle)
	DllCall($hKernel32, "int", "CloseHandle", "int", $iHandle)
EndFunc   ;==>CloseHandle

Func OpenProcess($iPID)
	Local Const $aProcess = DllCall($hKernel32, "int", "OpenProcess", "int", 0x1F0FFF, "int", 1, "int", $iPID)

	Return $aProcess[0]
EndFunc   ;==>OpenProcess

Func ReadProcessMemory($hProcess, $iAddress, $aType = "dword")
	Local $dBuffer = DllStructCreate($aType)

	DllCall($hKernel32, "int", "ReadProcessMemory", "int", $hProcess, "int", $iAddress, "ptr", DllStructGetPtr($dBuffer), "int", DllStructGetSize($dBuffer), "int", "")

	Return DllStructGetData($dBuffer, 1)
EndFunc   ;==>ReadProcessMemory

Func ScanForPtr($hProcess, $sByteString, $iOffset = 0, $iStartAddr = 0x401000, $iEndAddr = 0x900000)
	Local Const $dQuery = DllStructCreate("dword;dword;dword;dword;dword;dword;dword")
	Local Const $iQueryPtr = DllStructGetPtr($dQuery)
	Local Const $iQuerySize = DllStructGetSize($dQuery)
	Local $iSearch, $sBinString, $dBuffer, $iBlockSize

	$sByteString = BinaryToString("0x" & $sByteString)

	While ($iStartAddr < $iEndAddr)
		DllCall($hKernel32, "int", "VirtualQueryEx", "int", $hProcess, "int", $iStartAddr, "ptr", $iQueryPtr, "int", $iQuerySize)

		If (DllStructGetData($dQuery, 5) = 4096) Then
			$dBuffer = DllStructCreate("byte[" & DllStructGetData($dQuery, 4) & "]")
			DllCall($hKernel32, "int", "ReadProcessMemory", "int", $hProcess, "int", $iStartAddr, "ptr", DllStructGetPtr($dBuffer), "int", DllStructGetSize($dBuffer), "int", "")

			$sBinString = BinaryToString(DllStructGetData($dBuffer, 1))
			$iSearch = StringInStr($sBinString, $sByteString, 2)

			If ($iSearch > 0) Then
				Return ("0x" & SwapEndianness(Hex(StringToBinary(StringMid($sBinString, ($iSearch + $iOffset), 4)))))
			EndIf
		EndIf

		$iBlockSize = DllStructGetData($dQuery, 4)

		If (Not $iBlockSize) Then
			ExitLoop
		Else
			$iStartAddr += $iBlockSize
		EndIf
	WEnd

	Return SetError(1, 0, "0x000000")
EndFunc   ;==>ScanForPtr

Func _InjectDll($ProcessId, $DllPath)
	If $ProcessId == 0 Then Return SetError(1, "", False)
	If Not (FileExists($DllPath)) Then Return SetError(2, "", False)
	If Not (StringRight($DllPath, 4) == ".dll") Then Return SetError(3, "", False)

	$Kernel32 = DllOpen("kernel32.dll")
	If @error Then Return SetError(4, "", False)

	$DLL_Path = DllStructCreate("char[255]")
	DllCall($Kernel32, "DWORD", "GetFullPathNameA", "str", $DllPath, "DWORD", 255, "ptr", DllStructGetPtr($DLL_Path), "int", 0)
	If @error Then Return SetError(5, "", False)

	$hProcess = DllCall($Kernel32, "DWORD", "OpenProcess", "DWORD", 0x1F0FFF, "int", 0, "DWORD", $ProcessId)
	If @error Then Return SetError(6, "", False)

	$hModule = DllCall($Kernel32, "DWORD", "GetModuleHandleA", "str", "kernel32.dll")
	If @error Then Return SetError(7, "", False)

	$lpStartAddress = DllCall($Kernel32, "DWORD", "GetProcAddress", "DWORD", $hModule[0], "str", "LoadLibraryA")
	If @error Then Return SetError(8, "", False)

	$lpParameter = DllCall($Kernel32, "DWORD", "VirtualAllocEx", "int", $hProcess[0], "int", 0, "ULONG_PTR", DllStructGetSize($DLL_Path), "DWORD", 0x3000, "int", 4)
	If @error Then Return SetError(9, "", False)

	DllCall("kernel32.dll", "BOOL", "WriteProcessMemory", "int", $hProcess[0], "DWORD", $lpParameter[0], "str", DllStructGetData($DLL_Path, 1), "ULONG_PTR", DllStructGetSize($DLL_Path), "int", 0)
	If @error Then Return SetError(10, "", False)

	$hThread = DllCall($Kernel32, "int", "CreateRemoteThread", "DWORD", $hProcess[0], "int", 0, "int", 0, "DWORD", $lpStartAddress[0], "DWORD", $lpParameter[0], "int", 0, "int", 0)
	If @error Then Return SetError(11, "", False)

	DllCall($Kernel32, "BOOL", "CloseHandle", "DWORD", $hProcess[0])
	DllClose($Kernel32)

	Return SetError(0, "", True)
EndFunc   ;==>_InjectDll

Func SwapEndianness($aHex)
	Return StringMid($aHex, 7, 2) & StringMid($aHex, 5, 2) & StringMid($aHex, 3, 2) & StringMid($aHex, 1, 2)
EndFunc   ;==>SwapEndianness

Func MsgBoxOK($aMsg)

	$Form2 = GUICreate("GW Hack Menu", 255, 176, -1, -1, $WS_POPUP, $WS_EX_TOPMOST)
	GUISetBkColor(0x000000, $Form2)
	GUICtrlSetDefColor(0xFFFFFF, $Form2)
	$Label1 = GUICtrlCreatePic("", 3,3, 252, 80,-1,$GUI_WS_EX_PARENTDRAG)

	If @Compiled Then
		_Resource_SetToCtrlID($Label1, 'LOGO_JPG')
	Else
		GUICtrlSetImage($Label1, "logo.jpg")
	EndIf

	$Label2 = GUICtrlCreateLabel($aMsg, 8, 88, 236, 41, $SS_CENTER)
	GUICtrlSetBkColor(-1, 0x000000)
	$Button1 = MyGuiCtrlCreateButton("OK", 136, 144, 113, 25)
	GUISetState(@SW_SHOW)

	While 1
		$nMsg = GUIGetMsg()
		Switch $nMsg
			Case $Button1
				GUIDelete($Form2)
				ExitLoop
		EndSwitch
	WEnd
EndFunc   ;==>MsgBoxOK

#Region GuiCreationFunctions
Func MyGuiCtrlCreateButton($sText, $iX, $iY, $iW, $iH, $iColor = 0xFFFFFF, $iBgColor = 0x222222, $iPenSize = 1, $iStyle = 0, $iStyleEx = 0)
	Return GuiCtrlCreateBorderLabel($sText, $iX, $iY, $iW, $iH, $iColor, $iBgColor, $iPenSize, BitOR($iStyle, $SS_CENTER, $SS_CENTERIMAGE), $iStyleEx)
EndFunc   ;==>MyGuiCtrlCreateButton

Func _GUICtrlTab_SetBkColor($hWnd, $hSysTab32, $sBkColor)
	Local $aTabPos = ControlGetPos($hWnd, "", $hSysTab32)
	Local $aTab_Rect = _GUICtrlTab_GetItemRect($hSysTab32, -1)
	GUICtrlCreateLabel("", $aTabPos[0] + 2, $aTabPos[1] + $aTab_Rect[3] + 4, $aTabPos[2] - 6, $aTabPos[3] - $aTab_Rect[3] - 7)
	GUICtrlSetBkColor(-1, $sBkColor)
	GUICtrlSetState(-1, $GUI_DISABLE)
EndFunc   ;==>_GUICtrlTab_SetBkColor

Func GuiCtrlCreateBorderLabel($sText, $iX, $iY, $iW, $iH, $iColor, $iBgColor, $iPenSize = 1, $iStyle = -1, $iStyleEx = 0)
	GUICtrlCreateLabel("", $iX - $iPenSize, $iY - $iPenSize, $iW + 2 * $iPenSize, $iH + 2 * $iPenSize, 0)
	GUICtrlSetState(-1, $GUI_DISABLE)
	GUICtrlSetBkColor(-1, $iColor)
	Local $nID = GUICtrlCreateLabel($sText, $iX, $iY, $iW, $iH, $iStyle, $iStyleEx)
	GUICtrlSetBkColor(-1, $iBgColor)
	Return $nID
EndFunc   ;==>GuiCtrlCreateBorderLabel

Func _GuiRoundCorners($h_win, $iSize) ; thanks gafrost
	Local $XS_pos, $XS_ret
	$XS_pos = WinGetPos($h_win)
	$XS_ret = DllCall("gdi32.dll", "long", "CreateRoundRectRgn", "long", 0, "long", 0, "long", $XS_pos[2] + 1, "long", $XS_pos[3] + 1, "long", $iSize, "long", $iSize)
	If $XS_ret[0] Then
		DllCall("user32.dll", "long", "SetWindowRgn", "hwnd", $h_win, "long", $XS_ret[0], "int", 1)
	EndIf
EndFunc   ;==>_GuiRoundCorners

Func GuiCtrlCreateRect($x, $y, $width, $height)
	GUICtrlCreateLabel("", $x, $y, $width, $height)
	GUICtrlSetBkColor(-1, $COLOR_WHITE)
	GUICtrlSetState(-1, $GUI_DISABLE)
EndFunc   ;==>GuiCtrlCreateRect

Func GuiCtrlUpdateData($hCtrl, $data)
	If GUICtrlRead($hCtrl) <> $data Then GUICtrlSetData($hCtrl, $data)
EndFunc   ;==>GuiCtrlUpdateData
#EndRegion GuiCreationFunctions
