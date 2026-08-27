#pragma once

//==========================================================
// BuildConfig
//   Feature flags by build type. In the Distribute (release)
//   build, all debug features and editors are disabled.
//==========================================================
#ifdef DISTRIBUTE_BUILD
inline constexpr bool kDebugFeatures = false;   // release: debug/editor OFF
#else
inline constexpr bool kDebugFeatures = true;    // dev: enabled
#endif
