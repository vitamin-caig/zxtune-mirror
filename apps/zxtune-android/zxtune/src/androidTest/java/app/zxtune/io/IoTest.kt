package app.zxtune.io

import org.junit.Assert.*
import org.junit.Test
import java.io.*
import java.nio.ByteBuffer
import java.nio.MappedByteBuffer

class IoTest {
    @Test
    fun testNonexisting() {
        val file = File("nonexisting_name")
        assertFalse(file.exists())
        try {
            Io.readFrom(file)
            fail("Unreachable")
        } catch (e: IOException) {
            assertNotNull("Thrown exception", e)
        }
        try {
            //really FileInputStream ctor throws
            Io.readFrom(FileInputStream(file))
            fail("Unreachable")
        } catch (e: IOException) {
            assertNotNull("Thrown exception", e)
        }
    }

    @Test
    fun testEmpty() {
        val file = createFile(0, 0)
        try {
            Io.readFrom(file)
            fail("Unreachable")
        } catch (e: IOException) {
            assertNotNull("Thrown exception", e)
        }
        val stream: InputStream = FileInputStream(file)
        try {
            Io.readFrom(stream)
            fail("Unreachable")
        } catch (e: IOException) {
            assertNotNull("Thrown exception", e)
        }
    }

    @Test
    fun test1k() {
        val size = 1024
        assertTrue(size < Io.INITIAL_BUFFER_SIZE)
        assertTrue(size < Io.MIN_MMAPED_FILE_SIZE)
        val file = createFile(1, size)
        Io.readFrom(file).let { buf ->
            assertTrue(buf.isDirect)
            assertFalse(buf is MappedByteBuffer)
            checkBuffer(buf, 1, size)
        }
        Io.readFrom(FileInputStream(file)).let { buf ->
            assertFalse(buf.isDirect)
            checkBuffer(buf, 1, size)
        }
        Io.readFrom(FileInputStream(file), size.toLong()).let { buf ->
            assertTrue(buf.isDirect)
            checkBuffer(buf, 1, size)
        }
    }

    @Test
    fun test300k() {
        val size = 300 * 1024
        assertTrue(size > Io.INITIAL_BUFFER_SIZE)
        assertTrue(size > Io.MIN_MMAPED_FILE_SIZE)
        val file = createFile(2, size)
        Io.readFrom(file).let { buf ->
            assertTrue(buf.isDirect)
            assertTrue(buf is MappedByteBuffer)
            checkBuffer(buf, 2, size)
        }
        Io.readFrom(FileInputStream(file)).let { buf ->
            assertFalse(buf.isDirect)
            checkBuffer(buf, 2, size)
        }
        Io.readFrom(FileInputStream(file), size.toLong()).let { buf ->
            assertTrue(buf.isDirect)
            checkBuffer(buf, 2, size)
        }
    }

    @Test
    fun testWrongSizeHint() {
        val file = createFile(3, 2 * 1024)
        checkBuffer(Io.readFrom(FileInputStream(file)), 3, 2 * 1024)
        try {
            Io.readFrom(FileInputStream(file), 1024)
            fail("Unreachable")
        } catch (e: IOException) {
            assertNotNull("Thrown exception", e)
        }
        try {
            Io.readFrom(FileInputStream(file), (3 * 1024).toLong())
            fail("Unreachable")
        } catch (e: IOException) {
            assertNotNull("Thrown exception", e)
        }
    }
}

private fun generate(fill: Int, size: Int, obj: File) = FileOutputStream(obj).use { out ->
    repeat(size) {
        out.write(fill)
    }
}

private fun createFile(fill: Int, size: Int): File {
    val result = File.createTempFile("test", "io")
    if (size != 0) {
        generate(fill, size, result)
    }
    assertTrue(result.canRead())
    assertEquals(size.toLong(), result.length())
    return result
}

private fun checkBuffer(buf: ByteBuffer, fill: Int, size: Int) {
    assertEquals(size.toLong(), buf.limit().toLong())
    for (idx in 0 until size) {
        assertEquals(fill.toLong(), buf[idx].toLong())
    }
}
