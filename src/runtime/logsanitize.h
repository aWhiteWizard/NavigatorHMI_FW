/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\logsanitize.h
 * @Description: 日志/输出脱敏工具（V-2 F18 2026-09-06 凭据分离规则落地）——
 *               敏感凭据（URL 密码 / password·token·secret 键值）**禁止**进日志/普通输出，
 *               统一经本工具掩码后输出。接入点：V+1 MQTT 凭据（broker user:pass、TLS key）等。
 *               随 fwconfig.h 一起被 main/httpreceiver/cli 编译（无独立消费者前确保编译覆盖——审查 🟡）。
 */
#pragma once

#include <QString>
#include <QStringList>

namespace navihmi {

/// URL 内嵌凭据掩码：`scheme://user:pass@host/path` → `scheme://user:***@host/path`
/// （MQTT broker URL / RTSP URL 等含 user:pass@ 时打日志前调用；无凭据原样返回；
///  凭据判定限定 authority 段——"://" 后至首个 '/'、'?' 或行尾之间找 '@'，防 path/query 误判）
inline QString redactUrlCredential(const QString& url)
{
    const int schemeEnd = url.indexOf(QLatin1String("://"));
    if (schemeEnd < 0)
        return url;
    // authority 段结束（首个 / 或 ?）
    const int authEnd = url.indexOf(QLatin1Char('/'), schemeEnd + 3);
    const int queryEnd = url.indexOf(QLatin1Char('?'), schemeEnd + 3);
    int end = url.size();
    if (queryEnd >= 0 && queryEnd < end)
        end = queryEnd;
    if (authEnd >= 0 && authEnd < end)
        end = authEnd;
    const int at = url.indexOf(QLatin1Char('@'), schemeEnd + 3);
    if (at < 0 || at > end)
        return url;   // authority 内无 @（无凭据）
    const int colon = url.indexOf(QLatin1Char(':'), schemeEnd + 3);
    if (colon < 0 || colon > at)
        return url;   // 仅用户名或裸 @ 段——不掩码
    // user:pass@ → user:***@（密码长度不泄露）
    return url.left(colon + 1) + QStringLiteral("***") + url.mid(at);
}

/// 键值日志脱敏：命中敏感键（password/token/secret/passwd 等小写键名，调用方传参）的值掩为 ***。
/// 支持形态：`key=value` / `key= value`（= 后可选空格）/ `"key":"value"` / `"key": "value"`（JSON
/// 紧凑或 Indented 冒号后空格）——值起点跳过空白，值到 '&'、'"' 或行尾止（含空白值终止于行尾——密码类
/// 约定不含空白；调用方按紧凑/常规输出即可）。未命中原样返回。
inline QString redactSensitiveValues(const QString& line, const QStringList& sensitiveKeys)
{
    QString out = line;
    for (const QString& key : sensitiveKeys) {
        int idx = 0;
        while (idx < out.size()) {
            const int kPos = out.indexOf(key, idx);
            if (kPos < 0)
                break;
            // 向后找分隔：'='（key=）或 '"' + ':'（"key":）——取最近的合法分隔
            int vStart = -1;
            const int eq = out.indexOf(QLatin1Char('='), kPos + key.size());
            const int colon = out.indexOf(QLatin1String("\":"), kPos + key.size());
            int sep = -1;
            if (eq >= 0 && (colon < 0 || eq < colon))
                sep = eq;
            else if (colon >= 0)
                sep = colon + 1;   // 指向冒号后（含可能的空格/引号在下方跳过）
            if (sep < 0) {
                idx = kPos + key.size();
                continue;
            }
            // 值起点：跳过空白与开引号
            vStart = sep + 1;
            while (vStart < out.size() && out.at(vStart).isSpace())
                ++vStart;
            if (vStart < out.size() && out.at(vStart) == QLatin1Char('"'))
                ++vStart;
            // 值终点：& / " / 行尾（空白值不在敏感键场景——密码等无空格；若值全空白则无掩码必要）
            int vEnd = vStart;
            while (vEnd < out.size() && out.at(vEnd) != QLatin1Char('&')
                   && out.at(vEnd) != QLatin1Char('"'))
                ++vEnd;
            if (vEnd > vStart) {
                out.replace(vStart, vEnd - vStart, QStringLiteral("***"));
                idx = vStart + 3;
            } else {
                idx = sep + 1;
            }
        }
    }
    return out;
}

} // namespace navihmi
