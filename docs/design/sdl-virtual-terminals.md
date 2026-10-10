# SDL virtual terminals

User direction, 2026-10-10: core provides terminal VTs and fullscreen SDL VTs.
X11 servers, window managers and toolkits are runtime packages. Legacy native
GTK desktop/window-manager compatibility is not required. Preserve the old
repository and its implementation as historical references.

## Boundary

One graphical VT has one presentation surface. Core owns VT allocation,
activation, lifecycle, framebuffer presentation, input/focus delivery and the
browser bridges required by SDL. It does not understand X protocol, X windows,
desktop policy, stacking, decorations, snapping or a window-manager socket.
Keep SDL as the application API; keep its internal host transport small and
versioned rather than treating every SDL API function as a kernel primitive.

VT fullscreen means filling the OS viewport. Entering browser fullscreen is
optional and separate. Support viewport-sized surfaces and fixed logical sizes
with scaling/letterboxing and correctly transformed pointer coordinates. SDL
fullscreen/display-mode requests must report the implemented behavior honestly;
browser support does not imply physical exclusive display-mode switching.

Multiple SDL applications may occupy separate VTs. A console SDL backend can
reject a second top-level window explicitly; multi-window applications use a
packaged SDL X11 backend inside an X desktop. That backend and the core SDL
backend must be selected intentionally; compatibility is not automatic.

Inactive VTs stop unnecessary presentation and lose input focus. Do not freeze
their processes, sockets, timers or background jobs. Focus transitions release
held input and pointer capture; visibility must not make display-server request
processing depend on an indefinitely blocked present. Background audio policy
is separate from graphics presentation. Preserve a host-accessible switcher and
recovery terminal even when a graphical application crashes or captures input.

## Verified starting point and limits

Inspection of this checkout confirms inherited /run/wm.sock policy in
src/kernel.js and fullscreen rejection in libc/src/__SDL_video.c. The old
apps/x11-server/port/browser-screen.c creates one SDL window and presents the
X framebuffer through SDL_GetWindowSurface. These are code observations, not
an acceptance run of X11 on the new core.

The old X server README describes software rendering, a fixed-size display,
and exclusions for GLX, DRI2, DRI3 and MIT-SHM. The architecture permits a
userland X server but does not itself implement these extensions or arbitrary
X application compatibility. Existing full-frame copies are implementation
costs, not a proven lower bound or established native performance parity.

## Migration and acceptance

First implement a terminal plus two independent graphical VTs, switching,
focus cleanup, sizing, application exit and crash recovery. Verify hidden
applications and background shell jobs continue useful work without spinning
on presentation. Then remove inherited WM protocol, compositor policy and
desktop chrome together with obsolete tests, retaining surface transport and
needed SDL browser bridges. Test the actual package X server with multiple
clients and a packaged window manager on the new VT backend. Only then describe
the new graphics contract as implemented and consider freezing its ABI.

Package builds, source discovery and hosting retain the existing rule: no
packages checkout is read by core assembly or hosting. Runtime package operations
alone obtain X11 and other application inputs.

## Concrete VT lifecycle and pacing contract

The page allocates a canvas when a process worker starts; a VT becomes selectable
only when SDL creates its one window. The canvas transfers directly to that
process worker. SDL rejects an additional top-level window. The WebGPU handle
table and every requested GPUDevice live in that process, never in the kernel.
SDL_GetWGPUSurface validates its handle against that process's SDL window table.
The process configures the shared SDL/WebGPU canvas; X receives no GPU sharing API.

Present imports suspend the guest through JSPI for a deadline-based interval:
60 Hz while active, 10 Hz while inactive or the browser tab is hidden. Only one
present call is outstanding on that C stack; there is no frame transfer queue.
An inactive application is never parked waiting for activation. Its worker event
loop, other guest processes, socket operations and timers continue; a sequential
application resumes from a present within the 100 ms pacing interval plus host
scheduling delay. Browser background throttling may extend that delay. Callback
frame loops receive the same active/inactive cadence. An event-driven application
with no pending present schedules no periodic presentation work.

Viewport changes resize resizable SDL windows through queued SDL events. Fixed
logical windows retain dimensions and are letterboxed, with input transformed
through that content box. Window fullscreen denotes the VT viewport; no exclusive
monitor mode is promised. Switches release held keys/buttons, canvas pointer
capture and pointer lock. Alt+1 selects the terminal; Alt+2..9 select graphical
VTs. The page switcher remains outside the application canvas.

Exit/crash removes that process's page canvas and tab and returns an active VT
to the terminal. Exec replaces the canvas before starting the new image. Thus
old workers cannot present into a reused VT. Normal invocation cleanup destroys
owned devices; forced worker termination ends its device ownership. SDL device
loss is an error on subsequent rendering rather than a shared-device restart;
other processes and the terminal remain independent. These are implementation
contracts; acceptance evidence records which paths have actually been exercised.
