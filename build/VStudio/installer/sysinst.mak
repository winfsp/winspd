# Input variables:
#     OutDir
#     ToolsDir
#     SourceInf
#     MyProductName
#     MyDescription
#     MyCompanyName
#     MyCopyright
#     MyVersion
#     MyBaseName

goal: \
	$(OutDir)sysinst\winspd.inf \
	$(OutDir)sysinst\winspd-x64.sys \
	$(OutDir)sysinst\winspd-x64.dll \
	$(OutDir)sysinst\winspd-x64.cat \
	$(OutDir)sysinst\winspd-x86.sys \
	$(OutDir)sysinst\winspd-x86.dll \
	$(OutDir)sysinst\winspd-x86.cat \
	$(OutDir)sysinst\devsetup-x64.exe \
	$(OutDir)sysinst\devsetup-x86.exe

$(OutDir)sysinst:
	mkdir "$@"

$(OutDir)sysinst\winspd-x64.sys: $(OutDir)sysinst $(OutDir)winspd-x64.sys
	copy /b "$(@D)\..\$(@F)" + nul "$@" >nul
$(OutDir)sysinst\winspd-x64.dll: $(OutDir)sysinst $(OutDir)winspd-x64.dll
	copy /b "$(@D)\..\$(@F)" + nul "$@" >nul
$(OutDir)sysinst\winspd-x86.sys: $(OutDir)sysinst $(OutDir)winspd-x86.sys
	copy /b "$(@D)\..\$(@F)" + nul "$@" >nul
$(OutDir)sysinst\winspd-x86.dll: $(OutDir)sysinst $(OutDir)winspd-x86.dll
	copy /b "$(@D)\..\$(@F)" + nul "$@" >nul
$(OutDir)sysinst\devsetup-x64.exe: $(OutDir)sysinst $(OutDir)devsetup-x64.exe
	copy /b "$(@D)\..\$(@F)" + nul "$@" >nul
$(OutDir)sysinst\devsetup-x86.exe: $(OutDir)sysinst $(OutDir)devsetup-x86.exe
	copy /b "$(@D)\..\$(@F)" + nul "$@" >nul

$(OutDir)sysinst\winspd.inf: $(OutDir)sysinst $(SourceInf)
	call "$(ToolsDir)mkinf.bat" \
		MyProductName:"$(MyProductName)" \
		MyDescription:"$(MyDescription)" \
		MyCompanyName:"$(MyCompanyName)" \
		MyCopyright:"$(MyCopyright)" \
		MyVersion:$(MyVersion) \
		MyBaseName:$(MyBaseName) \
		DllFilesWow64:dll.files.wow64 -- \
		$(SourceInf) "$@"

$(OutDir)sysinst\winspd-x64.cat: \
	$(OutDir)sysinst\winspd.inf \
	$(OutDir)sysinst\winspd-x64.sys \
	$(OutDir)sysinst\winspd-x64.dll \
	$(OutDir)sysinst\winspd-x86.dll
	call "$(ToolsDir)mkcat.bat" \
		x64 "$(@D)" winspd.inf winspd-x64.sys winspd-x64.dll winspd-x86.dll

$(OutDir)sysinst\winspd-x86.cat: \
	$(OutDir)sysinst\winspd.inf \
	$(OutDir)sysinst\winspd-x86.sys \
	$(OutDir)sysinst\winspd-x86.dll
	call "$(ToolsDir)mkcat.bat" \
		x86 "$(@D)" winspd.inf winspd-x86.sys winspd-x86.dll

clean:
	rmdir /s/q $(OutDir)sysinst
