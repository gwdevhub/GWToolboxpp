#RequireAdmin

#Include <WinAPI.au3>
#include <File.au3>
#include <ComboConstants.au3>
#include <GUIConstantsEx.au3>
#include <GuiComboBox.au3>

Global $mKernelHandle, $mGWProcHandle, $mCharname

Global $WinList = WinList("[CLASS:ArenaNet_Dx_Window_Class]")
Global $gwPID
Switch $WinList[0][0]
	Case 0
		MsgBox($MB_ICONERROR, "GWToolbox++", "Error: Guild Wars is not running")
		Exit
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

Global $processes = _ProcessGetLoadedModules($gwPID)

Global $gui = GUICreate("GW loaded modules list", 400, 300)
Global $edit = GUICtrlCreateEdit("", 0, 0, 400, 300)
GUICtrlSetData(-1, $processes)
GUISetState(@SW_SHOW, $gui)

For $i = 0 To UBound($processes)-1
	GUICtrlSetData($edit, $processes[$i] & @CRLF, 1)
Next

While GUIGetMsg() <> $GUI_EVENT_CLOSE
	Sleep(1)
WEnd


; #FUNCTION#;===============================================================================
;
; Name...........: _ProcessGetLoadedModules
; Description ...: Returns an array containing the full path of the loaded modules
; Syntax.........: _ProcessGetLoadedModules($iPID)
; Parameters ....:
; Return values .: Success - An array with all the paths
;               : Failure - -1 and @error=1 if the specified process couldn't be opened.
; Author ........: Andreas Karlsson (monoceres) & ProgAndy
; Modified.......:
; Remarks .......:
; Related .......:
; Link ..........;
; Example .......; No
;
;;==========================================================================================
Func _ProcessGetLoadedModules($iPID)
    Local Const $PROCESS_QUERY_INFORMATION=0x0400
    Local Const $PROCESS_VM_READ=0x0010
    Local $aCall, $hPsapi=DllOpen("Psapi.dll")
    Local $hProcess, $tModulesStruct
    $tModulesStruct=DllStructCreate("hwnd [200]")
    Local $SIZEOFHWND = DllStructGetSize($tModulesStruct)/200
    $hProcess=_WinAPI_OpenProcess(BitOR($PROCESS_QUERY_INFORMATION,$PROCESS_VM_READ),False,$iPID)
    If Not $hProcess Then Return SetError(1,0,-1)
    $aCall=DllCall($hPsapi,"int","EnumProcessModules","ptr",$hProcess,"ptr",DllStructGetPtr($tModulesStruct),"dword",DllStructGetSize($tModulesStruct),"dword*","")
    If $aCall[4]>DllStructGetSize($tModulesStruct) Then
        $tModulesStruct=DllStructCreate("hwnd ["&$aCall[4]/$SIZEOFHWND&"]")
        $aCall=DllCall($hPsapi,"int","EnumProcessModules","ptr",$hProcess,"ptr",DllStructGetPtr($tModulesStruct),"dword",$aCall[4],"dword*","")
    EndIf
    Local $aReturn[$aCall[4]/$SIZEOFHWND]
    For $i=0 To Ubound($aReturn)-1

$aCall=DllCall($hPsapi,"dword","GetModuleFileNameExW","ptr",$hProcess,"ptr",DllStructGetData($tModulesStruct,1,$i+1),"wstr","","dword",65536)
$aReturn[$i]=$aCall[3]

Next
    _WinAPI_CloseHandle($hProcess)
    DllClose($hPsapi)
    Return $aReturn
EndFunc


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
