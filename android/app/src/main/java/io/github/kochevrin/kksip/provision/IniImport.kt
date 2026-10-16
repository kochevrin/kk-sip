package io.github.kochevrin.kksip.provision

import android.net.Uri
import android.util.Base64
import io.github.kochevrin.kksip.sip.Account
import java.io.ByteArrayOutputStream
import java.util.zip.Inflater

/**
 * Accounts from a kk-sip INI file: the policy.ini format of docs/provisioning.md. Keys
 * outside a section are general settings, every [Account…] section is an account. On the
 * phone these become the user's own accounts (nothing is locked).
 *
 * The same text also arrives packed into a link, `kksip://import?d=<base64url(deflate)>`,
 * which is what the QR code from the computer holds: the phone's camera opens kk-sip.
 */
object IniImport {
    data class Result(val accounts: List<Account>, val general: Map<String, String>)

    fun parse(text: String): Result {
        val general = mutableMapOf<String, String>()
        val groups = linkedMapOf<String, MutableMap<String, String>>()
        var section: String? = null
        for (raw in text.lineSequence()) {
            val line = raw.trim().removePrefix("﻿")
            if (line.isEmpty() || line.startsWith(";") || line.startsWith("#")) continue
            if (line.startsWith("[") && line.endsWith("]")) {
                section = line.substring(1, line.length - 1).trim()
                continue
            }
            val eq = line.indexOf('=')
            if (eq <= 0) continue
            val key = line.substring(0, eq).trim().lowercase()
            val value = unquote(line.substring(eq + 1).trim())
            val s = section
            if (s == null || s.equals("General", true)) general[key] = value
            else groups.getOrPut(s) { mutableMapOf() }[key] = value
        }
        val accounts = groups
            .filterKeys { it.startsWith("account", ignoreCase = true) }
            .entries
            // Account, Account2, …, Account10: shorter names first, as on the desktop.
            .sortedWith(compareBy({ it.key.length }, { it.key.lowercase() }))
            .mapNotNull { (_, v) -> toAccount(v) }
        return Result(accounts, general)
    }

    private fun toAccount(v: Map<String, String>): Account? {
        val server = v["server"].orEmpty()
        if (server.isEmpty()) return null
        fun bool(k: String, default: Boolean) = v[k]?.lowercase()?.let { it == "1" || it == "true" || it == "yes" } ?: default
        return Account(
            label = v["label"].orEmpty(),
            server = server,
            domain = v["domain"].orEmpty(),
            proxy = v["proxy"].orEmpty(),
            user = v["user"].orEmpty(),
            authUser = v["authuser"].orEmpty(),
            password = v["password"].orEmpty(),
            displayName = v["displayname"].orEmpty(),
            transport = v["transport"]?.lowercase()?.takeIf { it == "udp" || it == "tcp" || it == "tls" } ?: "udp",
            srtp = bool("srtp", false),
            natRewrite = bool("natrewrite", false),
            enabled = bool("enabled", true),
        )
    }

    /** QSettings writes values with special characters in double quotes, with \ escapes. */
    private fun unquote(v: String): String {
        if (v.length < 2 || !v.startsWith("\"") || !v.endsWith("\"")) return v
        val s = v.substring(1, v.length - 1)
        val out = StringBuilder()
        var i = 0
        while (i < s.length) {
            val c = s[i]
            if (c == '\\' && i + 1 < s.length) {
                when (val n = s[i + 1]) {
                    'n' -> out.append('\n')
                    't' -> out.append('\t')
                    'r' -> out.append('\r')
                    else -> out.append(n)
                }
                i += 2
            } else {
                out.append(c); i++
            }
        }
        return out.toString()
    }

    /** The INI text packed in a kksip://import link, or null if it isn't one. */
    fun fromLink(uri: Uri): String? {
        if (uri.scheme != "kksip" || uri.host != "import") return null
        val d = uri.getQueryParameter("d") ?: return null
        return runCatching {
            val packed = Base64.decode(d, Base64.URL_SAFE or Base64.NO_PADDING or Base64.NO_WRAP)
            val inflater = Inflater(true) // raw deflate
            inflater.setInput(packed)
            val out = ByteArrayOutputStream()
            val buf = ByteArray(4096)
            while (!inflater.finished()) {
                val n = inflater.inflate(buf)
                if (n == 0 && (inflater.needsInput() || inflater.needsDictionary())) break
                out.write(buf, 0, n)
            }
            inflater.end()
            out.toString(Charsets.UTF_8.name())
        }.getOrNull()
    }

    /**
     * Merges imported accounts into the existing list: the same server and user updates
     * the account in place (keeping its id), anything else is added at the end.
     */
    fun merge(existing: List<Account>, imported: List<Account>): List<Account> {
        val out = existing.toMutableList()
        for (a in imported) {
            val i = out.indexOfFirst { it.server.equals(a.server, true) && it.user == a.user && it.domain == a.domain }
            if (i >= 0) out[i] = a.copy(id = out[i].id) else out += a
        }
        return out
    }
}
