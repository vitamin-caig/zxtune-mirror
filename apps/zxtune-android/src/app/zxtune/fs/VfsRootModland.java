/**
 *
 * @file
 *
 * @brief Implementation of VfsRoot over ftp://ftp.modland.com catalogue
 *
 * @author vitamin.caig@gmail.com
 *
 */

package app.zxtune.fs;

/**
 *
 * Paths:
 *
 * 1) modland:/
 * 2) modland:/Authors
 * 3) modland:/Authors/${author_letter}
 * 4) modland:/Authors/${author_letter}/${author_name}?id=${id}
 * 5) modland:/Collections
 * 6) modland:/Collections/${collection_letter}
 * 7) modland:/Collections/${collection_letter}/${collection_name}?id=${id}
 * 8) modland:/Formats
 * 9) modland:/Formats/${format_letter}
 *10) modland:/Formats/${format_letter}/${format_name}?id=${id}
 */

import android.content.Context;
import android.net.Uri;
import android.text.format.Formatter;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;

import app.zxtune.Log;
import app.zxtune.R;
import app.zxtune.fs.modland.Catalog;
import app.zxtune.fs.modland.Group;
import app.zxtune.fs.modland.Track;
import app.zxtune.ui.IconSource;

final class VfsRootModland implements VfsRoot, IconSource {

  private final static String TAG = VfsRootModland.class.getName();

  private final static String SCHEME = "modland";

  private static class Storage {
    private final static String SCHEME = "ftp";
    private final static String HOST = "ftp.modland.com";

    static boolean checkUri(Uri uri) {
      return SCHEME.equals(uri.getScheme()) && HOST.equals(uri.getHost());
    }

    static Uri.Builder makeUri() {
      return new Uri.Builder().scheme(SCHEME)
        .authority(HOST);
    }
  }

  private final static int POS_CATEGORY = 0;
  private final static int POS_LETTER = 1;
  private final static int POS_NAME = 2;
  private final static int POS_FILENAME = 3;

  private final static String PARAM_ID = "id";

  private final static String NOT_LETTER = "#";

  private final Context context;
  private final Catalog catalog;
  private final GroupsDir groups[];

  VfsRootModland(Context context) {
    this.context = context;
    this.catalog = Catalog.create(context);
    this.groups = new GroupsDir[] {
      new GroupsDir("Authors",
        R.string.vfs_modland_authors_name, R.string.vfs_modland_authors_description,
        catalog.getAuthors()),
      new GroupsDir("Collections",
        R.string.vfs_modland_collections_name, R.string.vfs_modland_collections_description,
        catalog.getCollections()),
      new GroupsDir("Formats",
        R.string.vfs_modland_formats_name, R.string.vfs_modland_formats_description,
        catalog.getFormats())
    };
  }

  @Override
  public Uri getUri() {
    return rootUri().build();
  }

  @Override
  public String getName() {
    return context.getString(R.string.vfs_modland_root_name);
  }

  @Override
  public String getDescription() {
    return context.getString(R.string.vfs_modland_root_description);
  }

  @Override
  public VfsObject getParent() {
    return null;
  }

  @Override
  public void enumerate(Visitor visitor) throws IOException {
    for (GroupsDir group : groups) {
      visitor.onDir(group);
    }
  }

  @Override
  public VfsObject resolve(Uri uri) throws IOException {
    if (SCHEME.equals(uri.getScheme())) {
      return resolvePath(uri);
    } else if (Storage.checkUri(uri)) {
      final Track track = new Track(uri.getEncodedPath(), 0);
      return new FreeTrackFile(track);
    }
    return null;
  }

  @Override
  public int getResourceId() {
    return R.drawable.ic_browser_vfs_modland;
  }
  
  private Uri.Builder rootUri() {
    return new Uri.Builder().scheme(SCHEME);
  }

  private boolean isLetter(char c) {
    return (Character.isLetter(c) && Character.isUpperCase(c));
  }

  private VfsObject resolvePath(Uri uri) throws IOException {
    final List<String> path = uri.getPathSegments();
    if (path.isEmpty()) {
      return this;
    } else {
      final String category = path.get(POS_CATEGORY);
      for (GroupsDir group : groups) {
        if (category.equals(group.getPath())) {
          return group.resolve(uri, path);
        }
      }
      return null;
    }
  }

  private class GroupsDir implements VfsDir {

    private final String path;
    private final int nameRes;
    private final int descRes;
    private final Catalog.Grouping group;

    GroupsDir(String path, int nameRes, int descRes, Catalog.Grouping group) {
      this.path = path;
      this.nameRes = nameRes;
      this.descRes = descRes;
      this.group = group;
    }

    final String getPath() {
      return path;
    }

    @Override
    public Uri getUri() {
      return groupsUri().build();
    }

    @Override
    public String getName() {
      return context.getString(nameRes);
    }

    @Override
    public String getDescription() {
      return context.getString(descRes);
    }

    @Override
    public VfsObject getParent() {
      return VfsRootModland.this;
    }

    @Override
    public void enumerate(Visitor visitor) throws IOException {
      visitor.onDir(new GroupByLetterDir(NOT_LETTER));
      for (char c = 'A'; c <= 'Z'; ++c) {
        visitor.onDir(new GroupByLetterDir(String.valueOf(c)));
      }
    }

    final VfsObject resolve(Uri uri, List<String> path) throws IOException {
      if (POS_CATEGORY == path.size() - 1) {
        return this;
      } else {
        final String letter = Uri.decode(path.get(POS_LETTER));
        if (letter.equals(NOT_LETTER) || (letter.length() == 1 && isLetter(letter.charAt(0)))) {
          return new GroupByLetterDir(letter).resolve(uri, path);
        } else {
          return null;
        }
      }
    }

    final Uri.Builder groupsUri() {
      return rootUri().appendPath(path);
    }

    private class GroupByLetterDir extends StubObject implements VfsDir {

      private final String letter;

      GroupByLetterDir(String letter) {
        this.letter = letter;
      }

      @Override
      public Uri getUri() {
        return groupLetterUri().build();
      }

      @Override
      public String getName() {
        return letter;
      }

      @Override
      public VfsObject getParent() {
        return GroupsDir.this;
      }

      @Override
      public void enumerate(final Visitor visitor) throws IOException {
        group.query(letter, new Catalog.GroupsVisitor() {

          @Override
          public void setCountHint(int count) {
            visitor.onItemsCount(count);
          }

          @Override
          public void accept(Group obj) {
            visitor.onDir(new GroupDir(obj));
          }
        });
      }

      final VfsObject resolve(Uri uri, List<String> path) throws IOException {
        if (POS_LETTER == path.size() - 1) {
          return this;
        } else {
          try {
            final int id = Integer.parseInt(uri.getQueryParameter(PARAM_ID));
            final Group obj = group.query(id);
            return obj != null
                    ? new GroupDir(obj).resolve(uri, path)
                    : null;
          } catch (IOException e) {
            throw e;
          } catch (Exception e) {
            Log.d(TAG, e, "resolve %s", uri);
            return null;
          }
        }
      }

      final Uri.Builder groupLetterUri() {
        return groupsUri().appendPath(letter);
      }

      private class GroupDir implements VfsDir {

        private Group obj;

        GroupDir(Group obj) {
          this.obj = obj;
        }

        @Override
        public Uri getUri() {
          return groupUri().build();
        }

        @Override
        public String getName() {
          return obj.name;
        }

        @Override
        public String getDescription() {
          return context.getResources().getQuantityString(R.plurals.tracks, obj.tracks, obj.tracks);
        }

        @Override
        public VfsObject getParent() {
          return GroupByLetterDir.this;
        }

        @Override
        public void enumerate(final Visitor visitor) throws IOException {
          group.queryTracks(obj.id, new Catalog.TracksVisitor() {
            
            @Override
            public void setCountHint(int count) {
              visitor.onItemsCount(count);
            }
            
            @Override
            public boolean accept(Track obj) {
              visitor.onFile(new TrackFile(obj));
              return true;
            }
          });
        }

        final VfsObject resolve(Uri uri, List<String> path) throws IOException {
          if (POS_NAME == path.size() - 1) {
            return this;
          } else {
            final String filename = path.get(POS_FILENAME);
            final Track track = group.findTrack(obj.id, filename);
            return track != null
              ? new TrackFile(track)
              : null;
          }
        }

        final Uri.Builder groupUri() {
          return groupLetterUri()
            .appendPath(obj.name)
            .appendQueryParameter(PARAM_ID, String.valueOf(obj.id));
        }
        
        private class TrackFile extends FreeTrackFile {

          TrackFile(Track track) {
            super(track);
          }
          
          @Override
          public VfsObject getParent() {
            return GroupDir.this;
          }

          @Override
          Uri.Builder trackUri() {
            return groupUri()
              .appendPath(track.filename);
          }
        }
      }
    }
  }

  private class FreeTrackFile extends StubObject implements VfsFile {

    protected final Track track;

    FreeTrackFile(Track track) {
      this.track = track;
    }

    @Override
    public Uri getUri() {
      return trackUri().build();
    }

    @Override
    public String getName() {
      return track.filename;
    }
    
    @Override
    public VfsObject getParent() {
      return null;
    }

    @Override
    public String getSize() {
      return Formatter.formatShortFileSize(context, track.size);
    }

    @Override
    public ByteBuffer getContent() throws IOException {
      return catalog.getTrackContent(track.path);
    }
    
    Uri.Builder trackUri() {
      return Storage.makeUri().encodedPath(track.path);
    }
  }
}
