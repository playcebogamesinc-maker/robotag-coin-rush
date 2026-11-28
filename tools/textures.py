#!/usr/bin/env python3
"""Coin Rush's stone textures, made from noise (docs/environment.md): tileable 512 px images
(each covers 2 x 2 m: materials use tiling 0.5) with a green-up normal map each.

  content/textures/env/sandstone.png (+ _n): ashlar wall blocks, 0.5 m courses
  content/textures/env/slabs.png (+ _n):     1 m floor slabs (seams on whole metres, under the rails)
  content/textures/env/glyphs.png (+ _n):    carved hieroglyph columns for the glyph panels

The images are committed (content); rerun only to change them:
  python3 games/coin-rush/tools/textures.py games/coin-rush
Needs numpy and Pillow."""
import os, random, sys

import numpy as np
from PIL import Image

N = 512            # Pixels per side = 2 m.
PX = N / 2.0       # Pixels per metre.
SEED = 11


def noise(rng, scale, n=N):
    """Tileable fractal noise in about [-1, 1]: white noise filtered in frequency space (the
    FFT wraps, so the edges match). `scale` = feature size in pixels."""
    f = np.fft.fftfreq(n)
    r = np.sqrt(f[None, :] ** 2 + f[:, None] ** 2)
    r[0, 0] = 1.0
    amp = r ** -1.6 * (r > 1.0 / (scale * 4)) * np.exp(-(r * scale / 3.0) ** 2)
    w = rng.standard_normal((n, n)) + 1j * rng.standard_normal((n, n))
    out = np.real(np.fft.ifft2(w * amp))
    return out / (np.abs(out).max() + 1e-9)


def blur(a, k):
    """Wrapping box blur, k pixels, twice (smooth bevels)."""
    for _ in range(2):
        acc = np.zeros_like(a)
        for d in range(-k, k + 1):
            acc += np.roll(a, d, axis=0)
        a = acc / (2 * k + 1)
        acc = np.zeros_like(a)
        for d in range(-k, k + 1):
            acc += np.roll(a, d, axis=1)
        a = acc / (2 * k + 1)
    return a


def normal_map(height, strength):
    """Height (0..1, pixels) -> RGB normal map, OpenGL style (green up: +v is up the image)."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * strength
    dy = (np.roll(height, 1, axis=0) - np.roll(height, -1, axis=0)) * 0.5 * strength
    n = np.stack([-dx, -dy, np.ones_like(height)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).round().astype(np.uint8)


def save(path, rgb):
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB").save(path, optimize=True)


def srgb(linear):
    return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * np.power(linear, 1 / 2.4) - 0.055)


def colorize(base, tone, grain, speck):
    """base: linear RGB; tone: per-pixel brightness factor (H x W)."""
    c = np.array(base)[None, None, :] * tone[..., None]
    c = c * (1 + 0.08 * grain[..., None] * np.array([1.0, 0.9, 0.7]))
    c = c * (1 - 0.35 * speck[..., None])
    return srgb(np.clip(c, 0, 1)) * 255


def blocks(rng, courses, lengths, joint, chip):
    """A running-bond block pattern: per-pixel block id, and a height map (1 on the faces,
    0 in the joints, bevelled, chipped edges)."""
    ids = np.zeros((N, N), np.int32)
    inside = np.zeros((N, N))
    ch = N // courses
    for row in range(courses):
        # Block lengths that add up to the image width, with a random offset (they wrap).
        xs, x = [], 0
        while x < N - lengths[0] * PX * 0.6:
            xs.append(x)
            x += int(PX * rng.uniform(*lengths))
        xs.append(N)
        shift = rng.randrange(N)
        for k in range(len(xs) - 1):
            x0, x1 = xs[k], xs[k + 1]
            cols = (np.arange(x0, x1) + shift) % N
            ids[row * ch:(row + 1) * ch, cols] = row * 100 + k
            inside[row * ch + joint:(row + 1) * ch - joint, (np.arange(x0 + joint, x1 - joint) + shift) % N] = 1
    edge_noise = noise(np.random.default_rng(rng.randrange(1 << 30)), 6)
    h = blur(inside, 3)
    h = np.clip((h - 0.5 + chip * edge_noise) * 2.2 + 0.5, 0, 1)
    return ids, h


def per_block(ids, rng, lo, hi):
    vals = {}
    out = np.zeros(ids.shape)
    for i in np.unique(ids):
        vals[i] = rng.uniform(lo, hi)
        out[ids == i] = vals[i]
    return out


def sandstone(rng, nrng):
    ids, h = blocks(rng, courses=4, lengths=(0.7, 1.25), joint=3, chip=0.35)
    tone = per_block(ids, rng, 0.8, 1.15)
    grain = noise(nrng, 3) * 0.6 + noise(nrng, 40) * 0.6
    stain = noise(nrng, 90)
    speck = (noise(nrng, 1.5) > 0.55).astype(float) * 0.6
    pits = np.clip(noise(nrng, 4) - 0.45, 0, 1) * 2.5
    face = tone * (1 + 0.12 * stain) * (1 - 0.25 * pits)
    t = face * (0.35 + 0.65 * h)                 # Joints dark.
    col = colorize([0.62, 0.42, 0.24], t, grain, speck)
    height = h * 0.8 + 0.06 * grain - 0.15 * pits + 0.05 * stain
    return col, normal_map(height, 3.0)


def slabs(rng, nrng):
    # 2 x 2 slabs of 1 m: joints on the image edges and the middle (= whole metres in the world).
    h = np.ones((N, N))
    j = 3
    for c in (0, N // 2):
        idx = (np.arange(c - j, c + j)) % N
        h[idx, :] = 0
        h[:, idx] = 0
    edge_noise = noise(nrng, 6)
    h = np.clip((blur(h, 3) - 0.5 + 0.3 * edge_noise) * 2.2 + 0.5, 0, 1)
    ids = (np.arange(N)[:, None] // (N // 2)) * 2 + np.arange(N)[None, :] // (N // 2)
    tone = per_block(ids, rng, 0.82, 1.12)
    grain = noise(nrng, 3) * 0.5 + noise(nrng, 50) * 0.7
    wear = noise(nrng, 120)
    speck = (noise(nrng, 1.5) > 0.6).astype(float) * 0.5
    # A few cracks: random walks, darkened and sunk.
    crack = np.zeros((N, N))
    for _ in range(3):
        x, y, a = rng.randrange(N), rng.randrange(N), rng.uniform(0, 6.28)
        for _ in range(rng.randrange(60, 180)):
            a += rng.uniform(-0.5, 0.5)
            x, y = (x + np.cos(a) * 1.5) % N, (y + np.sin(a) * 1.5) % N
            crack[int(y), int(x)] = 1
    crack = np.clip(blur(crack, 1) * 3, 0, 1)
    t = tone * (1 + 0.15 * wear) * (0.3 + 0.7 * h) * (1 - 0.3 * crack)
    col = colorize([0.5, 0.33, 0.19], t, grain, speck)
    height = h * 0.7 + 0.05 * grain - 0.25 * crack
    return col, normal_map(height, 2.5)


def glyphs(rng, nrng):
    """Columns of carved signs on smooth stone: 4 columns per metre-wide half (each 0.25 m)."""
    carve = np.zeros((N, N))
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    colw = N // 8
    # Column dividers.
    for c in range(8):
        x = c * colw
        carve[:, (np.arange(x - 1, x + 2)) % N] = 1
    cell = colw  # Square cells.
    for c in range(8):
        for r in range(N // cell):
            cx, cy, s = c * colw + colw / 2, r * cell + cell / 2, cell * 0.36
            kind = rng.randrange(7)
            dx, dy = xx - cx, yy - cy
            m = np.zeros((N, N), bool)
            if kind == 0:   # Sun disk.
                m = (dx ** 2 + dy ** 2 < s ** 2) & (dx ** 2 + dy ** 2 > (s * 0.6) ** 2)
            elif kind == 1:  # Ankh.
                m = ((np.abs(dx) < s * 0.15) & (dy > -s * 0.2) & (dy < s)) | \
                    ((np.abs(dy - s * 0.05) < s * 0.13) & (np.abs(dx) < s * 0.7)) | \
                    ((dx ** 2 / 0.25 + (dy + s * 0.55) ** 2 / 0.2 < s ** 2 * 0.5) &
                     (dx ** 2 / 0.25 + (dy + s * 0.55) ** 2 / 0.2 > s ** 2 * 0.18))
            elif kind == 2:  # Water (zigzag).
                for k in (-0.4, 0.0, 0.4):
                    m |= (np.abs(dy - k * s - 0.18 * s * np.sin(dx / s * 7)) < s * 0.09) & (np.abs(dx) < s)
            elif kind == 3:  # Eye.
                m = ((dx ** 2 / 1.0 + dy ** 2 / 0.18 < s ** 2) & (dx ** 2 / 1.0 + dy ** 2 / 0.18 > s ** 2 * 0.55)) | \
                    (dx ** 2 + dy ** 2 < (s * 0.2) ** 2) | \
                    ((np.abs(dx + s * 0.2) < s * 0.09) & (dy > s * 0.3) & (dy < s))
            elif kind == 4:  # Bird: body, head, legs.
                m = ((dx + s * 0.1) ** 2 / 0.6 + dy ** 2 / 0.3 < (s * 0.6) ** 2) | \
                    ((dx - s * 0.45) ** 2 + (dy + s * 0.4) ** 2 < (s * 0.22) ** 2) | \
                    ((np.abs(dx) < s * 0.07) & (dy > s * 0.2) & (dy < s * 0.9))
            elif kind == 5:  # Reed / feather.
                m = ((np.abs(dx - 0.25 * dy) < s * 0.2 * (1 - np.abs(dy) / s)) & (np.abs(dy) < s))
            else:  # Steps / cartouche bars.
                for k in (-0.5, 0.0, 0.5):
                    m |= (np.abs(dy - k * s) < s * 0.12) & (np.abs(dx) < s * (0.9 - 0.3 * abs(k)))
            carve[m] = 1
    carve = blur(carve, 1)
    grain = noise(nrng, 3) * 0.5 + noise(nrng, 60) * 0.6
    speck = (noise(nrng, 1.5) > 0.6).astype(float) * 0.4
    t = (1 + 0.1 * noise(nrng, 100)) * (1 - 0.55 * carve)
    col = colorize([0.66, 0.46, 0.27], t, grain, speck)
    height = 1 - carve * 0.8 + 0.04 * grain
    return col, normal_map(height, 3.0)


def main():
    out = os.path.join(sys.argv[1], "content/textures/env")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("sandstone", sandstone), ("slabs", slabs), ("glyphs", glyphs)):
        rng = random.Random(f"{SEED}{name}")
        nrng = np.random.default_rng([SEED, len(name), ord(name[0])])
        col, nor = fn(rng, nrng)
        save(os.path.join(out, name + ".png"), col)
        save(os.path.join(out, name + "_n.png"), nor)
        with open(os.path.join(out, name + "_n.png.texture.json"), "w") as f:
            f.write('{\n  "type": "texture",\n  "version": 1,\n  "kind": "normal"\n}\n')
        print(name, col.shape)


main()
