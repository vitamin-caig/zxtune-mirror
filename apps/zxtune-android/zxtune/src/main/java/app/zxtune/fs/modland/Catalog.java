/**
 * @file
 * @brief Catalog interface
 * @author vitamin.caig@gmail.com
 */

package app.zxtune.fs.modland;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.io.IOException;
import java.nio.ByteBuffer;

import app.zxtune.fs.HttpProvider;
import app.zxtune.fs.cache.CacheDir;

public abstract class Catalog {

  public abstract static class GroupsVisitor {

    public void setCountHint(int size) {
    }

    public abstract void accept(Group obj);
  }

  public abstract static class TracksVisitor {

    public void setCountHint(int size) {
    }

    //too many tracks possible, so enable breaking
    public abstract boolean accept(Track obj);
  }

  public interface Grouping {

    /**
     * Query set of group objects by filter
     * @param filter letter(s) or '#' for non-letter entries
     * @param visitor result receiver
     */
    void query(String filter, GroupsVisitor visitor) throws IOException;

    /**
     * Query single group object
     * @param id object identifier
     * @return null if no object found
     */
    @Nullable
    Group query(int id) throws IOException;

    /**
     * Query group's tracks
     * @param id object identifier
     * @param visitor result receiver
     */
    void queryTracks(int id, TracksVisitor visitor) throws IOException;

    /**
     * Query track by name
     * @param id object identifier
     * @param filename track filename
     * @return null if nothing found
     */
    @Nullable
    Track findTrack(int id, String filename) throws IOException;
  }

  public abstract Grouping getAuthors();

  public abstract Grouping getCollections();

  public abstract Grouping getFormats();

  /**
   * Get track file content
   * @param path path to module starting from /pub/..
   * @return content
   */
  @NonNull
  public abstract ByteBuffer getTrackContent(String path) throws IOException;

  public static Catalog create(Context context, HttpProvider http, CacheDir cache) throws IOException {
    final Catalog remote = new RemoteCatalog(http);
    final Database db = new Database(context);
    return new CachingCatalog(remote, db, cache);
  }
}
