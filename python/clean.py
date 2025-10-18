# VeloTick marker
import time
import velotick


def sample_clean_once():
    t = velotick.get_raw_tick()
    if t:
        velotick.put_clean_tick(t.ts, t.instrument, t.p, t.p)


if __name__ == "__main__":
    for _ in range(10):
        sample_clean_once()
        time.sleep(0.01)
# VeloTick marker