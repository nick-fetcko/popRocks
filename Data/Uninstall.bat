REG DELETE "HKEY_CURRENT_USER\Software\Classes\Directory\shell\Visualize with popRocks" /f
REG DELETE "HKEY_CURRENT_USER\Software\Classes\Applications\popRocks.exe\DefaultIcon" /f
rmdir /S /Q "%APPDATA%\Fetcko\popRocks"
