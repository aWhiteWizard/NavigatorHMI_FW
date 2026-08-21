#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Download AMap street tiles (style=8) for given bounds + zoom range -> tiles/z/x/y.png"""
import argparse, math, os, sys, time, urllib.request

TILE_URL = "https://webrd0{s}.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x={x}&y={y}&z={z}"
SUBDOMAINS = ["1", "2", "3", "4"]
UA = "NavigatorHMI/1.0 (Industrial HMI Config Tool)"

def lonlat_to_tile(lng, lat, z):
    n = 2 ** z
    x = (lng + 180.0) / 360.0 * n
    lat_rad = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    return x, y

def tile_range(lng_min, lng_max, lat_min, lat_max, z):
    x0, y_top = lonlat_to_tile(lng_min, lat_max, z)
    x1, y_bottom = lonlat_to_tile(lng_max, lat_min, z)
    xs = range(int(math.floor(x0)), int(math.floor(x1)) + 1)
    ys = range(int(math.floor(y_top)), int(math.floor(y_bottom)) + 1)
    return list(xs), list(ys)

def fetch_tile(z, x, y, out_path):
    url = TILE_URL.format(s=SUBDOMAINS[(x + y + z) % len(SUBDOMAINS)], x=x, y=y, z=z)
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Referer": "https://www.amap.com/"})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=15) as r:
                data = r.read()
            if len(data) < 200:
                return False
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            with open(out_path, "wb") as f:
                f.write(data)
            return True
        except Exception as e:
            time.sleep(1.5 * (attempt + 1))
    print(f"  !! fail z{z}/x{x}/y{y}", file=sys.stderr)
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lng-min", type=float, required=True)
    ap.add_argument("--lng-max", type=float, required=True)
    ap.add_argument("--lat-min", type=float, required=True)
    ap.add_argument("--lat-max", type=float, required=True)
    ap.add_argument("--z-min", type=int, default=10)
    ap.add_argument("--z-max", type=int, default=15)
    ap.add_argument("--out", default="tiles")
    args = ap.parse_args()

    total = ok = 0
    for z in range(args.z_min, args.z_max + 1):
        xs, ys = tile_range(args.lng_min, args.lng_max, args.lat_min, args.lat_max, z)
        print(f"z{z}: x{len(xs)} y{len(ys)} = {len(xs)*len(ys)} tiles")
        for x in xs:
            for y in ys:
                out = os.path.join(args.out, str(z), str(x), f"{y}.png")
                if os.path.exists(out) and os.path.getsize(out) > 200:
                    ok += 1; total += 1; continue
                if fetch_tile(z, x, y, out):
                    ok += 1
                total += 1
                time.sleep(0.05)
    print(f"=== done: {ok}/{total} tiles -> {args.out} ===")
    return 0 if ok == total else 1

if __name__ == "__main__":
    sys.exit(main())