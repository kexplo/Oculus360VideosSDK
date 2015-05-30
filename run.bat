@rem Run.bat for Oculus360VideosSDK
@if "%1" NEQ "debug" (
    @if "%1" NEQ "release" (
	    @if "%1" NEQ "clean" (
            @echo The first parameter to run.bat must be one of debug, release or clean.
            @goto:EOF
		)
	)
)
@call ..\build.cmd com.oculus.oculus360videossdk bin/Oculus360VideosSDK-debug.apk com.oculus.oculus360videossdk.MainActivity %1 %2
