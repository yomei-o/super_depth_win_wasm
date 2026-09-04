# 形式 — インストーラの三層と、中のデータ

全部 `depth-build115.exe`（2149104 バイト）から実測したもの。
`python tools/unpack.py` がこの文書のとおりに解く。**インストーラは
走らせない。**

## 一層目 — PackageForTheWeb の外皮

PE32、GUI、5 セクション。ところが**ファイルの 93.9% がセクションの外**に
ある。リソースの文字列がこう名乗る:

```
FileDescription  PackageForTheWeb Stub
FileVersion      2.04.001
CompanyName      InstallShield Software Corporation
```

追記部分は **Microsoft のキャビネット**そのもの。`MSCF` が
**ファイル位置 0x20b07** にあり、そこから最後まで（2015209 バイト）が
`cbCabinet` と一致する。だから Windows の `expand.exe` で開く:

```
expand disk1.cab -F:* tmp/disk1
```

中身は 12 ファイルの `DISK1\`（`AUTORUN.INF` `DISK1.ID` `SETUP.EXE`
`SETUP.INI` `SETUP.INS` `SETUP.ISS` `SETUP.PKG` `_INST32I.EX_`
`_ISDEL.EXE` `_SETUP.1` `_SETUP.DLL` `_SETUP.LIB`）。
`SETUP.INI` が `[Startup] AppName=SuperDepth`。

## 二層目 — InstallShield 3 の書庫 `_SETUP.1`

先頭 4 バイトが `13 5d 65 8c` = **0x8C655D13**。1446975 バイト。
ファイル表は末尾にあり、`Group1`..`Group4` のグループ表のすぐ後ろから
**名前レコードと 23 バイトのフィールド組が交互に並ぶ**。

名前レコード:

```
+0x00 dword  このレコードの長さ = 43 + 名前の長さ
+0x04 word   0x0100
+0x06 byte   名前の長さ
+0x07 char   名前
             0 が 13 バイト
```

23 バイトのフィールド組:

```
+0x00 3 バイト  旗。01 01 00 / 01 03 00 / 01 00 00
+0x03 dword    展開後の大きさ
+0x07 dword    圧縮後の大きさ
+0x0b dword    データの位置（`_SETUP.1` の中の絶対位置）
+0x0f word     DOS 時刻、word DOS 日付
+0x13 dword    属性
```

**フィールド組は「次の名前」のもの。** 最初の名前のぶんはグループ表の
直後に置かれていて、これがヘッダのファイル表ポインタ（+0x32）が
2 レコードずれて見える原因。名前とその後ろの組を対にすると大きさは全部
合うのに中身が入れ替わる —— `staff.dar` が JPEG になり `index.html` が
MIDI になったので気付いた。**拡張子と中身の署名が合うことを検査に入れて
ある**（`tools/unpack.py` の `SIGN`）。

各メンバのデータは **PKWare Data Compression Library の implode**。
先頭 2 バイトはこの書庫では全部 `00 06`（生のリテラル、4096 バイトの窓）。
Mark Adler の blast（`tools/blast.c`）が解く。45 ファイルとも展開後の
大きさが表と一致し、署名も合う。

## 三層目 — 45 ファイル

`disk/` に出る。README.md の表を見よ。

## `.dar` — パターンの書庫 — 確定

`depth.dar` `depth1.dar` `depth2.dar` `space.dar` `ending.dar` `staff.dar`。
本体の文字列が `pic\depth.dar` なので、製品では `pic\` に置かれる。
`Pat*`（`PatEntryDAR` `PatBuildDAR` `PatAlloc2` `PatEntryID(#%d)`
`PatEntryName('%s')`）が扱う「パターン」の入れ物。

読みかたは `FUN_00419700`（`PatEntryDAR`）と `FUN_004199c0` そのまま。
`python tools/dar.py <file>` が同じ歩き方をする。

```
+0x00  char[5]  "DAR:8"      FUN_004199c0 が 5 バイト比べる
+0x05  byte     版。0 か 1（2 以上は撥ねる）
+0x06  word     パターン数    ← あの関数の返り値
+0x08  dword    4
+0x0c  0 が 4 バイト
+0x10  RGBQUAD × 256         パレット 1024 バイト。頭は Windows の
                             16 色システムパレット
+0x40c パターンの並び        ※ 0x410 ではない
```

パターン 1 枚（版 1。版 0 はヘッダ長 8 固定）:

```
word  ヘッダ長 hlen          このワードの「次」から数える
word  幅
word  高さ
word  1 行のバイト数 stride
word  符号つき。このファイル群では -1
[hlen > 13]
word  ×3                    +6 / +0x1a / +0x12 へ。3 つめは高さの控えを上書き
[hlen > 15]
byte  名前の長さ             幅ワードから +14
char  名前                   +15 から。4 の倍数に詰める
byte  旗 ×4                 名前の後ろ。1 つは >> 3 されて種類になる
画素  高さ × stride バイト
```

次のパターンは **幅ワードの位置 + hlen + 高さ × stride**。
この歩き方で 6 本とも**ファイルの末尾にぴったり着く**:

| | パターン数 | 大きさ |
|---|---|---|
| `depth.dar` | 2887 | 1335036 |
| `depth1.dar` / `depth2.dar` | 9 | 132072 |
| `space.dar` | 50 | 336660 |
| `staff.dar` | 291 | 330160 |
| `ending.dar` | 2 | 18444 |

**素の 8bpp のものは描けている。** `depth1.dar` の 9 枚は青い海のタイル
（`sea01`..）、`staff.dar` の 1 枚目は Bio_100% のロゴ（`biologo_staff`
300x184）で、パレットも当たっている。

### まだ分からないところ

`stride` が「幅を 4 に丸めた数」にならないパターンがある:

* `kgfwhite` 16x33 stride **4** —— 16 画素に 4 バイトなので 8bpp ではない
  （1bpp のマスクなら 2 バイト、4 に丸めて 4）
* `depthlogo  t` 372x145 stride **260** —— 1 画素 1 バイトに足りない

`FUN_00419700` が名前の後ろの旗の 1 つを `>> 3` して
`piVar13 + 9` に入れ、その値で `local_14[]` の 5 本のポインタから 1 本を
選んでいる（`local_14[0]` はファイルの先頭 + 8、残りは
`FUN_00414ee0()` で確保した領域）。**つまりパターンには種類があり、
種類ごとに読み方が違う。** `FUN_0041a3e0`（`PatBuildDAR`）と
`FUN_0041a590` / `FUN_0041a670` / `FUN_0041a730` を読むこと。

## `stage3.bin`

6632 バイト、先頭が `SDEPTH`。本体の文字列にも `SDEPTH` と `stage3.bin` が
ある。まだ読んでいない。

## 音

* 効果音は素の RIFF/WAVE。本体は `%s\%s.wav`（`sound\*.wav`）を
  `sndPlaySoundA` で鳴らす
* BGM は SMF（`sound\%s.mid`）。本体に `MIDI Sound Driver - SMFDrv` と
  `BMIDIPLAY_ONCE` / `_REPEAT` / `_STOP` / `_DRIVER_CLOSE` があり、
  `mciSendCommandA` で鳴らす
