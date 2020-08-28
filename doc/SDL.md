# Momo の SDL 機能をカスタマイズする

## 概要

Momo の SDL 機能をカスタマイズする方法を、実際のカスタマイズ例を交えて説明します。

## 想定する読み手

- Momo の SDL 機能のカスタマイズに興味がある
- C++ の初歩的な知識がある

## ミュート・ボタンを実装する

このドキュメントでは、 MacOS で Momo の SDL 機能にミュート・ボタンを実装します。  
ミュート・ボタンは WebRTC で送信される映像・音声をミュートするものとします。

MacOS 以外の OS を利用する場合は、ビルド方法などが異なります。  
各 OS での Momo のビルド/実行方法は `doc` ディレクトリ以下のドキュメントを参照してください。

### 1. SDL_image でボタンを表示する

#### 1.1 SDL について

SDL はメディアやデバイスへの低レベルなアクセスを可能にする、クロス・プラットフォームの開発ライブラリーです。  
C で実装されていますが、様々なプログラミング言語のバインディングが存在します。  
Momo は C++ で実装されており SDL を直接利用できるため、バインディングは必要ありません.

#### 1.2 SDL_image を追加して Momo をビルドする

今回は、画像を SDL で描画することでボタンを実装しました。  
SDL は機能毎にライブラリが分割されており、画像を扱うライブラリは SDL_image という名前で配布されています。

SDL_image は Momo で利用されていなかったため、ビルドの修正が必要でした。  
MacOS における Momo のビルドは `./build/build.sh macos` で実行できますが、 `build.sh` の内部では、ビルドに必要な依存のインストールと cmake によるビルドが行われています。  
前者は `build/macos/install_deps.sh` 、後者は `CMakeLists.txt` に詳細が記載されています。  
これらのファイルに以下の修正を加えることで、 Momo から SDL_image を利用することができるようになりました。

- SDL_image のソースを取得してビルドする
- SDL_image のヘッダー・ファイルを Momo のビルド時にインクルードする
- SDL_image をビルドしたバイナリを Momo のビルド時にリンクする

#### 1.3 ボタンを表示する

Momo の SDL 機能は `src/sdl_renderer` 以下で実装されており、特にレンダリングは `sdl_renderer.cpp` の `SDLRenderer::RenderThread` 関数で実装されています。

SDL では以下のような手順でレンダリングを行います。

1. SDL_Surface を作成する
    - WebRTC で受信した映像は、 SDL_CreateRGBSurfaceFrom を呼び出し、フレームの映像を元に Surface を作成します
    - SDL_image を利用する場合は IMG_Load を呼び出し、画像ファイルから SDL_Surface を作成します
2. SDL_CreateTextureFromSurface を呼び出し、 SDL_Surface から SDL_Texture を作成する
3. SDL_RenderCopy を呼び出し、 SDL_Texture の内容を SDL_Renderer にコピーする
4. SDL_RenderPresent を呼び出し、レンダリングを行う

SDL_Surface と SDL_Texture の違いは、前者がソフトウェアのメモリ上のデータであるのに対して、後者はレンダリングを行うハードウェアに依存したデータとなっています。

### 2. ミュート機能を実装する

#### 2.1 SDL_Event でボタンのクリックを検知する

前の章で SDL を利用してボタンを表示しましたが、この章ではボタンを押された際の処理を実装していきます。  
まずは、ボタンを押せるようにする必要があります。

SDL には SDL_Event という仕組みがあり、各種のイベントに対応した処理を実行することができます。  
Momo では SDL_Event を利用して以下の処理が実装されています。

- ウィンドウがリサイズされた際に、 WebRTC で受信した映像の大きさを再計算する処理
- F キーが押された際に、 SDL の画面をフル・スクリーンにする処理
- Q キーが押された際に、 Momo を終了する処理
- SDL 終了時の処理

これらの処理は `sdl_renderer.cpp` の `SDLRenderer::PollEvent()` に実装されていますが、そこに、ボタンの領域がクリックされたことを検知する条件分岐を追加して、ミュート・ボタンの処理を実装していきます。

また、ボタンが常に表示されていては邪魔なので、ボタンの領域の上にマウスがある場合のみに、ボタンを表示する処理を実装しました。

#### 2.2 WebRTC で送信する映像、音声をミュートする

この章では、ミュート・ボタンが押された際の処理を実装していきます。  
Momo が WebRTC で映像と音声を送信する仕組みは libwebrtc に依存しています。  
そのため、 libwebrtc の API を調べて、映像・音声をミュートする方法を探す必要があります。

libwebrtc の API を調査する際には、 [Chromium Code Search](https://source.chromium.org/) が非常に便利なのでこちらを利用することを推奨します。

今回は、 `VideoTrackInterface, AudioTrackInterface` に実装されている、 `set_enabled` という関数を利用してミュート機能を実装します。  
set_enabled に true を渡して呼び出せばトラックを有効化し、 false を渡して呼び出せばトラックを無効化することができます。

Momo で映像・音声を送信するトラックは `src/rtc/rtc_manager.cpp` の `RTCManager` というクラスで管理されています。  
ここに set_enabled を呼び出す関数を追加します。  
また、 SDL の処理を行うクラス `SDLRender` は `RTCManager` にアクセスできなかったので、 `SDLRenderer::SetRTCManager` という関数を追加して `SDLRender` に `RTCManager` の参照を渡せるように修正しました。

これで、ミュート・ボタンが押された際に WebRTC の映像と音声をミュートできるようになりました。

### まとめ

駆け足ですが、 Momo の SDL 機能をカスタマイズする例を説明しました。  
SDL で GUI や処理を作り込むのは、各プラットフォームの GUI ライブラリなどと比べると作り込みが必要ですが、マルチ・プラットフォームでハードウェアを活用するアプリケーションを実装することができます。  
みなさんも、是非 Momo の SDL 機能をカスタマイズしてみてください。

### おまけ: ビルドの話

TODO: Windows で SDL_image をビルドするのが難しかった話を書きます

## WIP: 参考文献

TODO: 開発中に参考にしたページへのリンクを張ります