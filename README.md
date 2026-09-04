# Super Depth for Windows — WASM 移植

Bio_100% の『SuperDepth for Windows』（Build 115、1999-02-14）を解析して
C で書き直し、ブラウザで動かす。

```
Super Depth ver1.00a Copyright(c)1991 Hideki Mori and Yasuo Futatsugi
Copyright(C)1991 alty & tacox / Bio_100%
Copyright(C)1998-1999 Bio_100% Inc.
```

エミュレータではない。実行形式を Ghidra と自作の道具で読み、何をしている
関数なのかを確かめてから、同じ振る舞いをする C を手で書く。描画はソフト
ウェアラスタライズのみ（**WebGL は使わない**）。

同じやり方の前作: [soko_ban_wasm](https://github.com/yomei-o/soko_ban_wasm)、
[lord_monarch_wasm](https://github.com/yomei-o/lord_monarch_wasm)、
PC-98 版の [super_depth_wasm](https://github.com/yomei-o/super_depth_wasm)

## いまの状態

**インストーラから中身を全部取り出して、Ghidra で読める状態にしたところ。**
移植そのものはまだ始まっていない。

* [x] **インストーラの解体** — `depth-build115.exe` から 45 ファイル
      （`python tools/unpack.py`、`disk/` に出る。三層ぜんぶ解いてあり、
      インストーラは走らせない。[docs/format.md](docs/format.md)）
* [x] **本体の見取り** — `disk/superdepth.exe` は PE32、MSVC 6.0、
      344KB（.text 242KB / 587 関数）。**描画は GDI の DIB**
      （`CreateDIBitmap` / `StretchDIBits` / `SetDIBitsToDevice` +
      `CreatePalette`）、音は `sndPlaySoundA`（WAV）と `mciSendCommandA`
      （MIDI）、入力は `GetAsyncKeyState` と `joyGetPos`。
      **DirectX は import に無く、`LoadLibraryA` で動的に読む** ——
      つまり移植の基準になる道は GDI のほうで、これは canvas に素直に乗る
* [x] Ghidra で解析（`out/superdepth.c` 587 関数、`out/superdepth.asm`
      47311 命令。`out/` は clone に入らないので RESUME の手順で作り直す）
* [x] **`.dar`（パターンの書庫）の読みかた** — `FUN_00419700` そのままで
      6 本とも末尾にぴったり着く（`python tools/dar.py disk/depth1.dar`）。
      素の 8bpp は PNG に描けている（海のタイル、Bio_100% のロゴ）。
      1 画素 1 バイトでない種類が残っている（[docs/format.md](docs/format.md)）
* [ ] `stage3.bin`（"SDEPTH"）の面データ
* [ ] WinGL（本体の描画層）の移植
* [ ] 音（WAV は素の RIFF、BGM は SMF。ブラウザで MIDI をどう鳴らすかは未定）
* [ ] ゲーム本体

## 取り出したもの（`disk/`）

| | |
|---|---|
| `superdepth.exe` | 344064 バイト。本体 |
| `depth.dar` | 1335036。パターン 2887 枚 |
| `depth1.dar` / `depth2.dar` | 132072 ずつ |
| `space.dar` / `ending.dar` / `staff.dar` | 背景・エンディング・スタッフ |
| `stage3.bin` | 6632。"SDEPTH" で始まる面データ |
| `bgm01`..`bgm15.mid`, `finst1.mid` | BGM（SMF） |
| `*.wav` | 効果音（RIFF） |
| `depth2..5.jpg`, `ielike.gif`, `index.html` | HTML マニュアル |
| `demo1.dat` | デモの記録 |
| `ipatcfg.exe`, `ipatinfo.txt`, `readme.txt` | オンラインアップデート関係と説明 |

## 道具

```
python tools/unpack.py                      インストーラから disk/ を作る
sh tools/cc.sh -O2 -Itools -o tmp/unlib.exe tools/unlib.c tools/blast.c
```

`tools/blast.c` / `blast.h` は Mark Adler の blast（zlib contrib、PKWare の
implode を解く）。`tools/unlib.c` がそれを呼ぶ小さな口。

## 決まりごと

* **GUI の窓を開かない。** 確認は PNG に描いて読む
* **WebGL 禁止**（対象機に無い）
* **インストーラは走らせない。** 中身は解いて取り出す
* Bash のヒアドキュメントはバックスラッシュを食う。`\n` を含むものは
  Write/Edit で書く
* `depth-build115.exe` は git に入れない（`tools/unpack.py` で再現できる）。
  `disk/` の中身は入れる
