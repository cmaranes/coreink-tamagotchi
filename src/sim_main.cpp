// Desktop entry point: M5GFX runs SDL on the main thread and our Arduino-style
// setup()/loop() on a worker thread.
#include <M5GFX.h>
#if defined(SDL_h_)

void setup(void);
void loop(void);

int user_func(bool* running) {
  setup();
  do {
    loop();
  } while (*running);
  return 0;
}

int main(int, char**) { return lgfx::Panel_sdl::main(user_func, 128); }

#endif
