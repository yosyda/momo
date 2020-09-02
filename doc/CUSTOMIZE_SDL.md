# Momo の SDL 機能をカスタマイズする

Momo の SDL 機能をカスタマイズする方法を、実際のカスタマイズ例を交えて説明します。  
カスタマイズ例として、 Momo が送信する映像・音声をミュートするボタンを追加しています。

## ミュート・ボタンを追加したサンプル・プログラムを動かす

### サンプル・プログラムの動作環境

- MacOS Catalina

### サンプル・プログラムをビルドする

Momo の `feature/sdl-sample` ブランチをビルドします。

```
git checkout feature/sdl-sample
build/build.sh macos
```

また、 GitHub Actions を利用して Momo をビルドすることもできます。  
詳細は Momo のソースの `.github/workflows` 以下を参照してください。

### サンプル・プログラムを実行する

Momo の映像・音声を Ayame Lite で受信し、ミュート・ボタンの動作を確認する手順を説明します。  
例では、 `momo-sdl-sample` というルーム ID を使用していますが、推測されにくい値に変更することを推奨します。

#### 1. Momo を実行する

以下のコマンドで Momo を実行します。

```
_build/macos/momo --resolution VGA --use-sdl ayame wss://ayame-lite.shiguredo.jp/signaling momo-sdl-sample
```

#### 2. Ayame Web SDK で接続する

ブラウザで以下の Ayame Web SDK のページを開き、接続ボタンをクリックします。

https://openayame.github.io/ayame-web-sdk-samples/sendrecv.html?roomId=momo-sdl-sample

#### 3. 映像・音声をミュートする

カーソルを Momo のウィンドウの右下に持っていくと、カメラとマイクのアイコンが表示されます。  
カメラをクリックすれば映像が、マイクをクリックすれば音声が、それぞれミュートされます。  
ミュートを解除したい場合は、再度アイコンをクリックします。

### サンプル・プログラム実行時の注意点

#### ボタンが表示されない

Momo の実行パスの直下に、ボタンのアイコンが入った `html` ディレクトリーが必要です。  
ミュート・ボタンのアイコンを実行時にロードしているためです。

## カスタマイズのための Tips

### WebRTC で送信する映像・音声をミュートする

libwebrtc の `VideoTrackInterface, AudioTrackInterface` に実装されている `set_enabled` を利用しています。  
libwebrtc のソース・コードの調査には [Chromium Code Search](https://source.chromium.org/) が便利です。

### Momo のソースで SDL を利用している箇所

`src/sdl_renderer` ディレクトリ以下です。

### SDL_image でミュート・ボタンのアイコンを表示する

ミュート・ボタンのアイコンを SDL で表示するために [SDL_image](https://www.libsdl.org/projects/SDL_image/) を利用しています。  
SDL_image で表示したボタンに、クリック時の処理を実装するには SDL_Event を利用したコーディングが必要です。  
詳細は後のセクションで説明します。

### SDL を利用したレンダリングの流れ

SDL では以下の流れでレンダリングを行います。  
Momo では `src/sdl_renderer/sdl_renderer.cpp` の `SDLRenderer::RenderThread` に処理が実装されています。

1. SDL_Surface を作成する
    - WebRTC で受信した映像の場合は、 SDL_CreateRGBSurfaceFrom を呼び出し、フレームの映像を元に Surface を作成します
    - SDL_image を利用する場合は IMG_Load を呼び出し、画像ファイルから SDL_Surface を作成します
2. SDL_CreateTextureFromSurface を呼び出し、 SDL_Surface から SDL_Texture を作成する
3. SDL_RenderCopy を呼び出し、 SDL_Texture の内容を SDL_Renderer にコピーする
4. SDL_RenderPresent を呼び出し、レンダリングを行う

### SDL_image を追加して Momo をビルドする

MacOS における Momo のビルドは `./build/build.sh macos` で実行しますが、 `build.sh` の内部では、ビルドに必要な依存のインストールと cmake によるビルドが行われています。  
前者は `build/macos/install_deps.sh` 、後者は `CMakeLists.txt` に処理が記述されています。

SDL_image を追加してビルドを行うために、上記のファイルに以下の修正を加えています。

- SDL_image のソースを取得してビルドする
- SDL_image のヘッダー・ファイルを Momo のビルド時に参照する
- SDL_image をビルドしたライブラリを Momo のビルド時にリンクする

### SDL_Event によるイベントのハンドリング

SDL には [SDL_Event](https://wiki.libsdl.org/SDL_Event) という仕組みがあり、入力デバイスや各種イベントをトリガーして処理を実行することが可能です。  
Momo は以下の処理に SDL_Event を利用しています

- ウィンドウがリサイズされた際に、 WebRTC で受信した映像の大きさを再計算する処理
- F キーが押された際に、 SDL の画面をフル・スクリーンにする処理
- Q キーが押された際に、 Momo を終了する処理
- SDL 終了時の処理

これらの処理は `sdl_renderer.cpp` の `SDLRenderer::PollEvent()` に実装されています。

今回のカスタマイズでは、ミュート・ボタンがクリックされた際のイベントをハンドリングする処理を追加しています。
また、ボタンが常に表示されていては邪魔なので、ボタンの領域の上にマウスがある場合のみボタンを表示する処理も実装しています。