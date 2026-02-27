#include <cstdint>
#include <windows.h>

#include <SDL3/SDL.h>

namespace dxvk::wsi {

  inline SDL_Window* fromHwnd(HWND hWindow) {
    return reinterpret_cast<SDL_Window*>(hWindow);
  }

  inline HWND toHwnd(SDL_Window* pWindow) {
    return reinterpret_cast<HWND>(pWindow);
  }

  // SDL3 uses SDL_DisplayID (Uint32, starting from 1, 0 = invalid)
  inline SDL_DisplayID fromHmonitor(HMONITOR hMonitor) {
    return static_cast<SDL_DisplayID>(reinterpret_cast<uintptr_t>(hMonitor));
  }

  inline HMONITOR toHmonitor(SDL_DisplayID displayId) {
    return reinterpret_cast<HMONITOR>(static_cast<uintptr_t>(displayId));
  }

}
