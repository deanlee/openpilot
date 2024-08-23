#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <cassert>

void embedRaylibInQtWidget(void *wid, void *window) {
  // Retrieve the native handle of the Raylib window
  ::Window raylibWindow = *((::Window *)window);

  // Retrieve the native handle of the Qt widget
  ::Window qtWindow = *((::Window *)wid);  // parent->winId();

  // Open the X display
  Display *display = XOpenDisplay(NULL);
  if (display == nullptr) {
    // fprintf(stderr, "Unable to open X display\n");
    assert(0);
    return;
  }

  // Reparent the Raylib window to be a child of the Qt widget
  XReparentWindow(display, raylibWindow, qtWindow, 0, 0);

  // Map the Raylib window to make it visible
  XMapWindow(display, raylibWindow);

  // Set the Raylib window to always be on top within the parent widget
  XRaiseWindow(display, raylibWindow);

  // Hide the Raylib window from the taskbar and window switcher
  Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
  Atom skipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
  Atom skipPager = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);

  XChangeProperty(display, raylibWindow, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&skipTaskbar, 1);
  XChangeProperty(display, raylibWindow, wmState, XA_ATOM, 32, PropModeAppend, (unsigned char *)&skipPager, 1);

  // Map the Raylib window to make it visible
  XMapWindow(display, raylibWindow);

  // Close the X display (when done with X operations)
  XCloseDisplay(display);
}
