# ============================================================
# pack_assets.ps1
#   Asset/ 以下を 1 つの assets.pak にまとめる。
#   Distribute 構成の PreBuildEvent から呼ばれ、出来た assets.pak が
#   Src/Project.rc 経由で exe へ埋め込まれる。
#
#   ■ 何のためか
#   アセットをフォルダで配ると、モデルも音も持っていかれる。
#   借りているものもあるので、そのまま置いておけない。
#
#   ■ 形式(リトルエンディアン)
#     'K''P''A''K'      : 目印 (4 byte)
#     uint32 version    : = 1
#     uint32 count      : ファイル数
#     [count 回]
#       uint32 pathLen  : 相対パスのバイト数
#       byte[] path     : "Asset/Data/silvia.gltf" のような相対パス
#       uint32 dataLen  : 中身のバイト数
#       byte[] data     : 中身
# ============================================================
$ErrorActionPreference = "Stop"

# プロジェクトの根 = このスクリプトの1つ上(tools の親)
$root     = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root "Asset"
$outPak   = Join-Path $root "assets.pak"

if (-not (Test-Path $assetDir)) {
    Write-Error "Asset フォルダが見つかりません: $assetDir"
    exit 1
}

# 遊ぶ側が書き換えるものは同梱しない。
# 埋め込むと、pak の中の古い値をいつまでも読むことになる
# ※ WindowSettings.csv はここへ入れないこと。
#    ゲームが書き換えるものではなく、起動時に読むだけの設定。
#    外すと、配布ビルドが窓の大きさを読めずに落ちる
# DEM(基盤地図情報の標高)は使っていない。
# 地形は手で彫ったもの(height_edit.r32)。
# 取り込みからやり直したくなったときのために手元には残すが、
# 配るものへ入れる意味は無い(8.7MB)
$excludeNames = @(
    "height.r32",
    "PlayerProfile.txt",
    "AudioSettings.txt",
    "StageChoice.txt",
    "CarChoice.txt",
    "ModProfiles.json",
    "imgui.ini"
)

# 作業用のゴミ。元データや控えは配らない
$junkPattern = '\.(pak|bak|bak_.+|blend\d*|blend1|preedit|psd|xcf)$'

# ※ height_edit.r32 は外さないこと。
#    「作業中のもの」ではなく、彫った地形そのもの＝配るマップ。
#    外すと、配布ビルドが真っ平らな地面で始まる

$files = Get-ChildItem -Path $assetDir -Recurse -File | Where-Object {
    ($excludeNames -notcontains $_.Name) -and
    ($_.Name -notmatch $junkPattern) -and
    # MOD は exe の隣の Mods/ へ移した。
    # 手元に古い Asset/Mods が残っていても埋め込まない
    ($_.FullName -notmatch '\\Asset\\Mods\\')
}

# まず平文で書き出す。FileStream なら PS5.1 でも 7 でも同じに動く
$fs = [System.IO.File]::Open($outPak, [System.IO.FileMode]::Create)
$bw = New-Object System.IO.BinaryWriter($fs)
try {
    $bw.Write([byte[]]@(0x4B, 0x50, 0x41, 0x4B))   # "KPAK"
    $bw.Write([uint32]1)                           # version
    $bw.Write([uint32]$files.Count)                # count

    # パスは CP932 で入れる。
    # 実行時の narrow 文字列リテラルが MSVC により CP932 になるので、
    # 日本語名のファイルでもキーが一致する
    $enc932 = [System.Text.Encoding]::GetEncoding(932)

    foreach ($f in $files) {
        $rel = $f.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
        $pathBytes = $enc932.GetBytes($rel)
        $data      = [System.IO.File]::ReadAllBytes($f.FullName)

        $bw.Write([uint32]$pathBytes.Length)
        $bw.Write($pathBytes)
        $bw.Write([uint32]$data.Length)
        $bw.Write($data)
    }
}
finally {
    $bw.Flush(); $bw.Dispose(); $fs.Dispose()
}

# XOR で撹拌する。AssetCrypt.h と同じ式にすること。
# 片方だけ直すと、読めない pak が出来上がる。
#
# PowerShell のループで数百MBを回すと待たされるので、C# を借りる
Add-Type -TypeDefinition @'
public static class PakXor {
    public static void Apply(byte[] d) {
        byte[] K = { 0x5A, 0xC3, 0x91, 0x2E, 0x7F, 0xB4, 0x68, 0xD1 };
        for (long i = 0; i < d.LongLength; i++) {
            d[i] ^= (byte)(K[i & 7] ^ (byte)(i * 31 + 7));
        }
    }
}
'@
$bytes = [System.IO.File]::ReadAllBytes($outPak)
[PakXor]::Apply($bytes)
[System.IO.File]::WriteAllBytes($outPak, $bytes)

# MSBuild は assets.pak が変わっても rc.exe を回し直さない(Project.rc 自体は
# 変わっていないため)。埋め込み直させるために、rc の日付を更新する
$rc = Join-Path $root "Src\Project.rc"
if (Test-Path $rc) { (Get-Item $rc).LastWriteTime = Get-Date }

$sizeMB = [math]::Round((Get-Item $outPak).Length / 1MB, 2)
Write-Host "[pack_assets] $($files.Count) 個 -> assets.pak ($sizeMB MB)"
