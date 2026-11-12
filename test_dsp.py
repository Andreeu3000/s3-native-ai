import s3dsp, array, math, time
print(s3dsp.impl())

a = array.array('f', [1,2,3,4,5,6,7,8])
b = array.array('f', [8,7,6,5,4,3,2,1])
print("dot", s3dsp.dot(a, b), "esperado 120.0")

N = 1024
x = array.array('f', [0.0]*(2*N))
for n in range(N):
    x[2*n] = math.sin(2*math.pi*50*n/N)
s3dsp.fft(x)
mag = [x[2*k]**2 + x[2*k+1]**2 for k in range(N//2)]
print("pico FFT en bin", mag.index(max(mag)), "esperado 50")

n = 1024
u = array.array('f', [i*0.001 for i in range(n)])
v = array.array('f', [1.0]*n)
def t(f, r=20):
    s = time.ticks_us()
    for _ in range(r): f()
    return time.ticks_diff(time.ticks_us(), s)//r
def py():
    return sum(p*q for p, q in zip(u, v))
print("python us:", t(py))
print("dot_ansi us:", t(lambda: s3dsp.dot_ansi(u, v)))
print("dot us:", t(lambda: s3dsp.dot(u, v)))
