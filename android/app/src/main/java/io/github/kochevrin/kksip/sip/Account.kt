package io.github.kochevrin.kksip.sip

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/** Same fields and meaning as the desktop AccountConfig and docs/provisioning.md. */
data class Account(
    val id: String = UUID.randomUUID().toString().take(8),
    val label: String = "",
    val server: String = "",
    val domain: String = "",
    val proxy: String = "",
    val user: String = "",
    val authUser: String = "",
    val password: String = "",
    val displayName: String = "",
    val transport: String = "udp",
    val srtp: Boolean = false,
    val natRewrite: Boolean = false,
    val enabled: Boolean = true, // off: kept, but neither registered nor offered for calls
) {
    val sipDomain: String get() = domain.ifEmpty { server }
    val title: String get() = label.ifEmpty { "$user@$sipDomain" }
    val isComplete: Boolean get() = server.isNotBlank() && user.isNotBlank()
    /** Registered and offered for calls. */
    val isUsable: Boolean get() = enabled && isComplete

    fun toJson(): JSONObject = JSONObject()
        .put("id", id).put("label", label).put("server", server).put("domain", domain)
        .put("proxy", proxy).put("user", user).put("authUser", authUser)
        .put("password", password).put("displayName", displayName)
        .put("transport", transport).put("srtp", srtp).put("natRewrite", natRewrite)
        .put("enabled", enabled)

    companion object {
        fun fromJson(o: JSONObject) = Account(
            id = o.optString("id").ifEmpty { UUID.randomUUID().toString().take(8) },
            label = o.optString("label"),
            server = o.optString("server"),
            domain = o.optString("domain"),
            proxy = o.optString("proxy"),
            user = o.optString("user"),
            authUser = o.optString("authUser"),
            password = o.optString("password"),
            displayName = o.optString("displayName"),
            transport = o.optString("transport", "udp"),
            srtp = o.optBoolean("srtp"),
            natRewrite = o.optBoolean("natRewrite"),
            enabled = o.optBoolean("enabled", true),
        )
    }
}

/** The user's accounts in the app's private preferences. */
class AccountStore(context: Context) {
    private val prefs = context.getSharedPreferences("accounts", Context.MODE_PRIVATE)

    fun load(): List<Account> {
        val json = prefs.getString("accounts", null) ?: return emptyList()
        return runCatching {
            val arr = JSONArray(json)
            List(arr.length()) { Account.fromJson(arr.getJSONObject(it)) }
        }.getOrDefault(emptyList())
    }

    fun save(accounts: List<Account>) {
        val arr = JSONArray()
        accounts.forEach { arr.put(it.toJson()) }
        prefs.edit().putString("accounts", arr.toString()).apply()
    }

    var currentId: String?
        get() = prefs.getString("current", null)
        set(value) = prefs.edit().putString("current", value).apply()
}
