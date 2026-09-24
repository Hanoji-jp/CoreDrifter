@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM release.bat
REM   Distribute ビルドを作って、GitHub のリリースへ上げる。
REM   ゲーム内の自動更新(HjUpdater)がこれを見つけて落としてくる。
REM
REM   使い方:  tools\release.bat v1.0.1
REM            (引数なしで走らせると聞かれる)
REM
REM   要るもの: Visual Studio(MSBuild) と GitHub CLI(gh auth login)
REM
REM   ■ ソースの置き場とリリースの置き場は別
REM     バージョン管理 … Hanoji-jp/CoreDrifter (git のリモート)
REM     リリース       … Hanoji-jp/DRIFT-PROJECT (ここ)
REM
REM     REPO を変えるときは Src/Application/Const/UpdaterConst.h の
REM     Owner / Repo も必ず合わせること。片方だけ変えると、
REM     上げた先を誰も見に行かない。
REM ============================================================

set "REPO=Hanoji-jp/DRIFT-PROJECT"
set "ROOT=%~dp0.."
set "OUTDIR=%ROOT%\x64\Distribute"
set "NOTES=%~dp0release_notes_tmp.md"
set "STAGE=%~dp0_release_stage"

REM ---- 版 ----
set "VERSION=%~1"
if "%VERSION%"=="" set /p VERSION="version (例 v1.0.1): "
if "%VERSION%"=="" (
    echo [ERROR] 版が指定されていません。
    pause & exit /b 1
)
REM git のタグに空白は使えない。弾いておく
if not "%VERSION%"=="%VERSION: =%" (
    echo [ERROR] 版に空白は使えません。v1.0.1 のように書いてください。
    pause & exit /b 1
)
set "ZIP=%~dp0DriftProject_%VERSION%.zip"

echo [INFO] 版      : %VERSION%
echo [INFO] 上げ先  : %REPO%
echo [INFO] 出力    : %OUTDIR%
echo.

REM ---- リリースの説明(メモ帳が開く) ----
> "%NOTES%" echo ## %VERSION%
>>"%NOTES%" echo.
>>"%NOTES%" echo ### 変更
>>"%NOTES%" echo -
echo [INFO] メモ帳に変更点を書いて、保存して閉じてください。
notepad "%NOTES%"

REM ---- version.txt を書く(アップデータが比べる相手) ----
REM     zip の中にも入れる。入れ忘れると展開後に v0.0.0 扱いになり、
REM     毎回「更新があります」になる
> "%ROOT%\version.txt" echo %VERSION%
echo [INFO] version.txt = %VERSION%

REM ---- 上げ先が使えるか先に見ておく ----
REM     リリースはタグに紐づくので、コミットが1つも無いリポジトリには作れない。
REM     ビルドとzipを全部終えてから気づくと、そのぶんが丸ごと無駄になる
echo [INFO] 上げ先を確かめています...
gh api "repos/%REPO%/commits?per_page=1" >NUL 2>&1
if errorlevel 1 (
    echo [ERROR] 上げ先 %REPO% が使えません。よくある原因:
    echo         - リポジトリが空^(コミットが1つも無い^)
    echo           リリースはタグに紐づくので、最初のコミットが要ります。
    echo           GitHub で README を1つ作るだけで足ります。
    echo         - リポジトリ名が違う / 権限が無い
    echo         - gh の認証切れ ... gh auth login
    pause ^& exit /b 1
)

REM ---- Distribute をビルド ----
echo [INFO] MSBuild を探しています...
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo [ERROR] MSBuild が見つかりません。Visual Studio の C++ を入れてください。
    pause & exit /b 1
)

REM ライブラリ(DirectXTK など)は Release のものを使う。
REM 先に Release を通しておかないと、リンクで転ぶ
echo [INFO] ライブラリを用意しています...
"%MSBUILD%" "%ROOT%\BaseFramework.sln" /p:Configuration=Release /p:Platform=x64 /nr:false /v:minimal
if errorlevel 1 (
    echo [ERROR] Release のビルドに失敗しました。
    pause & exit /b 1
)

echo [INFO] Distribute^|x64 をビルドしています...
"%MSBUILD%" "%ROOT%\Project.vcxproj" /p:Configuration=Distribute /p:Platform=x64 /nr:false /v:minimal
if errorlevel 1 (
    echo [ERROR] ビルドに失敗しました。
    pause & exit /b 1
)
if not exist "%OUTDIR%\Project.exe" (
    echo [ERROR] %OUTDIR% に Project.exe がありません。
    pause & exit /b 1
)

REM ---- 配るものを集める ----
REM     zip の一番上に置く。展開すると exe の隣に並ぶ形にしたいので、
REM     フォルダで包まない
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"

copy /Y "%OUTDIR%\Project.exe" "%STAGE%\Project.exe" >NUL
copy /Y "%ROOT%\version.txt"   "%STAGE%\version.txt" >NUL

REM Asset は同梱しない。
REM     Distribute ビルドは Asset/ を assets.pak にまとめて exe へ
REM     埋め込んでいる(tools\pack_assets.ps1 + Src\Project.rc)。
REM     フォルダで置くと、モデルも音もそのまま持っていかれる。
REM
REM     MOD だけは遊ぶ側が入れるものなので、空の入れ物を置いておく(Asset の外)
mkdir "%STAGE%\Mods\Body" 2>NUL
mkdir "%STAGE%\Mods\Wheel" 2>NUL

REM Steam の DLL は exe へ埋め込めない。
REM     アセットと違って、OS が読み込み時に解決するので隣に要る。
REM     無いと「steam_api64.dll が見つかりません」で起動しない。
REM
REM     ビルド出力にあるものを使う。プロジェクトの根から取ると、
REM     そちらだけ古いときに気づけない
if exist "%OUTDIR%\steam_api64.dll" (
    copy /Y "%OUTDIR%\steam_api64.dll" "%STAGE%\steam_api64.dll" >NUL
) else (
    copy /Y "%ROOT%\steam_api64.dll" "%STAGE%\steam_api64.dll" >NUL
)
if not exist "%STAGE%\steam_api64.dll" (
    echo [ERROR] steam_api64.dll が見つかりません。これが無いと起動しません。
    pause & exit /b 1
)

if exist "%OUTDIR%\steam_appid.txt" copy /Y "%OUTDIR%\steam_appid.txt" "%STAGE%\steam_appid.txt" >NUL

REM ---- zip ----
echo [INFO] zip を作っています...
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -Force"
rmdir /s /q "%STAGE%"
if not exist "%ZIP%" (
    echo [ERROR] zip を作れませんでした。
    pause & exit /b 1
)

REM ---- GitHub のリリースを作る ----
echo [INFO] リリースを作っています...
gh release create "%VERSION%" "%ZIP%" --repo %REPO% --title "Release %VERSION%" --notes-file "%NOTES%"
if errorlevel 1 (
    echo [ERROR] リリースの作成に失敗しました。よくある原因:
    echo         - 上げ先が空^(コミットが1つも無い^)。README を1つ作れば直ります
    echo         - 同じ版が既にある。別の版にするか、先に消してください
    echo         - gh の認証切れ ... gh auth login
    del "%ZIP%"   2>NUL
    del "%NOTES%" 2>NUL
    pause & exit /b 1
)

echo.
echo [SUCCESS] %VERSION% を公開しました。
echo https://github.com/%REPO%/releases/tag/%VERSION%
del "%ZIP%"   2>NUL
del "%NOTES%" 2>NUL
pause
endlocal
