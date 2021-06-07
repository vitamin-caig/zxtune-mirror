package app.zxtune.playlist.xspf

import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import app.zxtune.TimeStamp
import app.zxtune.core.Identifier
import app.zxtune.io.Io.copy
import app.zxtune.playlist.Item
import app.zxtune.playlist.xspf.XspfIterator.parse
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * TODO:
 * - item's properties
 */
@RunWith(AndroidJUnit4::class)
class XspfPlaylistTest {
    @Test(expected = IOException::class)
    fun parseEmptyString() {
        parse(getPlaylistResource("zero"))
        fail("Should not be called")
    }

    @Test
    fun parseEmpty() = parse(getPlaylistResource("empty")).let {
        assertTrue(it.isEmpty())
    }

    @Test
    fun parseMobileV1() = parse(getPlaylistResource("mobile_v1")).let { list ->
        assertEquals(3, list.size.toLong())
        with(list[0]) {
            assertEquals("file:/folder/only%20duration", location)
            assertEquals(TimeStamp.createFrom(123456, TimeUnit.MILLISECONDS), duration)
            assertEquals("", title)
            assertEquals("", author)
        }
        with(list[1]) {
            assertEquals(
                "joshw:/some/%D1%84%D0%B0%D0%B9%D0%BB.7z#%D0%BF%D0%BE%D0%B4%D0%BF%D0%B0%D0%BF%D0%BA%D0%B0%2Fsubfile.mp3",
                location
            )
            assertEquals(TimeStamp.createFrom(2345, TimeUnit.MILLISECONDS), duration)
            assertEquals("Название", title)
            assertEquals("Author Unknown", author)
        }
        with(list[2]) {
            assertEquals("hvsc:/GAMES/file.sid#%233", location)
            assertEquals(TimeStamp.createFrom(6789, TimeUnit.MILLISECONDS), duration)
            assertEquals("Escaped%21", title)
            assertEquals("Me%)", author)
        }
    }

    @Test
    fun parseMobileV2() = parse(getPlaylistResource("mobile_v2")).let { list ->
        assertEquals(3, list.size.toLong())
        with(list[0]) {
            assertEquals("file:/folder/only%20duration", location)
            assertEquals(TimeStamp.createFrom(123456, TimeUnit.MILLISECONDS), duration)
            assertEquals("", title)
            assertEquals("", author)
        }
        with(list[1]) {
            assertEquals(
                "joshw:/some/%D1%84%D0%B0%D0%B9%D0%BB.7z#%D0%BF%D0%BE%D0%B4%D0%BF%D0%B0%D0%BF%D0%BA%D0%B0%2Fsubfile.mp3",
                location
            )
            assertEquals(TimeStamp.createFrom(2345, TimeUnit.MILLISECONDS), duration)
            assertEquals("Название", title)
            assertEquals("Author Unknown", author)
        }
        with(list[2]) {
            assertEquals("hvsc:/GAMES/file.sid#%233", location)
            assertEquals(TimeStamp.createFrom(6789, TimeUnit.MILLISECONDS), duration)
            assertEquals("Escaped%21", title)
            assertEquals("Me%)", author)
        }
    }

    @Test
    fun parseDesktop() = parse(getPlaylistResource("desktop")).let { list ->
        assertEquals(2, list.size.toLong())
        with(list[0]) {
            assertEquals("chiptunes/RP2A0X/nsfe/SuperContra.nsfe#%232", location)
            assertEquals(TimeStamp.createFrom(111300, TimeUnit.MILLISECONDS), duration)
            assertEquals("Stage 1 - Lightning and Grenades", title)
            assertEquals("H.Maezawa", author)
        }
        with(list[1]) {
            assertEquals("../chiptunes/DAC/ZX/dst/Untitled.dst", location)
            assertEquals(TimeStamp.createFrom(186560, TimeUnit.MILLISECONDS), duration)
            assertEquals("Untitled2(c)&(p)Cj.Le0'99aug", title)
            assertEquals("", author)
        }
    }

    @Test
    fun createEmpty() = with(ByteArrayOutputStream(1024)) {
        Builder(this).apply {
            writePlaylistProperties("Пустой", null)
        }.finish()
        assertEquals(getPlaylistReference("empty"), toString())
    }

    @Test
    fun createFull() = with(ByteArrayOutputStream(1024)) {
        Builder(this).apply {
            writePlaylistProperties("Полный", 3)
            writeTrack(makeItem("file:/folder/only duration", "", "", 123456))
            writeTrack(
                makeItem(
                    "joshw:/some/файл.7z#подпапка/subfile.mp3",
                    "Название",
                    "Author Unknown",
                    2345
                )
            )
            writeTrack(makeItem("hvsc:/GAMES/file.sid##3", "Escaped%21", "Me%)", 6789))
        }.finish()
        assertEquals(getPlaylistReference("mobile_v2"), toString())
    }
}

private fun getPlaylistResource(name: String) =
    XspfPlaylistTest::class.java.classLoader!!.getResourceAsStream("playlists/$name.xspf")

private fun getPlaylistReference(name: String) = with(ByteArrayOutputStream()) {
    copy(getPlaylistResource(name), this)
    toString()
}

private fun makeItem(id: String, title: String, author: String, duration: Long) =
    Item(createIdentifier(id), title, author, TimeStamp.createFrom(duration, TimeUnit.MILLISECONDS))

// To force encoding
private fun createIdentifier(decoded: String) = with(Uri.parse(decoded)) {
    Identifier(Uri.Builder().scheme(scheme).path(path).fragment(fragment).build())
}
