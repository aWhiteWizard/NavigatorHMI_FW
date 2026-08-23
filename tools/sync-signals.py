# E 循环: 统一 18 个 Hmi 组件全 19 事件信号声明（审查 699b9804 必修1）
# 防生成器输出 onHmiXXX 对未声明信号 → Cannot assign 整屏失败（坑5）
import re, glob, io

SIGNALS = ["hmiClicked","hmiPressed","hmiReleased","hmiValueChanged","hmiAlarmTrigger",
           "hmiAlarmAck","hmiAlarmClear","hmiTimer","hmiSystemStart","hmiSystemShutdown",
           "hmiScreenLoad","hmiScreenUnload","hmiInput","hmiOn","hmiOff",
           "hmiProgressComplete","hmiUserChanged","hmiAck","hmiSelect"]

files = glob.glob(r"D:\workspace\code\NavigatorHMI_FW\src\qml\components\Hmi*.qml")
for f in files:
    with io.open(f, "r", encoding="utf-8") as fh:
        lines = fh.readlines()
    sig_re = re.compile(r"^\s*signal (hmi\w+)\s*\(")
    existing = set()
    sig_idx = []
    for i, l in enumerate(lines):
        m = sig_re.match(l)
        if m:
            existing.add(m.group(1))
            sig_idx.append(i)
    missing = [s for s in SIGNALS if s not in existing]
    if not missing:
        print(f"{f.split(chr(92))[-1]}: ok (all 19)")
        continue
    # 插入位置: 有信号则最后一个 signal 行后; 无信号则最后一个 property 行后
    if sig_idx:
        insert_at = sig_idx[-1]
    else:
        prop_idx = [i for i, l in enumerate(lines) if re.match(r"^\s*property ", l)]
        insert_at = prop_idx[-1] if prop_idx else 0
    new_block = ["    signal %s()\n" % s for s in missing]
    lines = lines[:insert_at+1] + new_block + lines[insert_at+1:]
    with io.open(f, "w", encoding="utf-8", newline="") as fh:
        fh.writelines(lines)
    print(f"{f.split(chr(92))[-1]}: +{len(missing)} signals {missing}")
print("done")
