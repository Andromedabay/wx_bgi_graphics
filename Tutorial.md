## BGI Double Buffering Tutorial

This tutorial explains how to use your enhanced BGI implementation to:

    - Initialize graphics

    - Create and manage multiple off‑screen buffers

    - Draw into memory instead of the screen

    - Flip buffers for smooth, flicker‑free rendering

    - Use more than two buffers if desired

    - Understand safe sequencing for multi‑threaded rendering

This document includes explanations and correct buffer usage explanation.

### 1. Conceptual Steps (English Explanation)

##### Step 1 — Initialize Graphics

Start the graphics system and create the main window. This automatically creates the front buffer, which is the visible on‑screen surface.

##### Step 2 — Create One or More Off‑Screen Buffers

Allocate additional buffers in memory. These are your back buffers.

##### Step 3 — Select an Off‑Screen Buffer for Drawing

Switch the active drawing target so all drawing goes into a chosen back buffer. In Double-Buffering you deliberatley avoid drawing on the visible (active) buffer.

##### Step 4 — Draw Your Frame Into the Back Buffer

Use normal BGI drawing commands. Nothing appears on screen yet.

##### Step 5 — Flip the Back Buffer to the Front Buffer

Make the selected back buffer visible by swapping or copying it to the front buffer.

##### Step 6 — Clear the Back Buffer

Prepare it for the next frame.

##### Step 7 — Draw the Next Frame

Render the next frame into the same or another back buffer.

##### Step 8 — Flip Again

Display the new frame.

##### Step 9 — Loop

Repeat the process for animation.

##### Step 10 — Cleanup

Destroy buffers and close graphics.
  
![Double-buffering minimum requirements](images/Minimum_Buffers_for_Double_buffering.png)
  
### 2. Pseudo‑Code (Generic Explanation)

```
initialize_graphics()
create_window(W, H)  // creates FRONT BUFFER automatically

backBuffer = create_buffer(W, H)

while running:
    set_active_buffer(backBuffer)
    clear_buffer(backBuffer)

    draw_scene()

    set_visual_buffer(backBuffer)  // flip to screen
end while

destroy_buffer(backBuffer)
shutdown_graphics()
```
  
![Double buffering explained](images/DoubleBuffering.png)
  

### 3. Pseudo‑Code Using Your Actual Library Function Names

```C++
initwindow(width, height, "BGI Window")  // creates FRONT BUFFER

int backBuffer = createBuffer(width, height)

while (running) {
    // Draw into off-screen memory
    setActiveBuffer(backBuffer)
    clearBuffer(backBuffer)

    // Standard BGI drawing calls
    setcolor(WHITE)
    line(10, 10, 200, 200)
    outtextxy(20, 220, "Frame Rendering")

    // Flip to screen
    setVisualBuffer(backBuffer)
}

destroyBuffer(backBuffer)
closegraph()
```

### 4. Using Multiple Buffers (Triple Buffering or More)

Your library supports creating multiple buffers:

```C++
int buffers[6];
for (int i = 0; i < 6; i++)
    buffers[i] = createBuffer(width, height);
```    

You can draw into any of them:

```C++
setActiveBuffer(buffers[i]);
draw_scene();
```

You can flip any of them to the screen:

```C++
setVisualBuffer(buffers[i]);
```

You can cycle through them:

```C++
current = (current + 1) % 6;
setVisualBuffer(buffers[current]);
```

This allows for advanced rendering pipelines, temporal effects, or multi‑stage processing.

### 5. Multi‑Threaded Rendering Considerations

Multi-Threading Double-Buffers can be dangerous. Why?
- The front buffer is a shared resource
- If two threads call setVisualBuffer() at the same time, you get race conditions
- You may flip mid‑draw, causing tearing or corrupted frames
- BGI drawing functions are not thread‑safe unless you explicitly lock them
  
Using multiple buffers in a multi‑threaded environment is possible, but requires careful synchronization.

<B>Safe Model</B>

    * One thread draws into back buffers.

    * Another thread (or the same one) flips buffers to the screen.

    * Use mutexes or atomic flags to coordinate which buffer is ready.

<B>Example Pattern</B>

```
Thread A (renderer):
    setActiveBuffer(buffers[next])
    clearBuffer(buffers[next])
    draw_scene()
    mark buffers[next] as ready

Thread B (display):
    wait for ready buffer
    setVisualBuffer(readyBuffer)
```

This avoids race conditions and ensures stable rendering.

### 6. Should You Use More Than 2 Buffers?
✔ Yes, if:
- You want triple buffering (reduces blocking)
- You want multi‑stage pipelines (e.g., one thread draws backgrounds, another draws sprites)
- You want temporal effects (motion blur, history buffers)

✖ No, if:
- You only need simple animation
- You want maximum FPS
- You don’t want to manage synchronization
  
For most real‑time rendering, 2 or 3 buffers is ideal. 3+ buffers is possible but requires careful design.

### 7. Summary

Double buffering in your enhanced BGI library follows this loop:

```C++
setActiveBuffer(backBuffer)
clearBuffer(backBuffer)
draw()
setVisualBuffer(backBuffer)
```

For advanced use, you may create multiple buffers and rotate through them, provided you manage synchronization carefully when using multiple threads.