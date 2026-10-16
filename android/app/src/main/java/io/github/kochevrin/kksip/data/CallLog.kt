package io.github.kochevrin.kksip.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.concurrent.Executors

data class CallRecord(
    val id: Long,
    val accountId: String,
    val number: String,
    val name: String,
    val incoming: Boolean,
    val missed: Boolean,
    val startedAt: Long, // epoch ms
    val durationSec: Int,
)

/** Recent calls in a small SQLite table, newest first, the last [KEEP] kept. */
class CallLog(context: Context) {
    private val db = Helper(context.applicationContext)
    private val io = Executors.newSingleThreadExecutor()
    private val _records = MutableStateFlow<List<CallRecord>>(emptyList())
    val records: StateFlow<List<CallRecord>> = _records.asStateFlow()

    init {
        io.execute { _records.value = query() }
    }

    fun add(r: CallRecord) = io.execute {
        db.writableDatabase.apply {
            insert("calls", null, ContentValues().apply {
                put("account_id", r.accountId)
                put("number", r.number)
                put("name", r.name)
                put("incoming", if (r.incoming) 1 else 0)
                put("missed", if (r.missed) 1 else 0)
                put("started_at", r.startedAt)
                put("duration", r.durationSec)
            })
            execSQL("DELETE FROM calls WHERE id NOT IN (SELECT id FROM calls ORDER BY started_at DESC LIMIT $KEEP)")
        }
        _records.value = query()
    }

    fun delete(id: Long) = io.execute {
        db.writableDatabase.delete("calls", "id = ?", arrayOf(id.toString()))
        _records.value = query()
    }

    fun clear() = io.execute {
        db.writableDatabase.delete("calls", null, null)
        _records.value = emptyList()
    }

    private fun query(): List<CallRecord> = db.readableDatabase.rawQuery(
        "SELECT id, account_id, number, name, incoming, missed, started_at, duration " +
            "FROM calls ORDER BY started_at DESC", null,
    ).use { c ->
        buildList {
            while (c.moveToNext()) add(
                CallRecord(
                    c.getLong(0), c.getString(1), c.getString(2), c.getString(3),
                    c.getInt(4) != 0, c.getInt(5) != 0, c.getLong(6), c.getInt(7),
                )
            )
        }
    }

    private class Helper(context: Context) : SQLiteOpenHelper(context, "calls.db", null, 1) {
        override fun onCreate(db: SQLiteDatabase) {
            db.execSQL(
                "CREATE TABLE calls (id INTEGER PRIMARY KEY AUTOINCREMENT, account_id TEXT NOT NULL, " +
                    "number TEXT NOT NULL, name TEXT NOT NULL, incoming INTEGER NOT NULL, " +
                    "missed INTEGER NOT NULL, started_at INTEGER NOT NULL, duration INTEGER NOT NULL)"
            )
            db.execSQL("CREATE INDEX calls_started ON calls(started_at)")
        }

        override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) = Unit
    }

    companion object {
        const val KEEP = 500
    }
}
