if exist ..\GWCA (
  rmdir /S /Q Dependencies\GWCA
  mklink /J Dependencies\GWCA ..\GWCA
)