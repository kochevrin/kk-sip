package io.github.kochevrin.kksip.sip

/** Port of the desktop SipUri helpers (src/core/SipUri.cpp). */
object SipUri {
    data class Parsed(val displayName: String, val user: String, val host: String)

    private val junk = Regex("""[\s\-.()]""")
    private val linkScheme = Regex("""^(tel|callto):(//)?""", RegexOption.IGNORE_CASE)
    private val uriScheme = Regex("""^(sips?|tel):""", RegexOption.IGNORE_CASE)

    /** Keeps only characters that make sense in a typed or pasted phone number. */
    fun cleanNumber(input: String): String {
        val s = input.trim()
        return if ('@' in s) s else s.replace(junk, "")
    }

    /** A dialable SIP URI from whatever the user typed, or "" for unusable input. */
    fun toTarget(input: String, account: Account): String {
        var s = cleanNumber(input)
        if (s.isEmpty()) return ""
        val hasScheme = s.startsWith("sip:", true) || s.startsWith("sips:", true)
        if (!hasScheme) {
            s = s.replace(linkScheme, "")
            if ('@' !in s) s += "@" + account.sipDomain
            s = "sip:$s"
        }
        if (account.transport != "udp" && !s.contains(";transport=", true))
            s += ";transport=" + account.transport
        return s
    }

    /** Parses `"Alice" <sip:101@pbx;transport=tcp>`. */
    fun parse(uri: String): Parsed {
        var addr = uri.trim()
        var name = ""
        val lt = addr.indexOf('<')
        if (lt >= 0) {
            name = addr.substring(0, lt).trim().removeSurrounding("\"")
            val gt = addr.indexOf('>', lt)
            addr = if (gt > lt) addr.substring(lt + 1, gt) else addr.substring(lt + 1)
        }
        addr = addr.replace(uriScheme, "").substringBefore(';')
        val at = addr.indexOf('@')
        return if (at >= 0) Parsed(name, addr.substring(0, at), addr.substring(at + 1))
        else Parsed(name, addr, "")
    }
}
