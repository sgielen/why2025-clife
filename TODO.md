# Stuff that should be fixed

* fflush() is broken due to __getreent(), needs wrapping
* window_framebuffer_free doesn't "move" the other framebuffer
* events are only delivered once per frame, and then only one input. 
* clean frames don't get re-blitted but due to lack of occlusion culling this causes graphical glitches
* We include the `select()` system call, but it only kind of works.

