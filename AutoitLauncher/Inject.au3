#RequireAdmin
#include <Array.au3>
#include <GUIConstantsEx.au3>
#include <File.au3>
#include <WinAPIProc.au3>

Global $mKernelHandle, $mGWProcHandle, $mGWProcId

If Not InjectGW() Then MsgBox(0, "Inject Error", "There was an error in injection, code: " & @error)

Func InjectGW()
	Local $lWinList = WinList("[CLASS:ArenaNet_Dx_Window_Class]")

	Switch $lWinList[0][0]
		Case 0
			Exit MsgBox(0, "Injector - Err", "Cannot find an open Guild Wars client.")
		Case Else
			Local $lComboStr
			Local $lCharArray[$lWinList[0][0]][2]
			Local $lFirstChar
			Local $lCharNameRva

			For $i = 1 To $lWinList[0][0]
				Local $lPID = WinGetProcess($lWinList[$i][1])
				MemoryOpen($lPID)
				Local $lModuleBase, $lModuleSize
				GetModuleInfo($lModuleBase, $lModuleSize)
				If $i = 1 Then
					$lCharNameRva = ScanForCharname($lModuleBase, $lModuleSize)
					$lFirstChar = MemoryRead($lModuleBase + $lCharNameRva, 'wchar[30]')
					$lCharArray[$i - 1][0] = $lFirstChar
				Else
					$lCharArray[$i - 1][0] = MemoryRead($lModuleBase + $lCharNameRva, 'wchar[30]')
				EndIf
                $lCharArray[$i - 1][1] = $mGWProcId
				MemoryClose()

				$lComboStr &= $lCharArray[$i - 1][0]
				If $i <> $lWinList[0][0] Then $lComboStr &= '|'
			Next
			DllClose($mKernelHandle)

			Local $lDLLList = _FileListToArray(@ScriptDir, '*.dll', 1)
			If Not IsArray($lDLLList) Then Exit MsgBox(0, "Injector - Err", "Cannot find any DLL's to inject.")

			Local $mGUI = GUICreate("GW Injector", 161, 109)

			Local $comboCharname = GUICtrlCreateCombo("", 9, 11, 142, 25, 3)
			GUICtrlSetData(-1, $lComboStr, $lFirstChar)

			Local $comboDLL = GUICtrlCreateCombo("", 9, 40, 142, 25, 3)
			GUICtrlSetData(-1, _ArrayToString($lDLLList,'|',1), $lDLLList[1])

			Local $buttonRun = GUICtrlCreateButton("Inject", 9, 67, 142, 24)

			GUISetState(@SW_SHOW, $mGUI)

			Local $lWait = True
			While $lWait
				Switch GUIGetMsg()
					Case $buttonRun
						$lWait = False
					Case -3
						Exit
				EndSwitch
				Sleep(20)
			WEnd
			Local $lTmp
			For $i = 0 To UBound($lCharArray) - 1
				$lTmp = GUICtrlRead($comboCharname)
				If $lCharArray[$i][0] = $lTmp Then
					$lTmp = $lCharArray[$i][1]
					ExitLoop
				EndIf
			Next

			Return _InjectDll($lTmp,GUICtrlRead($comboDLL))
	EndSwitch
EndFunc   ;==>InjectGW

Func MemoryOpen($aPID)
	$mKernelHandle = DllOpen('kernel32.dll')
	Local $lOpenProcess = DllCall($mKernelHandle, 'int', 'OpenProcess', 'int', 0x1F0FFF, 'int', 1, 'int', $aPID)
	$mGWProcHandle = $lOpenProcess[0]
	$mGWProcId = $aPID
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

Func GetModuleInfo(ByRef $aModuleBase, ByRef $aModuleSize)
	Local $lProcName = _WinAPI_GetProcessName($mGWProcId)
	Local $lProcNameLength = StringLen($lProcName)

	Local $lModules = _WinAPI_EnumProcessModules($mGWProcId)
	For $i = 1 To $lModules[0][0]
		Local $lModuleName = StringRight($lModules[$i][1], $lProcNameLength)
		If StringCompare($lModuleName, $lProcName, $STR_NOCASESENSE) = 0 Then
			Local $lModuleInfo = _WinAPI_GetModuleInformation($mGWProcHandle, $lModules[$i][0])
			$aModuleBase = DllStructGetData($lModuleInfo, 'BaseOfDll')
			$aModuleSize = DllStructGetData($lModuleInfo, 'SizeOfImage')
			Return True
		EndIf
	Next
	Return False
EndFunc   ;==>GetModuleInfo

Func ScanForCharname($aModuleBase, $aModuleSize)
	Local $lCharNameCode = BinaryToString('0x8BF86A03680F0000C08BCFE8')
	Local $lCurrentSearchAddress = $aModuleBase
	Local $lMBI[7], $lMBIBuffer = DllStructCreate('dword;dword;dword;dword;dword;dword;dword')
	Local $lSearch, $lTmpMemData, $lTmpAddress, $lTmpBuffer = DllStructCreate('ptr'), $i

	While $lCurrentSearchAddress < $aModuleBase + $aModuleSize
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
				DllCall($mKernelHandle, 'int', 'ReadProcessMemory', 'int', $mGWProcHandle, 'int', $lTmpAddress - 0x42, 'ptr', DllStructGetPtr($lTmpBuffer), 'int', DllStructGetSize($lTmpBuffer), 'int', '')
				Return DllStructGetData($lTmpBuffer, 1) - $aModuleBase
			EndIf

			$lCurrentSearchAddress += $lMBI[3]
		EndIf
	WEnd

	Return False
EndFunc   ;==>ScanForCharname

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
