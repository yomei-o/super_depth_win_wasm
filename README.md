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

**遊べます: https://yomei-o.github.io/super_depth_win_wasm/**

<kbd>←</kbd><kbd>→</kbd> で艦を動かし、<kbd>Z</kbd> で船首側、<kbd>X</kbd>
で船尾側に爆雷を落とす。<kbd>Esc</kbd> でポーズ。1 分放っておくと原作の
録画（`demo1.dat`）が再生される。

同じやり方の前作: [soko_ban_wasm](https://github.com/yomei-o/soko_ban_wasm)、
[lord_monarch_wasm](https://github.com/yomei-o/lord_monarch_wasm)、
PC-98 版の [super_depth_wasm](https://github.com/yomei-o/super_depth_wasm)

## いまの状態

**ひと勝負が最後まで通る。** ロゴ → タイトル → ゲーム → 面クリア →
ゲームオーバー → 名前入力 → 得点表 → タイトル。

* [x] **インストーラの解体** — `depth-build115.exe` から 45 ファイル
      （`python tools/unpack.py`、`disk/` に出る。三層ぜんぶ解いてあり、
      インストーラは走らせない。[docs/format.md](docs/format.md)）
* [x] **本体の見取り** — `disk/superdepth.exe` は PE32、MSVC 6.0、
      344KB（.text 242KB / 610 関数）。**描画は GDI の DIB**
      （`CreateDIBitmap` / `StretchDIBits` / `SetDIBitsToDevice` +
      `CreatePalette`）、音は `sndPlaySoundA`（WAV）と `mciSendCommandA`
      （MIDI）、入力は `GetAsyncKeyState` と `joyGetPos`。
      **DirectX は import に無く、`LoadLibraryA` で動的に読む** ——
      つまり移植の基準になる道は GDI のほうで、これは canvas に素直に乗る
* [x] **`.dar`（パターンの書庫）** — `FUN_00419700` と `FUN_0041a590` の
      とおりに読める。6 本とも末尾にぴったり着き、**画素は「透明の数・
      不透明の数・画素（4 の倍数に詰める）」の走の並び**
      （`python tools/dar.py disk/staff.dar sheet.png`）
* [x] **描画層** — `src/video.c`。640x480 の 8bpp、パターン描画（`y + 0x20`
      と原作のクリップつき）、原作のフォントの引きかた（`base + ASCII`）、
      行ごとに正弦でずらす描画（`vid_pat_wave`）
* [x] **BGM** — 原作の SMF を自前で合成する（`src/smf.c` / `src/synth.c` は
      user 自身の [windepth_wasm](https://github.com/yomei-o/windepth_wasm)
      から。同じ Bio_100% の WinDepth 用に書かれたもの）
* [x] **効果音** — 名前で引く 12 本（`disk/*.wav`）。ページが fetch して
      鳴らす。位置による左右も原作どおり
* [x] **状態機械**（`src/game.c`）— Bio ロゴ、海のタイトルとメニュー、
      記録画面、版とクレジット、デモの再生
* [x] **ゲーム本体**（`src/play.c`）— 艦・爆雷・敵 6 種・魚雷・砲弾・
      アイテム 7 種・連鎖・得点とポップアップ・レーダー・ステータス表示・
      面クリアの演出・ポーズ・ゲームオーバーの名前入力
* [ ] **面クリアの次のモード**（`FUN_0040c9e0`、1329 行）。空の面らしい
      （`disk/depth3.jpg` / `depth4.jpg` の画面）
* [ ] 得点表の保存（原作はレジストリ）
* [ ] `stage3.bin` — **本体はこれを読まない**（面の敵は `0x43fae8` の表）

## 検査

```
sh tools/build.sh check
```

native の道具と検査を全部作って走らせ、PNG を `tmp/` に吐く。

| | |
|---|---|
| `tmp/dar_check.exe` | 6 本の `.dar` を歩いて末尾に着くか、行の走が幅に足りるか |
| `tmp/logo_check.exe` | Bio ロゴ（184 行の出し入れ、枠、ボタンで飛ばす） |
| `tmp/title_check.exe` | タイトル、メニュー、記録画面、デモへの落ち |
| `tmp/play_check.exe` | 面の作り、艦、爆雷と当たり、面クリア、ポーズ、名前入力、デモ再生 |
| `tests/wasm_check.js` | WASM を node で動かして 1 枚描き、音が出ているか |

絵は `tmp/sd_shot.exe` で見る:

```
tmp/sd_shot.exe play  disk/depth.dar tmp/a.png 200   200 フレーム遊んだ画面
tmp/sd_shot.exe game  disk/depth.dar tmp/b.png 30    状態機械を 30 フレーム
tmp/sd_shot.exe sheet disk/depth1.dar tmp/c.png      パターン一覧
tmp/sd_shot.exe list  disk/depth.dar                 名前と大きさ
```

## 取り出したもの（`disk/`）

| | |
|---|---|
| `superdepth.exe` | 344064 バイト。本体 |
| `depth.dar` | 1335036。パターン 2887 枚 |
| `depth1.dar` / `depth2.dar` | 132072 ずつ。海と空のタイル |
| `space.dar` / `ending.dar` / `staff.dar` | 星雲・地球・Bio_100% のロゴ |
| `stage3.bin` | 6632。"SDEPTH" で始まる面データ（本体は読まない） |
| `bgm01`..`bgm15.mid`, `finst1.mid` | BGM（SMF） |
| `*.wav` | 効果音（RIFF）。表にある `plane` だけ入っていない |
| `depth2..5.jpg`, `ielike.gif`, `index.html` | HTML マニュアル |
| `demo1.dat` | デモの記録（1 フレーム 1 バイト） |
| `ipatcfg.exe`, `ipatinfo.txt`, `readme.txt` | オンラインアップデート関係と説明 |

## 道具

```
python tools/unpack.py                      インストーラから disk/ を作る
sh tools/cc.sh -O2 -Itools -o tmp/unlib.exe tools/unlib.c tools/blast.c
```

`tools/blast.c` / `blast.h` は Mark Adler の blast（zlib contrib、PKWare の
implode を解く）。`tools/unlib.c` がそれを呼ぶ小さな口。

Ghidra の作り直しは [RESUME.md](RESUME.md) に手順がある。
**`tools/ghidra/MakeThunkFuncs.java` を先に流すこと** —— 0x401005 の JMP 表
の飛び先は関数ポインタ経由でしか呼ばれず、素の解析では関数にならない。

## 決まりごと

* **GUI の窓を開かない。** 確認は PNG に描いて読む
* **WebGL 禁止**（対象機に無い）
* **インストーラは走らせない。** 中身は解いて取り出す
* **推測で埋めない。** 分からない所は `out/superdepth.c` を読み直すか、
  分からないと書く
* Bash のヒアドキュメントはバックスラッシュを食う。`\n` を含むものは
  Write/Edit で書く
* `depth-build115.exe` は git に入れない（`tools/unpack.py` で再現できる）。
  `disk/` の中身は入れる
