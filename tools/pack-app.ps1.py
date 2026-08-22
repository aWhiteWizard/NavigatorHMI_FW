# 打包 FW 工程包: app.navihmi(ZIP) = inner .navihmi + tiles/ (正斜杠, Python zipfile)
import zipfile, os, shutil

SRC_INNER = r"D:\workspace\test_project\output\B4_测试工程.navihmi"
ZIPTMP = r"D:\workspace\code\NavigatorHMI_FW\tiles-demo\ziptmp"
OUT = r"D:\workspace\code\NavigatorHMI_FW\tiles-demo\app.navihmi"

# 1. 更新 inner
shutil.copy2(SRC_INNER, os.path.join(ZIPTMP, "app.navihmi"))
print("inner updated:", os.path.getsize(os.path.join(ZIPTMP, "app.navihmi")))

# 2. 打包 (正斜杠)
with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
    for root, dirs, files in os.walk(ZIPTMP):
        for f in sorted(files):
            p = os.path.join(root, f)
            rel = os.path.relpath(p, ZIPTMP).replace("\\", "/")
            z.write(p, rel)
print("packed:", OUT, os.path.getsize(OUT), "entries:", len(zipfile.ZipFile(OUT).namelist()))
# 校验 PK magic
with open(OUT, "rb") as fh:
    print("magic:", fh.read(2))
