
import s3img
import time
import gc

ok = 0
fail = 0

def check(name, condition):
    global ok, fail
    if condition:
        ok += 1
        print("OK:", name)
    else:
        fail += 1
        print("FAIL:", name)

# RGB888: rojo, verde, azul y blanco
rgb = bytearray([
    255,0,0,
    0,255,0,
    0,0,255,
    255,255,255
])

gray = bytearray(4)

s3img.grayscale(rgb, gray, 2, 2, 3)

print("Grayscale:", list(gray))

check("grayscale RGB888", list(gray) == [76,149,28,255])

# Inversion
s3img.invert(gray)

check("invert", list(gray) == [179,106,227,0])

# Monocromatico
mono = bytearray(4)
s3img.threshold(gray, mono, 128)

check("threshold", list(mono) == [255,0,255,0])

# Sobel
src = bytearray([
    0,0,0,
    0,255,0,
    0,0,0
])

dst = bytearray(9)

s3img.sobel(src, dst, 3, 3)

print("Sobel:", list(dst))

check("sobel", any(dst))

# Resize
small = bytearray([10,20,30,40])
large = bytearray(16)

s3img.resize(small, large, 2, 2, 4, 4, 1)

check("resize", len(large) == 16)
check("resize corners", large[0] == 10 and large[15] == 40)

# Benchmark
N = 320 * 240
src = bytearray([i & 255 for i in range(N)])
dst = bytearray(N)

def benchmark(fn, repetitions=20):
    start = time.ticks_us()

    for _ in range(repetitions):
        fn()

    return time.ticks_diff(time.ticks_us(), start) // repetitions

gc.collect()

print("Sobel 320x240:", benchmark(
    lambda: s3img.sobel(src, dst, 320, 240)
), "us")

print("Grayscale 320x240:", benchmark(
    lambda: s3img.grayscale(
        bytearray(320*240*3),
        dst, 320, 240, 3
    ), 3
), "us")

print("RESULTADO:", ok, "OK,", fail, "FAIL")

