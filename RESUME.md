# 引継ぎ（2026-09-05 未明 — タイトルまで動く）

この節を最初に読めば続きから入れる。**移植は始まっている**:
インストーラを解体し、Ghidra で読める土台を作り、
**Bio ロゴ（0x10）→ 海のタイトル（0x1e）とそのメニュー**が
原作どおりに動く。

user の依頼: 「super depth の windows 版を ghidra で解析して wasm 版を
作って」。素材は `depth-build115.exe` 一つだけだった。

## いまの状態

* `python tools/unpack.py` で **45 ファイルが `disk/` に出る**（三層とも
  解いてある。[docs/format.md](docs/format.md)）。インストーラは走らせない
* Ghidra 12.1.3 で `disk/superdepth.exe` を解析済み。作り直しは下記
* `out/superdepth.c` 587 関数（**落ちた関数は 0**）、
  `out/superdepth.asm` 47311 命令。32bit の MSVC なので**逆コンパイルの
  質がとても良い** —— PC-98 の 16bit とは別世界で、ほぼそのまま読める
* **`.dar` は読める**（`src/dar.c`、検査つき）。**描画層もある**
  （`src/video.c`：640x480 の 8bpp、パターン、原作のフォント）
* **BGM は鳴る**（`src/smf.c` / `src/synth.c`。user 自身の
  [windepth_wasm](https://github.com/yomei-o/windepth_wasm) から）
* **状態機械が動く**（`src/game.c`）。0x0a → 0x10（Bio ロゴ）→ 0x1e
  （海のタイトル、メニュー・スタッフの流れる字・泳ぐ潜水艦）→
  Game Start で 0x32、放置 1 分でデモ（0x33）、Exit で 0x5a。
  検査は `tmp/logo_check.exe` と `tmp/title_check.exe`（PNG も出る）
* **WASM とページもある**（`index.html` / `superdepth.js`）。ゲームが
  そのまま動き、`0`/`1`/`2` で展示画面にも切り替わる:
  https://yomei-o.github.io/super_depth_win_wasm/
* **記録画面（FUN_00414b00）と 0x32 から先は未移植。** 下に書いた

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

## 本体の作り（読めたところ）

### 毎フレームの流れ — `FUN_004209a0`

窓オブジェクトの毎フレーム処理。WinGL の側。

```
FUN_00430f50(pad, out)        キー表を舐めて pad のビットを作る（下記）
  → DAT_004bf83c.. に 1 ビットずつばらす
GetLocalTime / GetCursorPos   → これも global へ
thunk_FUN_00401500(surface)   ★ゲームの 1 フレーム（下記）
FUN_00422410 / FUN_00425f80
画面の端 2 本を消す
必要なら解像度を変える        DAT_004bf8bc: 0=640x480 1=320x240 2=1280x960
FUN_00417e70(surface, hdc)    ★present（DC へ転送）
DAT_004bf820 = (frame+1) & 0x800000ff   ★フレーム数
DAT_004bf810[0xd7] を DAT_004bfc18 へ複写   ★前フレームの入力（エッジ検出用）
```

**タイマは 33ms**（`WinMain` の `FUN_00424870(this, 0x21, 200, 0)`。
第 2 引数 200 は取りこぼしの追いつき上限。windepth は 50ms=20fps だった）。

### pad のビットとキー — `DAT_00444c20` の表（実測）

| ビット | 既定のキー |
|---|---|
| 0x01 UP | K / ↑ / テンキー8 |
| 0x02 DOWN | J / ↓ / テンキー2 |
| 0x04 LEFT | H / ← / テンキー4 |
| 0x08 RIGHT | L / → / テンキー6 |
| 0x10 BTN1 | Z / Space（2P はテンキー0） |
| 0x20 BTN2 | X / Enter（2P はテンキー5） |
| 0x40 BTN3 | Shift |
| 0x80 BTN4 | Q |
| 0x1000 START | F2 |
| 0x8000 | Esc（表の外、直接 GetAsyncKeyState） |

**windepth_wasm と同じ並び**（同じ WinGL なので当然）。あちらの
`index.html` の KEYMAP がそのまま使える。

### ゲームの状態機械 — `FUN_00401500`

`switch (DAT_004bf894)` ひとつ。`FUN_00421da0(n)` が次の状態を入れ、
`DAT_004bf89c` が「入ったばかり」の旗。

| 状態 | 中身 |
|---|---|
| 0x0a | 初期化して 0x10 へ |
| 0x0f / 0x10 | **Bio_100% のロゴ**。`staff.dar` を 291 枚読み、`bgm01`、184 行の配列 `DAT_0044653c` を乱数で 0/1 にしてロゴを行ごとに出し入れする（`DAT_004492c0` が 0=出す 2=消す）。消し終わりで 0x1e へ |
| 0x1e | **タイトル（海）**。`depth1.dar` を 9 枚読み（**同じスロット 0xb47 に上書き**）、`bgm02`、空・海面・海中のタイルを敷き、9 枠の潜水艦を 7 レーンに流し、スタッフの字を流す。メニューは overlay hook（下記）。`DAT_00449284 = 0x708` は**減らす所が無い**（死んでいる）|
| 0x32 / 0x33 / 0x34 / 0x35 | タイトルの続き。0x35 は 0x10 へ戻る（デモの輪） |
| 0x46 | **SOUND TEST**（"SOUND TEST" / "Bio_100% LOGO" / "BGM %02d" / `bgm%02d`） |
| 0x5a | 版とクレジット（"Super Depth ver1.00a Copyright(c)1991 Hideki Mori and Yasuo Futatsugi" / "alty & tacox / Bio_100%"） |

`DAT_004492f0` の内側の switch と `"Game Design - alty & tacox"` から
0x25 バイト刻みの表を引く所がスタッフロールの文字。

### パターンのスロットは使い回す

`FUN_00419700`（`PatEntryDAR`）の第 1 引数が**入れる先のスロット番号**で、
ゲームは

* `depth.dar` の 2887 枚を 0..2886 に
* その上の **0xb47 = 2887 から**、場面ごとに `staff.dar`（291 枚）や
  `depth1.dar`（9 枚）を**上書きして**使う

だから状態 0x0f では 0xb47 が `biologo_staff`、状態 0x1e では 0xb47 が
`sea01` になる。**同じ番号が場面によって別の絵**という前提で読むこと。

### パレットは場面の書庫のもの（実測）

画面のパレットは `thunk_FUN_004033a0` の最後の
`FUN_004178e0(surface, 0x280, 0x1e0, 0xb48, ...)` で決まる。0xb48 は
**場面の書庫の 2 枚目**なので、画面のパレットは depth.dar のものでは
なく `staff.dar` / `depth1.dar` のものになる。矛盾しないのは切り分けが
あるから —— 3 つの書庫を突き合わせると

| 番号 | |
|---|---|
| 0..118 | どの書庫でも同一（VGA の 20 色＋灰の階調） |
| 119..245 | **書庫ごとに違う**。場面の絵の専用色 |
| 246..255 | どの書庫でも同一（原色 8 色） |

で、**depth.dar の 2887 枚は 119..245 を 1 バイトも使っていない**
（92.8% が 119 未満、7.2% が 246 以上、間はゼロ）。だから 8bpp の面
一枚と場面のパレット一つで正しく出る。真色の画面では WinGL が
パターンごとに自分のパレットで展開する（`FUN_00414f60` の case 1）が、
結果は同じになる。

### 毎フレームの尾部（switch の後、どの状態でも走る）

1. `DAT_0046217c` が残っていれば**奇数フレームだけ画面全体を 0xff**に
   して減らす（爆発の白フラッシュ。7 や 6 が入る）
2. `DAT_004492cc` があれば消す —— **これを 1 にする所は binary 中に無い**
3. **枠**: `DAT_004bf8b8`（レジストリの FullScreen、既定 0）が 0 なら
   パターン **0x9d9 を四辺に敷き詰める**（刻みは 0x9c9 の大きさ =
   32x32）。フルスクリーンなら同じ場所を黒で塗る。
   **これが `FUN_00409000` の `y + 0x20` の理由** —— 上下左右 32 px は枠
4. `DAT_004bf898`（その状態に入ってからのフレーム数）を増やす
5. `"%2dFPS"` を 8x8 フォントで (col 0x4b, row 0) に出す。**条件なし**。
   ベータなので出しっぱなしになっている

### overlay hook —— メニューは状態機械の外にいる

`FUN_004148f0(fn, 1)` が `DAT_004bf164` に関数を入れ、各状態の最後の
`(*DAT_004bf164)()` がそれを呼ぶ。0x401100 からの thunk 表を引くと

| 入る値 | 中身 |
|---|---|
| `LAB_0040118b` | `FUN_00414920` = **タイトルのメニュー**（3 項目） |
| `LAB_004010af` | `FUN_00414b00` = 記録画面（未移植） |

`FUN_00414920` は上下でカーソル、`FUN_00402de0`（BTN1）か
`FUN_00402ec0`（START）で決定、`FUN_00402800` で **SUPER DEPTH の
ロゴをタイル 16x16 で組み立てて**描き、項目を 3 行書き、
**`DAT_004bf16c` を数えて 0x708（59 秒）でデモ（0x33）へ**。
項目は `DAT_00441d68` の 3 本（" Game Start " / "   Record   " /
"    Exit    "）。選択中は 0x580（黄）、それ以外は 0x881（黒）。

決定の判定は **BTN1 を先に見て、押されていなければ START を見る**。
どちらも乱数を 1 個捨てるので、順番を変えると乱数列がずれる。

### 効果音の名前表（`PTR_DAT_0044277c`）

`FUN_0041fd00(name)` はこの 12 本から名前で引く:
drop, burn, eneshot, item, depth01..06, plane, finalt09。
**`plane.wav` はインストーラに入っていない**（`depth-build115.exe`
の中に "plane" という文字列すら無い）。WinDepth 側には
ある音なので、エンジンごと持ってきた名前が残っているのだと思われる。

### BGM のモード（`FUN_00420980(mode, name)`）

`0..3` が再生、`4` が停止。2 と 3 は**同じ曲なら鳴らし直さない**
（面の途中で復活しても曲が頭に戻らない）。呼び出し側は 0x10 が
`(0, bgm01)`、0x1e が `(1, bgm02)`、面が `(3, bgm03/bgm06)`、
SOUND TEST が `(1, bgm%02d)`。**モードのどれが loop なのかは SMFDrv を
読んでいない。** bgm01 が 3.8 秒のジングルで 0、bgm02 が 43 秒の
タイトル曲で 1 なので「0 は一回だけ、他は loop」と読んで実装した。
これは呼び出し側からの読みで、ドライバから確かめた事実ではない。

## 次にやること（順）

1. ~~`.dar` の展開~~ 済み（`src/dar.c`、docs/format.md）
2. ~~描画層~~ 済み（`src/video.c`）
3. ~~BGM~~ 済み（`src/smf.c` / `src/synth.c` を windepth_wasm から）
4. ~~WASM とページ~~ 済み（いまは絵と音の展示）
5. ~~状態 0x0f/0x10（Bio ロゴ）~~ 済み（`src/game.c`、`tests/logo_check.c`）。
   184 行を 4 行ずつ、0x78 フレーム待って 4 行ずつ消す。乱数は MSVC の
   `rand()`（種は 1 固定なので毎回同じ順）
6. ~~状態 0x1e（海のタイトル）~~ 済み（`src/game.c`、`tests/title_check.c`）。
   メニュー（`FUN_00414920`）と タイル組み立てのロゴ（`FUN_00402800`）も
7. **記録画面（`FUN_00414b00`）**。メニューの Record から入る。
   レジストリ `Software\Bio_100%\SuperDepth` の `rank%d` を読む所と、
   `"Super Depth Top Score Ranking"` / `"rankin = %d rcurX = %02d"`
8. **`stage3.bin`** の 24 バイトレコードの意味（`FUN_00413df0` の
   switch が kind 0..3 を runtime の型 0/1/2/3/0x32 に写す所から）
9. **ゲーム本体**。状態 0x32..0x35 の先（`FUN_00405c10` が毎フレーム
   呼ばれる本体）と、`stage3.bin` を使う所
10. ~~効果音~~ 半分済み。名前は上の表、ページが `disk/*.wav` を
   fetch して鳴らす（`sd_se_take`）。鳴らす場所は本体の移植と一緒

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
