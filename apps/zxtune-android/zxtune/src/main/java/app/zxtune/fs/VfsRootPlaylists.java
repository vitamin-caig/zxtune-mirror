/**
 * @file
 * @brief
 * @author vitamin.caig@gmail.com
 */
package app.zxtune.fs;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.Nullable;

import java.util.List;

import app.zxtune.R;
import app.zxtune.playlist.XspfStorage;

final class VfsRootPlaylists extends StubObject implements VfsRoot {

  static final String SCHEME = "playlists";

  private final Context context;
  private final XspfStorage storage;

  VfsRootPlaylists(Context context) {
    this.context = context;
    this.storage = new XspfStorage(context);
  }

  @Override
  public Uri getUri() {
    return rootUriBuilder().build();
  }

  @Override
  public String getName() {
    return context.getString(R.string.vfs_playlists_root_name);
  }

  @Override
  public String getDescription() {
    return context.getString(R.string.vfs_playlists_root_description);
  }

  @Override
  @Nullable
  public VfsObject getParent() {
    return null;
  }

  @Override
  public void enumerate(Visitor visitor) {
    for (String name : storage.enumeratePlaylists()) {
      visitor.onFile(new PlaylistFile(name));
    }
  }

  private static Uri.Builder rootUriBuilder() {
    return new Uri.Builder().scheme(SCHEME);
  }

  @Override
  @Nullable
  public VfsObject resolve(Uri uri) {
    if (SCHEME.equals(uri.getScheme())) {
      final List<String> path = uri.getPathSegments();
      if (path.isEmpty()) {
        return this;
      } else if (path.size() == 1) {
        final String name = path.get(0);
        return new PlaylistFile(name);
      }
    }
    return null;
  }

  private class PlaylistFile extends StubObject implements VfsFile {

    private final String name;

    PlaylistFile(String name) {
      this.name = name;
    }

    @Override
    public Uri getUri() {
      return rootUriBuilder().appendPath(name).build();
    }

    @Override
    public String getName() {
      return name;
    }

    @Override
    public VfsObject getParent() {
      return VfsRootPlaylists.this;
    }

    @Override
    public String getSize() {
      return getDescription();
    }

    @Override
    @Nullable
    public Object getExtension(String id) {
      if (VfsExtensions.FILE.equals(id)) {
        return storage.findPlaylistFile(name);
      } else {
        return super.getExtension(id);
      }
    }
  }
}
