/**
 * @file
 * @brief Catalog interface
 * @author vitamin.caig@gmail.com
 */

package app.zxtune.fs.amp

import android.content.Context
import app.zxtune.fs.http.MultisourceHttpProvider
import java.io.IOException

interface Catalog {

    abstract class WithCountHint {
        open fun setCountHint(count: Int) {
        }
    }

    abstract class GroupsVisitor : WithCountHint() {
        abstract fun accept(obj: Group)
    }

    abstract class AuthorsVisitor : WithCountHint() {
        abstract fun accept(obj: Author)
    }

    abstract class TracksVisitor : WithCountHint() {
        abstract fun accept(obj: Track)
    }

    abstract class FoundTracksVisitor : WithCountHint() {
        abstract fun accept(author: Author, track: Track)
    }

    /**
     * Query all groups
     * @param visitor result receiver
     */
    @Throws(IOException::class)
    fun queryGroups(visitor: GroupsVisitor)

    /**
     * Query authors by handle filter
     * @param handleFilter letter(s) or '0-9' for non-letter entries
     * @param visitor result receiver
     */
    @Throws(IOException::class)
    fun queryAuthors(handleFilter: String, visitor: AuthorsVisitor)

    /**
     * Query authors by country id
     * @param country scope
     * @param visitor result receiver
     */
    @Throws(IOException::class)
    fun queryAuthors(country: Country, visitor: AuthorsVisitor)

    /**
     * Query authors by group id
     * @param group scope
     * @param visitor result receiver
     */
    @Throws(IOException::class)
    fun queryAuthors(group: Group, visitor: AuthorsVisitor)

    /**
     * Query authors's tracks
     * @param author scope
     * @param visitor result receiver
     */
    @Throws(IOException::class)
    fun queryTracks(author: Author, visitor: TracksVisitor)

    /**
     * Find tracks by query substring
     * @param query string to search in filename/title
     * @param visitor result receiver
     */
    @Throws(IOException::class)
    fun findTracks(query: String, visitor: FoundTracksVisitor)

    companion object {
        const val NON_LETTER_FILTER = "0-9"

        @JvmStatic
        fun create(context: Context, http: MultisourceHttpProvider): CachingCatalog {
            val remote = RemoteCatalog(http)
            val db = Database(context)
            return CachingCatalog(remote, db)
        }
    }
}
