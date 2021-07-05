package app.zxtune.preferences

import android.os.Bundle
import androidx.core.content.edit
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import app.zxtune.Preferences
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ProviderClientTest {
    @Before
    fun setup() {
        val prefs =
            Preferences.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())
        prefs.edit(commit = true) {
            clear()
            putBoolean("zxtune.boolean", true)
            putInt("zxtune.int", 123)
            putLong("zxtune.long", 456)
            putString("zxtune.string", "test")
            putString("zxtune.long_in_string", "7890")
            putString("ignored.string", "ignored1")
            putBoolean("ignored.boolean", false)
            putInt("ignored.int", 23)
            putLong("ignored.long", 56)
        }
    }

    @Test
    fun testProviderClient() {
        val client = ProviderClient(ApplicationProvider.getApplicationContext())
        client.all.run {
            assertEquals(9, size().toLong())
            assertTrue(getBoolean("zxtune.boolean", false))
            assertFalse(getBoolean("none.boolean", false))
            assertEquals(123, getInt("zxtune.int", 0).toLong())
            assertEquals(23, getInt("none.int", 23).toLong())
            assertEquals(456, getLong("zxtune.long", 0))
            assertEquals(56, getLong("none.long", 56))
            assertEquals("test", getString("zxtune.string", ""))
            assertEquals("7890", getString("zxtune.long_in_string", ""))
            assertEquals("ignored1", getString("ignored.string", ""))
            assertEquals("default", getString("none.string", "default"))
        }
        val bundle = Bundle().apply {
            putLong("none.long", 567L)
            putString("none.string", "none")
            putBoolean("zxtune.boolean", false)
            putInt("zxtune.int", 12)
        }
        client.run {
            set("none.boolean", true)
            set("none.int", 234)
            set("zxtune.long", 45L)
            set("zxtune.string", "str")
            set(bundle)
        }
        assertEquals(13, client.all.size().toLong())
        client.get("none.")!!.run {
            assertEquals(4, size().toLong())
            assertTrue(getBoolean("none.boolean", false))
            assertEquals(234, getInt("none.int", 23).toLong())
            assertEquals(567, getLong("none.long", 56))
            assertEquals("none", getString("none.string", "default"))
        }
        client.get("zxtune.")!!.run {
            assertEquals(5, size().toLong())
            assertFalse(getBoolean("zxtune.boolean", true))
            assertEquals(12, getInt("zxtune.int", 23).toLong())
            assertEquals(45, getLong("zxtune.long", 56))
            assertEquals("str", getString("zxtune.string", "default"))
        }

        //TODO: test removal
    }
}
