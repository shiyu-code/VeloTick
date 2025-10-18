# VeloTick marker
import time
from collections import deque
import velotick


def run():
    start = time.time()
    windows = {}
    while time.time() - start < 2.0:
        t = velotick.get_raw_tick()
        if t is None:
            time.sleep(0.001)
            continue
        dq = windows.setdefault(t.instrument, deque(maxlen=5))
        dq.append(t.p)
        ma5 = sum(dq) / len(dq)
        velotick.put_clean_tick(t.ts, t.instrument, t.p, ma5)


if __name__ == "__main__":
    run()
# VeloTick marker