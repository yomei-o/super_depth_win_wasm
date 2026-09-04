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

### 画素は「べた」ではなく走の並び — 確定

1 行は**走（run）の並び**で、走 1 つが

```
dword   下位ワード = 透明な画素の数
        上位ワード = 続く不透明な画素の数
byte    不透明な画素。4 の倍数に詰める
```

これは `FUN_0041a590` が**逆に**やっていることそのまま。あの関数は
ビットマップから走の並びを作る側で、
`*param_2 = 不透明 << 0x10 | 透明 & 0xffff` を書き、次の走の位置を
`local_18 + (不透明 + 7 & 0xfffffffc)` で進める —— つまり走 1 つは
**4 + align4(不透明)** バイト。

**4 の倍数への詰めを落とすと、長さが 4 の倍数でない走の後ろが全部
横にずれる。** 最初はそれで `depthlogo` が縞になった。

これで `stride` が幅より小さいパターンの説明もつく:

| | |
|---|---|
| `sea01` 64x64 stride 68 | 走 1 つ（透明 0・不透明 64）+ 64 画素 = 68 |
| `biologo_staff` 300x184 stride 304 | 同じく 4 + 300 |
| `kgfwhite` 16x33 stride 4 | 走 1 つ（透明 16・不透明 0）だけ。**全部透明** |
| `depthlogo` 372x145 stride 260 | 透明の多いロゴなので画素が少ない |
| `earth192` 131x129 stride 132 | 丸い惑星。走で抜けている |

`word 3`（`piVar13[8]`）は**透明色の番号**。`-1` なら透明なし
（`PatBuildDAR` が `if (-1 < (int)uVar1)` で走の並びを作るかどうかを
決めている）。`sys16` は 254。

描けたもの: `depth1.dar` の海のタイル、`staff.dar` の Bio_100% ロゴ、
`depthlogo`（THE ULTIMATE HYPER BATTLESHIP の副題つき）、`ending.dar` の
惑星、`space.dar` の背景。

### まだ分からないところ

* **`depthlogo` だけ 180 度回って出る。** 同じファイルの
  `biologo_staff` は正しい向きなので、行の順（下から上）や左右の向きが
  パターンごとに違うのか、絵がそう入っているだけなのかは未確定。
  タイトル画面を描けば分かる
* 名前の後ろの旗（`hlen >= 18 + 名前の長さ` のときだけある）の意味。
  1 つは `>> 3` されて「種類」になり、`local_14[]` の 5 本から 1 本を
  選ぶ。`FUN_0041a3e0`（`PatBuildDAR`）とその下の
  `FUN_0041a670` / `FUN_0041a730` を読むこと

## `stage3.bin`

6632 バイト、先頭が `SDEPTH`。本体の文字列にも `SDEPTH` と `stage3.bin` が
ある。まだ読んでいない。

## 音

* 効果音は素の RIFF/WAVE。本体は `%s\%s.wav`（`sound\*.wav`）を
  `sndPlaySoundA` で鳴らす
* BGM は SMF（`sound\%s.mid`）。本体に `MIDI Sound Driver - SMFDrv` と
  `BMIDIPLAY_ONCE` / `_REPEAT` / `_STOP` / `_DRIVER_CLOSE` があり、
  `mciSendCommandA` で鳴らす
