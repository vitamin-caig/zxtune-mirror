package app.zxtune

import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.concurrent.TimeUnit

class TimeStampTest {
    @Test
    fun `test empty`() = with(TimeStamp.EMPTY) {
        assertEquals(0L, convertTo(TimeUnit.MICROSECONDS))
        assertEquals("0:00", toString())
    }

    @Test
    fun `test factory and precision`() {
        with(TimeStamp.createFrom(12345678, TimeUnit.MICROSECONDS)) {
            assertEquals(12345000L, convertTo(TimeUnit.MICROSECONDS))
        }
        with(TimeStamp.createFrom(12345678, TimeUnit.MILLISECONDS)) {
            assertEquals(12345678L, convertTo(TimeUnit.MILLISECONDS))
        }
        with(TimeStamp.createFrom(123456, TimeUnit.SECONDS)) {
            assertEquals(123456000, convertTo(TimeUnit.MILLISECONDS))
        }
    }

    @Test
    fun `test serialize`() {
        with(TimeStamp.createFrom(62, TimeUnit.SECONDS)) {
            assertEquals("1:02", toString())
        }
        with(TimeStamp.createFrom(3723, TimeUnit.SECONDS)) {
            assertEquals("1:02:03", toString())
        }
    }

    @Test
    fun `test compare`() {
        val s1 = TimeStamp.createFrom(1, TimeUnit.SECONDS)
        val ms1000 = TimeStamp.createFrom(1000, TimeUnit.MILLISECONDS)
        val ms1001 = TimeStamp.createFrom(1001, TimeUnit.MILLISECONDS)

        testCompare(s1, s1, 0)
        testCompare(s1, ms1000, 0)
        testCompare(s1, ms1001, -1)
        testCompare(ms1000, ms1000, 0)
        testCompare(ms1000, ms1001, -1)
    }

    @Test
    fun `test multiply`() = with(TimeStamp.createFrom(33, TimeUnit.MILLISECONDS)) {
        assertEquals(99, multiplies(3).convertTo(TimeUnit.MILLISECONDS))
    }
}

private fun testCompare(lh: TimeStamp, rh: TimeStamp, expected: Int) {
    val msg = "${lh.convertTo(TimeUnit.MILLISECONDS)}ms vs ${rh.convertTo(TimeUnit.MILLISECONDS)}ms"
    assertEquals(msg, expected, lh.compareTo(rh))
    assertEquals(msg, -expected, rh.compareTo(lh))
    assertEquals(msg, expected == 0, lh == rh)
    assertEquals(msg, lh == rh, rh == lh)
}
