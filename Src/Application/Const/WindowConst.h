#pragma once

// 窓の大きさ。
//
// ■ なぜ既定値が要るか
// 普段は Asset/Data/WindowSettings.csv から読む。
// 読めなかったときに 0 で窓を作ろうとすると、何も出ないまま終わる。
//
// 配布ビルドはアセットを exe へ埋め込むので、
// 1つ入れ忘れただけでこの状態になる。実際に一度なった。
//
// 落ちるより、既定の大きさで開いて気づけるほうがよい。
namespace WindowConst
{
	constexpr int DefaultW = 1280;
	constexpr int DefaultH = 720;
}
