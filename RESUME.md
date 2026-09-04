# 引継ぎ（2026-09-04 夜 — 始めたところ）

この節を最初に読めば続きから入れる。**まだ移植は始まっていない。**
今夜やったのは「インストーラを解体して、Ghidra で読める土台を作る」まで。

user の依頼: 「super depth の windows 版を ghidra で解析して wasm 版を
作って」。素材は `depth-build115.exe` 一つだけだった。

## いまの状態

* `python tools/unpack.py` で **45 ファイルが `disk/` に出る**（三層とも
  解いてある。[docs/format.md](docs/format.md)）。インストーラは走らせない
* Ghidra 12.1.3 で `disk/superdepth.exe` を解析済み。作り直しは下記
* `out/superdepth.c` 587 関数（**落ちた関数は 0**）、
  `out/superdepth.asm` 47311 命令。32bit の MSVC なので**逆コンパイルの
  質がとても良い** —— PC-98 の 16bit とは別世界で、ほぼそのまま読める

## 本体の見取り（`disk/superdepth.exe`）

PE32 / MSVC 6.0 / image base 0x400000 / entry 0x426b04（RVA 0x26b04）。
.text 242347、.data は仮想 848552（実体 36864 = 大きな BSS）、
.rsrc 20704、.reloc あり。

**移植の道が見えている部分:**

| | |
|---|---|
| 画面 | GDI の DIB。`CreateDIBitmap` `StretchDIBits` `SetDIBitsToDevice` `BitBlt` `CreatePalette` `RealizePalette` `GetSystemPaletteEntries`。**256 色のパレット画面**で、canvas に素直に乗る |
| 音 | `sndPlaySoundA`（WAV）と `mciSendCommandA`（MIDI）。`WINMM` の `midiOutGetNumDevs` `waveOutGetNumDevs` で有無を見る |
| 時間 | `timeGetTime` |
| 入力 | `GetAsyncKeyState`、`joyGetPos` / `joyGetDevCaps`（ジョイスティック） |
| 設定 | レジストリ `Software\Bio_100%\SuperDepth`、`rank%d` に得点表 |
| DirectX | **import に無い。`LoadLibraryA` + `GetProcAddress` で動的**。readme が「DirectX 3 以降が入っていればフルスクリーンと DirectSound」と書いている。**基準は GDI の道** |

自分の層に名前が付いている: `WinGL(%d-%d)` `WinGLScreenMode(%d)` が描画層、
`MIDI Sound Driver - SMFDrv` と `BMIDIPLAY_*` が音、`Pat*` がパターン
（`.dar`）。窓のクラス名は `SimpleWindow`。

デバッグ表示の書式が残っている: `%2dF SE:%s BGM:%s`、
`rankin = %d rcurX = %02d rcurY = %02d`、`Super Depth Top Score Ranking`。

## 次にやること（順）

1. **`.dar` の展開を確定させる。** docs/format.md の推測を
   `FUN_0041a3e0`（`PatBuildDAR`）と `FUN_004199c0`（ヘッダを見る所）、
   `FUN_00419700`（`PatEntryDAR`）で裏を取る。出せたら PNG に描いて確認
   （`tools/` に `dar.py` を作る）
2. **`stage3.bin`（"SDEPTH"）** の面データ
3. **WinGL** の移植 —— 画面の大きさ（readme は 640x480 以上、
   ウインドウモード推奨）と、DIB のパレットの扱いから
4. **ゲーム本体**。`WinMain` から追う
5. **音**。WAV は素の RIFF なのでそのまま鳴らせる。BGM は SMF なので
   ブラウザで鳴らすには合成器が要る（`soko_ban_wasm` の MMD2 は自前で
   書いた。ここは GM の SMF なので別問題。後回し）

## Ghidra（この機械に入っている）

パスはグローバルの `~/.claude/CLAUDE.md` にも控えてある。
**プロジェクトのディレクトリは先に作っておくこと**（無いと
`Directory not found` で落ちる）:

```sh
export JAVA_HOME='C:\prog\ghidra\jdk-21.0.12.1+1'
GH="C:\prog\ghidra\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat"

mkdir -p ghidra_proj out
cmd //c "$GH" ghidra_proj sd -import disk/superdepth.exe -overwrite     # 50 秒
cmd //c "$GH" ghidra_proj sd -process superdepth.exe -noanalysis \
    -scriptPath tools/ghidra -postScript DumpAsm.java out/superdepth.asm
cmd //c "$GH" ghidra_proj sd -process superdepth.exe -noanalysis \
    -scriptPath tools/ghidra -postScript DumpAll.java out/superdepth.c
```

`ghidra_proj/` と `out/` は .gitignore。DumpAll は 1 分ほど（16bit の
soko_ban では 15 分かかったが、こちらは速い）。

## 作業のしかた（守ること）

* **GUI を開かない。** PNG に描いて `Read` で見る
* **WebGL 禁止**（対象機に無い）。ソフトで描く
* **インストーラを走らせない。** 中身は解いて取り出す（もう解けている）
* **推測で埋めない。** 分からないところは `out/superdepth.c` を読み直すか、
  分からないと書く。「教科書どおり」は根拠ではない（soko_ban で
  プレーンとビットの対応をそれで間違えた）
* Bash のヒアドキュメントは**バックスラッシュを食う**。python のパッチは
  Write でファイルに書いてから走らせる
* ビルドは MSVC 直叩き（`tools/cc.sh`）。node は PATH に無い:
  `PATH="/c/prog/emsdk/emsdk/node/22.16.0_64bit/bin:$PATH"`

## この game について

Bio_100% の『SuperDepth』（1991、PC-98）の Windows 移植のベータ版。
Build 115 = 1999-02-14。`readme.txt` によると

* Win95 以降 / NT4 SP3 以降、256 色以上・640x480 以上
* **ベータなので 5 面以降は「遊べないステージばかり」**（Build 114 の note）
* オンラインアップデート機能つき（`ipatcfg.exe`、
  `http://bio.and.or.jp/cgi-bin2/sdepth`）
* HTML のマニュアルが同梱（`index.html`）

PC-98 版の移植 [super_depth_wasm](https://github.com/yomei-o/super_depth_wasm)
が別にある。**同じ game の別実装**なので、面データやパターンの並びは
比べる価値がある（あちらの docs が使えるかもしれない）。
