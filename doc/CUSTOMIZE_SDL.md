# Momo の SDL 機能のカスタマイズ

このドキュメントでは、 Momo の SDL 機能のカスタマイズ方法を説明します。  
まず、 Momo の SDL 機能の実装について解説した後、カスタマイズ例として、ミュート・ボタンを追加する流れを解説します。

## 対象とする読者

以下のような読者を想定しています。

- Momo の　SDL 機能をカスタマイズしたい
- 調べながら C++ を書ける
- SDL の初歩的な知識がある
  - これから SDL を学習する方は、 Momo のソース・コードと併せて [SDL 入門のための参考リンク集](#SDL\ 入門のための参考リンク集) を参照することをおすすめします。

## Momo の SDL 機能の実装

### SDL 機能が実装されている箇所

`src/sdl_renderer` ディレクトリ以下です。

### SDL で実装されている処理

Momo の SDL 機能では以下の2つの処理が実装されています。

- WebRTC で受信した映像の描画
- SDL_Event のハンドリング

以降の章では、これらの処理について説明していきます。

### WebRTC で受信した映像の描画

Momo の SDL 機能では以下の流れで WebRTC で受信した映像を毎フレーム描画しています。  

1. SDL_Surface を作成する
    - WebRTC で受信したフレームのイメージを元に、SDL_CreateRGBSurfaceFrom を呼び出し SDL_Surface を作成する
2. SDL_CreateTextureFromSurface を呼び出し、 SDL_Surface から SDL_Texture を作成する
3. SDL_RenderCopy を呼び出し、 SDL_Texture の内容を SDL_Renderer にコピーする
4. SDL_RenderPresent を呼び出し、描画する

実際は、各種リソースの開放処理などが記述されていますが、説明を簡単にするために省いています。

ソース上では `src/sdl_renderer/sdl_renderer.cpp` の `SDLRenderer::RenderThread` が該当します。

#### SDL_Sutface と SDL_Texture の違いについて

SDL_Surface がレンダリングにソフトウェアを利用するのに対して、 SDL_Texture はハードウェアを利用します。  
そのため、 SDL_Texture を使用することで、ハードウェアを利用したレンダリングを行うことが出来ます。

参照: https://stackoverflow.com/a/26113388

### SDL_Event のハンドリング

SDL では [SDL_Event](https://wiki.libsdl.org/SDL_Event) を利用して、入力デバイスや各種イベントをトリガーにして処理を実行できます。  
Momo では以下の処理が SDL_Event を利用して実装されています。

- ウィンドウがリサイズされた際に、表示する映像の大きさを再計算する処理
- F キーが押された際に、 SDL のウィンドウをフル・スクリーンにする処理
- Q キーが押された際に、 Momo を終了する処理
- SDL 終了時の処理

ソース上の該当箇所は `src/sdl_renderer/sdl_renderer.cpp` の `SDLRenderer::PollEvent()` です。  
以下のように `while (SDL_PollEvent(&e) > 0)` でループを回しながら、発生した SDL_Event に対応する処理を実行します。

```
...
void SDLRenderer::PollEvent() {
  SDL_Event e;
  while (SDL_PollEvent(&e) > 0) {

    // SDL のウィンドウのりサイズに対応する条件分岐
    if (e.type == SDL_WINDOWEVENT &&
        e.window.event == SDL_WINDOWEVENT_RESIZED &&
        e.window.windowID == SDL_GetWindowID(window_)) {

      // SDL のウィンドウがリサイズされた際の処理がここで実行される
...
```

## カスタマイズ例: ミュート・ボタンの追加

この章では、 Momo の SDL 機能にミュート・ボタンを追加する流れを説明します。  
作業には macOS 10.15.6 を利用しています。

カスタマイズした Momo のソース・コードは `feature/sdl-sample` ブランチにあるので、この章を読む際は是非参考にしてください。

### 仕様

- Momo の SDL 機能に WebRTC で送信する映像・音声をミュートするボタンを追加する
- ミュート・ボタンはウィンドウの右下に追加する

### ミュート・ボタンを表示する方法の検討

ミュート・ボタンを表示する方法として以下の2つを検討しました。  
SDL は高度に抽象化された GUI ライブラリーではないため、ボタンのように見える物を表示するアプローチを取る必要があります。

- [SDL_image](https://www.libsdl.org/projects/SDL_image/) を利用して、画像を描画してボタンとする
- [SDL_ttf](https://www.libsdl.org/projects/SDL_ttf/) を利用して、矩形の上に文字を描画してボタンとする

SDL_image, SDL_ttf は SDL の機能を拡張するためのライブラリーで、 SDL_image は画像ファイルを、 SDL_ttf は TrueType フォントを SDL で扱うために必要になります。
これらのライブラリーは SDL とは別に配布されているため、 Momo から利用するにはビルドの修正が必要です。  
今回は、ビルドへの追加が簡単そうだった SDL_image を利用することにしました。

### ビルドに SDL_image を追加

macOS における Momo のビルドは `./build/build.sh macos` で実行しますが、 `build.sh` の内部では、ビルドに必要な依存のインストールと cmake によるビルドが行われています。  
前者は `build/macos/install_deps.sh` 、後者は `CMakeLists.txt` に処理が記述されています。

SDL_image を追加してビルドを行うために、上記のファイルに以下の修正を加えています。

- SDL_image のソースを取得してビルドする
- SDL_image のヘッダー・ファイルを Momo のビルド時に参照する
- SDL_image をビルドしたライブラリを Momo のビルド時にリンクする

### ミュート・ボタンの表示

SDL_image では IMG_Load 関数で画像を SDL_Surface として読み込むことが出来ます。  

```
SDL_Surface* surface = IMG_Load("./path/to/image.png");
```

SDL_Surface を読み込んでから表示するまでの流れは `SDL による映像の描画` の章と同様です。  
WebRTC の映像のフレームを SDL_RenderCopy した上に、ミュート・ボタンの画像を SDL_RenderCopy します。

### ミュート機能の実装方法の検討

Momo の映像と音声の送信は libwebrtc が担っており、 SDL を経由していません。
そのため、 libwebrtc の API で映像と音声をミュートする方法を調べる必要があります。  
調査の結果、今回は `VideoTrackInterface, AudioTrackInterface` に実装されている `set_enabled` を利用することにしました。  

また、 libwebrtc のソースの調査には [Chromium Code Search](https://source.chromium.org/) を利用しました。  
ブラウザで webrtc のソースコードが閲覧、検索できるので非常に便利です。

### ミュート機能の実装

`SDLRenderer::PollEvent()` にミュート・ボタンがクリックされた際の条件分岐を追加して、前の章で調べた `set_enabled` を呼び出します。  
これで、ミュート・ボタンををクリックすると、 WebRTC で送信される映像と音声がミュートされるようになりました。  
現在の実装では、 Momo の実行時に `--show-me` オプションを指定して、送信する映像を SDL のウィンドウに表示した際もミュートが動作することも確認しています。

実際のコードでは以下のように `set_enabled` をラップした `RTCManager::IsVideoEnabled` 、 `RTCManager::IsAudioEnabled` を実装して呼び出しています。

```
...
      if (is_menu_displayed_) {
        SDL_Rect camera_button_dst_rect = GetCameraButtonDstRect();
        if (CheckCollision(mouse_position_x, mouse_position_y, camera_button_dst_rect)) {
          if (rtc_manager_ != nullptr) {
            rtc_manager_->SetVideoEnabled(!rtc_manager_->IsVideoEnabled());
          }
        }

        SDL_Rect mic_button_dst_rect = GetMicButtonDstRect();
        if (CheckCollision(mouse_position_x, mouse_position_y, mic_button_dst_rect)) {
          if (rtc_manager_ != nullptr) {
            rtc_manager_->SetAudioEnabled(!rtc_manager_->IsAudioEnabled());
          }
...
```

また、ミュート機能と併せて以下の処理も実装しました。

- 映像、音声がミュートされている時にボタンの画像を切り替える処理
- 映像、音声がミュートされている状態で、再度ミュート・ボタンをクリックされた時にアンミュートする処理
- (ボタンが常に表示されて邪魔だったので、)ポインターがボタンの上にある時のみにボタンを表示する処理

## ミュート・ボタンを追加した Momo を動かす

以下はミュート・ボタンの動作を確認している様子です。  
Momo の Ayame モードでカメラの映像を送信し、 Ayame のサンプルで受信しています。

[![ミュート・ボタンを追加した Momo](https://i.gyazo.com/8601a660c2245d39b87f44aab51e96e4.gif)](https://gyazo.com/8601a660c2245d39b87f44aab51e96e4)

### ビルド方法 & 実行例

```
# Momo の feature/sdl-sample ブランチを git clone します
git clone https://github.com/shiguredo/momo.git -b feature/sdl-sample

# ディレクトリに移動して Momo をビルドします
cd momo & build/build.sh macos

# Ayame Lite に接続するコマンド例
_build/macos/momo --resolution VGA --use-sdl ayame wss://ayame-lite.shiguredo.jp/signaling momo-sdl-sample
```

Momo で Ayame に接続する詳細な方法については [USE_AYAME.md](./USE_AYAME.md) を参照してください。

### 動作環境 & 注意点

- macOS 10.15.6 のみで動作を確認しています
- Momo を実行するパスの直下に、ソースに含まれる `html` ディレクトリが存在する必要があります
  - ミュート・ボタンの画像ファイルを動的に読み込んでいるためです

## SDL 入門のための参考リンク集

### [SDL2の紹介](https://www.slideshare.net/nyaocat/sdl2)

SDL の特徴が日本語でわかりやすくまとめられています。

### [Lazy Foo's Productions](https://lazyfoo.net/tutorials/SDL/index.php)

SDL を利用してゲーム・プログラミングを学ぶためのサイトです。  
サンプル・コードが豊富なので、 SDL の API の利用方法を調べる際に参考になります。

### [SDL Wiki](https://wiki.libsdl.org/FrontPage)

SDL 本家の Wiki です。  
API のリファレンスや外部チュートリアルへのリンク集、 FAQs などの情報がまとめられています。