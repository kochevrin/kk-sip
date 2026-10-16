package io.github.kochevrin.kksip.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.concurrent.Executors

data class Contact(val id: Long, val name: String, val number: String)

/** kk-sip's own phone book, as on the desktop: a name and a number, CSV in and out. */
class ContactBook(context: Context) {
    private val db = Helper(context.applicationContext)
    private val io = Executors.newSingleThreadExecutor()
    private val _contacts = MutableStateFlow<List<Contact>>(emptyList())
    val contacts: StateFlow<List<Contact>> = _contacts.asStateFlow()

    init {
        io.execute { reload() }
    }

    /** Name for a number, matched on digits only (so "+380 44 …" finds "38044…"). */
    fun nameFor(number: String): String {
        val key = digits(number)
        if (key.isEmpty()) return ""
        return _contacts.value.firstOrNull { digits(it.number) == key }?.name.orEmpty()
    }

    fun save(c: Contact) = io.execute {
        val values = ContentValues().apply { put("name", c.name); put("number", c.number) }
        if (c.id > 0) db.writableDatabase.update("contacts", values, "id = ?", arrayOf(c.id.toString()))
        else db.writableDatabase.insert("contacts", null, values)
        reload()
    }

    fun delete(id: Long) = io.execute {
        db.writableDatabase.delete("contacts", "id = ?", arrayOf(id.toString()))
        reload()
    }

    /** Adds what is not there yet (same name and number); returns through [onDone] how many. */
    fun merge(found: List<Contact>, onDone: (Int) -> Unit) = io.execute {
        val known = _contacts.value.map { it.name to digits(it.number) }.toMutableSet()
        var added = 0
        db.writableDatabase.beginTransaction()
        try {
            for (c in found) {
                if (c.number.isBlank() || !known.add(c.name to digits(c.number))) continue
                db.writableDatabase.insert("contacts", null,
                    ContentValues().apply { put("name", c.name); put("number", c.number) })
                added++
            }
            db.writableDatabase.setTransactionSuccessful()
        } finally {
            db.writableDatabase.endTransaction()
        }
        reload()
        onDone(added)
    }

    private fun reload() {
        _contacts.value = db.readableDatabase.rawQuery(
            "SELECT id, name, number FROM contacts ORDER BY name COLLATE NOCASE, number", null,
        ).use { c -> buildList { while (c.moveToNext()) add(Contact(c.getLong(0), c.getString(1), c.getString(2))) } }
    }

    private class Helper(context: Context) : SQLiteOpenHelper(context, "contacts.db", null, 1) {
        override fun onCreate(db: SQLiteDatabase) {
            db.execSQL("CREATE TABLE contacts (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, number TEXT NOT NULL)")
        }

        override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) = Unit
    }

    companion object {
        fun digits(s: String) = s.filter { it.isDigit() || it == '*' || it == '#' }

        /** The desktop's CSV: "name,number" (comma or semicolon, quotes with ""), header optional. */
        fun parseCsv(text: String): List<Contact> = text.lineSequence().mapNotNull { line ->
            val cols = splitCsv(line)
            if (cols.size < 2) return@mapNotNull null
            val number = cols[1].trim()
            if (number.isEmpty() || number.equals("number", true)) null else Contact(0, cols[0].trim(), number)
        }.toList()

        fun toCsv(list: List<Contact>): String = buildString {
            append("name,number\n")
            fun q(s: String) = "\"" + s.replace("\"", "\"\"") + "\""
            for (c in list) append(q(c.name)).append(',').append(q(c.number)).append('\n')
        }

        private fun splitCsv(line: String): List<String> {
            val out = mutableListOf<String>()
            val cur = StringBuilder()
            var quoted = false
            var i = 0
            while (i < line.length) {
                val ch = line[i]
                when {
                    quoted && ch == '"' && i + 1 < line.length && line[i + 1] == '"' -> { cur.append('"'); i++ }
                    ch == '"' -> quoted = !quoted
                    !quoted && (ch == ',' || ch == ';') -> { out += cur.toString(); cur.clear() }
                    else -> cur.append(ch)
                }
                i++
            }
            out += cur.toString()
            return out
        }
    }
}
